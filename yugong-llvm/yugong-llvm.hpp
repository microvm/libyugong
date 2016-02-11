#ifndef __YUGONG_LLVM_HPP__
#define __YUGONG_LLVM_HPP__

#include "yugong.hpp"

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>

#include <string>

namespace yg {

    class YGStackMapHelper {

    };

    class ProxyMemoryManager: public llvm::RTDyldMemoryManager {
        ProxyMemoryManager(const ProxyMemoryManager&) = delete;
        void operator=(const ProxyMemoryManager&) = delete;

        std::unique_ptr<llvm::SectionMemoryManager> mm;

        uint8_t *StackMapSection;
        uintptr_t StackMapSectionSize;

        public:
        ProxyMemoryManager(std::unique_ptr<llvm::SectionMemoryManager> mm);

        uint8_t *allocateCodeSection(uintptr_t Size, unsigned Alignment,
                unsigned SectionID, llvm::StringRef SectionName) override;

        uint8_t *allocateDataSection(uintptr_t Size, unsigned Alignment,
                unsigned SectionID, llvm::StringRef SectionName,
                bool isReadOnly) override;

        bool finalizeMemory(std::string *ErrMsg = nullptr) override;

        virtual void invalidateInstructionCache();
    };
}

#endif // __YUGONG_LLVM_HPP__
