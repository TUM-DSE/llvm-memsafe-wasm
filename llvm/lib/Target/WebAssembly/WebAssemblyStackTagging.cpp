//===- WebAssemblyStackTagging.cpp - Stack tagging in IR --===//
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
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Printable.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/MemoryTaggingSupport.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>

using namespace llvm;

#define DEBUG_TYPE "wasm-stack-tagging"

namespace {

void replaceAllUsesWith(
    Instruction *I, Value *NewI,
    llvm::function_ref<bool(Use &)> ShouldReplace = [](auto &U) {
      return true;
    });

class WebAssemblyStackTagging : public FunctionPass {

public:
  static char ID;

  WebAssemblyStackTagging() : FunctionPass(ID) {
    initializeWebAssemblyStackTaggingPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override { return "WebAssembly Stack Tagging"; }

private:
  // Function *F = nullptr;
  // Function *NewSegmentStackFunc = nullptr;
  // const DataLayout *DL = nullptr;
  // // AAResults *AA = nullptr;
  // const StackSafetyGlobalInfo *SSI = nullptr;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    // AU.addRequired<StackSafetyGlobalInfoWrapperPass>();
    // if (MergeInit)
    // AU.addRequired<AAResultsWrapperPass>();
  }

  bool isAllocKind(Attribute Attr, AllocFnKind Kind) const {
    if (!Attr.hasAttribute(Attribute::AllocKind))
      return false;

    return (Attr.getAllocKind() & Kind) != AllocFnKind::Unknown;
  }

