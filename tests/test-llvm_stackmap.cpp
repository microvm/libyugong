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

#include <vector>
#include <string>
#include <memory>

using namespace std;
using namespace llvm;
using namespace yg;

int main() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    InitializeNativeTargetDisassembler();

    LLVMContext ctx;
    auto m = make_unique<Module>("the_module", ctx);
    IRBuilder<> b(ctx);

    YGStackMapHelper smhelper(ctx, *m);

    vector<Type*> bar_argtys = { };
    Type *bar_retty = Type::getVoidTy(ctx);

    FunctionType *bar_sig = FunctionType::get(bar_retty, bar_argtys, false);
    Function *bar = Function::Create(bar_sig, Function::ExternalLinkage, "bar", m.get());
    {
        BasicBlock *entry = BasicBlock::Create(ctx, "entry", bar);
        b.SetInsertPoint(entry);
        b.CreateRetVoid();
    }

    verifyFunction(*bar);

    vector<Type*> foo_argtys = { };
    Type *foo_retty = Type::getVoidTy(ctx);

    FunctionType *foo_sig = FunctionType::get(foo_retty, foo_argtys, false);
    Function *foo = Function::Create(foo_sig, Function::ExternalLinkage, "foo", m.get());
    {
        BasicBlock *entry = BasicBlock::Create(ctx, "entry", foo);
        b.SetInsertPoint(entry);
        vector<Value*> args = {};
        b.CreateCall(bar, args);
        vector<Value*> kas = {};
        smhelper.create_stack_map(b, kas);
        b.CreateRetVoid();
    }

    verifyFunction(*foo);

    m->dump();

    ygt_print("Creating execution engine...\n");
    ExecutionEngine *ee = EngineBuilder(move(m)).setEngineKind(EngineKind::JIT).create();
    ygt_print("Execution engine created.\n");

    StackMapSectionRecorder smsc;
    ygt_print("Registering JIT event listener...\n");
    ee->RegisterJITEventListener(&smsc);

    ygt_print("JIT compiling...\n");
    ee->finalizeObject();

    ygt_print("Stack map sections:\n");
    for (auto &sec: smsc.sections()) {
        ygt_print("  section: start=%p, size=%" PRIxPTR "\n", sec.start, sec.size);
        smhelper.add_stackmap_section(sec.start, sec.size);
    }


    ygt_print("Getting foo...\n");
    void (*the_real_foo)() = (void(*)())ee->getFunctionAddress("foo");
    ygt_print("the_real_foo == %p\n", the_real_foo);

    ygt_print("Calling real_foo...\n");

    the_real_foo();

    ygt_print("Returned from foo.\n");

    return 0;
}
