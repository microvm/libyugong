#include <yugong.hpp>
#include <yugong-llvm.hpp>

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

#include <vector>
#include <string>
#include <memory>

using namespace std;
using namespace llvm;
using namespace yg;

int main() {
    LLVMContext ctx;
    Module *m = new Module("the_module", ctx);
    IRBuilder<> b(ctx);

    YGStackMapHelper smhelper(ctx, *m);

    vector<Type*> bar_argtys = { };
    Type *bar_retty = Type::getVoidTy(ctx);

    FunctionType *bar_sig = FunctionType::get(bar_retty, bar_argtys, false);
    Function *bar = Function::Create(bar_sig, Function::ExternalLinkage, "bar", m);
    {
        BasicBlock *entry = BasicBlock::Create(ctx, "entry", bar);
        b.SetInsertPoint(entry);
        b.CreateRetVoid();
    }

    verifyFunction(*bar);

    vector<Type*> foo_argtys = { };
    Type *foo_retty = Type::getVoidTy(ctx);

    FunctionType *foo_sig = FunctionType::get(foo_retty, foo_argtys, false);
    Function *foo = Function::Create(foo_sig, Function::ExternalLinkage, "foo", m);
    {
        BasicBlock *entry = BasicBlock::Create(ctx, "entry", foo);
        b.SetInsertPoint(entry);
        vector<Value*> args = {};
        b.CreateCall(bar, args);
        vector<Value*> kas = {};
        smhelper.createStackMap(b, kas);
        b.CreateRetVoid();
    }

    verifyFunction(*foo);

    m->dump();

    return 0;
}
