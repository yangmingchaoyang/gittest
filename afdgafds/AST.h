#ifndef __AST_H__
#define __AST_H__

#include "Lexer.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#ifdef linux    //linux
#include "KaleidoscopeJIT.h"
#else //windows
#include "../include/KaleidoscopeJIT.h"
#endif

using namespace llvm;
using namespace llvm::orc;

Function *printFunc;	//printf函数声明

class PrototypeAST;
Function *getFunction(std::string Name);
static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
	const std::string &VarName);

	//IR 部分
	//LLVMContext 是 LLVM 中的一个上下文对象，用于管理 LLVM 的全局数据，例如类型和常量。每个 LLVMContext 实例包含一个独立的 LLVM 数据集，可以用于并行编译。
	//TheContext 是一个静态变量，这意味着它在整个程序生命周期内都有效，并且在所有函数中共享。
	static LLVMContext TheContext;
	//IRBuilder 是一个方便的类，用于生成 LLVM IR 指令。它提供了创建基本块、指令和其他 LLVM IR 构造的简单接口。
	static IRBuilder<> Builder(TheContext);
	/*Module 是 LLVM 中的一个类，表示一个完整的 LLVM IR 模块。模块是 LLVM IR 代码的顶级容器，包含函数和全局变量等。
Owner 是一个 unique_ptr，管理一个新的 Module 实例，其名称为 "test"。该模块使用 TheContext 作为其上下文。*/
	std::unique_ptr<Module> Owner(new Module("test", TheContext));
	/*
	TheModule 是一个指向 Module 的指针，用于表示当前正在操作的模块。
	TheModule 是一个静态变量，在整个程序生命周期内都有效，并且在所有函数中共享。
	*/
	static /*std::unique_ptr<Module>*/Module * TheModule;
	//NamedValues 是一个映射，键是变量的名称，值是指向 AllocaInst（分配指令）的指针。
	static std::map<std::string, AllocaInst *> NamedValues;
	/*
	TheFPM 是一个 unique_ptr，管理一个 legacy::FunctionPassManager 实例。
	FunctionPassManager 是一个管理和运行函数级别优化和分析 passes 的类。它允许对函数进行各种优化和转换。
	*/
	static std::unique_ptr<legacy::FunctionPassManager> TheFPM;
	/*KaleidoscopeJIT 是一个自定义的 JIT 编译器，用于即时编译和执行 LLVM IR 代码。这个 JIT 编译器通常是基于 LLVM 的 Orc JIT 或其他 JIT 基础设施构建的。*/
	static std::unique_ptr<KaleidoscopeJIT> TheJIT;
	//包含每个元素的最新原型
	/*PrototypeAST 是用于表示函数原型的抽象语法树节点。它包含函数的名称及其参数类型。
这个映射用于跟踪和管理所有已声明的函数原型，以便在代码生成和调用时使用。*/
	static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

	//表达式抽象语法树基类
	class ExprAST {
	public:
		virtual ~ExprAST() = default;
		virtual Value *codegen() = 0;
	};

	/*statement部分 -- lh*/
	//statement 基类
	class StatAST {
	public:
		virtual ~StatAST() = default;
		virtual Value* codegen() = 0;
	};

	std::unique_ptr<StatAST> LogError(const char *Str);

	//report errors found during code generation
	Value *LogErrorV(const char *Str) {
		LogError(Str);
		return nullptr;
	}

	//数字抽象语法树
	class NumberExprAST : public StatAST {
		int Val;

	public:
		NumberExprAST(int Val) : Val(Val) {}

		Value * codegen() {
			return ConstantInt::get(TheContext, APInt(32,Val,true));
		}
	};

	//变量抽象语法树
	class VariableExprAST : public StatAST {
		std::string Name;

	public:
		std::string getName() {
			return Name;
		}

		VariableExprAST(const std::string &Name) : Name(Name) {}

		Value * codegen() {
			// Look this variable up in the function.
			Value *V = NamedValues[Name];
			if (!V)
				return LogErrorV("Unknown variable name");
			return Builder.CreateLoad(V, Name.c_str());
		}
	};

	class NegExprAST : public StatAST {
		std::unique_ptr<StatAST> EXP;

		public:
			NegExprAST(std::unique_ptr<StatAST> EXP)
				: EXP(std::move(EXP)) {
			}

			Value * codegen() {
				auto Value = EXP->codegen();
				if (!Value)
					return nullptr;

				return Builder.CreateNeg(Value);
			}
	};

	//'+','-','*','/'二元运算表达式抽象语法树
	class BinaryExprAST : public StatAST {
		char Op;
		std::unique_ptr<StatAST> LHS, RHS;

	public:
		BinaryExprAST(char Op, std::unique_ptr<StatAST> LHS,
			std::unique_ptr<StatAST> RHS)
			: Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}


		Value * codegen() {

			Value *L = LHS->codegen();
			Value *R = RHS->codegen();
			if (!L || !R)
				return nullptr;

			switch (Op) {
			case '+':
				return Builder.CreateAdd(L, R, "addtmp");
			case '-':
				return Builder.CreateSub(L, R, "subtmp");
			case '*':
				return Builder.CreateMul(L, R, "multmp");
			case '/':
				L = Builder.CreateExactSDiv(L, R, "divtmp");
				// Convert bool 0/1 to int 0 or 1
				return Builder.CreateUIToFP(L, Type::getInt32Ty(TheContext), "booltmp");
			default:
				return LogErrorV("invalid binary operator");
			}
		}
	};

	//函数原型抽象语法树--函数名和参数列表
	class PrototypeAST {
		std::string Name;
		std::vector<std::string> Args;

	public:
		PrototypeAST(const std::string &Name, std::vector<std::string> Args)
			: Name(Name), Args(std::move(Args)) {}

		const std::string &getName() const { return Name; }

		Function * codegen() {
			//不允许函数重定义
			Function *TheFunction = TheModule->getFunction(Name);
			if (TheFunction)
				return (Function*)LogErrorV("Function cannot be redefined.");

			// 函数形参类型为 int
			std::vector<Type*> Integers(Args.size(),
				Type::getInt32Ty(TheContext));
			FunctionType *FT =
				FunctionType::get(Type::getInt32Ty(TheContext), Integers, false);

			// 注册该函数
			Function *F =
				Function::Create(FT, Function::ExternalLinkage, Name, TheModule);

			// 为函数参数命名
			unsigned Idx = 0;
			for (auto &Arg : F->args())
				Arg.setName(Args[Idx++]);

			return F;
		}
	};

	// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
	// the function.  This is used for mutable variables etc.
	static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
		const std::string &VarName) {
		IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
			TheFunction->getEntryBlock().begin());
		return TmpB.CreateAlloca(Type::getInt32Ty(TheContext), nullptr,
			VarName.c_str());
	}

	//空语句
	class NullStatAST:public StatAST {
	public:
		Value *codegen() {
            return Builder.getInt32(0); //null always return 0
		}
	};

	//变量声明语句
	class DecAST : public StatAST {
		std::vector<std::string> VarNames;
		std::unique_ptr<StatAST> Body;

	public:
		DecAST(std::vector<std::string> VarNames, std::unique_ptr<StatAST> Body)
			:VarNames(std::move(VarNames)), Body(std::move(Body)) {}

		Value *codegen() {
			std::vector<AllocaInst *> OldBindings;

			Function *TheFunction = Builder.GetInsertBlock()->getParent();

			for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
				const std::string &VarName = VarNames[i];

				Value *InitVal = ConstantInt::get(TheContext, APInt(32,0));

				AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
				Builder.CreateStore(InitVal, Alloca);

				OldBindings.push_back(NamedValues[VarName]);
				NamedValues[VarName] = Alloca;
			}

			return nullptr;
		}
	};

	//块语句
	class BlockStatAST : public StatAST {
		std::vector<std::unique_ptr<DecAST>> DecList;
		std::vector<std::unique_ptr<StatAST>> StatList;

	public:
		BlockStatAST(std::vector<std::unique_ptr<DecAST>> DecList, std::vector<std::unique_ptr<StatAST>> StatList)
			:DecList(std::move(DecList)), StatList(std::move(StatList)){}

	public:
		Value* codegen()
		{
			for (int i = 0; i < DecList.size(); i++)
			{
				DecList[i]->codegen();
			}
			for (int j = 0; j < StatList.size(); j++)
			{
				StatList[j]->codegen();
			}
			return Builder.getInt32(0); //block always return 0
		}
	};

	//Text //暂时不用--MT
	class TextAST {

	};

	class PrintStatAST : public StatAST {
        std::string text;
		std::vector<std::unique_ptr<StatAST>> expr;

	  public:
        PrintStatAST(std::string text, std::vector<std::unique_ptr<StatAST>> expr):
            text(text), expr(std::move(expr)){}
        Value *codegen()
        {
			Function *TheFunction = Builder.GetInsertBlock()->getParent();

            std::vector<llvm::Value *> paramArrayRef;
            Value *intFormat = Builder.CreateGlobalStringPtr(text.c_str());
            paramArrayRef.push_back(intFormat);

			for (int i = 0; i<expr.size(); i++)//auto tmp = expr.begin(); tmp != expr.end(); tmp++)
			{
				Value *tmpValue = expr[i]->codegen();
				paramArrayRef.push_back(tmpValue);
			}

			Builder.CreateCall(printFunc, paramArrayRef);

			Value *num = Builder.getInt32(0);//print always return 0
			return num;
        }
	};

	//IF Statement
	class IfStatAST : public StatAST {
		std::unique_ptr<StatAST> Cond;
		std::unique_ptr<StatAST> Then, Else;

	public:
		IfStatAST(std::unique_ptr<StatAST> Cond, std::unique_ptr<StatAST> Then,
			std::unique_ptr<StatAST> Else)
			: Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

		Value *codegen() {
			Value *CondV = Cond->codegen();
			if (!CondV)
				return nullptr;

			// Convert condition to a bool by comparing non-equal to 0.0.
			CondV = Builder.CreateICmpNE(
				CondV, Builder.getInt32(0), "ifcond");

			Function *TheFunction = Builder.GetInsertBlock()->getParent();

			// Create blocks for the then and else cases.  Insert the 'then' block at the
			// end of the function.
			BasicBlock *ThenBB = BasicBlock::Create(TheContext, "then", TheFunction);
			BasicBlock *MergeBB = BasicBlock::Create(TheContext, "ifcont");
			BasicBlock *ElseBB = nullptr;
			if (Else != nullptr) {
				ElseBB = BasicBlock::Create(TheContext, "else");
				Builder.CreateCondBr(CondV, ThenBB, ElseBB);
			}
			else {
				Builder.CreateCondBr(CondV, ThenBB, MergeBB);
			}

			// Emit then value.
			Builder.SetInsertPoint(ThenBB);

			Value *ThenV = Then->codegen();
			if (!ThenV)
				return nullptr;

			Builder.CreateBr(MergeBB);
			// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
			ThenBB = Builder.GetInsertBlock();

			// Emit else block.
			if (ElseBB != nullptr) {
				TheFunction->getBasicBlockList().push_back(ElseBB);
				Builder.SetInsertPoint(ElseBB);

				Value *ElseV = Else->codegen();
				if (!ElseV)
					return nullptr;
				Builder.CreateBr(MergeBB);

				// Codegen of 'Else' can change the current block, update ElseBB for the PHI.
				ElseBB = Builder.GetInsertBlock();
			}

			// Emit merge block.
			TheFunction->getBasicBlockList().push_back(MergeBB);
			Builder.SetInsertPoint(MergeBB);

			return nullptr;
		}

	};

	class RetStatAST : public StatAST {
		std::unique_ptr<StatAST> Val;

		public:
			RetStatAST(std::unique_ptr<StatAST> Val)
				: Val(std::move(Val)) {}

			Value *codegen() {
				Function *TheFunction = Builder.GetInsertBlock()->getParent();
				if (Value *RetVal = Val->codegen()) {
					Builder.CreateRet(RetVal);
					BasicBlock *afterRet = BasicBlock::Create(TheContext, "afterReturn", TheFunction);
					Builder.SetInsertPoint(afterRet);

					return RetVal;
				}
			}
	};

	class AssStatAST : public StatAST {
		std::unique_ptr<VariableExprAST> Name;
		std::unique_ptr<StatAST> Expression;

	public:
		AssStatAST(std::unique_ptr<VariableExprAST> Name, std::unique_ptr<StatAST> Expression)
			: Name(std::move(Name)), Expression(std::move(Expression)) {}

		Value *codegen() {
			Value* EValue = Expression->codegen();
			if (!EValue)
				return nullptr;

			Value *Variable = NamedValues[Name->getName()];
			if (!Variable)
				return LogErrorV("Unknown variable name");

			Builder.CreateStore(EValue, Variable);

			return EValue;
		}
	};

	//函数抽象语法树
	class FunctionAST {
		std::unique_ptr<PrototypeAST> Proto;
		std::unique_ptr<StatAST> Body;

	public:
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
			std::unique_ptr<StatAST> Body)
			: Proto(std::move(Proto)), Body(std::move(Body)) {}

		Function * codegen() {
			//可在当前模块中获取任何先前声明的函数的函数声明
			auto &P = *Proto;
			FunctionProtos[Proto->getName()] = std::move(Proto);
			Function *TheFunction = getFunction(P.getName());
			if (!TheFunction)
				return nullptr;

			// Create a new basic block to start insertion into.
			BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
			Builder.SetInsertPoint(BB);

			// Record the function arguments in the NamedValues map.
			NamedValues.clear();
			for (auto &Arg : TheFunction->args()) {

				// Create an alloca for this variable.
				AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

				// Store the initial value into the alloca.
				Builder.CreateStore(&Arg, Alloca);

				// Add arguments to variable symbol table.
				NamedValues[Arg.getName()] = Alloca;
			}

			Body->codegen();

			Builder.CreateRet(Builder.getInt32(0)); //如果函数没有返回语句，添加个RETURN 0
			verifyFunction(*TheFunction);

			return TheFunction;
		}
	};

	//函数调用抽象语法树
	class CallExprAST : public StatAST {
		std::string Callee;
		std::vector<std::unique_ptr<StatAST>> Args;
	public:
		CallExprAST(const std::string &Callee,
			std::vector<std::unique_ptr<StatAST>> Args)
			: Callee(Callee), Args(std::move(Args)) {}

		Value * codegen() {
			// Look up the name in the global module table.
			Function *CalleeF = getFunction(Callee);
			if (!CalleeF)
				return LogErrorV("Unknown function referenced");

			// If argument mismatch error.
			if (CalleeF->arg_size() != Args.size())
				return LogErrorV("Incorrect # arguments passed");

			std::vector<Value *> ArgsV;
			for (unsigned i = 0, e = Args.size(); i != e; ++i) {
				ArgsV.push_back(Args[i]->codegen());
				if (!ArgsV.back())
					return nullptr;
			}

			return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
		}
	};


	//程序的抽象语法树
	class ProgramAST {
		std::vector<std::unique_ptr<FunctionAST>> funcs;

	public:
		ProgramAST(std::vector<std::unique_ptr<FunctionAST>> funcs)
			:funcs(std::move(funcs)) {}
	};

	class WhileStatAST:public StatAST{
		std::unique_ptr<StatAST> Expr, Stat;
	public:
		WhileStatAST(std::unique_ptr<StatAST> Expr, std::unique_ptr<StatAST> Stat):
			Expr(std::move(Expr)), Stat(std::move(Stat)){}

		Value *codegen()
		{
			Function *TheFunction = Builder.GetInsertBlock()->getParent();
			BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);
			BasicBlock *AfterBB = BasicBlock::Create(TheContext, "afterLoop", TheFunction);

			Value *EndCond = Expr->codegen();
			if(!EndCond)
				return nullptr;
			EndCond = Builder.CreateICmpNE(EndCond, Builder.getInt32(0),
					"loopCondIn");
			Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

			Builder.SetInsertPoint(LoopBB);
			Value *inLoopVal = Stat->codegen();
			if(!inLoopVal)
				return nullptr;
			EndCond = Builder.CreateICmpNE(Expr->codegen(),
				Builder.getInt32(0), "loopCondOut");
			Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

			Builder.SetInsertPoint(AfterBB);

			return Builder.getInt32(0);
		}
	};


	//创建和初始化模块和传递管理器
	static void InitializeModuleAndPassManager() {

		// Open a new module.
		TheModule = Owner.get();//llvm::make_unique<Module>("VSL jit", TheContext);
		//设置模块的数据布局，使其与JIT编译器的目标机器数据布局一致。
		//它描述了目标平台的内存布局规则，包括数据对齐方式、类型大小等。
		//通过定义数据布局，LLVM可以对内存布局进行优化，减少内存碎片，提高缓存命中率，从而提高程序的整体性能。
		TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

		// Create a new pass manager attached to it.
		TheFPM = llvm::make_unique<legacy::FunctionPassManager>(TheModule);
		//向Pass管理器添加一系列优化Pass，用于优化函数级别的LLVM IR代码。
		// Promote allocas to registers.
		//将分配在栈上的局部变量提升到寄存器中，以减少内存访问，提高执行速度。
		TheFPM->add(createPromoteMemoryToRegisterPass());
		// Do simple "peephole" optimizations and bit-twiddling optzns.
		//进行简单的“窥孔”优化和位操作优化，合并指令，减少冗余操作。
		TheFPM->add(createInstructionCombiningPass());
		// Reassociate expressions.
		//重新关联表达式，以便更好地进行后续优化。
		TheFPM->add(createReassociatePass());
		// Eliminate Common SubExpressions.
		//消除公共子表达式，减少重复计算。
		TheFPM->add(createGVNPass());
		// Simplify the control flow graph (deleting unreachable blocks, etc).
		//：简化控制流图（CFG），删除不可达的基本块，优化分支和跳转。
		TheFPM->add(createCFGSimplificationPass());
		//初始化Pass管理器，使其为即将进行的优化Pass运行做好准备。
		TheFPM->doInitialization();

	}

	Function *getFunction(std::string Name) {
		// First, see if the function has already been added to the current module.
		if (auto *F = TheModule->getFunction(Name))
			return F;

		// If not, check whether we can codegen the declaration from some existing
		// prototype.
		auto FI = FunctionProtos.find(Name);
		if (FI != FunctionProtos.end())
			return FI->second->codegen();

		// If no existing prototype exists, return null.
		return nullptr;
	}

#endif
