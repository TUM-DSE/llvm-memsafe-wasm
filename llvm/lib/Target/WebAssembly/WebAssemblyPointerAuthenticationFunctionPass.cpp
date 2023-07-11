//===- WebAssemblyPointerAuthenticationFunctionPass.cpp - Pointer Authentication for WASM --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//

// TODO: remove unnecessary includes
#include "WebAssembly.h"
#include "Utils/WebAssemblyUtilities.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AliasSetTracker.h"
// #include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/StackSafetyAnalysis.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/LiveRegUnits.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsWebAssembly.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/MemoryTaggingSupport.h"
#include <algorithm>
#include <cassert>
#include <list>
#include <memory>
#include <utility>
#include <iostream>

using namespace llvm;

#define DEBUG_TYPE "wasm-pointer-authentication-function"

namespace {
class WebAssemblyPointerAuthenticationFunction final : public FunctionPass {
  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override { return "WebAssembly Pointer Authentication Function"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<AAResultsWrapperPass>();
    AU.setPreservesCFG();
  }

public:
  static char ID;
  // WebAssemblyPointerAuthenticationFunction() : FunctionPass(ID) {
  //   initializeWebAssemblyPointerAuthenticationFunctionPass(*PassRegistry::getPassRegistry());
  // }
  WebAssemblyPointerAuthenticationFunction() : FunctionPass(ID) {}
};
} // end anonymous namespace

char WebAssemblyPointerAuthenticationFunction::ID = 0;
INITIALIZE_PASS(WebAssemblyPointerAuthenticationFunction, DEBUG_TYPE,
                      "WebAssembly Pointer Authentication Function Pass", false, false)

FunctionPass *llvm::createWebAssemblyPointerAuthenticationFunctionPass() {
  return new WebAssemblyPointerAuthenticationFunction();
}

std::string getAliasResultString(AliasResult result) {
    switch (result) {
        case AliasResult::NoAlias:
            return "NoAlias";
        case AliasResult::MayAlias:
            return "MayAlias";
        case AliasResult::PartialAlias:
            return "PartialAlias";
        case AliasResult::MustAlias:
            return "MustAlias";
    }
}

// TODO: AliasAnalysis does not account for loops apparently => test
void findAllAliasesOfValue(Value &V, SmallVector<Value *, 8> &Aliases, AliasAnalysis &AA, Function &F) {
  // The pointer itself counts as one of its own aliases
  Aliases.emplace_back(&V);

  // std::cout << "  Value \"" << V.getName().str() << "\" is aliased by:" << std::endl;
  for (BasicBlock &BB : F) {
    for (Value &OtherValue : BB) {
      // Only iterate on all other values
      if (&V == &OtherValue) {
        continue;
      }

      AliasResult aliasResult = AA.alias(&V, &OtherValue);
      if (aliasResult != AliasResult::NoAlias) {
        // std::cout << "    Other value \"" << OtherValue.getName().str() << "\" is a: " << getAliasResultString(aliasResult) << std::endl;

        Aliases.emplace_back(&OtherValue);
      }
    }
  }
}

// We define a function as external if it is declared, but not defined, in
// the current module.
bool isExternalFunction(Function &F, Function &BaseFunction) {
  // std::cout << "==== Checking whether function \"" << F.getName().str() << "\" is an external function." << std::endl;

  if (F.isDeclaration() && !F.isIntrinsic()) {
    // std::cout << "==== From the base function: " << BaseFunction.getName().str() << " Function \"" << F.getName().str() << "\" is an external function." << std::endl;
    // Check if the function has any external linkage
    // if (F.hasExternalLinkage() || F.hasAvailableExternallyLinkage()) {
    if (F.hasExternalLinkage()) {
      // std::cout << "==== with external linkage" << std::endl;
      return true;
    }


  }
  // std::cout << "==== From the base function: " << BaseFunction.getName().str() << " Function \"" << F.getName().str() << "\" is NOT an external function." << std::endl;
  return false;
}

