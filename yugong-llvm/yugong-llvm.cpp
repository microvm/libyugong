#include "yugong-llvm.hpp"

namespace yg {
    ProxyMemoryManager::ProxyMemoryManager(std::unique_ptr<llvm::SectionMemoryManager> mm):
        mm(std::move(mm)), StackMapSection(nullptr), StackMapSectionSize(0LL) {}

    
    uint8_t *ProxyMemoryManager::allocateCodeSection(uintptr_t Size, unsigned Alignment, unsigned SectionID, llvm::StringRef SectionName) {
        printf("allocateCodeSection. Size=%" PRIuPTR ", Alignment=%u, SectionID=%u, SectionName=%s\n", Size, Alignment,
                SectionID, SectionName.str().c_str());
        return mm->allocateCodeSection(Size, Alignment, SectionID, SectionName);

    }
}
