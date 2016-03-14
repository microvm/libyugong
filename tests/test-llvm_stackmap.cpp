#include "test_common.hpp"

#include <yugong.hpp>
#include <yugong-llvm.hpp>

#include <llvm/ExecutionEngine/MCJIT.h> // Force MCJIT linking
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler.h>
#include <llvm/MC/MCInstPrinter.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>

#include <algorithm>
#include <vector>
#include <string>
#include <memory>
#include <tuple>
#include <utility>

using namespace std;
using namespace llvm;
using namespace yg;

smid_t next_smid = 0x1000;

Function* make_leaf(string name, LLVMContext &ctx, Module &m) {
    IRBuilder<> b(ctx);

    vector<Type*> paramtys = { };
    Type *retty = Type::getVoidTy(ctx);

    FunctionType *sig = FunctionType::get(retty, paramtys, false);
    Function *func = Function::Create(sig, Function::ExternalLinkage, name, &m);

    BasicBlock *entry = BasicBlock::Create(ctx, "entry", func);
    b.SetInsertPoint(entry);
    b.CreateRetVoid();


    return func;
}

void declare_ss(LLVMContext &ctx, Module &m) {
    // signature: i64 yg_stack_swap(i64* from, i64* to, i64 value)

    Type *i64 = Type::getInt64Ty(ctx);
    Type *i64ptr = Type::getInt64PtrTy(ctx);
    vector<Type*> paramtys = { i64ptr, i64ptr, i64 };
    Type *retty = i64;

    FunctionType *ss_sig = FunctionType::get(retty, paramtys, false);
    Function::Create(ss_sig, Function::ExternalLinkage, "yg_stack_swap", &m);
}

pair<Function*, smid_t> make_ss_leaf(string name, YGStack *local, YGStack *remote, StackMapHelper &smhelper, LLVMContext &ctx, Module &m) {
    IRBuilder<> b(ctx);

    vector<Type*> paramtys = { };
    Type *retty = Type::getVoidTy(ctx);

    FunctionType *sig = FunctionType::get(retty, paramtys, false);
    Function *func = Function::Create(sig, Function::ExternalLinkage, name, &m);

    BasicBlock *entry = BasicBlock::Create(ctx, "entry", func);
    b.SetInsertPoint(entry);

    Type *i64 = Type::getInt64Ty(ctx);
    Type *i64ptr = Type::getInt64PtrTy(ctx);

    Constant *local_const_int = ConstantInt::get(i64, reinterpret_cast<uint64_t>(local));
    Constant *remote_const_int = ConstantInt::get(i64, reinterpret_cast<uint64_t>(remote));

    Value *local_const_ptr = ConstantExpr::getIntToPtr(local_const_int, i64ptr);
    Value *remote_const_ptr = ConstantExpr::getIntToPtr(remote_const_int, i64ptr);

    Constant *I64_0 = ConstantInt::get(i64, 0L);

    Function *ss = m.getFunction("yg_stack_swap");
    vector<Value*> args = { local_const_ptr, remote_const_ptr, I64_0 };

    b.CreateCall(ss, args);

    vector<Value*> kas = { };
    smid_t smid = next_smid++;
    smhelper.create_stack_map(smid, b, kas);

    b.CreateRetVoid();

    return make_pair(func, smid);
}

pair<Function*, smid_t> make_caller(string name, Function *callee, StackMapHelper &smhelper, LLVMContext &ctx, Module &m) {
    IRBuilder<> b(ctx);

    auto i32 = Type::getInt32Ty(ctx);
    auto i64 = Type::getInt64Ty(ctx);
    auto f = Type::getFloatTy(ctx);
    auto d = Type::getDoubleTy(ctx);
    vector<Type*> paramtys = { i32, i64, f, d };
    Type *retty = Type::getVoidTy(ctx);

    FunctionType *sig = FunctionType::get(retty, paramtys, false);
    Function *func = Function::Create(sig, Function::ExternalLinkage, name, &m);

    BasicBlock *entry = BasicBlock::Create(ctx, "entry", func);
    b.SetInsertPoint(entry);

    vector<Value*> params;
    for (auto &arg : func->getArgumentList()) {
        params.push_back(&arg);
    }

    // Do some random computation
    auto I32 = ConstantInt::get(i32, 1L);
    auto I64 = ConstantInt::get(i64, 12345678902468L);
    auto F = ConstantFP::get(f, float(3.0));
    auto D = ConstantFP::get(d, double(4.0));

    auto x0 = b.CreateAdd(params[0], I32);
    auto x1 = b.CreateAdd(params[1], I64);
    auto x2 = b.CreateFAdd(params[2], F);
    auto x3 = b.CreateFAdd(params[3], D);

    // This is for testing the "Direct" stack map location.
    Value *mem = b.CreateAlloca(i64);
    auto I64_MAGIC = ConstantInt::get(i64, 0xdeadbeefcafebabeUL);
    b.CreateStore(I64_MAGIC, mem);

    vector<Value*> args = { };
    b.CreateCall(callee, args);

    vector<Value*> kas = { params[0], params[1], params[2], params[3], I32, I64, F, D, x0, x1, x2, x3, mem};
    smid_t smid = next_smid++;
    smhelper.create_stack_map(smid, b, kas);
    b.CreateRetVoid();

    return make_pair(func, smid);
}

YGStack main_stack, coro_stack;

struct MyCtx {
    smid_t foo_smid, bar_smid;
    StackMapHelper *smhelper;
};

