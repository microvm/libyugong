#include "yugong-llvm.hpp"
#include "yugong-debug.hpp"

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

#include <vector>

namespace yg {
    using namespace std;
    using namespace llvm;

    const smid_t YGStackMapHelper::FIRST_SMID = 0x1000;

    YGStackMapHelper::YGStackMapHelper(LLVMContext &_ctx, Module &_module):
        ctx(&_ctx), module(&_module), next_smid(FIRST_SMID)
    {
        i64 = Type::getInt64Ty(*ctx);
        i32 = Type::getInt32Ty(*ctx);
        void_ty = Type::getVoidTy(*ctx);
        I32_0 = ConstantInt::get(i32, 0);

        auto param_tys = vector<Type*> { i64, i32 };
        stackmap_sig = FunctionType::get(void_ty, param_tys, true);
        stackmap_func = Function::Create(stackmap_sig, Function::ExternalLinkage,
                "llvm.experimental.stackmap", module);
    }

    smid_t YGStackMapHelper::createStackMap(IRBuilder<> &builder, ArrayRef<Value*> kas) {
        smid_t smid = next_smid;
        next_smid++;

        auto smid_const = ConstantInt::get(i64, smid);
        auto sm_args = vector<Value*> { smid_const, I32_0 };

        for (auto ka : kas) {
            sm_args.push_back(ka);
        }

        auto sm = builder.CreateCall(stackmap_func, sm_args);

        smid_to_call[smid] = sm;

        return smid;
    }

    ProxyMemoryManager::ProxyMemoryManager(unique_ptr<SectionMemoryManager> mm):
        mm(move(mm)), StackMapSection(nullptr), StackMapSectionSize(0LL) {}

    
    uint8_t *ProxyMemoryManager::allocateCodeSection(uintptr_t Size,
            unsigned Alignment, unsigned SectionID, StringRef SectionName) {
        yg_debug("allocateCodeSection. Size=%" PRIuPTR
                ", Alignment=%u, SectionID=%u, SectionName=%s\n", Size, Alignment,
                SectionID, SectionName.str().c_str());
        return mm->allocateCodeSection(Size, Alignment, SectionID, SectionName);
    }

    uint8_t *ProxyMemoryManager::allocateDataSection(uintptr_t Size,
            unsigned Alignment, unsigned SectionID, StringRef SectionName,
            bool isReadOnly) {
        yg_debug("allocateDataSection. Size=%" PRIuPTR
                ", Alignment=%u, SectionID=%u, SectionName=%s, isReadOnly=%s\n",
                Size, Alignment, SectionID, SectionName.str().c_str(),
                isReadOnly?"true":"false");
        return mm->allocateDataSection(Size, Alignment, SectionID, SectionName, isReadOnly);
    }

    bool ProxyMemoryManager::finalizeMemory(string *ErrMsg) {
        return mm->finalizeMemory(ErrMsg);
    }

    void ProxyMemoryManager::invalidateInstructionCache() {
        mm->invalidateInstructionCache();
    }
}
