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

using namespace std;
using namespace llvm;
using namespace yg;

Function* make_leaf(string name, LLVMContext &ctx, Module &m) {
    IRBuilder<> b(ctx);

    vector<Type*> argtys = { };
    Type *retty = Type::getVoidTy(ctx);

    FunctionType *sig = FunctionType::get(retty, argtys, false);
    Function *func = Function::Create(sig, Function::ExternalLinkage, name, &m);

    BasicBlock *entry = BasicBlock::Create(ctx, "entry", func);
    b.SetInsertPoint(entry);
    b.CreateRetVoid();


    return func;
}

Function* make_caller(string name, Function *callee, StackMapHelper &smhelper, LLVMContext &ctx, Module &m) {
    IRBuilder<> b(ctx);

    auto i32 = Type::getInt32Ty(ctx);
    auto i64 = Type::getInt64Ty(ctx);
    auto f = Type::getFloatTy(ctx);
    auto d = Type::getDoubleTy(ctx);
    vector<Type*> argtys = { i32, i64, f, d };
    Type *retty = Type::getVoidTy(ctx);

    FunctionType *sig = FunctionType::get(retty, argtys, false);
    Function *func = Function::Create(sig, Function::ExternalLinkage, name, &m);

    BasicBlock *entry = BasicBlock::Create(ctx, "entry", func);
    b.SetInsertPoint(entry);

    vector<Value*> params;
    for (auto &arg : func->getArgumentList()) {
        params.push_back(&arg);
    }

    // Do some random computation
    auto I32 = ConstantInt::get(i32, 1L);
    auto I64 = ConstantInt::get(i64, 2L);
    auto F = ConstantFP::get(f, float(3.0));
    auto D = ConstantFP::get(d, double(4.0));

    auto x0 = b.CreateAdd(params[0], I32);
    auto x1 = b.CreateAdd(params[1], I64);
    auto x2 = b.CreateFAdd(params[2], F);
    auto x3 = b.CreateFAdd(params[3], D);

    vector<Value*> args = { };
    b.CreateCall(callee, args);

    vector<Value*> kas = { params[0], params[1], params[2], params[3], I32, I64, F, D, x0, x1, x2, x3 };
    smhelper.create_stack_map(b, kas);
    b.CreateRetVoid();

    return func;
}

int main() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    InitializeNativeTargetDisassembler();

    LLVMContext ctx;
    auto m = make_unique<Module>("the_module", ctx);

    StackMapHelper smhelper(ctx, *m);

    Function *bar = make_leaf("bar", ctx, *m);

    verifyFunction(*bar);

    Function *foo = make_caller("foo", bar, smhelper, ctx, *m);

    verifyFunction(*foo);

    m->dump();

    ygt_print("Creating execution engine...\n");
    ExecutionEngine *ee = EngineBuilder(move(m)).setEngineKind(EngineKind::JIT).create();
    ygt_print("Execution engine created.\n");

    StackMapSectionRecorder smsr;
    ygt_print("Registering JIT event listener...\n");
    ee->RegisterJITEventListener(&smsr);

    ygt_print("JIT compiling...\n");
    ee->finalizeObject();

    ygt_print("Adding stack map sections...\n");
    smhelper.add_stackmap_sections(smsr, *ee);


    ygt_print("Getting foo...\n");
    void (*the_real_foo)() = (void(*)())ee->getFunctionAddress("foo");
    ygt_print("the_real_foo == %p\n", the_real_foo);

    ygt_print("Calling real_foo...\n");

    the_real_foo();

    ygt_print("Returned from foo.\n");

    return 0;
}