// void findAllFunctionsWhereValueIsPassedAsArgument(Value &V, SmallVector<Function*, 8> &FunctionCalls) {
//   for (User *U : V.users()) {
//     if (CallInst *CI = dyn_cast<CallInst>(U)) {
//       for (Value *Arg : CI->args()) {
//         if (Arg == &V) {
//           FunctionCalls.emplace_back(CI->getCalledFunction());
//         }
//       }
//     }
//     // Consider all users, and recurse on them, not just the functions with value as parameter
//     findAllFunctionsWhereValueIsPassedAsArgument(*U, FunctionCalls);
//   }
// }


// Find all function calls that use the specified value as an argument.
// Once we found a function, we also have to recursively find all of
// the functions that use that function('s return value).
void findAllFunctionsWhereValueIsPassedAsArgument(Value &V, SmallVector<Function*, 8> &FunctionCalls) {
  // std::cout << "  Value \"" << V.getName().str() << "\" is used in functions:" << std::endl;
  // errs() << "  Value \"" << V << "\" is used in functions:\n";

  for (User *U : V.users()) {
    // TODO: we can't only consider function users, we also have to consider e.g. normal loads and stores, which are not function calls
    // TODO: what if we add and subtract, and then use the new pointer to load => probably counts as an alias, but we could just recursively directly trace those calls
    if (CallInst *CI = dyn_cast<CallInst>(U)) {
      for (Value *Arg : CI->args()) {
        if (Arg == &V) {
          // std::cout << "    Function \"" << CI->getCalledFunction()->getName().str() << "\"" << std::endl;
          // errs() << "    Function: " << CI << "\n";

          FunctionCalls.emplace_back(CI->getCalledFunction());

          // If our value is being passed as an argument to another function, we need to check that, in that other function, the pointer has no other uses as well.
          // if (pointerAuthenticationIsSuitable(&V, CI->getCalledFunction(), AA)) {
          //   return
          // }

          // TODO: only do this if the original value, that was passed to another function, is again returned from this function
          // TODO: would a simple "if (CI == &V)" work?
          // Recursively checks if any other functions use the function's return value
          findAllFunctionsWhereValueIsPassedAsArgument(*CI, FunctionCalls);
        }
      }
    }
  }
}

// A value has other uses if it is passed as a function parameter to any other
// function.
//
// TODO: more tricky
// A pointer has other uses if it is used as a parameter by external functions.
bool valueHasOtherUses(Value &V, Function &F, AliasAnalysis &AA) {
  SmallVector<Function*, 8> functionsUsingValue;
  findAllFunctionsWhereValueIsPassedAsArgument(V, functionsUsingValue);

  return !functionsUsingValue.empty();
  // // TODO: !functionsOutsideModuleUsingPointer.empty() vs assert that all functionsUsingPointer are from this module
  // for (auto function: functionsUsingValue) {
  //   // if (isExternalFunction(*function)) {
  //   if (isExternalFunction(*function, F)) {
  //     return true;
  //   }
  //   // TODO: we need a module pass with alias analysis having been performed on the entire module,
  //   // since now we are basically analysing another function, and the alias analysis is not guaranteed to have run over this until now.

  //   // If our value is being passed as an argument to another function, we need to check that, recursively, in that other function, the pointer has no other uses as well.
  //   // if (pointerAuthenticationIsSuitable())
  // }

  // return false;
}

// Checks whether the value is a parameter of a function.
bool valueIsParameterOfFunction(Value &V, Function &F) {
  for (Argument &arg : F.args()) {
    if (&arg == &V) {
      return true;
    }
  }
  return false;
}