void coro(MyCtx *myctx) {
    ygt_print("Hi! This is coro. foo_smid=%" PRIx64 ", bar_smid=%" PRIx64 ", smhelper=%p\n",
            myctx->foo_smid, myctx->bar_smid, myctx->smhelper);
    yg_stack_swap(&coro_stack, &main_stack, 0L);

    ygt_print("Hi! This is coro, again. I guess I am called from bar this time.\n");

    YGCursor cursor(main_stack);
    StackMapHelper &smhelper = *myctx->smhelper;

    for(int i=0; i<3; i++) {
        uintptr_t pc = cursor._cur_pc();
        smid_t smid = smhelper.cur_smid(cursor);
        ygt_print("  PC: %" PRIxPTR ", SMID: %" PRIx64 "\n", pc, smid);

        if (smid == myctx->foo_smid) {
            ygt_print("    This is foo.\n");
            int32_t p0, c0, x0;
            int64_t p1, c1, x1;
            float   p2, c2, x2;
            double  p3, c3, x3;
            uint64_t *mem;
            vector<katype::KAType> types = {
                    katype::I32, katype::I64, katype::FLOAT, katype::DOUBLE,
                    katype::I32, katype::I64, katype::FLOAT, katype::DOUBLE,
                    katype::I32, katype::I64, katype::FLOAT, katype::DOUBLE,
                    katype::PTR
            };
            vector<void*> ptrs = {
                    &p0, &p1, &p2, &p3, &c0, &c1, &c2, &c3, &x0, &x1, &x2, &x3, &mem
            };

            smhelper.dump_keepalives(cursor, types, ptrs);

            ygt_print("      KAs: "
                    "%" PRId32 ", %" PRId64 ", %f, %lf, "
                    "%" PRId32 ", %" PRId64 ", %f, %lf, "
                    "%" PRId32 ", %" PRId64 ", %f, %lf, "
                    "%p\n",
                    p0, p1, p2, p3, c0, c1, c2, c3, x0, x1, x2, x3, mem
                    );

            uint64_t memval = *mem;
            ygt_print("      *mem=%" PRIu64 " (0x%" PRIx64 ")\n", memval, memval);
        } else if (smid == myctx->bar_smid) {
            ygt_print("    This is bar.\n");
        } else if (smid == 0) {
            ygt_print("    No stack map. pc=%" PRIxPTR "\n", pc);
        } else {
            ygt_print("    Unrecognized stackmap ID %" PRIx64 "\n", smid);
        }

        cursor.step();
    }

    ygt_print("Swapping back...\n");
    yg_stack_swap(&coro_stack, &main_stack, 0L);
}

int main() {
    ygt_print("Hello!\n");
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    InitializeNativeTargetDisassembler();

    ygt_print("Creating LLVM context and module...\n");
    LLVMContext ctx;
    auto m = make_unique<Module>("the_module", ctx);

    ygt_print("Creating StackMapHelper...\n");
    StackMapHelper smhelper;

    ygt_print("Declaring yg_stack_swap as an LLVM-level function...\n");
    declare_ss(ctx, *m);

    ygt_print("Constructing bar, the ss leaf...\n");
    Function *bar;
    smid_t bar_smid;
    tie(bar, bar_smid) = make_ss_leaf("barss", &main_stack, &coro_stack, smhelper, ctx, *m);

    verifyFunction(*bar);

    ygt_print("Constructing foo, the caller...\n");
    Function *foo;
    smid_t foo_smid;
    tie(foo, foo_smid) = make_caller("foo", bar, smhelper, ctx, *m);

    verifyFunction(*foo);

    m->dump();

    Function *ss_func = m->getFunction("yg_stack_swap");

    ygt_print("Creating execution engine...\n");
    ExecutionEngine *ee = EngineBuilder(move(m)).setEngineKind(EngineKind::JIT).create();
    ygt_print("Execution engine created.\n");

    StackMapSectionRecorder smsr;
    ygt_print("Registering JIT event listener...\n");
    ee->RegisterJITEventListener(&smsr);

    EHFrameSectionRegisterer reg;
    ygt_print("Registering EHFrame registerer...\n");
    ee->RegisterJITEventListener(&reg);

    ygt_print("Adding global mapping yg_stack_swap...\n");
    ee->addGlobalMapping(ss_func, reinterpret_cast<void*>(yg_stack_swap));

    ygt_print("JIT compiling...\n");
    ee->finalizeObject();

    ygt_print("Adding stack map sections...\n");
    smhelper.add_stackmap_sections(smsr, *ee);

    ygt_print("Registering EH frames...\n");
    reg.registerEHFrameSections();

    uintptr_t yg_stack_swap_addr = ee->getFunctionAddress("yg_stack_swap");
    ygt_print("yg_stack_swap_addr=%" PRIxPTR ", yg_stack_swap=%p\n", yg_stack_swap_addr, yg_stack_swap);
    assert(yg_stack_swap_addr == reinterpret_cast<uintptr_t>(yg_stack_swap));

    ygt_print("Getting foo...\n");
    void (*the_real_foo)(int32_t, int64_t, float, double) = (void(*)(int32_t, int64_t, float, double))
            ee->getFunctionAddress("foo");
    ygt_print("the_real_foo == %p\n", the_real_foo);

    ygt_print("Prepare corotine for introspection...\n");
    coro_stack = YGStack::alloc(8*1024*1024);
    coro_stack.init(reinterpret_cast<uintptr_t>(coro));

    MyCtx my_ctx = { foo_smid, bar_smid, &smhelper };

    ygt_print("Swap-stack to prepare the coroutine...\n");
    yg_stack_swap(&main_stack, &coro_stack, reinterpret_cast<uintptr_t>(&my_ctx));

    ygt_print("Back from coro.\n");

    ygt_print("Calling the_real_foo...\n");

    the_real_foo(100, 200, 300.0f, 400.0);

    ygt_print("Returned from foo.\n");

    return 0;
}
