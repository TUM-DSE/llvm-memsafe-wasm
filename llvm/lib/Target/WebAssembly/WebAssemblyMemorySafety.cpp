//===- WebAssemblyMemorySafety.cpp - Memory Safety for WASM --===//
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
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Utils/BuildLibCalls.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/MemoryTaggingSupport.h"
#include <algorithm>
#include <cassert>
#include <list>
#include <memory>
#include <optional>
#include <utility>

using namespace llvm;

#define DEBUG_TYPE "wasm-stack-tagging"

namespace {

class SafeStackSlotAnalysis : public InstVisitor<SafeStackSlotAnalysis, bool> {
private:
  Value *U = nullptr;
  std::list<std::pair<Instruction *, Instruction *>> WorkList;

  void addUsersToWorklist(Instruction &I) {
    for (auto *User : I.users()) {
      if (auto *U = dyn_cast<Instruction>(User)) {
        WorkList.emplace_back(std::pair(&I, U));
      }
    }
  }

public:
  SafeStackSlotAnalysis() {}

  bool check(AllocaInst *I) {
    this->addUsersToWorklist(*I);
    std::set<Instruction *> Visited;
    Visited.insert(I);

    while (!WorkList.empty()) {
      auto [Val, User] = WorkList.back();
      WorkList.pop_back();

      if (!Visited.insert(User).second) {
        // we've already checked this value and it is safe
        continue;
      }

      this->U = Val;
      // if a single user is unsafe, return immediately
      if (!this->visit(*User)) {
        return false;
      }
    }

    // we've visited all users (recursive), and should be fine
    return true;
  }

  bool visitCastInst(CastInst &I) {
    // if we are bitcasting to an int, this is not safe
    if (!I.getDestTy()->isPointerTy() || !I.getSrcTy()->isPointerTy()) {
      return false;
    }

    this->addUsersToWorklist(I);
    return true;
  }

  bool visitSelectInst(SelectInst &I) {
    // Only safe if all users are safe
    this->addUsersToWorklist(I);
    return true;
  }

  bool visitIntrinsicInst(IntrinsicInst &I) {
    // assume only intrinsics returning void are safe (e.g. llvm.lifetime.start)
    return I.getType()->isVoidTy();
  }

  // always unsafe
  bool visitMemIntrinsic(MemIntrinsic &I) { return false; }
  bool visitCallBase(CallBase &I) { return false; }
  bool visitTerminator(Instruction &I) { return false; }
  bool visitGetElementPtrInst(GetElementPtrInst &I) { return false; }

  // always safe
  bool visitAtomicCmpXchgInst(AtomicCmpXchgInst &I) { return true; }
  bool visitAtomicRMWInst(AtomicRMWInst &I) { return true; }
  bool visitFenceInst(FenceInst &I) { return true; }
  bool visitPHINode(PHINode &I) { return true; }
  bool visitLoadInst(LoadInst &I) { return true; }
  bool visitStoreInst(StoreInst &I) { return true; }
  bool visitDbgInfoIntrinsic(DbgInfoIntrinsic &I) { return true; }

  bool visitInstruction(Instruction &I) {
    llvm_unreachable("All cases should be handled above");
  }
};

class WebAssemblyMemorySafety : public FunctionPass {

public:
  static char ID;

  WebAssemblyMemorySafety() : FunctionPass(ID) {
    initializeWebAssemblyMemorySafetyPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override { return "WebAssembly Memory Safety"; }

private:
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }

