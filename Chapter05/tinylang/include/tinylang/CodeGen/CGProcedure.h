#ifndef TINYLANG_CODEGEN_CGPROCEDURE_H
#define TINYLANG_CODEGEN_CGPROCEDURE_H

#include "tinylang/AST/AST.h"
#include "tinylang/CodeGen/CGModule.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"

namespace llvm {
class Function;
}

namespace tinylang {

class CGProcedure {//用于处理和生成 LLVM IR（中间表示）的过程
  CGModule &CGM;//引用 CGModule 对象，用于访问模块和全局状态。
  llvm::IRBuilder<> Builder;//llvm::IRBuilder 对象，用于生成 LLVM IR 指令。

  llvm::BasicBlock *Curr;//当前正在生成指令的基本块（llvm::BasicBlock）。

  ProcedureDeclaration *Proc;//当前处理的过程声明（ProcedureDeclaration）。
  llvm::FunctionType *Fty;//当前过程的函数类型（llvm::FunctionType）。
  llvm::Function *Fn;//当前过程对应的 LLVM 函数对象（llvm::Function）。

  struct BasicBlockDef {
    // Maps the variable (or formal parameter) to its definition.
    //变量的 LLVM 表达式（llvm::Value），TrackingVH 是一种智能指针，用于追踪 LLVM 值的引用，防止被释放。
    llvm::DenseMap<Decl *, llvm::TrackingVH<llvm::Value>> Defs;
    // Set of incompleted phi instructions.
    //跟踪尚未完成的 phi 指令。
    llvm::DenseMap<llvm::PHINode *, Decl *> IncompletePhis;
    // Block is sealed, that is, no more predecessors will be added.
    //标记基本块是否已经封闭，即不再接受新的前驱块。
    unsigned Sealed : 1;

    BasicBlockDef() : Sealed(0) {}
  };

  llvm::DenseMap<llvm::BasicBlock *, BasicBlockDef> CurrentDef;

  //将局部变量的值（LLVM 表达式）存储到基本块的定义映射中，以便后续可以使用。
  void writeLocalVariable(llvm::BasicBlock *BB, Decl *Decl, llvm::Value *Val);
  //从基本块的定义映射中查找变量，并返回其对应的 LLVM 表达式值。
  llvm::Value *readLocalVariable(llvm::BasicBlock *BB, Decl *Decl);
  //处理控制流中的复杂情况，特别是涉及到 phi 指令时，确保能够正确获取变量的值。
  llvm::Value *readLocalVariableRecursive(llvm::BasicBlock *BB, Decl *Decl);
  //创建一个新的 phi 指令节点，初始化为空，等待后续的操作数添加。这对于处理控制流中的变量非常重要。
  llvm::PHINode *addEmptyPhi(llvm::BasicBlock *BB, Decl *Decl);
  //将 phi 指令与实际的控制流路径中的值进行关联，确保 phi 指令能够正确选择变量的值。
  void addPhiOperands(llvm::BasicBlock *BB, Decl *Decl,
                      llvm::PHINode *Phi);
  //进行 phi 指令的优化，以提高生成的 LLVM IR 的效率。优化可能包括移除冗余的 phi 指令等。
  void optimizePhi(llvm::PHINode *Phi);
  //确保基本块的结构不被更改。这对于控制流图的稳定性和正确性非常重要。
  void sealBlock(llvm::BasicBlock *BB);

  //在函数中管理形式参数，确保每个参数在 LLVM IR 中都有对应的 llvm::Argument 对象。
  llvm::DenseMap<FormalParameterDeclaration *, llvm::Argument *> FormalParams;
  //将变量的值（LLVM 表达式）存储到基本块的定义映射中，类似于 writeLocalVariable，但可能用于全局变量或其他变量类型。
  void writeVariable(llvm::BasicBlock *BB, Decl *Decl, llvm::Value *Val);
  //从基本块的定义映射中查找变量，并返回其对应的 LLVM 表达式值，类似于 readLocalVariable。
  llvm::Value *readVariable(llvm::BasicBlock *BB, Decl *Decl);
  //根据变量声明 Decl 的类型信息，创建或获取相应的 LLVM 类型（llvm::Type）。这是在生成 LLVM IR 时将高级语言类型转换为 LLVM 类型的重要步骤。
  llvm::Type *mapType(Decl *Decl);
  //根据过程的参数类型和返回类型定义函数的 LLVM 类型。这对于创建过程对应的 LLVM 函数非常重要。
  llvm::FunctionType *createFunctionType(ProcedureDeclaration *Proc);
  //将过程声明转换为实际的 LLVM 函数对象，并将其添加到模块中。这是生成完整的 LLVM IR 函数定义的关键步骤。
  llvm::Function *createFunction(ProcedureDeclaration *Proc, llvm::FunctionType *FTy);
protected:
//设置当前基本块并更新插入点。
  void setCurr(llvm::BasicBlock *BB) {
    Curr = BB;
    Builder.SetInsertPoint(Curr);
  }
  //理中缀表达式（如 a + b），生成对应的 LLVM IR 代码。
  llvm::Value *emitInfixExpr(InfixExpression *E);
  //处理前缀表达式（如 -a），生成对应的 LLVM IR 代码。
  llvm::Value *emitPrefixExpr(PrefixExpression *E);
  //处理一般表达式，将表达式 E 转换为 LLVM IR 代码。它可能会调用 emitInfixExpr 和 emitPrefixExpr 以处理不同类型的表达式。
  llvm::Value *emitExpr(Expr *E);
//处理赋值语句，生成对应的 LLVM IR 代码来进行变量赋值。
  void emitStmt(AssignmentStatement *Stmt);
  //处理过程调用语句，生成调用相应函数的 LLVM IR 代码。
  void emitStmt(ProcedureCallStatement *Stmt);
  //处理条件语句（if 语句），生成 LLVM IR 代码来实现条件分支。
  void emitStmt(IfStatement *Stmt);
  //处理返回语句，生成 LLVM IR 代码来处理函数返回值。
  void emitStmt(WhileStatement *Stmt);
  void emitStmt(ReturnStatement *Stmt);
  //处理一个语句列表 Stmts，依次生成每个语句的 LLVM IR 代码。
  void emit(const StmtList &Stmts);

public:
  CGProcedure(CGModule &CGM)
      : CGM(CGM), Builder(CGM.getLLVMCtx()),
        Curr(nullptr){};
  //生成给定过程声明 Proc 的 LLVM IR 代码。它会设置基本块、生成语句和表达式等。
  void run(ProcedureDeclaration *Proc);
  //可能是一个重载函数，用于运行过程生成，可能用于处理没有参数的情况或者作为默认的执行入口。
  void run();
};
} // namespace tinylang
#endif