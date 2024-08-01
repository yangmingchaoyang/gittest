#include "tinylang/CodeGen/CGProcedure.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/Casting.h"

using namespace tinylang;

//llvm::Value *Val 代表在基本块 BB 中定义的变量的值。这个值可能是一个常量、一个计算结果、一个函数参数，或者其他类型的 LLVM IR 值。
//writeLocalVariable 函数将这个值与基本块 BB 中的变量声明 Decl 关联起来，并更新 CurrentDef[BB].Defs 中的映射关系。
void CGProcedure::writeLocalVariable(llvm::BasicBlock *BB,
                                     Decl *Decl,
                                     llvm::Value *Val) {
  assert(BB && "Basic block is nullptr");
  assert(
      (llvm::isa<VariableDeclaration>(Decl) ||
       llvm::isa<FormalParameterDeclaration>(Decl)) &&
      "Declaration must be variable or formal parameter");
  assert(Val && "Value is nullptr");
  CurrentDef[BB].Defs[Decl] = Val;
}

llvm::Value *
CGProcedure::readLocalVariable(llvm::BasicBlock *BB,
                               Decl *Decl) {
  assert(BB && "Basic block is nullptr");
  assert(
      (llvm::isa<VariableDeclaration>(Decl) ||
       llvm::isa<FormalParameterDeclaration>(Decl)) &&
      "Declaration must be variable or formal parameter");
  auto Val = CurrentDef[BB].Defs.find(Decl);
  if (Val != CurrentDef[BB].Defs.end())
    return Val->second;
  return readLocalVariableRecursive(BB, Decl);
}

llvm::Value *CGProcedure::readLocalVariableRecursive(
    llvm::BasicBlock *BB, Decl *Decl) {
  llvm::Value *Val = nullptr;
  if (!CurrentDef[BB].Sealed) {
    // Add incomplete phi for variable.
    llvm::PHINode *Phi = addEmptyPhi(BB, Decl);
    CurrentDef[BB].IncompletePhis[Phi] = Decl;
    Val = Phi;
  } else if (auto *PredBB = BB->getSinglePredecessor()) {
    // Only one predecessor.
    Val = readLocalVariable(PredBB, Decl);
  } else {
    // Create empty phi instruction to break potential
    // cycles.
    llvm::PHINode *Phi = addEmptyPhi(BB, Decl);
    Val = Phi;
    writeLocalVariable(BB, Decl, Val);
    addPhiOperands(BB, Decl, Phi);
  }
  writeLocalVariable(BB, Decl, Val);
  return Val;
}

llvm::PHINode *
CGProcedure::addEmptyPhi(llvm::BasicBlock *BB, Decl *Decl) {
  return BB->empty()
             ? llvm::PHINode::Create(mapType(Decl), 0, "",
                                     BB)
             : llvm::PHINode::Create(mapType(Decl), 0, "",
                                     &BB->front());
}

void CGProcedure::addPhiOperands(llvm::BasicBlock *BB,
                                 Decl *Decl,
                                 llvm::PHINode *Phi) {
  for (auto I = llvm::pred_begin(BB),
            E = llvm::pred_end(BB);
       I != E; ++I) {
    Phi->addIncoming(readLocalVariable(*I, Decl), *I);
  }
  optimizePhi(Phi);
}

void CGProcedure::optimizePhi(llvm::PHINode *Phi) {
  llvm::Value *Same = nullptr;
  for (llvm::Value *V : Phi->incoming_values()) {
    if (V == Same || V == Phi)
      continue;
    if (Same && V != Same)
      return;
    Same = V;
  }
  if (Same == nullptr)
    Same = llvm::UndefValue::get(Phi->getType());
  // Collect phi instructions using this one.
  llvm::SmallVector<llvm::PHINode *, 8> CandidatePhis;
  //Phi->uses() 返回一个迭代器范围，表示所有引用当前 phi 指令 (Phi) 的使用点。
  for (llvm::Use &U : Phi->uses()) {
    if (auto *P =
            llvm::dyn_cast<llvm::PHINode>(U.getUser()))
      if (P != Phi)
        CandidatePhis.push_back(P);
  }
  //将当前 phi 指令 (Phi) 替换为一个新的值 (Same)。
  Phi->replaceAllUsesWith(Same);
  Phi->eraseFromParent();
  //确保所有受当前 phi 指令影响的其他 phi 指令也得到优化。
  for (auto *P : CandidatePhis)
    optimizePhi(P);
}
//封闭基本块的目的是在控制流图中标记一个基本块为最终状态，不再允许添加新的前驱基本块。
void CGProcedure::sealBlock(llvm::BasicBlock *BB) {
  assert(!CurrentDef[BB].Sealed &&
         "Attempt to seal already sealed block");
  for (auto PhiDecl : CurrentDef[BB].IncompletePhis) {
    addPhiOperands(BB, PhiDecl.second, PhiDecl.first);
  }
  CurrentDef[BB].IncompletePhis.clear();
  CurrentDef[BB].Sealed = true;
}

