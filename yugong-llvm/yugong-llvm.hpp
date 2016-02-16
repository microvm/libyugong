#ifndef __YUGONG_LLVM_HPP__
#define __YUGONG_LLVM_HPP__

#include "yugong.hpp"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Object/StackMapParser.h>

#include <map>
#include <string>
#include <vector>

namespace yg {
    using namespace std;
    using namespace llvm;

    typedef uint64_t smid_t;

    class ProxyMemoryManager;

    typedef StackMapV1Parser<support::endianness::native> SMParser;

    class YGStackMapHelper {
            YGStackMapHelper(const YGStackMapHelper&) = delete;
            void operator=(const YGStackMapHelper&) = delete;

        public:
            YGStackMapHelper(LLVMContext &_ctx, Module &_module);

            smid_t create_stack_map(IRBuilder<> &builder, ArrayRef<Value*> kas);

            void add_stackmap_section(uint8_t *start, uintptr_t size);

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
            vector<unique_ptr<SMParser>> sm_parsers;
    };

    class StackMapSectionRecorder: public JITEventListener {
            static const string STACKMAP_SECTION_NAME;

        public:
            struct Section {
                uint8_t* start;
                uintptr_t size;
            };

        private:
            vector<Section> _sections;

        public:
            void NotifyObjectEmitted(const object::ObjectFile &Obj,
                                         const RuntimeDyld::LoadedObjectInfo &L) override;

            void clear() { _sections.clear(); }
            typedef vector<Section>::iterator sections_iterator;
            typedef iterator_range<sections_iterator> sections_range_iterator;
            sections_range_iterator sections() { return iterator_range<sections_iterator>(_sections.begin(), _sections.end()); }
    };
}

#endif // __YUGONG_LLVM_HPP__
