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

    smid_t YGStackMapHelper::create_stack_map(IRBuilder<> &builder, ArrayRef<Value*> kas) {
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

    void YGStackMapHelper::add_stackmap_section(uint8_t *start, uintptr_t size) {
        yg_debug("Found stackmap section. start=%p size=%" PRIxPTR "\n", start, size);

        auto sm_parser = make_unique<SMParser>(ArrayRef<uint8_t>(start, size));
        yg_debug("sm_parser == %p\n", sm_parser.get());
        sm_parsers.push_back(move(sm_parser));
    }

#ifdef __APPLE__
    const string StackMapSectionRecorder::STACKMAP_SECTION_NAME = "__llvm_stackmaps";
#else
    const string StackMapSectionRecorder::STACKMAP_SECTION_NAME = ".llvm_stackmaps";
#endif

    void StackMapSectionRecorder::NotifyObjectEmitted(const object::ObjectFile &Obj,
                                 const RuntimeDyld::LoadedObjectInfo &L) {
        for (auto section : Obj.sections()) {
            StringRef name;
            section.getName(name);
            if (name == STACKMAP_SECTION_NAME) {
                uint8_t* begin = reinterpret_cast<uint8_t*>(L.getSectionLoadAddress(name));
                uintptr_t size = static_cast<uintptr_t>(section.getSize());
                Section sec = {begin, size};
                _sections.push_back(sec);
            }
        }
    }

}