  Value *alignAllocSize(Value *AllocSize, Instruction *InsertBefore) {
    auto *Ty = AllocSize->getType();
    assert(Ty->isIntegerTy(32) && "Only able to handle i32 as alloc size");

    const int32_t Align = 16;
    auto *Add = BinaryOperator::CreateAdd(
        AllocSize, ConstantInt::get(Ty, Align - 1), "", InsertBefore);
    auto *And = BinaryOperator::CreateAnd(
        Add, ConstantInt::get(Ty, ~(Align - 1)), "", InsertBefore);
    return And;
  }
};

bool WebAssemblyStackTagging::runOnFunction(Function &F) {
  if (!F.hasFnAttribute(Attribute::SanitizeWasmMemSafety) ||
      F.getName().starts_with("__wasm_memsafety_"))
    return false;

  DataLayout DL = F.getParent()->getDataLayout();

  SmallVector<AllocaInst *, 8> AllocaInsts;
  SmallVector<std::pair<AllocFnKind, CallInst *>, 8> CallsToAllocFunctions;

  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *Alloca = dyn_cast<AllocaInst>(&I)) {
        LLVM_DEBUG(dbgs() << "Checking alloca: " << *Alloca << "\n");

        // TODO: check which allocas we actually need to protect and which we
        // don't We cannot use Alloca->isArrayAllocation(), as it returns true
        // for [i8 x 16] and some more cases
        AllocaInsts.emplace_back(Alloca);
      }
      if (auto *Call = dyn_cast<CallInst>(&I)) {
        auto *CalledFunction = Call->getCalledFunction();
        auto Attr =
            CalledFunction->getFnAttribute(Attribute::AttrKind::AllocKind);
        if (Attr.hasAttribute(Attribute::AllocKind)) {
          if (isAllocKind(Attr, AllocFnKind::Alloc)) {
            CallsToAllocFunctions.emplace_back(
                std::pair(AllocFnKind::Alloc, Call));
          } else if (isAllocKind(Attr, AllocFnKind::Realloc)) {
            CallsToAllocFunctions.emplace_back(
                std::pair(AllocFnKind::Realloc, Call));
          } else if (isAllocKind(Attr, AllocFnKind::Free)) {
            CallsToAllocFunctions.emplace_back(
                std::pair(AllocFnKind::Free, Call));
          }
          // if (isAllocKind(Attr, AllocFnKind::Uninitialized)) {
          //   errs() << "AllocKind Uninitialized\n";
          // }
          // if (isAllocKind(Attr, AllocFnKind::Zeroed)) {
          //   errs() << "AllocKind Zeroed\n";
          // }
          // if (isAllocKind(Attr, AllocFnKind::Aligned)) {
          //   errs() << "AllocKind Aligned\n";
          // }
        }
      }
    }
  }

  auto SafeMallocFn = F.getParent()->getOrInsertFunction(
      "__wasm_memsafety_malloc",
      FunctionType::get(Type::getInt64Ty(F.getContext()),
                        {
                            Type::getInt32Ty(F.getContext()),
                            Type::getInt32Ty(F.getContext()),
                        },
                        false));
  auto SafeFreeFn = F.getParent()->getOrInsertFunction(
      "__wasm_memsafety_free",
      FunctionType::get(Type::getVoidTy(F.getContext()),
                        {
                            Type::getInt64Ty(F.getContext()),
                        },
                        false));

  for (auto [Kind, Call] : CallsToAllocFunctions) {
    switch (Kind) {
    case llvm::AllocFnKind::Alloc: {
      // TODO: handle functions other than c malloc
      auto *NewCall =
          CallInst::Create(SafeMallocFn,
                           {Call->getArgOperand(0),
                            ConstantInt::get(Type::getInt32Ty(F.getContext()),
                                             /* align = */ 16)},
                           Call->getName(), Call);
      replaceAllUsesWith(Call, NewCall);
      break;
    }
    case llvm::AllocFnKind::Free: {
      // TODO: we need a way to get the original pointer... or we just make the free function accept a pointer
      // This will be a problem for other functions taking pointers as well, so we might just get it right before
      // hacking something that just works for this function.
      auto *NewCall = CallInst::Create(SafeFreeFn, {Call->getArgOperand(0)},
                                       Call->getName(), Call);
      replaceAllUsesWith(Call, NewCall);
      break;
    }
    default:
      llvm_unreachable("Not yet implemented.");
    }
    Call->eraseFromParent();
  }

  DominatorTree DT(F);
  auto *NewSegmentStackFunc = Intrinsic::getDeclaration(
      F.getParent(), Intrinsic::wasm_segment_stack_new,
      {Type::getInt32Ty(F.getContext())});
  auto *FreeSegmentStackFunc = Intrinsic::getDeclaration(
      F.getParent(), Intrinsic::wasm_segment_stack_free,
      {Type::getInt32Ty(F.getContext())});

  for (auto *Alloca : AllocaInsts) {
    Alloca->setAlignment(std::max(Alloca->getAlign(), Align(16)));

    DataLayout DL = F.getParent()->getDataLayout();
    Value *AllocSize;
    if (Alloca->isArrayAllocation()) {
      auto ElementSize = DL.getTypeAllocSize(Alloca->getAllocatedType());
      auto *NumElements = Alloca->getArraySize();
      NumElements = CastInst::CreateIntegerCast(
          NumElements, Type::getInt32Ty(F.getContext()), false, "", Alloca);
      AllocSize = BinaryOperator::CreateMul(
          NumElements, ConstantInt::get(NumElements->getType(), ElementSize),
          "", Alloca);
    } else {
      AllocSize =
          ConstantInt::get(Type::getInt32Ty(F.getContext()),
                           DL.getTypeAllocSize(Alloca->getAllocatedType()));
    }

    // align the size to 16 bytes
    AllocSize = this->alignAllocSize(AllocSize, Alloca);

    auto *NewStackSegmentInst =
        CallInst::Create(NewSegmentStackFunc, {Alloca, AllocSize});
    NewStackSegmentInst->insertAfter(Alloca);

    replaceAllUsesWith(Alloca, NewStackSegmentInst);

    // Add free in every block that has a terminator
    // TODO: potential to optimize for code size -- create a unified return
    // block, with a phi node that collects the return value; then free the
    // stack blocks and then return the phi value
    // TODO: this does not work properly with variable length arrays at the
    // moment: segment.free_stack is not inserted.
    for (auto &BB : F) {
      // Check if the current block is dominated by the alloca -- if not, skip
      // this block
      if (!DT.dominates(Alloca, &BB)) {
        continue;
      }

      auto *Terminator = BB.getTerminator();

      while (isa_and_nonnull<UnreachableInst>(Terminator)) {
        Terminator = Terminator->getPrevNonDebugInstruction();
      }

      if (!Terminator)
        continue;

      auto IsTailCall = [](Instruction *I) {
        auto *Call = dyn_cast<CallInst>(I);
        return Call && Call->isTailCall();
      };

      if (!isa<ReturnInst>(Terminator) && !IsTailCall(Terminator))
        continue;

      auto *FreeSegmentInst = CallInst::Create(
          FreeSegmentStackFunc, {NewStackSegmentInst, Alloca, AllocSize});
      FreeSegmentInst->insertBefore(Terminator);
    }
  }

  return true;
}

