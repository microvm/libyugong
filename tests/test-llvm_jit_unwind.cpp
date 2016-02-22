#include "test_common.hpp"

#include <yugong.hpp>

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

void subro();

void declare_host_funcs(LLVMContext &ctx, Module &m) {
    // signature: i64 yg_stack_swap(i64* from, i64* to, i64 value)

    Type *i64 = Type::getInt64Ty(ctx);
    Type *i64ptr = Type::getInt64PtrTy(ctx);
    Type *voidty = Type::getVoidTy(ctx);

    {
		vector<Type*> paramtys = { i64ptr, i64ptr, i64 };
		Type *retty = i64;

		FunctionType *sig = FunctionType::get(retty, paramtys, false);
		Function::Create(sig, Function::ExternalLinkage, "yg_stack_swap", &m);
    }

    {
		vector<Type*> paramtys = { };
		Type *retty = voidty;

		FunctionType *sig = FunctionType::get(retty, paramtys, false);
		Function::Create(sig, Function::ExternalLinkage, "subro", &m);
    }
}

Function* make_call_leaf(string name, LLVMContext &ctx, Module &m) {
    IRBuilder<> b(ctx);

    vector<Type*> paramtys = { };
    Type *retty = Type::getVoidTy(ctx);

    FunctionType *sig = FunctionType::get(retty, paramtys, false);
    Function *func = Function::Create(sig, Function::ExternalLinkage, name, &m);

    BasicBlock *entry = BasicBlock::Create(ctx, "entry", func);
    b.SetInsertPoint(entry);

    Function *subro = m.getFunction("subro");
    vector<Value*> args = { };

    b.CreateCall(subro, args);


    b.CreateRetVoid();

    return func;
}

Function* make_ss_leaf(string name, YGStack *local, YGStack *remote, LLVMContext &ctx, Module &m) {
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

    b.CreateRetVoid();

    return func;
}

Function* make_caller(string name, Function *callee, LLVMContext &ctx, Module &m) {
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

    vector<Value*> args = { };
    b.CreateCall(callee, args);

    b.CreateRetVoid();

    return func;
}

YGStack main_stack, coro_stack;

void coro(uintptr_t notused) {
    ygt_print("Hi! This is coro, again. I guess I am called from bar this time.\n");

    YGCursor cursor(main_stack);

    unw_cursor_t &unw_cursor = cursor.unw_cursor;

    for(int i=0; i<3; i++) {
        uintptr_t pc = cursor._cur_pc();
        uintptr_t sp = cursor._cur_sp();
        ygt_print("  PC: %" PRIxPTR ", SP: %" PRIxPTR "\n", pc, sp);

        int rv = unw_step(&unw_cursor);
        ygt_print("  unw_step returns %d\n", rv);
    }

    ygt_print("Swapping back...\n");
    yg_stack_swap(&coro_stack, &main_stack, 0L);
}

void subro() {
    ygt_print("Hi! This is subro.\n");

    unw_context_t unw_ctx;
    unw_cursor_t unw_cursor;
    unw_getcontext(&unw_ctx);
    unw_init_local(&unw_cursor, &unw_ctx);

    for(int i=0; i<6; i++) {
        uintptr_t pc, sp;
        unw_get_reg(&unw_cursor, UNW_REG_IP, reinterpret_cast<unw_word_t*>(&pc));
        unw_get_reg(&unw_cursor, UNW_REG_SP, reinterpret_cast<unw_word_t*>(&sp));
        ygt_print("  PC: %" PRIxPTR ", SP: %" PRIxPTR "\n", pc, sp);

        int rv = unw_step(&unw_cursor);
        ygt_print("  unw_step returns %d\n", rv);
    }

    ygt_print("Returning...\n");
}

int main(int argc, char** argv) {
    ygt_print("Hello!\n");
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    InitializeNativeTargetDisassembler();

    ygt_print("Creating LLVM context and module...\n");
    LLVMContext ctx;
    auto m = make_unique<Module>("the_module", ctx);

    ygt_print("Declaring yg_stack_swap as an LLVM-level function...\n");
    declare_host_funcs(ctx, *m);

    ygt_print("Constructing bar...\n");
    Function *bar;

    if (argc>1) {
        ygt_print("Making bar a swap-stack site...\n");
    	bar = make_ss_leaf("bar", &main_stack, &coro_stack, ctx, *m);
    } else {
        ygt_print("Making bar a call site...\n");
    	bar = make_call_leaf("bar", ctx, *m);
    }


    verifyFunction(*bar);

    ygt_print("Constructing foo, the caller...\n");
    Function *foo;
    foo = make_caller("foo", bar, ctx, *m);

    verifyFunction(*foo);

    m->dump();

    Function *subro_func = m->getFunction("subro");

    ygt_print("Creating execution engine...\n");
    ExecutionEngine *ee = EngineBuilder(move(m)).setEngineKind(EngineKind::JIT).create();
    ygt_print("Execution engine created.\n");

    ygt_print("Adding global mapping yg_stack_swap...\n");
    ee->addGlobalMapping("yg_stack_swap", reinterpret_cast<uint64_t>(yg_stack_swap));
    ygt_print("Adding global mapping coro...\n");

    // On Mac, function names are prefixed by '_'.
    ee->addGlobalMapping(subro_func, reinterpret_cast<void*>(subro));

    ygt_print("JIT compiling...\n");
    ee->finalizeObject();

    ygt_print("Getting foo...\n");
    void (*the_real_foo)(int32_t, int64_t, float, double) = (void(*)(int32_t, int64_t, float, double))
            ee->getFunctionAddress("foo");
    ygt_print("the_real_foo == %p\n", the_real_foo);

    void (*the_real_bar)() = (void(*)())
            ee->getFunctionAddress("bar");
    ygt_print("the_real_bar == %p\n", the_real_bar);

    ygt_print("main == %p\n", main);

    ygt_print("Prepare corotine for introspection...\n");
    coro_stack = YGStack::alloc(8*1024*1024);
    coro_stack.init(reinterpret_cast<uintptr_t>(coro));

    ygt_print("Calling the_real_foo...\n");

    the_real_foo(100, 200, 300.0f, 400.0);

    ygt_print("Returned from foo.\n");

    return 0;
}
