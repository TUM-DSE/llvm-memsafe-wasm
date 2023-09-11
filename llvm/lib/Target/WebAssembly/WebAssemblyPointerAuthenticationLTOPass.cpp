//===- WebAssemblyPointerAuthenticationLTOPass.cpp - Pointer Authentication for WASM --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//

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

#define DEBUG_TYPE "wasm-pointer-authentication-lto"

namespace {

class WebAssemblyPointerAuthenticationLTO : public ModulePass {

public:
  static char ID;

  WebAssemblyPointerAuthenticationLTO() : ModulePass(ID) {
    initializeWebAssemblyPointerAuthenticationLTOPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override;

  StringRef getPassName() const override { return "WebAssembly Pointer Authentication LTO"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<AAResultsWrapperPass>();
    AU.setPreservesCFG();
  }

private:
  AliasResult alias(Value &V1, Value &V2, Function &F) {
    AliasAnalysis& AA = getAnalysis<AAResultsWrapperPass>(F).getAAResults();
    return AA.alias(&V1, &V2);
  }

  AliasAnalysis& getAliasAnalysisForFunction(Function &F) {
    return getAnalysis<AAResultsWrapperPass>(F).getAAResults();
  }

// TODO: remove
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


// TODO: a potential bug in our implementation i thought of: what if a pointer is stored in a function, and then passed to another function where it is loaded from? then, it will be signed in the original function, but never authed in the other function, because memory locations passed as parameters are always classified as coming from elsewhere, which makes sense, because other modules could use this function and we can't assume they will sign the memory lcoation's stored pointer.
// A solution to this would most likely be the LTO pass, because there we can be certain no-one else would use the function that should auth



// We define a function as external if it is declared, but not defined, in
// the current module.
bool isExternalFunction(Function &F, Module &ParentModule) {
  // std::cout << "==== Checking whether function \"" << F.getName().str() << "\" is an external function." << std::endl;

  // Sanity check: If the function belongs to a different module than the
  // one we are currently analyzing, then it is definitely an external function.
  if (F.getParent() != &ParentModule) {
    // errs() << "Found external function from a different module: " << F.getName() << "\n";
    return true;
  }

  if (F.isDeclaration() && !F.isIntrinsic()) {
    // errs() << "Found external function: " << F.getName() << "\n";
    // std::cout << "==== From the base function: " << BaseFunction.getName().str() << " Function \"" << F.getName().str() << "\" is an external function." << std::endl;
    // Check if the function has any external linkage
    // if (F.hasExternalLinkage() || F.hasAvailableExternallyLinkage()) {
    // if (F.hasExternalLinkage()) {
    //   // std::cout << "==== with external linkage" << std::endl;
    //   return true;
    // }

    return true;
  }
  // std::cout << "==== From the base function: " << BaseFunction.getName().str() << " Function \"" << F.getName().str() << "\" is NOT an external function." << std::endl;
  return false;
}

// TODO: think of edge case: if we have 2 functions, and they call each other, then that would be endless recursion => check visitied funcitons+values used to visit

// TODO: read somewhere that AliasAnalysis does not account for loops apparently => test
void findAllAliasesOfValue(Value &V, SmallVector<Value*, 8> &Aliases, Function &F) {
  // The pointer itself counts as one of its own aliases
  Aliases.emplace_back(&V);

  // std::cout << "  Value \"" << V.getName().str() << "\" is aliased by:" << std::endl;
  for (BasicBlock &BB : F) {
    for (Value &OtherValue : BB) {
      // Only iterate on all other values
      if (&V == &OtherValue) {
        continue;
      }

      // AliasResult aliasResult = AA.alias(&V, &OtherValue);
      AliasResult aliasResult = alias(V, OtherValue, F);
      if (aliasResult != AliasResult::NoAlias) {
      // if (!AA.isNoAlias(&V, &OtherValue)) {
        // std::cout << "    Other value \"" << OtherValue.getName().str() << "\" is a: " << getAliasResultString(aliasResult) << std::endl;

        Aliases.emplace_back(&OtherValue);
      }
    }
  }
}

// TODO: think about function pointers, what if those somehow call external functions

// Tracks all visited values, and skips recursive call if we have already
// visited a certain value before (to avoid endless recursion).
// If we encounter an unidentifiable function (e.g. function pointer,
// vararg function), then we immediately return false.
bool findAllFunctionsWhereValueIsPassedAsArgumentHelper(Value &V, SmallVector<Function*, 8> &FunctionCalls, std::set<Value*> &VisitedValues, Module &BaseModule, Function &BaseFunction) {
  // TODO: what about aliases? Are their users transitively counted as users as well? TEST this!!!
  auto [_, ValueWasInserted] = VisitedValues.insert(&V);
  if (!ValueWasInserted) {
    // errs() << "in find all functions where passed as param: Found value we have seen before: " << V.getName().str() << "; exiting to prevent infinite loop\n";
    // We found a value we have seen before, so were are in some sort of loop.
    // Therefore, we have already checked this value and all of its users.
    // We want to continue searching, so don't mark this as an error.
    return true;
  }

  // std::cout << "  Value \"" << V.getName().str() << "\" is used in functions:" << std::endl;
  // errs() << "  Value \"" << V << "\" is used in functions:\n";

  for (User *U : V.users()) {
    // TODO: we can't only consider function users, we also have to consider e.g. normal loads and stores, which are not function calls
    // TODO: what if we add and subtract, and then use the new pointer to load => probably counts as an alias, but we could just recursively directly trace those calls
    // We use CallBase to check for both InvokeInst and CallInst.
    if (auto *CI = dyn_cast<CallBase>(U)) {
      size_t ArgIndex = 0;
      for (Value *Arg : CI->args()) {
        if (Arg == &V) {
          // std::cout << "    Function \"" << CI->getCalledFunction()->getName().str() << "\"" << std::endl;
          // errs() << "    Function: " << CI << "\n";

          auto PassedToFunction = CI->getCalledFunction();
          errs() << "Passed to function " << PassedToFunction->getName() << "\n";

          // This might occur if the CallInst we tried to convert to a Function
          // didn't have a known function signature at compile-time, e.g. because
          // it was a function pointer, or if the argument size doesn't match
          // the argument index, indicating a vararg function. We can't track
          // function pointers further, so we are conservative and mark this
          // as potentially calling external functions.
          if (PassedToFunction == nullptr || PassedToFunction->arg_size() <= ArgIndex) {
            // errs() << "We found a function that takes as value but we don't want to handle: " << PassedToFunction->getName() << "\n";
            errs() << "1\n";
            return false;
          }

          // errs() << "We found a function that was not a nullptr\n";

          // We only add valid function calls to the vector.
          assert(PassedToFunction != nullptr);
          FunctionCalls.emplace_back(PassedToFunction);

          // We need to check if our value has other users in the function
          // it is passed to.
          Value *ValueAsArg = PassedToFunction->getArg(ArgIndex);

          // The actual Value we passed to the function from another function
          // differs from the parameter Value used inside the function.
          assert(&V != ValueAsArg);

          // errs() << "Since Value " << V << " was passed to function " << PassedToFunction->getName() << " we need to recursively follow that function\n";
          // findAllFunctionsWhereValueIsPassedAsArgumentHelper(*ValueAsArg, FunctionCalls, VisitedValues);

          // TODO: This is DFS, moving this to a second for loop would make it BFS, which might be more efficient in our case.
          // TODO: actually, this wouldn't work, since we would also check valuecomesfromelsewhere, which would always be true, since it is the parameter

          // The reason we have to thoroughly analyze the Value passed to the
          // Function is that it might again have aliases inside that function,
          // which we would not detect with just a recursive call to
          // findAllFunctionsWhereValueIsPassedAsArgumentHelper.
          // However, passing to a recursive function is fine, since we already
          // analyzed and counted that function here.
          if (PassedToFunction == &BaseFunction) {
            continue;
          }
          // if (!memoryLocationIsSuitableForPA(*ValueAsArg, *PassedToFunction, BaseModule)) {
          // TODO: fixed bug here
          if (valueHasOtherUsesWithAA(*ValueAsArg, *PassedToFunction, BaseModule)) {
            // errs() << "This will always be not suitable since it's the argument of the function. part 2\n";
            errs() << "2\n";
            return false;
          }

          // Continue iterating over parameters even if we found our Value,
          // since the same Value could be passed multiple times to the
          // same function.
        }

        ++ArgIndex;
      }
    }
    // Also add all other functions that use the function's return value
    if (!findAllFunctionsWhereValueIsPassedAsArgumentHelper(*U, FunctionCalls, VisitedValues, BaseModule, BaseFunction)) {
      errs() << "3\n";
      return false;
    }
  }

  return true;
}

void printSmallVector(const llvm::SmallVectorImpl<Function*> &vec) {
    for (const auto &item : vec) {
        // llvm::errs() << item->getName() << " ";
    }
    // llvm::errs() << "\n";
}

// Find all function calls that use the specified value as an argument.
// Once we found a function, we also have to recursively find all of
// the functions that use that function('s return value).
// Additionally, returns false if we directly find some function we could
// not analyze further, and therefore classify as external.
bool findAllFunctionsWhereValueIsPassedAsArgument(Value &V, SmallVector<Function*, 8> &FunctionCalls, Module &BaseModule, Function &BaseFunction) {
  std::set<Value*> VisitedValues;
  // return findAllFunctionsWhereValueIsPassedAsArgumentHelper(V, FunctionCalls, VisitedValues, BaseModule);
  auto boolean = findAllFunctionsWhereValueIsPassedAsArgumentHelper(V, FunctionCalls, VisitedValues, BaseModule, BaseFunction);
  if (FunctionCalls.size() != 0) {
    // errs() << "found all functions where value is passed as arg: Val: " << V << "\n";
    printSmallVector(FunctionCalls);
  }
  return boolean;
}

// TODO: discuss depth first search (what we do) vs breadth first search in thesis

// A value has other uses if it is recursively passed as a function parameter
// to an external function.
// Therefore, once we see that a value is passed to a non-external function,
// we still need to check if the value has other uses in in that function.
// This function does not perform any Alias Analysis.
bool valueHasOtherUsesWithoutAA(Value &Value, Function &F, Module &BaseModule) {
  SmallVector<Function*, 8> FunctionsUsingValue;
  if (!findAllFunctionsWhereValueIsPassedAsArgument(Value, FunctionsUsingValue, BaseModule, F)) {
    // While searching for the functions, we already encountered some
    // error/invalid function that uses the Value, so we immediately return.
    errs() << "encountered error during find all functions\n";
    return true;
  }

  // TODO: !functionsOutsideModuleUsingPointer.empty() vs assert that all functionsUsingPointer are from this module
  for (auto FunctionUsingValue: FunctionsUsingValue) {
    // if (isExternalFunction(*function)) {
    if (isExternalFunction(*FunctionUsingValue, BaseModule)) {
      return true;
    }
  }

  // TODO: optimization: don't find all functions, just the first one that is 
  // // TODO: !functionsOutsideModuleUsingPointer.empty() vs assert that all functionsUsingPointer are from this module

  return false;
}

// This is used instead of the WithoutAA variant when Alias Analysis is
// required, but valueComesFromElsewhere should not be bundled in.
bool valueHasOtherUsesWithAA(Value &V, Function &F, Module &BaseModule) {
  SmallVector<Value*, 8> Aliases;
  findAllAliasesOfValue(V, Aliases, F);

  // TODO: optimization possibility: cache the aliases that were already found to be non-suitable
  // If any of the aliases are not suitable, then all of the aliases should be not suitable
  for (auto Alias : Aliases) {
    if (valueHasOtherUsesWithoutAA(*Alias, F, BaseModule)) {
      // errs() << "This will always be not suitable since it's the argument of the function. part 1\n";
      return true;
    }
  }

  return false;
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
  // errs() << "Checking value: " << V.getName().str() << "\n";

  auto [_, ValueWasInserted] = VisitedValues.insert(&V);
  if (!ValueWasInserted) {
    // We found a value we have seen before, so were are in some sort of loop.
    // Therefore, we continue searching, but skip re-entering the loop.
    errs() << "Found value we have seen before: " << V.getName().str() << "; exiting to prevent infinite loop\n";
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
    // Check if instruction is (directly) the return value of a function call.
    // TODO: check for superclass CallBase instead
    // if (isa<CallInst>(I)) {
    if (isa<CallBase>(I)) {
      // errs() << "Instruction: " << I << " is the return value of a function call\n";
      // return true;

      CallBase *call = cast<CallBase>(I);
      if (Function *calledFunction = call->getCalledFunction()) {
        errs() << "Instruction: " << I << " is the return value of a function call to " << calledFunction->getName() << "\n";
      } else {
        errs() << "Instruction: " << I << " is the return value of an indirect function call\n";
      }
      return true;
    }
    // // Check if value was loaded from a memory location, i.e. value is the
    // // return value of a load instruction.
    // if (isa<LoadInst>(I)) {
    //   errs() << "Instruction: " << I->getName() << " is a load inst\n";
    //   return true;
    // }

    // Check if value was loaded from a memory location, i.e. value is the
    // return value of a load instruction. Also, the loaded value has to be
    // a pointer.
    if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
      if (LI->getType()->isPointerTy()) {
        errs() << "Instruction: " << I->getName() << " is a load inst loading a pointer\n";
        return true;
      }
    }
    
    // Since `V` doesn't come from elsewhere directly, we have to
    // check whether any of the parameters/operands of the instruction `V`
    // come from elsewhere.
    for (auto &Op : I->operands()) {
      if (valueComesFromElsewhereHelper(*Op, ParentFunction, VisitedValues)) {
        errs() << "Resursive search in comes from elsewhere\n";
        return true;
      }
    }
  }
  
  return false;
}

// TODO: adapted for analysis over entire module
// Checks whether a value "comes from elsewhere".
// A value comes from elsewhere if any of the following conditions are met:
// 1. The value was passed as a parameter to the current function.
// 2. The value is the return value of any function.
// 3. The value was loaded from any memory location.
// 4. The value is a global value.
// In case the current value/instruction does not come from elsewhere, we also
// need to check whether any of its operands come from elsewhere.
bool valueComesFromElsewhere(Value &V, Function &ParentFunction) {
  std::set<Value*> VisitedValues;
  return valueComesFromElsewhereHelper(V, ParentFunction, VisitedValues);
}


// TODO:
// Rule Relaxations (only possible with module pass):
// - A value only has other uses if it is passed as a function parameter to an
//   **external** function (aliases must still be accounted for though) or comes
//   from such a function.

// Pointer Authentication Rules:
//
// A pointer (value), that is being stored in or loaded from a memory location,
// is suitable for pointer authentication, if that memory location has no other
// uses and does not come from elsewhere.
// A pointer is only suitable for PA, if all of its aliases are also suitable for
// PA.
bool memoryLocationIsSuitableForPA(Value &MemoryLocation, Function &F, Module &BaseModule) {
  SmallVector<Value*, 8> Aliases;
  findAllAliasesOfValue(MemoryLocation, Aliases, F);

  // TODO: optimization possibility: cache the aliases that were already found to be non-suitable
  // If any of the aliases are not suitable, then all of the aliases should be not suitable
  for (auto Alias : Aliases) {
    // if (valueHasOtherUsesWithoutAA(*Alias, F, BaseModule) || valueComesFromElsewhere(*Alias, F)) {
    //   // errs() << "This will always be not suitable since it's the argument of the function. part 1\n";
    //   return false;
    // }
    if (valueHasOtherUsesWithoutAA(*Alias, F, BaseModule)) { 
      errs() << "Value " << Alias->getName() << " has other uses\n";
      return false;
    }
    if (valueComesFromElsewhere(*Alias, F)) {
      errs() << "Value " << Alias->getName() << " comes from elsewhere\n";
      return false;
    }
  }

  return true;
}

void insertPACInstructions(SmallVector<StoreInst*, 8> &StorePointerInsts, SmallVector<LoadInst*, 8> &LoadPointerInsts, Function &F) {
  auto *PointerSignFunc = Intrinsic::getDeclaration(
      F.getParent(), Intrinsic::wasm_pointer_sign);
  auto *PointerAuthFunc = Intrinsic::getDeclaration(
      F.getParent(), Intrinsic::wasm_pointer_auth);

  // Add pointer signing inst before pointer store inst
  for (auto SI : StorePointerInsts) {
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
}

// Go through all load and stores of pointers and insert them into respective
// vector if they are suitable for pointer authentication.
bool authenticateStoredAndLoadedPointers(Function &F, Module &BaseModule, SmallVector<StoreInst*, 8> &StorePointerInsts, SmallVector<LoadInst*, 8> &LoadPointerInsts) {
  // Look for instructions that load/store a pointer
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (StoreInst *SI = dyn_cast<StoreInst>(&I)) {
        // Store(value, ptr): $value is stored at data address pointed to by $ptr
        // Check if value to be stored in memory is a pointer
        Value *PointerValueToStore = SI->getValueOperand();
        if (PointerValueToStore->getType()->isPointerTy()) {
          auto MemoryLocation = SI->getPointerOperand();
          // errs() << "==== Checking if store: " << SI->getName().str() << " is suitable for PA\n";

          if (memoryLocationIsSuitableForPA(*MemoryLocation, F, BaseModule)) {
            errs() << "Store instruction: " << SI << " is suitable for pointer authentication\n";
            // We shouldn't mutate the instructions we are iterating over
            StorePointerInsts.emplace_back(SI);
          } else {
            errs() << "Store instruction: " << SI << " is not suitable for pointer authentication\n";
          }
        }
      } else
      if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
        // Load(ptr): The data value located at the memory address pointed to by $ptr is returned
        // Check if value to be loaded from memory is a pointer
        if (LI->getType()->isPointerTy()) {
          auto MemoryLocation = LI->getPointerOperand();
          // errs() << "==== Checking if load: " << LI->getName().str() << " is suitable for PA\n";

          if (memoryLocationIsSuitableForPA(*MemoryLocation, F, BaseModule)) {
            errs() << "Load instruction: " << LI << " is suitable for pointer authentication\n";
            // std::cout << "Load instruction: " << LI->getName().str() << " is suitable for pointer authentication\n";
            // We shouldn't mutate the instructions we are iterating over
            LoadPointerInsts.emplace_back(LI);
          } else {
            errs() << "Load instruction: " << LI << " is not suitable for pointer authentication\n";
            // std::cout << "Load instruction: " << LI->getName().str() << " is not suitable for pointer authentication\n";
          }
        }
      }
    }
  }

