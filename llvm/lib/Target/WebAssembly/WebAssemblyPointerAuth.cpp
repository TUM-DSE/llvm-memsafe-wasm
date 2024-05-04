//===- WebAssemblyPointerAuth.cpp - Memory Safety for WASM --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//

#include "WebAssembly.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/StackSafetyAnalysis.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
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
#include "llvm/Transforms/Utils/BuildLibCalls.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/MemoryTaggingSupport.h"
#include <algorithm>
#include <cassert>
#include <list>
#include <memory>
#include <utility>
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

#define DEBUG_TYPE "wasm-ptr-auth"

namespace {

class WebAssemblyPointerAuth : public ModulePass,
                               public InstVisitor<WebAssemblyPointerAuth> {

public:
  static char ID;

  WebAssemblyPointerAuth() : ModulePass(ID) {
    initializeWebAssemblyPointerAuthPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override;

  StringRef getPassName() const override {
    return "WebAssembly Pointer Authentication";
  }

  void visitCallBase(CallBase &CB);

  void visitInstruction(Instruction &I);

  Value *instrumentValue(Value *Val, IRBuilder<> &IRB);

private:
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }

  // Recursive function to find GEP paths leading to functions
  void findFunctionGEP(Constant *C, std::vector<unsigned> &Indices,
                       std::vector<std::vector<unsigned>> &GepPaths) {
    if (auto *CE = dyn_cast<Function>(C)) {
      GepPaths.push_back(Indices);
    } else if (auto *CA = dyn_cast<ConstantArray>(C)) {
      for (unsigned I = 0; I < CA->getNumOperands(); I++) {
        Indices.push_back(I);
        findFunctionGEP(CA->getOperand(I), Indices, GepPaths);
        Indices.pop_back();
      }
    } else if (auto *CS = dyn_cast<ConstantStruct>(C)) {
      for (unsigned I = 0; I < CS->getNumOperands(); I++) {
        Indices.push_back(I);
        findFunctionGEP(CS->getOperand(I), Indices, GepPaths);
        Indices.pop_back();
      }
    }
  }

  void signVTables(Module &M) {
    FunctionType *FuncType =
        FunctionType::get(Type::getVoidTy(M.getContext()), false);
    Function *InitFunction = Function::Create(
        FuncType, Function::InternalLinkage, "init_globals", &M);

    BasicBlock *Entry =
        BasicBlock::Create(M.getContext(), "entry", InitFunction);
    IRBuilder<> IRB(Entry);

    auto *PointerSignIntr =
        Intrinsic::getDeclaration(&M, Intrinsic::wasm_pointer_sign);

    for (GlobalVariable &GV : M.globals()) {
      if (GV.getName() == "llvm.used" || GV.getName() == "llvm.global_ctors" || GV.getName() == "llvm.global_dtors") {
        continue;
      }

      std::vector<unsigned> Indices{};
      std::vector<std::vector<unsigned>> GepPaths{};
      if (!GV.hasInitializer()) {
        continue;
      }
      findFunctionGEP(GV.getInitializer(), Indices, GepPaths);
      for (auto GepPath : GepPaths) {
        std::vector<Value *> GepIndices{IRB.getInt32(0)};
        for (auto Index : GepPath) {
          GepIndices.emplace_back(IRB.getInt32(Index));
        }
        Value *GEP = IRB.CreateGEP(GV.getValueType(), &GV, GepIndices);

        auto *Value = IRB.CreateLoad(IRB.getPtrTy(), GEP);
        auto *SignedValue =
            IRB.CreateCall(PointerSignIntr, {Value, IRB.getInt64(0)});
        IRB.CreateStore(SignedValue, GEP);
      }
    }

    if (Entry->empty()) {
      // we didn't find a global to instrument, so delete the function and early
      // abort.
      InitFunction->eraseFromParent();
      return;
    }

    IRB.CreateRetVoid();

    appendToGlobalCtors(M, InitFunction, 0);
  }
};

bool WebAssemblyPointerAuth::runOnModule(Module &M) {
  signVTables(M);

  bool Changed = true;
  for (auto &F : M) {
    Changed = false;

    visit(F);
  }
  M.dump();
  return Changed;
}

void WebAssemblyPointerAuth::visitCallBase(llvm::CallBase &CB) {
  if (CB.isIndirectCall()) {
    auto *Op = CB.getCalledOperand();
    assert(Op->getType()->isPointerTy() && "Expected Op to be a function pointer.");

    auto *PointerAuthIntr =
        Intrinsic::getDeclaration(CB.getModule(), Intrinsic::wasm_pointer_auth);
    IRBuilder<> IRB(&CB);
    auto *AuthCallee =
        IRB.CreateCall(PointerAuthIntr, {Op, IRB.getInt64(0)});
    CB.setCalledOperand(AuthCallee);
  }

  IRBuilder<> IRB(&CB);
  for (unsigned I = 0; I < CB.arg_size(); ++I) {
    if (auto *SignedPtr = instrumentValue(CB.getArgOperand(I), IRB)) {
      CB.setArgOperand(I, SignedPtr);
    }
  }
}

void WebAssemblyPointerAuth::visitInstruction(llvm::Instruction &I) {
  IRBuilder<> IRB(&I);
  auto *PhiNode = dyn_cast<PHINode>(&I);

  for (unsigned J = 0; J < I.getNumOperands(); ++J) {
    if (PhiNode != nullptr) {
      auto *IncomingBlock = PhiNode->getIncomingBlock(J);
      if (auto *Term = IncomingBlock->getTerminator()) {
        IRB.SetInsertPoint(Term);
      } else {
        IRB.SetInsertPoint(IncomingBlock);
      }
    }
    if (auto *SignedPtr = instrumentValue(I.getOperand(J), IRB)) {
      I.setOperand(J, SignedPtr);
    }
  }
}

Value *WebAssemblyPointerAuth::instrumentValue(Value *Val, IRBuilder<> &IRB) {
  auto *Fn = dyn_cast<Function>(Val);
  if (Fn == nullptr || Fn->isIntrinsic()) {
    return nullptr;
  }
    auto *PointerSignIntr = Intrinsic::getDeclaration(
        IRB.GetInsertBlock()->getModule(), Intrinsic::wasm_pointer_sign);
    return IRB.CreateCall(PointerSignIntr, {Fn, IRB.getInt64(0)});
}

} // namespace

char WebAssemblyPointerAuth::ID = 0;

INITIALIZE_PASS_BEGIN(WebAssemblyPointerAuth, DEBUG_TYPE,
                      "WebAssembly Pointer Authentication", false, false)
INITIALIZE_PASS_END(WebAssemblyPointerAuth, DEBUG_TYPE,
                    "WebAssembly Pointer Authentication", false, false)

ModulePass *llvm::createWebAssemblyPointerAuthPass() {
  return new WebAssemblyPointerAuth();
}

#undef DEBUG_TYPE
