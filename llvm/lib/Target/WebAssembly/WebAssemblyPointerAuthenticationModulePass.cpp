//===- WebAssemblyPointerAuthentication.cpp - Memory Safety for WASM --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//

// TODO: remove unnecessary includes
#include "WebAssembly.h"
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

#define DEBUG_TYPE "wasm-pointer-authentication-module"

namespace {

class WebAssemblyPointerAuthenticationModule : public ModulePass {

public:
  static char ID;

  WebAssemblyPointerAuthenticationModule() : ModulePass(ID) {
    initializeWebAssemblyPointerAuthenticationModulePass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override;

  StringRef getPassName() const override { return "WebAssembly Pointer Authentication Module"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<AAResultsWrapperPass>();
    AU.setPreservesCFG();
  }
};

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

  std::cout << "  Value \"" << V.getName().str() << "\" is aliased by:" << std::endl;
  for (BasicBlock &BB : F) {
    for (Value &OtherValue : BB) {
      // Only iterate on all other values
      if (&V == &OtherValue) {
        continue;
      }

      // if (auto OtherValue = dyn_cast<Value>(&I)) {
        // TODO: !isNoAlias is more conservative than isMustAlias, because we also ignore cases of isMaybeAlias; either way non-deterministic?
        // if (!AA.isNoAlias(&V, OtherValue)) {
        // // if (AA.isMustAlias(&V, OtherValue)) {
        //   std::cout << "  Other value \"" << OtherValue->getName().str() << "\"" << std::endl;
        //   Aliases.emplace_back(OtherValue);
        // }
      // }

      AliasResult aliasResult = AA.alias(&V, &OtherValue);
      // if (!AA.isNoAlias()) {
      if (aliasResult != AliasResult::NoAlias) {
        std::cout << "    Other value \"" << OtherValue.getName().str() << "\" is a: " << getAliasResultString(aliasResult) << std::endl;

        Aliases.emplace_back(&OtherValue);
      }
    }
  }

  // // Perform alias analysis on the instruction or value
  // AliasSetTracker AST(AA);
  // AST.add(*InstToAnalyze);
  // AST.complete();