struct PtrUseVisitor : public InstVisitor<PtrUseVisitor> {
public:
  PtrUseVisitor(Value *NewI, Use *U) : NewI(NewI), Use(U) {}

  void visitInstruction(Instruction &I) {
    auto *IntToPtr = new IntToPtrInst(NewI, Use->get()->getType(), "", &I);
    Use->set(IntToPtr);
  }

  void visitLoadInst(LoadInst &I) {
    // TODO: emit intrinsic to load from segment
    auto *SegmentLoadIntr = Intrinsic::getDeclaration(
        I.getModule(), Intrinsic::wasm_segment_load, {I.getType()});

    auto *Val = CallInst::Create(SegmentLoadIntr,
                                 {
                                     NewI,
                                 },
                                 "", &I);
    I.replaceAllUsesWith(Val);
    I.eraseFromParent();
  }

  void visitStoreInst(StoreInst &I) {
    if (Use->getOperandNo() == 0) {
      // Pointer is being stored to memory
      // TODO
    } else {
      // Pointer is used to access memory
      auto *SegmentStoreIntr = Intrinsic::getDeclaration(
          I.getModule(), Intrinsic::wasm_segment_store,
          {I.getValueOperand()->getType()});

      CallInst::Create(SegmentStoreIntr,
                       {
                           NewI,
                           I.getValueOperand(),
                       },
                       "", &I);
      I.eraseFromParent();
    }
  }

  void visitGetElementPtrInst(GetElementPtrInst &GEP) {
    // This function transforms a GEP to a series of adds that should calculate
    // the same address
    Value *Base = NewI;
    Type *Ty = NewI->getType();
    Value *Offset = ConstantInt::get(Ty, 0);

    for (unsigned I = 0; I < GEP.getNumIndices(); ++I) {
      Value *Index = GEP.getOperand(I + 1);

      if (Offset->getType()->getIntegerBitWidth() < Ty->getIntegerBitWidth()) {
        Offset = new ZExtInst(Offset, Ty, "", &GEP);
      }
      if (Index->getType()->getIntegerBitWidth() < Ty->getIntegerBitWidth()) {
        Index = new ZExtInst(Index, Ty, "", &GEP);
      }

      Value *Multiplier;
      Type *SourceElemTy = GEP.getSourceElementType();
      if (SourceElemTy->isArrayTy()) {
        Multiplier = ConstantInt::get(Ty, SourceElemTy
                                                  ->getArrayElementType()
                                                  ->getPrimitiveSizeInBits() /
                                              8);
      } else if (SourceElemTy->isSingleValueType()) {
        Multiplier = ConstantInt::get(Ty, SourceElemTy->getPrimitiveSizeInBits() / 8);
      } else {
        GEP.dump();
        llvm_unreachable("Unable to handle GEP SourceElementType");
      }
      Index = BinaryOperator::CreateMul(Index, Multiplier, "", &GEP);

      Offset = BinaryOperator::CreateAdd(Offset, Index, "", &GEP);
    }

    Instruction *Result = BinaryOperator::CreateAdd(Base, Offset, "", &GEP);
    replaceAllUsesWith(&GEP, Result);
    GEP.eraseFromParent();
  }

private:
  Value *NewI;
  Use *Use;
};

void replaceAllUsesWith(Instruction *I, Value *NewI,
                        llvm::function_ref<bool(Use &)> ShouldReplace) {
  std::list<Use *> Uses;
  for (auto &Use : I->uses()) {
    if (Use.getUser() != NewI && ShouldReplace(Use))
      Uses.emplace_back(&Use);
  }

  for (auto *Use : Uses) {
    // Otherwise, insert int to ptr
    Instruction *User = dyn_cast<Instruction>(Use->getUser());
    assert(User);

    PtrUseVisitor Visitor(NewI, Use);
    Visitor.visit(User);
  }
}

} // namespace

char WebAssemblyStackTagging::ID = 0;

INITIALIZE_PASS_BEGIN(WebAssemblyStackTagging, DEBUG_TYPE,
                      "WebAssembly Stack Tagging", false, false)
// INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
// INITIALIZE_PASS_DEPENDENCY(StackSafetyGlobalInfoWrapperPass)
INITIALIZE_PASS_END(WebAssemblyStackTagging, DEBUG_TYPE,
                    "WebAssembly Stack Tagging", false, false)

FunctionPass *llvm::createWebAssemblyStackTaggingPass(bool IsOptNone) {
  return new WebAssemblyStackTagging();
}

#undef DEBUG_TYPE
