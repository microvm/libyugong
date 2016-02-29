#include "yugong-llvm.hpp"
#include "yugong-debug.hpp"

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

#include <cstring>
#include <vector>

namespace yg {
    using namespace std;
    using namespace llvm;

    const smid_t StackMapHelper::FIRST_SMID = 0x1000;

    StackMapHelper::StackMapHelper(LLVMContext &_ctx, Module &_module):
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

    smid_t StackMapHelper::create_stack_map(IRBuilder<> &builder, ArrayRef<Value*> kas) {
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

    void StackMapHelper::add_stackmap_sections(StackMapSectionRecorder &smsr, ExecutionEngine &ee) {
        for (auto &sec : smsr.sections()) {
            add_stackmap_section(sec, ee);
        }
    }

    void StackMapHelper::add_stackmap_section(AddrRange sec, ExecutionEngine &ee) {
        yg_debug("Adding stackmap section. start=%" PRIxPTR " size=%" PRIxPTR "\n", sec.start, sec.size);

        auto sm_parser = make_unique<SMParser>(ArrayRef<uint8_t>(reinterpret_cast<uint8_t*>(sec.start), sec.size));
        yg_debug("  SMParser address: %p\n", sm_parser.get());

        unsigned int i = 0;
        for (auto &rec : sm_parser->records()) {
            smid_t smid = rec.getID();
            string func_name = smid_to_call[smid]->getParent()->getParent()->getName();
            uint64_t func_addr = ee.getFunctionAddress(func_name);
            uint32_t inst_off = rec.getInstructionOffset();
            uintptr_t pc = func_addr + inst_off;
            yg_debug("  Entry %d: SMID=%" PRIx64 ", func_name=%s, func_addr=%" PRIx64 ", inst_off=%" PRIx32 ", PC=%" PRIxPTR "\n",
                    i, smid, func_name.c_str(), func_addr, inst_off, pc);

            pc_rec_index[pc] = make_pair(sm_parser.get(), i);
            i++;
        }

        parsers.push_back(move(sm_parser));
    }

    smid_t StackMapHelper::cur_smid(YGCursor &cursor) {
        uintptr_t pc = cursor._cur_pc();
        auto iter = pc_rec_index.find(pc);
        if (iter != pc_rec_index.end()) {
            SMParser* parser = iter->second.first;
            unsigned int index = iter->second.second;
            smid_t smid = parser->getRecord(index).getID();
            return smid;
        } else {
            return 0;
        }
    }

    void StackMapHelper::dump_keepalives(YGCursor &cursor, ArrayRef<katype::KAType> types, ArrayRef<void*> ptrs) {
        uintptr_t pc = cursor._cur_pc();
        auto parser_index = pc_rec_index.at(pc);

        SMParser* parser = parser_index.first;
        unsigned int index = parser_index.second;
        auto rec = parser->getRecord(index);

        unsigned int i = 0;
        for (auto &loc: rec.locations()) {
            dump_keepalive(cursor, *parser, loc, types[i], ptrs[i]);
            i++;
        }
    }

    void StackMapHelper::dump_keepalive(YGCursor &cursor, SMParser &parser, SMParser::LocationAccessor &loc,
            katype::KAType ty, void* ptr) {
        // Limitation: The current API does not support objects bigger than 64 bits.
        uint64_t value;

        SMParser::LocationKind kind = loc.getKind();

        switch (kind) {
        case SMParser::LocationKind::Register:
        case SMParser::LocationKind::Direct:
        case SMParser::LocationKind::Indirect: {
            uint16_t regnum = loc.getDwarfRegNum();
            uint64_t regval;
            unw_get_reg(&cursor.unw_cursor, regnum, &regval);

            if (kind == SMParser::LocationKind::Register) {
                value = regval;
                yg_debug("dump_keepalive. kind=Reg, regnum=%" PRIu16 ", value=%" PRIu64 " (0x%" PRIx64 ")\n", regnum, regval, regval);
            } else {
                int32_t offset = loc.getOffset();
                uint64_t sumval = regval + uint64_t(int64_t(offset));

                if (kind == SMParser::LocationKind::Direct) {
                    value = sumval;
                    yg_debug("dump_keepalive. kind=Direct, regnum=%" PRIu16 ", value=%" PRIu64 "+%" PRId32 "=%" PRIu64
                            " (0x%" PRIx64 "+%" PRId32 "=0x%" PRIx64 ")\n", regnum, regval, offset, sumval, regval, offset, sumval);
                } else {
                    value = _load_word(sumval);
                    yg_debug("dump_keepalive. kind=Inirect, regnum=%" PRIu16 ", addr=%" PRIu64 "+%" PRId32 "=%" PRIu64
                            " (0x%" PRIx64 "+%" PRId32 "=0x%" PRIx64 ")\n", regnum, regval, offset, sumval, regval, offset, sumval);
                }
            }
            break;
        }
        case SMParser::LocationKind::Constant: {
            uint32_t smallconst = loc.getSmallConstant();
            value = uint64_t(smallconst);
            yg_debug("dump_keepalive. kind=Constant, smallconst=%" PRId32 " (0x%" PRIx32 ")\n", smallconst, smallconst);
            break;
        }
        case SMParser::LocationKind::ConstantIndex: {
            uint32_t index = loc.getConstantIndex();
            value = parser.getConstant(index).getValue();
            yg_debug("dump_keepalive. kind=ConstantIndex, index=%" PRId32 ", value=%" PRIu64 " (0x%" PRIx64 ")\n", index, value, value);
            break;
        }
        }

        switch(ty) {
        case katype::I8: {
            memcpy(ptr, &value, sizeof(uint8_t));
            break;
        }
        case katype::I16: {
            memcpy(ptr, &value, sizeof(uint16_t));
            break;
        }
        case katype::I32: {
            memcpy(ptr, &value, sizeof(uint32_t));
            break;
        }
        case katype::I64: {
            memcpy(ptr, &value, sizeof(uint64_t));
             break;
        }
        case katype::FLOAT: {
            memcpy(ptr, &value, sizeof(float));
            break;
        }
        case katype::DOUBLE: {
            memcpy(ptr, &value, sizeof(double));
            break;
        }
        case katype::PTR: {
            memcpy(ptr, &value, sizeof(void*));
            break;
        }
        }
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
                uintptr_t begin = L.getSectionLoadAddress(name);
                uintptr_t size = section.getSize();
                AddrRange sec = {begin, size};
                _sections.push_back(sec);
            }
        }
    }

    const string EHFrameSectionRegisterer::EHFRAME_SECTION_NAME = ".eh_frame";

    void EHFrameSectionRegisterer::NotifyObjectEmitted(const object::ObjectFile &Obj,
                                 const RuntimeDyld::LoadedObjectInfo &L) {
#ifdef __APPLE__
        yg_debug("Do nothing on OSX.\n");
#else
        for (auto section : Obj.sections()) {
            StringRef name;
            section.getName(name);
            if (name == EHFRAME_SECTION_NAME) {
                uintptr_t addr = section.getAddress();
                uintptr_t begin = L.getSectionLoadAddress(name);
                uintptr_t size = section.getSize();

                uintptr_t end = begin + size;
                yg_debug("Found .eh_frame section. addr: %" PRIxPTR ", begin: %" PRIxPTR
                        ", size: %" PRIxPTR ", end: %" PRIxPTR "\n", addr, begin, size, end);

                AddrRange sec = {begin, size};
                _sections.push_back(sec);

            }
        }
#endif
    }

    void EHFrameSectionRegisterer::registerEHFrameSections() {
        for (auto sec : _sections) {
            uintptr_t start = sec.start;
            uintptr_t end = start + sec.size;

            uintptr_t cur = start;
            yg_debug("Parsing EH Section %" PRIxPTR "-%" PRIxPTR "...\n", start, end);
            while (cur < end) {
                yg_debug("cur == %" PRIxPTR "\n", cur);
                uint64_t length;
                uintptr_t cont;
                uint32_t small_length = *reinterpret_cast<uint32_t*>(cur);
                    if (small_length == 0) {
                        yg_debug("small_length == 0. Terminate.\n");
                        break;
                    } else if (small_length == 0xffffffff) {
                        length = *reinterpret_cast<uint32_t*>(cur + 4);
                        cont = cur + 12;
                    } else {
                        length = small_length;
                        cont = cur + 4;
                    }
                    uintptr_t next_rec = cont + length;

                    uint32_t cie_id = *reinterpret_cast<uint32_t*>(cont);
                    if (cie_id == 0) {
                        // This is a CIE. Skip it.
                        yg_debug("Skipping CIE 0x%" PRIxPTR "\n", cur);
                    } else {
                        // This is an FDE. Register with libunwind
                        yg_debug("Calling _unw_add_dynamic_fde(0x%" PRIxPTR ")\n", cur);
                        _unw_add_dynamic_fde(static_cast<unw_word_t>(cur));
                        yg_debug("Returned from _unw_add_dynamic_fde(0x%" PRIxPTR ")\n", cur);
                    }
                    cur = next_rec;
                }
        }
    }

}