//writeVariable 函数的作用是根据变量的类型和作用域，将一个值写入适当的位置。
void CGProcedure::writeVariable(llvm::BasicBlock *BB,
                                Decl *D, llvm::Value *Val) {
  if (auto *V = llvm::dyn_cast<VariableDeclaration>(D)) {//尝试将 D 转换为 VariableDeclaration 类型，如果成功则继续处理局部变量。
    if (V->getEnclosingDecl() == Proc)//检查变量是否属于当前过程。如果是，调用 writeLocalVariable 函数，将值写入局部变量。
      writeLocalVariable(BB, D, Val);
    else if (V->getEnclosingDecl() ==
             CGM.getModuleDeclaration()) {
      Builder.CreateStore(Val, CGM.getGlobal(D));//检查变量是否是全局变量。如果是，使用 Builder.CreateStore 将值存储到全局变量中。
    } else
      llvm::report_fatal_error(
          "Nested procedures not yet supported");
  } else if (auto *FP =
                 llvm::dyn_cast<FormalParameterDeclaration>(
                     D)) {//尝试将 D 转换为 FormalParameterDeclaration 类型，如果成功则继续处理形式参数。
    if (FP->isVar()) {
      //检查形式参数是否是变量。如果是，则使用 Builder.CreateStore 将值存储到形式参数的位置（FormalParams[FP]）。
      Builder.CreateStore(Val, FormalParams[FP]);
    } else
      writeLocalVariable(BB, D, Val);//如果形式参数不是变量，调用 writeLocalVariable 将值写入局部变量。
  } else
    llvm::report_fatal_error("Unsupported declaration");
}

llvm::Value *CGProcedure::readVariable(llvm::BasicBlock *BB,
                                       Decl *D) {
  if (auto *V = llvm::dyn_cast<VariableDeclaration>(D)) {
    if (V->getEnclosingDecl() == Proc)
      return readLocalVariable(BB, D);
    else if (V->getEnclosingDecl() ==
             CGM.getModuleDeclaration()) {
      return Builder.CreateLoad(mapType(D),
                                CGM.getGlobal(D));
    } else
      llvm::report_fatal_error(
          "Nested procedures not yet supported");
  } else if (auto *FP =
                 llvm::dyn_cast<FormalParameterDeclaration>(
                     D)) {
    if (FP->isVar()) {
      return Builder.CreateLoad(
          mapType(FP)->getPointerElementType(),
          FormalParams[FP]);
    } else
      return readLocalVariable(BB, D);
  } else
    llvm::report_fatal_error("Unsupported declaration");
}

llvm::Type *CGProcedure::mapType(Decl *Decl) {
  if (auto *FP = llvm::dyn_cast<FormalParameterDeclaration>(
          Decl)) {
    llvm::Type *Ty = CGM.convertType(FP->getType());
    if (FP->isVar())
      Ty = Ty->getPointerTo();
    return Ty;
  }
  if (auto *V = llvm::dyn_cast<VariableDeclaration>(Decl))
    return CGM.convertType(V->getType());
  return CGM.convertType(llvm::cast<TypeDeclaration>(Decl));
}

llvm::FunctionType *CGProcedure::createFunctionType(
    ProcedureDeclaration *Proc) {
  llvm::Type *ResultTy = CGM.VoidTy;
  if (Proc->getRetType()) {
    ResultTy = mapType(Proc->getRetType());
  }
  auto FormalParams = Proc->getFormalParams();
  llvm::SmallVector<llvm::Type *, 8> ParamTypes;
  for (auto FP : FormalParams) {
    llvm::Type *Ty = mapType(FP);
    ParamTypes.push_back(Ty);
  }
  return llvm::FunctionType::get(ResultTy, ParamTypes,
                                 /* IsVarArgs */ false);
}