// Tracks all visited values, and skips recursive call if we have already
// visited a certain value before (to avoid endless recursion).
bool valueComesFromElsewhereHelper(Value &V, Function &ParentFunction, std::set<Value*> &VisitedValues) {
  errs() << "Checking value: " << V.getName().str() << "\n";

  auto [_, wasInserted] = VisitedValues.insert(&V);
  if (!wasInserted) {
    // We found a value we have seen before, so were are in some sort of loop (maybe alias?)
    // We want to be extra careful here, so we say it comes from elsewhere if there's some loop
    errs() << "Found value we have seen before: " << V.getName().str() << "; exiting to prevent infinite loop\n";
    // return true;
    return false;
  }

  if (valueIsParameterOfFunction(V, ParentFunction)) {
    errs() << "Value: " << V.getName().str() << " is the parameter of function: " << ParentFunction.getName() << "\n";
    return true;
  }

  // A global value could be used across different modules, so we can never control/know that global values aren't used elsewhere
  if (isa<GlobalValue>(&V)) {
    errs() << "Value: " << V.getName().str() << " is a global value\n";
    return true;
  }

  // Checks, recursively, whether a Value was returned by a function call.
  if (auto *I = dyn_cast<Instruction>(&V)) {
    // llvm::errs() << "Instruction: ";
    // I->print(llvm::errs());
    // llvm::errs() << "\n";

    // Check if instruction is (directly) the return value of a function call.
    if (isa<CallInst>(I)) {
      // errs() << "Instruction: " << I << " is the return value of a function call\n";
      return true;
    }
    // Check if value was loaded from a memory location.
    if (isa<LoadInst>(I)) {
      return true;
    }
    
    // Since `V` doesn't come from elsewhere directly, we have to
    // check whether any of the parameters/operands of the instruction `V`
    // come from elsewhere.
    // llvm::errs() << "  with operands: ";
    for (auto &Op : I->operands()) {
      // Op->print(llvm::errs());

      if (valueComesFromElsewhereHelper(*Op, ParentFunction, VisitedValues)) {
        return true;
      }
    }
    // llvm::errs() << "\n";
  }
  
  // errs() << "Value: " << V << " does not come from elsewhere\n";
  return false;
}

// Checks whether a value "comes from elsewhere".
// A value comes from elsewhere if any of the following conditions are met:
// 1. The value was passed as a parameter to the current function.
// 2. The value is the return value of any function.
// 3. The value was loaded from any memory location.
// In case the current value/instruction does not come from elsewhere, we also
// need to check whether any of its operands come from elsewhere.
bool valueComesFromElsewhere(Value &V, Function &ParentFunction) {
  std::set<Value*> VisitedValues;
  return valueComesFromElsewhereHelper(V, ParentFunction, VisitedValues);
}

// Pointer Authentication Rules:
//
// A pointer (value), that is being stored in or loaded from a memory location,
// is suitable for pointer authentication, if that memory location has no other
// uses and does not come from elsewhere.
// A pointer is not suitable for PA, if any of its aliases are not suitable for
// PA.
//
// TODO:
// Rule Relaxations (only possible with module pass):
// - A value only has other uses if it is passed as a function parameter to an
//   **external** function (aliases must still be accounted for though).
bool memoryLocationIsSuitableForPA(Value &MemoryLocation, Function &F, AliasAnalysis &AA) {
  SmallVector<Value*, 8> Aliases;
  findAllAliasesOfValue(MemoryLocation, Aliases, AA, F);

  // TODO: optimization possibility: cache the aliases that were already found to be non-suitable
  // If any of the aliases are not suitable, then all of the aliases should be not suitable
  for (auto Alias : Aliases) {
    if (valueHasOtherUses(*Alias, F, AA)) {
      return false;
    }

    if (valueComesFromElsewhere(*Alias, F)) {
      return false;
    }
  }

  return true;
}


// Pointer Authentication Rules for Store Instructions:
//
// A pointer (value) is suitable for being signed before being stored in a
// memory location, if that memory location has no other uses.
// A value has other uses if it is passed as a function parameter to any other
// function.
// A value also has other uses if any of its aliases have other uses.
//
// TODO:
// Rule Relaxations (only possible with module pass):
// - A value only has other uses if it is passed as a function parameter to an
//   **external** function (aliases must still be accounted for though).
bool storeIsSuitableForPA(Value &MemoryLocation, Function &F, AliasAnalysis &AA) {
  SmallVector<Value*, 8> Aliases;
  findAllAliasesOfValue(MemoryLocation, Aliases, AA, F);

  // If any of the aliases are disallowed, then all of the aliases should be disallowed
  for (auto Alias : Aliases) {
    if (valueHasOtherUses(*Alias, F, AA)) {
      return false;
    }
  }

  return true;
}

// Pointer Authentication Rules for Load Instructions:
//
// A pointer (value) is suitable for being authenticated after being loaded
// from a memory location, if that memory location did not originate from any
// other function.
// TODO: aliases
// TODO: only disallow if it comes from an external function
bool loadIsSuitableForPA(Value &MemoryLocation, Function &F, AliasAnalysis &AA) {
  return !valueComesFromElsewhere(MemoryLocation, F);
}

