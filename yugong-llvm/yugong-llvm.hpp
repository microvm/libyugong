#ifndef __YUGONG_LLVM_HPP__
#define __YUGONG_LLVM_HPP__

#include "yugong.hpp"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

#include <map>
#include <string>
#include <vector>

namespace yg {
    using namespace std;
    using namespace llvm;

    typedef uint64_t smid_t;

    class YGStackMapHelper {
        public:
            YGStackMapHelper(const YGStackMapHelper&) = delete;
            YGStackMapHelper(LLVMContext &_ctx, Module &_module);

            smid_t createStackMap(IRBuilder<> &builder, ArrayRef<Value*> kas);

        private:
            LLVMContext *ctx;
            Module *module;
            FunctionType *stackmap_sig;
            Function *stackmap_func;
            Type *i64, *i32, *void_ty;
            Constant *I32_0;

            static const smid_t FIRST_SMID;
            smid_t next_smid;

            map<smid_t, Value*> smid_to_call;
    };

    class ProxyMemoryManager: public RTDyldMemoryManager {
        ProxyMemoryManager(const ProxyMemoryManager&) = delete;
        void operator=(const ProxyMemoryManager&) = delete;

        unique_ptr<SectionMemoryManager> mm;

        uint8_t *StackMapSection;
        uintptr_t StackMapSectionSize;

        public:
        ProxyMemoryManager(unique_ptr<SectionMemoryManager> mm);

        uint8_t *allocateCodeSection(uintptr_t Size, unsigned Alignment,
                unsigned SectionID, StringRef SectionName) override;

        uint8_t *allocateDataSection(uintptr_t Size, unsigned Alignment,
                unsigned SectionID, StringRef SectionName,
                bool isReadOnly) override;

        bool finalizeMemory(string *ErrMsg = nullptr) override;

        virtual void invalidateInstructionCache();
    };
}

#endif // __YUGONG_LLVM_HPP__