llvm::Function *
CGProcedure::createFunction(ProcedureDeclaration *Proc,
                            llvm::FunctionType *FTy) {
  llvm::Function *Fn = llvm::Function::Create(
      Fty, llvm::GlobalValue::ExternalLinkage,
      CGM.mangleName(Proc), CGM.getModule());
  // Give parameters a name.
  size_t Idx = 0;
  for (auto I = Fn->arg_begin(), E = Fn->arg_end(); I != E;
       ++I, ++Idx) {
    llvm::Argument *Arg = I;
    FormalParameterDeclaration *FP =
        Proc->getFormalParams()[Idx];
    if (FP->isVar()) {
      llvm::AttrBuilder Attr;
      llvm::TypeSize Sz =
          CGM.getModule()->getDataLayout().getTypeStoreSize(
              CGM.convertType(FP->getType()));
      Attr.addDereferenceableAttr(Sz);
      Attr.addAttribute(llvm::Attribute::NoCapture);
      Arg->addAttrs(Attr);
    }
    Arg->setName(FP->getName());
  }
  return Fn;
}

llvm::Value *
CGProcedure::emitInfixExpr(InfixExpression *E) {
  llvm::Value *Left = emitExpr(E->getLeft());
  llvm::Value *Right = emitExpr(E->getRight());
  llvm::Value *Result = nullptr;
  switch (E->getOperatorInfo().getKind()) {
  case tok::plus:
    Result = Builder.CreateNSWAdd(Left, Right);
    break;
  case tok::minus:
    Result = Builder.CreateNSWSub(Left, Right);
    break;
  case tok::star:
    Result = Builder.CreateNSWMul(Left, Right);
    break;
  case tok::kw_DIV:
    Result = Builder.CreateSDiv(Left, Right);
    break;
  case tok::kw_MOD:
    Result = Builder.CreateSRem(Left, Right);
    break;
  case tok::equal:
    Result = Builder.CreateICmpEQ(Left, Right);
    break;
  case tok::hash:
    Result = Builder.CreateICmpNE(Left, Right);
    break;
  case tok::less:
    Result = Builder.CreateICmpSLT(Left, Right);
    break;
  case tok::lessequal:
    Result = Builder.CreateICmpSLE(Left, Right);
    break;
  case tok::greater:
    Result = Builder.CreateICmpSGT(Left, Right);
    break;
  case tok::greaterequal:
    Result = Builder.CreateICmpSGE(Left, Right);
    break;
  case tok::kw_AND:
    Result = Builder.CreateAnd(Left, Right);
    break;
  case tok::kw_OR:
    Result = Builder.CreateOr(Left, Right);
    break;
  case tok::slash:
    // Divide by real numbers not supported.
    LLVM_FALLTHROUGH;
  default:
    llvm_unreachable("Wrong operator");
  }
  return Result;
}

// llvm::Value 是一个通用的基类，能够统一表示各种 IR 对象，支持 IR 的各种操作和计算。
llvm::Value *
CGProcedure::emitPrefixExpr(PrefixExpression *E) {
  llvm::Value *Result = emitExpr(E->getExpr());
  switch (E->getOperatorInfo().getKind()) {
  case tok::plus:
    // Identity - nothing to do.
    break;
  case tok::minus:
    Result = Builder.CreateNeg(Result);
    break;
  case tok::kw_NOT:
    Result = Builder.CreateNot(Result);
    break;
  default:
    llvm_unreachable("Wrong operator");
  }
  return Result;
}