  Value *alignAllocSize(Value *AllocSize, Instruction *InsertBefore) {
    auto *Ty = Type::getInt64Ty(AllocSize->getContext());
    Value *ZextValue =
        CastInst::CreateZExtOrBitCast(AllocSize, Ty, "", InsertBefore);

    const int32_t Align = 16;
    auto *Add = BinaryOperator::CreateAdd(
        ZextValue, ConstantInt::get(Ty, Align - 1), "", InsertBefore);
    auto *And = BinaryOperator::CreateAnd(
        Add, ConstantInt::get(Ty, ~(Align - 1)), "", InsertBefore);
    return And;
  }
};

bool WebAssemblyMemorySafety::runOnFunction(Function &F) {
  if (!F.hasFnAttribute(Attribute::SanitizeWasmMemSafety) ||
      F.getName().starts_with("__wasm_memsafety_"))
    return false;

  DataLayout DL = F.getParent()->getDataLayout();
  LLVMContext &Ctx(F.getContext());

  SmallVector<AllocaInst *, 8> AllocaInsts;
  std::optional<bool> FirstAllocaIsUntagged{};

  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *Alloca = dyn_cast<AllocaInst>(&I)) {
        LLVM_DEBUG(dbgs() << "Checking alloca: " << *Alloca << "\n");

        SafeStackSlotAnalysis Analysis;
        auto IsSafeAlloca = Analysis.check(Alloca);
        if (!IsSafeAlloca) {
          LLVM_DEBUG(dbgs() << "Alloca potentially unsafe, instrumenting.\n");
          AllocaInsts.emplace_back(Alloca);
        }
        if (!FirstAllocaIsUntagged.has_value()) {
          FirstAllocaIsUntagged = IsSafeAlloca;
        }
      }
    }
  }

  DominatorTree DT(F);
  auto *NewSegmentStackFunc =
      Intrinsic::getDeclaration(F.getParent(), Intrinsic::wasm_segment_new);
  auto *FreeSegmentStackFunc =
      Intrinsic::getDeclaration(F.getParent(), Intrinsic::wasm_segment_set_tag);

  // Alloca stack allocations
  for (auto *Alloca : AllocaInsts) {
    Alloca->setAlignment(std::max(Alloca->getAlign(), Align(16)));

    Value *AllocSize;
    if (Alloca->isArrayAllocation()) {
      auto ElementSize = DL.getTypeAllocSize(Alloca->getAllocatedType());
      auto *NumElements = Alloca->getArraySize();
      NumElements = CastInst::CreateIntegerCast(
          NumElements, Type::getInt64Ty(Ctx), false, "", Alloca);
      AllocSize = BinaryOperator::CreateMul(
          NumElements, ConstantInt::get(NumElements->getType(), ElementSize),
          "", Alloca);
    } else {
      AllocSize =
          ConstantInt::get(Type::getInt64Ty(Ctx),
                           DL.getTypeAllocSize(Alloca->getAllocatedType()));
    }
    // align the size to 16 bytes
    AllocSize = this->alignAllocSize(AllocSize, Alloca);

    auto *NewStackSegmentInst =
        CallInst::Create(NewSegmentStackFunc, {Alloca, AllocSize});
    NewStackSegmentInst->insertAfter(Alloca);

    Alloca->replaceUsesWithIf(NewStackSegmentInst, [&](Use &U) {
      return U.getUser() != NewStackSegmentInst;
    });

    // Add free in every block that has a terminator
    // TODO: potential to optimize for code size -- create a unified return
    // block, with a phi node that collects the return value; then free the
    // stack blocks and then return the phi value
    // TODO: this does not work properly with variable length arrays at the
    // moment: segment.free_stack is not inserted.
    for (auto &BB : F) {
      // Check if the current block is dominated by the alloca -- if not, skip
      // this block
      if (Alloca->getParent() != &BB && !DT.dominates(Alloca, &BB)) {
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

  // If we have unsafe allocas and the first alloca in the function is not
  // tagged, we insert an untagged guard slot. This ensures that we never have
  // adjacent slots with the same random tag, even if we get a collision between
  // different stack frames.
  // It is safe to access FirstAllocaIsUntagged when we have AllocaInsts.
  if (!AllocaInsts.empty() && !*FirstAllocaIsUntagged) {
    auto *InsertBefore = &F.getEntryBlock().front();
    new AllocaInst(Type::getInt8Ty(Ctx), 0, ConstantInt::get(Type::getInt64Ty(Ctx), 16), Align(16), "Guard", InsertBefore);
  }

  return !AllocaInsts.empty();
}

} // namespace

char WebAssemblyMemorySafety::ID = 0;

INITIALIZE_PASS_BEGIN(WebAssemblyMemorySafety, DEBUG_TYPE,
                      "WebAssembly Memory Safety", false, false)
INITIALIZE_PASS_END(WebAssemblyMemorySafety, DEBUG_TYPE,
                    "WebAssembly Memory Safety", false, false)

FunctionPass *llvm::createWebAssemblyMemorySafetyPass() {
  return new WebAssemblyMemorySafety();
}

#undef DEBUG_TYPE
