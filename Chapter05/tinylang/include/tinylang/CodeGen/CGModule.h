#ifndef TINYLANG_CODEGEN_CGMODULE_H
#define TINYLANG_CODEGEN_CGMODULE_H

#include "tinylang/AST/AST.h"
#include "llvm/IR/LLVMContext.h"//包含 LLVM 上下文的定义
#include "llvm/IR/Module.h"

namespace tinylang {

class CGModule {
  llvm::Module *M;//LLVM 模块的指针。这个模块对象是 LLVM 中的主要容器，用于存储所有的函数、全局变量等。

  ModuleDeclaration *Mod;//指向 TinyLang 的模块声明的指针。这个对象包含了 TinyLang 程序的模块级别信息。

  // Repository of global objects.
  //一个哈希映射，用于存储全局对象。Decl * 是声明的指针，llvm::GlobalObject * 是对应的 LLVM 全局对象。
  /*llvm::GlobalObject用途：表示 LLVM IR 中的全局对象，例如全局变量和函数。*/
  llvm::DenseMap<Decl *, llvm::GlobalObject *> Globals;

public:
/*
llvm::Type *VoidTy：LLVM 中的 void 类型。
llvm::Type *Int1Ty：LLVM 中的 i1 类型（1 位整数）。
llvm::Type *Int32Ty：LLVM 中的 i32 类型（32 位整数）。
llvm::Type *Int64Ty：LLVM 中的 i64 类型（64 位整数）。
llvm::Constant *Int32Zero：LLVM 中的 32 位整数常量 0。
*/
/*
llvm::LLVMContext Context;

// 基本类型
llvm::Type *VoidTy = llvm::Type::getVoidTy(Context);
llvm::Type *Int1Ty = llvm::Type::getInt1Ty(Context);
llvm::Type *Int32Ty = llvm::Type::getInt32Ty(Context);
llvm::Type *FloatTy = llvm::Type::getFloatTy(Context);
llvm::Type *DoubleTy = llvm::Type::getDoubleTy(Context);

// 复合类型
llvm::Type *Int32ArrayTy = llvm::ArrayType::get(Int32Ty, 10);
std::vector<llvm::Type*> StructElements = {Int32Ty, FloatTy};
llvm::Type *StructTy = llvm::StructType::create(Context, StructElements, "MyStruct");
llvm::Type *VectorTy = llvm::VectorType::get(Int32Ty, 4);

// 函数类型
std::vector<llvm::Type*> FuncParams = {Int32Ty, FloatTy};
llvm::FunctionType *FuncTy = llvm::FunctionType::get(DoubleTy, FuncParams, false);

// 常量
llvm::Constant *Int32Zero = llvm::ConstantInt::get(Int32Ty, 0);
llvm::Constant *FloatOne = llvm::ConstantFP::get(FloatTy, 1.0);
llvm::Constant *NullPtr = llvm::ConstantPointerNull::get(llvm::PointerType::get(Int32Ty, 0));
*/
  llvm::Type *VoidTy;
  llvm::Type *Int1Ty;
  llvm::Type *Int32Ty;
  llvm::Type *Int64Ty;
  llvm::Constant *Int32Zero;

public:
  CGModule(llvm::Module *M) : M(M) { initialize(); }
  void initialize();

  llvm::LLVMContext &getLLVMCtx() { return M->getContext(); }
  llvm::Module *getModule() { return M; }
  ModuleDeclaration *getModuleDeclaration() { return Mod; }

//将自定义的类型声明 (TypeDeclaration*) 转换为 LLVM 的类型 (llvm::Type*)。这通常用于将高级语言的类型转换为 LLVM IR 的类型。
  llvm::Type *convertType(TypeDeclaration *Ty);
//对声明 (Decl*) 进行名称重整（mangling），生成唯一的字符串名称。名称重整通常用于区分不同作用域中的同名实体。
  std::string mangleName(Decl *D);
//根据给定的声明 (Decl*)，返回对应的 LLVM 全局对象 (llvm::GlobalObject*)。这可以是全局变量或函数等。
  llvm::GlobalObject *getGlobal(Decl *);
//行与模块相关的主要工作。接受一个 ModuleDeclaration*，用于处理模块内的内容。具体的操作需要在方法实现中查看，通常包括代码生成、优化等步骤。
  void run(ModuleDeclaration *Mod);
};
} // namespace tinylang
#endif