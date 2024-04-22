//===- WebAssemblyMemorySafetyCustomSection.cpp - Memory Safety for WASM --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/WebAssemblyMCTargetDesc.h"
#include "WebAssembly.h"
#include "WebAssemblyMachineFunctionInfo.h"
#include "WebAssemblySubtarget.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineModuleInfoImpls.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <cassert>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "wasm-mem-safety-custom-section"

namespace {
class WebAssemblyMemorySafetyCustomSectionPass final : public ModulePass {
  StringRef getPassName() const override {
    return "WebAssembly MemSafety Custom Section Pass";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    ModulePass::getAnalysisUsage(AU);
  }

  bool runOnModule(Module &M) override;

public:
  static char ID;
  WebAssemblyMemorySafetyCustomSectionPass() : ModulePass(ID) {}
};

} // namespace

char WebAssemblyMemorySafetyCustomSectionPass::ID = 0;
INITIALIZE_PASS(WebAssemblyMemorySafetyCustomSectionPass, DEBUG_TYPE,
                "Move WASM mem safety instructions to custom sections", false,
                false)

ModulePass *llvm::createWebAssemblyMemorySafetyCustomSectionPass() {
  return new WebAssemblyMemorySafetyCustomSectionPass();
}

struct MIReplacementInfo {
  MachineInstr *MI;
  uint32_t Opcode;
  uint32_t MemFlags;
  uint64_t MemOffset;
  // TODO: at some later point, also add memory index; will be enabled in the
  // multiple memory proposal
  uint8_t NumDrops;
};

bool WebAssemblyMemorySafetyCustomSectionPass::runOnModule(Module &M) {
  auto *MMIWP = getAnalysisIfAvailable<MachineModuleInfoWrapperPass>();
  if (!MMIWP)
    return true;

  MachineModuleInfo &MMI = MMIWP->getMMI();

  for (auto &F : M) {
    std::vector<MIReplacementInfo> MIs;
    auto *MF = MMI.getMachineFunction(F);
    if (!MF) {
      continue;
    }

    for (auto &MBB : *MF) {
      for (auto &MI : MBB) {
        uint32_t Opcode = 0;
        uint8_t NumDrops = 0;
        uint64_t ConstantsOffset = 0;

        switch (MI.getOpcode()) {
        case WebAssembly::SEGMENT_STACK_NEW_A64:
        case WebAssembly::SEGMENT_STACK_NEW_A64_S:
          ConstantsOffset = 1;
          NumDrops = 1;
          Opcode = 0xfa02;
          break;
        case WebAssembly::SEGMENT_STACK_FREE_A64:
        case WebAssembly::SEGMENT_STACK_FREE_A64_S:
          NumDrops = 3;
          Opcode = 0xfa03;
          break;
        case WebAssembly::SEGMENT_FREE_A64:
        case WebAssembly::SEGMENT_FREE_A64_S:
          NumDrops = 2;
          Opcode = 0xfa01;
          break;
        default:
          continue;
        }

        uint32_t MemFlags = MI.getOperand(ConstantsOffset).getImm();
        uint64_t MemOffset = MI.getOperand(ConstantsOffset + 1).getImm();

        // TODO: check if this actually works or if we got the immediates the
        // wrong way around
        assert(Opcode != 0 && "Opcode should not be 0");
        MIs.emplace_back(
            MIReplacementInfo{&MI, Opcode, MemFlags, MemOffset, NumDrops});
      }
    }

    MDBuilder MDB(M.getContext());
    auto *I64Ty = Type::getInt64Ty(M.getContext());
    auto *I32Ty = Type::getInt32Ty(M.getContext());
    auto *I8Ty = Type::getInt8Ty(M.getContext());

    for (auto MIInfo : MIs) {
      SmallVector<Constant *, 6> Data{};

      Data.push_back(ConstantInt::get(I8Ty, MIInfo.NumDrops));
      // Manually encode Opcode as two i8, as we compress data using LEB128
      Data.push_back(ConstantInt::get(I8Ty, (MIInfo.Opcode >> 8) & 0xff));
      Data.push_back(ConstantInt::get(I8Ty, MIInfo.Opcode & 0xff));
      Data.push_back(ConstantInt::get(I32Ty, MIInfo.MemFlags));
      Data.push_back(ConstantInt::get(I64Ty, MIInfo.MemOffset));
      MDBuilder::PCSection PCSection = {
          MDB.createString("mem-safety!C")->getString(), Data};

      for (uint64_t I = 0; I < MIInfo.NumDrops; I++) {
        MIMetadata MIMD = MIInfo.MI->getDebugLoc();

        // Only add pcsections to the first drop instruction
        MDNode *PCSections = nullptr;
        if (I == 0) {
          PCSections = MDNode::concatenate(MIMD.getPCSections(),
                                           MDB.createPCSections({PCSection}));
        } else {
          PCSections = MIMD.getPCSections();
        }

        BuildMI(*MIInfo.MI->getParent(), *MIInfo.MI, {MIMD.getDL(), PCSections},
                MIInfo.MI->getMF()
                    ->getSubtarget<WebAssemblySubtarget>()
                    .getInstrInfo()
                    ->get(WebAssembly::DROP_I64));
      }
      MIInfo.MI->eraseFromParent();
    }
  }

  return true;
}

#undef DEBUG_TYPE