// TODO: what does the return bool mean?
bool authenticateStoredAndLoadedPointers(Function &F, AliasAnalysis &AA) {
  errs() << "=== Starting analysis on function: " << F.getName().str() << "\n";
  // F.dump();

  auto *PointerSignFunc = Intrinsic::getDeclaration(
      F.getParent(), Intrinsic::wasm_pointer_sign);
  auto *PointerAuthFunc = Intrinsic::getDeclaration(
      F.getParent(), Intrinsic::wasm_pointer_auth);

  SmallVector<StoreInst*, 8> StorePointerInsts;
  SmallVector<LoadInst*, 8> LoadPointerInsts;

  // Look for instructions that load/store a pointer
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (StoreInst *SI = dyn_cast<StoreInst>(&I)) {
        // Store(value, ptr): $value is stored at data address pointed to by $ptr
        // Check if value to be stored in memory is a pointer
        Value *PointerValueToStore = SI->getValueOperand();
        if (PointerValueToStore->getType()->isPointerTy()) {
          auto MemoryLocation = SI->getPointerOperand();
          errs() << "==== Checking if store: " << SI->getName().str() << " is suitable for PA\n";

          if (storeIsSuitableForPA(*MemoryLocation, F, AA)) {
            // errs() << "Store instruction: " << SI << " is suitable for pointer authentication\n";
            // We shouldn't mutate the instructions we are iterating over
            StorePointerInsts.emplace_back(SI);
          } else {
            // errs() << "Store instruction: " << SI << " is not suitable for pointer authentication\n";
          }
        }
      } else
      if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
        // Load(ptr): The data value located at the memory address pointed to by $ptr is returned
        // Check if value to be loaded from memory is a pointer
        if (LI->getType()->isPointerTy()) {
          auto MemoryLocation = LI->getPointerOperand();
          errs() << "==== Checking if load: " << LI->getName().str() << " is suitable for PA\n";

          if (loadIsSuitableForPA(*MemoryLocation, F, AA)) {
            // errs() << "Load instruction: " << LI << " is suitable for pointer authentication\n";
            // std::cout << "Load instruction: " << LI->getName().str() << " is suitable for pointer authentication\n";
            // We shouldn't mutate the instructions we are iterating over
            LoadPointerInsts.emplace_back(LI);
          } else {
            // errs() << "Load instruction: " << LI << " is not suitable for pointer authentication\n";
            // std::cout << "Load instruction: " << LI->getName().str() << " is not suitable for pointer authentication\n";
          }
        }
      }
    }
  }

  // Add pointer signing inst before pointer store inst
  for (auto SI : StorePointerInsts) {
    // TODO: consider maybe casting this to some sort of pointer type, just so we always know this value is indeed a pointer
    Value *PointerValueToStore = SI->getValueOperand();

    auto *PointerSignInst = CallInst::Create(PointerSignFunc, {PointerValueToStore});
    PointerSignInst->insertBefore(SI);

    // Replace the value operand in the store inst with the new signed value
    SI->setOperand(0, PointerSignInst);
  }

  // Add pointer authentication inst after pointer load inst
  for (auto LI : LoadPointerInsts) {
    auto *PointerAuthInst = CallInst::Create(PointerAuthFunc, {LI});
    PointerAuthInst->insertAfter(LI);

    // All further uses of the load's return value must use our authenticated pointer instead now
    LI->replaceUsesWithIf(PointerAuthInst, [&](Use &U) {
      return U.getUser() != PointerAuthInst;
    });
  }

  F.dump();

  return true;
}

bool WebAssemblyPointerAuthenticationFunction::runOnFunction(Function &F) {
  AliasAnalysis &AA = getAnalysis<AAResultsWrapperPass>().getAAResults();

  // F.dump();
  // return true;

  // errs() << "Function: " << F << "\n";
  // std::cout << "Function: " << F.getName().str() << std::endl;
  // TODO: use the return value somehow
  return authenticateStoredAndLoadedPointers(F, AA);
}