//不同表达式节点转换为 LLVM IR 中的 llvm::Value 对象。
//emitExpr 函数的作用是将抽象语法树（AST）中的不同类型的表达式节点转换为 LLVM IR 中的 llvm::Value 对象。这个函数是 LLVM IR 生成过程中的核心部分，负责处理不同类型的表达式，并返回适当的 IR 代码表示这些表达式
llvm::Value *CGProcedure::emitExpr(Expr *E) {
  //如果表达式 E 是中缀表达式（例如 a + b），则调用 emitInfixExpr 来生成 IR。emitInfixExpr 根据操作符生成相应的 IR 指令。
  if (auto *Infix = llvm::dyn_cast<InfixExpression>(E)) {
    return emitInfixExpr(Infix);
  //果表达式 E 是前缀表达式（例如 -a），则调用 emitPrefixExpr 来生成 IR。emitPrefixExpr 处理前缀操作符并生成对应的 IR。
  } else if (auto *Prefix =
                 llvm::dyn_cast<PrefixExpression>(E)) {
    return emitPrefixExpr(Prefix);
  // 果表达式 E 是变量访问（例如 x），则调用 readVariable 来读取变量的值并生成 IR。readVariable 根据变量的声明类型和作用域生成对应的 IR 代码。
  } else if (auto *Var =
                 llvm::dyn_cast<VariableAccess>(E)) {
    auto *Decl = Var->getDecl();
    // With more languages features in place, here you need
    // to add array and record support.
    return readVariable(Curr, Decl);
  //如果表达式 E 是常量访问（例如 42），则递归调用 emitExpr 来处理常量表达式。这里 getExpr 返回一个表示常量的表达式，并生成对应的 IR。
  } else if (auto *Const =
                 llvm::dyn_cast<ConstantAccess>(E)) {
    return emitExpr(Const->getDecl()->getExpr());
  //如果表达式 E 是整数文字（例如 42），则使用 llvm::ConstantInt::get 创建一个 llvm::ConstantInt 对象表示整数常量。CGM.Int64Ty 是整数的类型，IntLit->getValue() 是整数的值。
  } else if (auto *IntLit =
                 llvm::dyn_cast<IntegerLiteral>(E)) {
    return llvm::ConstantInt::get(CGM.Int64Ty,
                                  IntLit->getValue());
  //如果表达式 E 是布尔文字（例如 true 或 false），则使用 llvm::ConstantInt::get 创建一个 llvm::ConstantInt 对象表示布尔常量。CGM.Int1Ty 是布尔类型，BoolLit->getValue() 是布尔值（0 或 1）。
  } else if (auto *BoolLit =
                 llvm::dyn_cast<BooleanLiteral>(E)) {
    return llvm::ConstantInt::get(CGM.Int1Ty,
                                  BoolLit->getValue());
  }
  llvm::report_fatal_error("Unsupported expression");
}

void CGProcedure::emitStmt(AssignmentStatement *Stmt) {
  auto *Val = emitExpr(Stmt->getExpr());
  writeVariable(Curr, Stmt->getVar(), Val);
}

void CGProcedure::emitStmt(ProcedureCallStatement *Stmt) {
  llvm::report_fatal_error("not implemented");
}

void CGProcedure::emitStmt(IfStatement *Stmt) {
  bool HasElse = Stmt->getElseStmts().size() > 0;

  // Create the required basic blocks.
  llvm::BasicBlock *IfBB = llvm::BasicBlock::Create(
      CGM.getLLVMCtx(), "if.body", Fn);
  llvm::BasicBlock *ElseBB =
      HasElse ? llvm::BasicBlock::Create(CGM.getLLVMCtx(),
                                         "else.body", Fn)
              : nullptr;
  llvm::BasicBlock *AfterIfBB = llvm::BasicBlock::Create(
      CGM.getLLVMCtx(), "after.if", Fn);

  llvm::Value *Cond = emitExpr(Stmt->getCond());
  Builder.CreateCondBr(Cond, IfBB,
                       HasElse ? ElseBB : AfterIfBB);
  sealBlock(Curr);

  setCurr(IfBB);
  emit(Stmt->getIfStmts());
  if (!Curr->getTerminator()) {
    Builder.CreateBr(AfterIfBB);
  }
  sealBlock(Curr);

  if (HasElse) {
    setCurr(ElseBB);
    emit(Stmt->getElseStmts());
    if (!Curr->getTerminator()) {
      Builder.CreateBr(AfterIfBB);
    }
    sealBlock(Curr);
  }
  setCurr(AfterIfBB);
}