  // // Iterate over alias sets and print the aliasing values
  // for (llvm::AliasSet& AS : AST) {
  //   for (llvm::AliasSet::iterator I = AS.begin(), E = AS.end(); I != E; ++I) {
  //     llvm::Value* AliasedValue = *I;
  //     // Do something with the aliased value
  //   }
  // }

}

// We define a function as external if it is declared, but not defined, in
// the current module.
bool isExternalFunction(Function &F, Function &BaseFunction) {
  std::cout << "==== Checking whether function \"" << F.getName().str() << "\" is an external function." << std::endl;

  if (F.isDeclaration() && !F.isIntrinsic()) {
    std::cout << "==== From the base function: " << BaseFunction.getName().str() << " Function \"" << F.getName().str() << "\" is an external function." << std::endl;
    // Check if the function has any external linkage
    // if (F.hasExternalLinkage() || F.hasAvailableExternallyLinkage()) {
    if (F.hasExternalLinkage()) {
      std::cout << "==== with external linkage" << std::endl;
      return true;
    }


  }
  std::cout << "==== From the base function: " << BaseFunction.getName().str() << " Function \"" << F.getName().str() << "\" is NOT an external function." << std::endl;
  return false;
}

// Find all function calls that use the specified value as an argument.
// Once we found a function, we also have to recursively find all of
// the functions that use that function('s return value).
void findAllFunctionsWhereValueIsPassedAsArgument(Value &V, SmallVector<Function*, 8> &FunctionCalls) {
  // std::cout << "  Value \"" << V.getName().str() << "\" is used in functions:" << std::endl;
  errs() << "  Value \"" << V << "\" is used in functions:\n";

  for (User *U : V.users()) {
    // TODO: we can't only consider function users, we also have to consider e.g. normal loads and stores, which are not function calls
    if (CallInst *CI = dyn_cast<CallInst>(U)) {
      for (Value *Arg : CI->args()) {
        if (Arg == &V) {
          std::cout << "    Function \"" << CI->getCalledFunction()->getName().str() << "\"" << std::endl;

          FunctionCalls.emplace_back(CI->getCalledFunction());

          // If our value is being passed as an argument to another function, we need to check that, in that other function, the pointer has no other uses as well.
          // if (pointerAuthenticationIsSuitable(&V, CI->getCalledFunction(), AA)) {
          //   return
          // }

          // TODO: only do this if the original value, that was passed to another function, is again returned from this function
          // TODO: would a simple "if (CI == &V)" work?
          // Recursively checks if any other functions use the function's return value
          findAllFunctionsWhereValueIsPassedAsArgument(*CI, FunctionCalls);

          break;
        }
      }
    }
  }
}

// A pointer has other uses if it is used as a parameter by external functions.
bool valueHasOtherUses(Value &Pointer, Function &F, AliasAnalysis &AA) {
  SmallVector<Function*, 8> functionsUsingPointer;
  findAllFunctionsWhereValueIsPassedAsArgument(Pointer, functionsUsingPointer);

  // TODO: !functionsOutsideModuleUsingPointer.empty() vs assert that all functionsUsingPointer are from this module
  for (auto function: functionsUsingPointer) {
    // if (isExternalFunction(*function)) {
    if (isExternalFunction(*function, F)) {
      return true;
    }
    // TODO: we need a module pass with alias analysis having been performed on the entire module,
    // since now we are basically analysing another function, and the alias analysis is not guaranteed to have run over this until now.

    // If our value is being passed as an argument to another function, we need to check that, in that other function, the pointer has no other uses as well.
    // if (pointerAuthenticationIsSuitable())
  }

  return false;
}

// TODO: consider rephrasing to "is not used elsewhere"

// Pointer Authentication Rules:
//
// A pointer (value) is suitable for pointer authentication, if it has no
// other uses.
// A value has other uses if it is used as a parameter by other functions in
// the same module.
// A pointer can never be suitable if there exist aliases to it.
//
// Rule relaxations:
// 1. We consider a pointer with aliases suitable, if all of its aliases
//    are also suitable.
bool pointerAuthenticationIsSuitable(Value &Pointer, Function &F, AliasAnalysis &AA) {
  SmallVector<Value*, 8> Aliases;
  findAllAliasesOfValue(Pointer, Aliases, AA, F);

  // If any of the aliases are disallowed, then all of the aliases should be disallowed
  for (auto Alias : Aliases) {
    if (valueHasOtherUses(*Alias, F, AA)) {
      return false;
    }
  }

  return true;
}

// TODO: what does the return bool mean?
bool authenticateStoredAndLoadedPointers(Function &F, AliasAnalysis &AA) {
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
          if (pointerAuthenticationIsSuitable(*PointerValueToStore, F, AA)) {
            // We shouldn't mutate the instructions we are iterating over
            StorePointerInsts.emplace_back(SI);
          }
        }
      } else
      if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
        // Load(ptr): The data value located at the memory address pointed to by $ptr is returned
        // Check if value to be loaded from memory is a pointer
        if (LI->getType()->isPointerTy()) {
          if (pointerAuthenticationIsSuitable(*LI, F, AA)) {
            // We shouldn't mutate the instructions we are iterating over
            LoadPointerInsts.emplace_back(LI);
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

bool WebAssemblyPointerAuthenticationModule::runOnModule(Module &M) {

  // TODO: figure out what to return
  bool result = true;

  for (Function &F : M) {
    std::cout << "Function: " << F.getName().str() << std::endl;

    AliasAnalysis &AA = getAnalysis<AAResultsWrapperPass>(F).getAAResults();

    // TODO: use the return value somehow
    authenticateStoredAndLoadedPointers(F, AA);
  }

  return result;
}

} // namespace

char WebAssemblyPointerAuthenticationModule::ID = 0;

INITIALIZE_PASS_BEGIN(WebAssemblyPointerAuthenticationModule, DEBUG_TYPE,
                      "WebAssembly Pointer Authentication Module Pass", false, false)
INITIALIZE_PASS_END(WebAssemblyPointerAuthenticationModule, DEBUG_TYPE,
                    "WebAssembly Pointer Authentication Module Pass", false, false)

ModulePass *llvm::createWebAssemblyPointerAuthenticationModulePass() {
  return new WebAssemblyPointerAuthenticationModule();
}

#undef DEBUG_TYPE