  // We made changes if we added any pointer sign or auth instructions.
  bool modified = !(LoadPointerInsts.empty() && StorePointerInsts.empty());
  return modified;
}
};

bool WebAssemblyPointerAuthenticationLTO::runOnModule(Module &M) {
  // TODO: only run this on webassembly targets
  // TODO: keep in mind that this **must** only be run once, and it will be during LTO

  errs() << "=== In module: " << M.getName() << "\n";
  for (Function &F : M) {
    errs() << "function: " << F.getName() << "\n";
  }

  return false;

  // bool modified = false;

  // // We only want to insert the new pointer sign and auth instructions after
  // // the analysis of all functions.
  // std::map<Function*, std::pair<SmallVector<StoreInst*, 8>, SmallVector<LoadInst*, 8>>> functionPointerMap;

  // for (Function &F : M) {
  //   // errs() << "======= Checking function: " << F.getName() << "\n";

  //   SmallVector<StoreInst*, 8> storeList;
  //   SmallVector<LoadInst*, 8> loadList;

  //   if (authenticateStoredAndLoadedPointers(F, M, storeList, loadList)) {
  //     // Collect suitable Stores and Loads into vectors
  //     functionPointerMap[&F] = std::make_pair(storeList, loadList);
  //     modified = true;
  //   }
  // }

  // // Actually insert the new pointer authentication instructions
  // for (auto &[F, vectors] : functionPointerMap) {
  //   auto &[storeList, loadList] = vectors;
  //   insertPACInstructions(storeList, loadList, *F);
  // }

  // for (Function &F : M) {
  //   if (F.getName() == "__main_argc_argv" || F.getName() == "__original_main") {
  //     F.dump();
  //   }
  //   // errs() << "------ Printing altered function: " << F.getName() << "\n";
  //   // F.dump();
  // }

  // // TODO: potentially set to false in the future

  // // No changes relevant to other LLVM transformation passes were made.
  // // We simply added some instructions other passes are unaware of anyways.
  // // However, to be on the safe side, we will still indicate that the function
  // // was modified.
  // return modified;
}

} // namespace

char WebAssemblyPointerAuthenticationLTO::ID = 0;

INITIALIZE_PASS_BEGIN(WebAssemblyPointerAuthenticationLTO, DEBUG_TYPE,
                      "WebAssembly Pointer Authentication LTO Pass", false, false)
INITIALIZE_PASS_END(WebAssemblyPointerAuthenticationLTO, DEBUG_TYPE,
                    "WebAssembly Pointer Authentication LTO Pass", false, false)

ModulePass *llvm::createWebAssemblyPointerAuthenticationLTOPass() {
  return new WebAssemblyPointerAuthenticationLTO();
}

#undef DEBUG_TYPE
