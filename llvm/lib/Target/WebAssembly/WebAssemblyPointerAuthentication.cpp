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
#include "llvm/Analysis/BasicAliasAnalysis.h"
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

#define DEBUG_TYPE "wasm-pointer-authentication"

namespace {

class WebAssemblyPointerAuthentication : public FunctionPass {

public:
  static char ID;

  WebAssemblyPointerAuthentication() : FunctionPass(ID) {
    initializeWebAssemblyPointerAuthenticationPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override { return "WebAssembly Pointer Authentication"; }

private:
  // TODO: do i need this
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<AAResultsWrapperPass>();
    AU.setPreservesCFG();
  }
};


// TODO: AliasAnalysis does not account for loops
void findAllAliasesOfValue(Value &V, SmallVector<Value *, 8> &Aliases, AliasAnalysis &AA, Function &F) {
  std::cout << "Value \"" << V.getName().str() << "\" is aliased by:" << std::endl;
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      // TODO: is this cast to value necessary?
      if (auto OtherValue = dyn_cast<Value>(&I)) {
        // TODO: !isNoAlias is more conservative than isMustAlias, because we also ignore cases of isMaybeAlias; either way non-deterministic?
        if (!AA.isNoAlias(&V, OtherValue)) {
          std::cout << "  Other value \"" << OtherValue->getName().str() << "\"" << std::endl;
          Aliases.emplace_back(OtherValue);
        }
      }
    }
  }
}

// Find all function calls that use the specified value as an argument.
// This function also recursively finds all referenced/pointed to uses
// of the value.
void findAllFunctionsWhereValueIsPassedAsArgument(Value &V, SmallVector<CallInst*, 8> &FunctionCalls) {
  std::cout << "Value \"" << V.getName().str() << "\" is used in functions:" << std::endl;
  for (User *U : V.users()) {
    if (CallInst *CI = dyn_cast<CallInst>(U)) {
      for (Value *Arg : CI->args()) {
        if (Arg == &V) {
          std::cout << "  Function \"" << CI->getCalledFunction()->getName().str() << "\"" << std::endl;
          FunctionCalls.emplace_back(CI);
          break;
        }
      }
    }
  }
}

// Find all function calls that use the specified value as an argument.
// This function also recursively finds all referenced/pointed to uses
// of the value.
void findAllFunctionsWhereValueIsPassedAsArgument2(Value &V, SmallVector<CallInst*, 8> &FunctionCalls, Function &F) {
  std::cout << "Value \"" << V.getName().str() << "\" is used in functions:" << std::endl;
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (auto CI = dyn_cast<CallInst>(&I)) {
        for (Value *Arg : CI->args()) {
          if (Arg == &V) {
            std::cout << "  Function \"" << CI->getCalledFunction()->getName().str() << "\"" << std::endl;
            FunctionCalls.emplace_back(CI);
            break;
          }
        }
      }
    }
  }
}

bool authenticateStoredAndLoadedPointers(Function &F, AliasAnalysis &AA) {
  auto *PointerSignFunc = Intrinsic::getDeclaration(
      F.getParent(), Intrinsic::wasm_pointer_sign);
  auto *PointerAuthFunc = Intrinsic::getDeclaration(
      F.getParent(), Intrinsic::wasm_pointer_auth);

  // TODO: remove
  if (F.getName() != "__main_argc_argv") {
    return true;
  }

  SmallVector<StoreInst*, 8> StorePointerInsts;
  SmallVector<LoadInst*, 8> LoadPointerInsts;

  // Look for instructions that load/store a pointer
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (StoreInst *SI = dyn_cast<StoreInst>(&I)) {
        // Store(value, ptr): $value is stored at data address pointed to by $ptr
        // Check if value to be stored in memory is a pointer
        if (SI->getValueOperand()->getType()->isPointerTy()) {
          // We shouldn't mutate the instructions we are iterating over
          StorePointerInsts.emplace_back(SI);
        }
      } else
      if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
        // Load(ptr): The data value located at the memory address pointed to by $ptr is returned
        // Check if value to be loaded from memory is a pointer
        if (LI->getType()->isPointerTy()) {
          // We shouldn't mutate the instructions we are iterating over
          LoadPointerInsts.emplace_back(LI);
        }
      }
    }
  }

  // Add pointer signing inst before pointer store inst
  for (auto SI : StorePointerInsts) {
  // for (auto SI : MatchedStoreInsts) {
    // std::cout << "Found a valid pointer store\n";

    // TODO: consider maybe casting this to some sort of pointer type, just so we always know this value is indeed a pointer
    // Sign the value (which is a pointer) that will be stored
    Value *PointerValueToStore = SI->getValueOperand();


    SmallVector<CallInst*, 8> functionCalls;
    findAllFunctionsWhereValueIsPassedAsArgument(*PointerValueToStore, functionCalls);
    // findAllFunctionsWhereValueIsPassedAsArgument2(*PointerValueToStore, functionCalls, F);

    SmallVector<Value*, 8> aliases;
    findAllAliasesOfValue(*PointerValueToStore, aliases, AA, F);

    auto *PointerSignInst = CallInst::Create(PointerSignFunc, {PointerValueToStore});
    PointerSignInst->insertBefore(SI);

    // Replace the value operand in the store inst with the new signed value
    SI->setOperand(0, PointerSignInst);
  }

  // Add pointer authentication inst after pointer load inst
  for (auto LI : LoadPointerInsts) {
  // for (auto LI : MatchedLoadInsts) {
    // std::cout << "Found a valid pointer load\n";

    // Load the pointer value, and then authenticate it
    auto *PointerAuthInst = CallInst::Create(PointerAuthFunc, {LI});
    PointerAuthInst->insertAfter(LI);

    // All further uses of the load's return value must use our authenticated pointer instead now
    LI->replaceUsesWithIf(PointerAuthInst, [&](Use &U) {
      return U.getUser() != PointerAuthInst;
    });
  }

  // F.dump();

  return true;
}

bool WebAssemblyPointerAuthentication::runOnFunction(Function &F) {
  AliasAnalysis &AA = getAnalysis<AAResultsWrapperPass>().getAAResults();
  return authenticateStoredAndLoadedPointers(F, AA);
}

} // namespace

char WebAssemblyPointerAuthentication::ID = 0;

INITIALIZE_PASS_BEGIN(WebAssemblyPointerAuthentication, DEBUG_TYPE,
                      "WebAssembly Pointer Authentication", false, false)
INITIALIZE_PASS_END(WebAssemblyPointerAuthentication, DEBUG_TYPE,
                    "WebAssembly Pointer Authentication", false, false)

FunctionPass *llvm::createWebAssemblyPointerAuthenticationPass() {
  return new WebAssemblyPointerAuthentication();
}

#undef DEBUG_TYPE