void CGProcedure::emitStmt(WhileStatement *Stmt) {
  // The basic block for the condition.
  llvm::BasicBlock *WhileCondBB = llvm::BasicBlock::Create(
      CGM.getLLVMCtx(), "while.cond", Fn);
  // The basic block for the while body.
  llvm::BasicBlock *WhileBodyBB = llvm::BasicBlock::Create(
      CGM.getLLVMCtx(), "while.body", Fn);
  // The basic block after the while statement.
  llvm::BasicBlock *AfterWhileBB = llvm::BasicBlock::Create(
      CGM.getLLVMCtx(), "after.while", Fn);

  Builder.CreateBr(WhileCondBB);
  sealBlock(Curr);
  setCurr(WhileCondBB);
  llvm::Value *Cond = emitExpr(Stmt->getCond());
  Builder.CreateCondBr(Cond, WhileBodyBB, AfterWhileBB);

  setCurr(WhileBodyBB);
  emit(Stmt->getWhileStmts());
  Builder.CreateBr(WhileCondBB);
  sealBlock(Curr);
  sealBlock(WhileCondBB);

  setCurr(AfterWhileBB);
}

void CGProcedure::emitStmt(ReturnStatement *Stmt) {
  if (Stmt->getRetVal()) {
    llvm::Value *RetVal = emitExpr(Stmt->getRetVal());
    Builder.CreateRet(RetVal);
  } else {
    Builder.CreateRetVoid();
  }
}

void CGProcedure::emit(const StmtList &Stmts) {
  for (auto *S : Stmts) {
    if (auto *Stmt = llvm::dyn_cast<AssignmentStatement>(S))
      emitStmt(Stmt);
    else if (auto *Stmt =
                 llvm::dyn_cast<ProcedureCallStatement>(S))
      emitStmt(Stmt);
    else if (auto *Stmt = llvm::dyn_cast<IfStatement>(S))
      emitStmt(Stmt);
    else if (auto *Stmt = llvm::dyn_cast<WhileStatement>(S))
      emitStmt(Stmt);
    else if (auto *Stmt =
                 llvm::dyn_cast<ReturnStatement>(S))
      emitStmt(Stmt);
    else
      llvm_unreachable("Unknown statement");
  }
}

//函数的创建、参数的处理、局部变量的分配、语句的生成
void CGProcedure::run(ProcedureDeclaration *Proc) {
  this->Proc = Proc;
  Fty = createFunctionType(Proc);
  Fn = createFunction(Proc, Fty);

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(
      CGM.getLLVMCtx(), "entry", Fn);
  setCurr(BB);

  size_t Idx = 0;
  auto &Defs = CurrentDef[BB];
  //遍历函数 Fn 的参数，将它们与形式参数声明 (FormalParameterDeclaration) 进行映射。
  for (auto I = Fn->arg_begin(), E = Fn->arg_end(); I != E;
       ++I, ++Idx) {
    llvm::Argument *Arg = I;
    FormalParameterDeclaration *FP =
        Proc->getFormalParams()[Idx];
    // Create mapping FormalParameter -> llvm::Argument for
    // VAR parameters.
    FormalParams[FP] = Arg;
    Defs.Defs.insert(
        std::pair<Decl *, llvm::Value *>(FP, Arg));
  }
/*
遍历过程中的声明，如果声明是 VariableDeclaration（局部变量），则：
使用 mapType(Var) 将变量映射到 LLVM 类型。
如果类型是聚合类型（如结构体、数组等），使用 Builder.CreateAlloca(Ty) 创建一个内存分配（alloca）指令，并将其添加到基本块的定义映射中。
*/
  for (auto *D : Proc->getDecls()) {
    if (auto *Var =
            llvm::dyn_cast<VariableDeclaration>(D)) {
      llvm::Type *Ty = mapType(Var);
      if (Ty->isAggregateType()) {
        llvm::Value *Val = Builder.CreateAlloca(Ty);
        Defs.Defs.insert(
            std::pair<Decl *, llvm::Value *>(Var, Val));
      }
    }
  }
  //获取过程中的所有语句，并调用 emit 方法将它们转换为 LLVM IR。
  auto Block = Proc->getStmts();
  emit(Proc->getStmts());
  if (!Curr->getTerminator()) {
    Builder.CreateRetVoid();
  }
  //调用 sealBlock 方法封闭当前基本块，表示该基本块的定义已经完成，不会再添加前驱基本块。
  sealBlock(Curr);
}

void CGProcedure::run() {}
