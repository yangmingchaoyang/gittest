#ifndef __PARSER_H__
#define __PARSER_H__
#include "AST.h"
#include "Lexer.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
using namespace llvm;

static int CurTok;
static std::map<char, int> BinopPrecedence;
static int getNextToken() { return CurTok = gettok(); }

static std::unique_ptr<StatAST> ParseExpression();
std::unique_ptr<StatAST> LogError(const char *Str);
static std::unique_ptr<StatAST> ParseNumberExpr();
static std::unique_ptr<StatAST> ParseParenExpr();
static std::unique_ptr<DecAST> ParseDec();
std::unique_ptr<StatAST> LogError(const char *Str);
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);
std::unique_ptr<StatAST> LogErrorS(const char *Str);
std::unique_ptr<DecAST> LogErrorD(const char *Str);
static std::unique_ptr<StatAST> ParseStatement();

//解析如下格式的表达式：
// identifer || identifier(expression list)
static std::unique_ptr<StatAST> ParseIdentifierExpr() {
	std::string IdName = IdentifierStr;

	getNextToken();

	//解析成变量表达式
	if (CurTok != '(')
		return llvm::make_unique<VariableExprAST>(IdName);

	// 解析成函数调用表达式
	getNextToken();
	std::vector<std::unique_ptr<StatAST>> Args;
	if (CurTok != ')') {
		while (true) {
			if (auto Arg = ParseExpression())
				Args.push_back(std::move(Arg));
			else
				return nullptr;

			if (CurTok == ')')
				break;

			if (CurTok != ',')
				return LogErrorS("Expected ')' or ',' in argument list");
			getNextToken();
		}
	}

	getNextToken();

	return llvm::make_unique<CallExprAST>(IdName, std::move(Args));
}

//解析取反表达式
static std::unique_ptr<StatAST> ParseNegExpr() {
	getNextToken();
	std::unique_ptr<StatAST> Exp = ParseExpression();
	if (!Exp)
		return nullptr;

	return llvm::make_unique<NegExprAST>(std::move(Exp));
}

//解析成 标识符表达式、整数表达式、括号表达式中的一种
static std::unique_ptr<StatAST> ParsePrimary() {
	switch (CurTok) {
	default:
		return LogError("unknown token when expecting an expression");
	case VARIABLE:
		return ParseIdentifierExpr();
	case INTEGER:
		return ParseNumberExpr();
	case '(':
		return ParseParenExpr();
	case '-':
		return ParseNegExpr();
	}
}

//GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

//解析二元表达式
//参数 ：
//ExprPrec 左部运算符优先级
//LHS 左部操作数
// 递归得到可以结合的右部，循环得到一个整体二元表达式
static std::unique_ptr<StatAST> ParseBinOpRHS(int ExprPrec,
	std::unique_ptr<StatAST> LHS) {

	while (true) {
		int TokPrec = GetTokPrecedence();

		// 当右部没有运算符或右部运算符优先级小于左部运算符优先级时 退出循环和递归
		if (TokPrec < ExprPrec)
			return LHS;

		if(CurTok == '}')
			return LHS;

		// 保存左部运算符
		int BinOp = CurTok;
		getNextToken();

		// 得到右部表达式
		auto RHS = ParsePrimary();
		if (!RHS)
			return nullptr;

		// 如果该右部表达式不与该左部表达式结合 那么递归得到右部表达式
		int NextPrec = GetTokPrecedence();
		if (TokPrec < NextPrec) {
			RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
			if (!RHS)
				return nullptr;
		}

		// 将左右部结合成新的左部
		LHS = llvm::make_unique<BinaryExprAST>(BinOp, std::move(LHS),
			std::move(RHS));
	}
}

// 解析得到表达式
static std::unique_ptr<StatAST> ParseExpression() {
	auto LHS = ParsePrimary();
	if (!LHS)
		return nullptr;

	return ParseBinOpRHS(0, std::move(LHS));
}

// numberexpr ::= number
static std::unique_ptr<StatAST> ParseNumberExpr() {
	auto Result = llvm::make_unique<NumberExprAST>(NumberVal);
	//略过数字获取下一个输入
	getNextToken();
	return std::move(Result);
}

//declaration::=VAR variable_list
static std::unique_ptr<DecAST> ParseDec() {
	//eat 'VAR'
	getNextToken();

	std::vector<std::string> varNames;
	//保证至少有一个变量的名字
	if (CurTok != VARIABLE) {
		return LogErrorD("expected identifier after VAR");
	}

	while (true)
	{
		varNames.push_back(IdentifierStr);
		//eat VARIABLE
		getNextToken();
		if (CurTok != ',')
			break;
		getNextToken();
		if (CurTok != VARIABLE) {
			return LogErrorD("expected identifier list after VAR");
		}
	}

	auto Body = nullptr;

	return llvm::make_unique<DecAST>(std::move(varNames), std::move(Body));
}

//null_statement::=CONTINUE
static std::unique_ptr<StatAST> ParseNullStat() {
	getNextToken();
	return llvm::make_unique<NullStatAST>();
}

//block::='{' declaration_list statement_list '}'
static std::unique_ptr<StatAST> ParseBlock() {
	//存储变量声明语句及其他语句
	std::vector<std::unique_ptr<DecAST>> DecList;
	std::vector<std::unique_ptr<StatAST>> StatList;
	getNextToken();   //eat '{'
	if (CurTok == VAR) {
		auto varDec = ParseDec();
		DecList.push_back(std::move(varDec));
	}
	while (CurTok != '}') {
		if (CurTok == VAR) {
			LogErrorS("Can't declare VAR here!");
		}
		else if (CurTok == '{') {
			ParseBlock();
		}
		else if (CurTok == CONTINUE) {
			getNextToken();
		}
		else {
			auto statResult = ParseStatement();
			StatList.push_back(std::move(statResult));
		}
	}
	getNextToken();  //eat '}'

	return llvm::make_unique<BlockStatAST>(std::move(DecList), std::move(StatList));
}

//prototype ::= VARIABLE '(' parameter_list ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
	if (CurTok != VARIABLE)
		return LogErrorP("Expected function name in prototype");

	std::string FnName = IdentifierStr;
	getNextToken();

	if (CurTok != '(')
		return LogErrorP("Expected '(' in prototype");

	std::vector<std::string> ArgNames;
	getNextToken();
	while (CurTok == VARIABLE)
	{
		ArgNames.push_back(IdentifierStr);
		getNextToken();
		if (CurTok == ',')
			getNextToken();
	}
	if (CurTok != ')')
		return LogErrorP("Expected ')' in prototype");

	// success.
	getNextToken(); // eat ')'.

	return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

//function ::= FUNC VARIABLE '(' parameter_lst ')' statement
static std::unique_ptr<FunctionAST> ParseFunc()
{
	getNextToken(); // eat FUNC.
	auto Proto = ParsePrototype();
	if (!Proto)
		return nullptr;

	auto E = ParseStatement();
	if (!E)
		return nullptr;

	return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
}

//解析括号中的表达式
static std::unique_ptr<StatAST> ParseParenExpr() {
	// 过滤'('
	getNextToken();
	auto V = ParseExpression();
	if (!V)
		return nullptr;

	if (CurTok != ')')
		return LogError("expected ')'");
	// 过滤')'
	getNextToken();
	return V;
}

//解析 IF Statement
static std::unique_ptr<StatAST> ParseIfStat() {
	getNextToken(); // eat the IF.

					// condition.
	auto Cond = ParseExpression();
	if (!Cond)
		return nullptr;

	if (CurTok != THEN)
		return LogErrorS("expected THEN");
	getNextToken(); // eat the THEN

	auto Then = ParseStatement();
	if (!Then)
		return nullptr;

	std::unique_ptr<StatAST> Else = nullptr;
	if (CurTok == ELSE) {
        getNextToken();
		Else = ParseStatement();
		if (!Else)
			return nullptr;
	}
	else if(CurTok != FI)
		return LogErrorS("expected FI or ELSE");

	getNextToken();

	return llvm::make_unique<IfStatAST>(std::move(Cond), std::move(Then),
		std::move(Else));
}

//PRINT,能输出变量和函数调用的值
static std::unique_ptr<StatAST> ParsePrintStat()
{
    std::string text = "";
	std::vector<std::unique_ptr<StatAST>> expr;
	getNextToken();//eat PRINT

    while(CurTok == VARIABLE || CurTok == TEXT || CurTok == '('
            || CurTok == '-' || CurTok == INTEGER)
    {
        if(CurTok == TEXT)
        {
            text += IdentifierStr;
            getNextToken();
        }
        else
        {
            text += " %d ";
			expr.push_back(std::move(ParseExpression()));
		}

        if(CurTok != ',')
            break;
        getNextToken(); //eat ','
    }

    return llvm::make_unique<PrintStatAST>(text, std::move(expr));
}

//解析 RETURN Statement
static std::unique_ptr<StatAST> ParseRetStat() {
	getNextToken();
	auto Val = ParseExpression();
	if (!Val)
		return nullptr;

	return llvm::make_unique<RetStatAST>(std::move(Val));
}

//解析 赋值语句
static std::unique_ptr<StatAST> ParseAssStat() {
	auto a = ParseIdentifierExpr();
	VariableExprAST* Name = (VariableExprAST*)a.get();
	auto NameV = llvm::make_unique<VariableExprAST>(Name->getName());
	if (!Name)
		return nullptr;
	if (CurTok != ASSIGN_SYMBOL)
		return LogErrorS("need := in assignment statment");
	getNextToken();

	auto Expression = ParseExpression();
	if (!Expression)
		return nullptr;

	return llvm::make_unique<AssStatAST>(std::move(NameV), std::move(Expression));
}

//解析while语句
static std::unique_ptr<StatAST> ParseWhileStat()
{
	getNextToken();//eat WHILE

	auto E = ParseExpression();
	if(!E)
		return nullptr;

	if(CurTok != DO)
		return LogErrorS("expect DO in WHILE statement");
	getNextToken();//eat DO

	auto S = ParseStatement();
	if(!S)
	return nullptr;

	if(CurTok != DONE)
		return LogErrorS("expect DONE in WHILE statement");
	getNextToken();//eat DONE

	return llvm::make_unique<WhileStatAST>(std::move(E), std::move(S));
}

static std::unique_ptr<StatAST> ParseStatement()
{
	switch (CurTok) {
		case IF:
			return ParseIfStat();
			break;
        case PRINT:
            return ParsePrintStat();
		case RETURN:
			return ParseRetStat();
		case VAR:
			return ParseDec();
			break;
		case '{':
			return ParseBlock();
			break;
		case CONTINUE:
			return ParseNullStat();
		case WHILE:
			return ParseWhileStat();
			break;
		default:
			auto E = ParseAssStat();
			return E;
	}
}

//解析程序结构
static std::unique_ptr<ProgramAST> ParseProgramAST() {
	//接受程序中函数的语法树
	std::vector<std::unique_ptr<FunctionAST>> Functions;

	//循环解析程序中所有函数
	while (CurTok != TOK_EOF) {
		auto Func=ParseFunc();
		Functions.push_back(std::move(Func));
	}

	return llvm::make_unique<ProgramAST>(std::move(Functions));
}

//错误信息打印
std::unique_ptr<StatAST> LogError(const char *Str) {
	fprintf(stderr, "Error: %s\n", Str);
	return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
	LogError(Str);
	return nullptr;
}
std::unique_ptr<StatAST> LogErrorS(const char *Str) {
	fprintf(stderr, "Error: %s\n", Str);
	return nullptr;
}
std::unique_ptr<DecAST> LogErrorD(const char *Str) {
	fprintf(stderr, "Error: %s\n", Str);
	return nullptr;
}

// Top-Level parsing
static void HandleFuncDefinition() {
	if (auto FnAST = ParseFunc()) {
		FnAST->codegen();
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

/*
Builder 的作用和功能：
创建指令：

Builder 可以用来创建各种 LLVM IR 指令，例如加载（load）、存储（store）、算术运算（add、sub）、逻辑运算（and、or）、条件分支（branch）、调用函数（call）等。
管理基本块：

Builder 可以将新创建的指令插入到指定的基本块中，确保指令在正确的位置和顺序上。
处理类型转换：

Builder 提供了方法来进行类型转换，例如 getInt8Ty() 可以获取 i8 类型（8 位整数类型），getPointerTo() 可以将该类型转换为指针类型。
使用上下文信息：

Builder 通常与 LLVM 的上下文对象（llvm::LLVMContext）一起使用，确保在同一上下文中创建的所有对象和指令都是一致的。
*/

/*
在 LLVM 中，llvm::LLVMContext 是一个核心类，用于管理 LLVM IR（Intermediate Representation，中间表示）的全局上下文信息。它在整个 LLVM 编译器框架中起着重要的作用。

主要作用和功能：
管理全局状态：

llvm::LLVMContext 对象维护了 LLVM 编译器中全局的状态信息，确保在整个编译过程中数据和操作的一致性。
创建和管理 LLVM IR：

所有的 LLVM IR 对象（如类型、指令、基本块、函数等）都是在特定的上下文中创建的。这样可以确保在同一个上下文中创建的对象是相互兼容的。
线程安全性：

llvm::LLVMContext 对象通常是线程局部的，这样可以避免多线程环境下的竞态条件和数据冲突。每个线程一般使用自己的上下文对象，保证操作的独立性和安全性。
全局对象管理：

LLVM IR 中的全局对象（如全局变量、全局函数）通常也是通过上下文管理和创建的，确保这些对象在整个程序中的唯一性和一致性。
类型系统管理：

上下文对象还负责 LLVM IR 中的类型系统管理，包括定义和处理各种数据类型，确保在编译过程中类型的正确性和互操作性。
*/

//声明printf函数
static void DeclarePrintfFunc()
{
	std::vector<llvm::Type *> printf_arg_types;
	// 向类型向量中添加一个元素，表示 printf 函数的第一个参数类型为 i8*（指向字符型的指针）。
	printf_arg_types.push_back(Builder.getInt8Ty()->getPointerTo());
	/*上下文隔离性：	
	LLVM 使用 llvm::LLVMContext 来隔离不同的编译单元或线程之间的 LLVM IR 对象。每个上下文对象都维护了一个独立的符号表和类型表，以确保在不同上下文中创建的相同类型的对象不会冲突。
	类型一致性：
	LLVM 中的类型系统是在上下文级别管理的。通过传递 TheContext 参数，确保 getInt32Ty 返回的是在相同上下文中创建的 i32 类型对象。这样可以避免在不同上下文中创建的类型之间出现不一致的情况。
	编译器优化：
	LLVM 通过使用上下文对象，可以进行更多的编译器优化。例如，通过共享类型信息和符号表，LLVM 可以更好地分析和优化生成的代码，提高编译器的效率和生成的代码质量*/
	FunctionType *printType = FunctionType::get(
		IntegerType::getInt32Ty(TheContext), printf_arg_types, true);
	/*
	函数类型 (FunctionType*)
	这是一个描述函数签名的对象，指定了函数的返回类型和参数类型。
	链接属性 (Linkage)
	指定了函数在链接时的可见性和链接方式。常见的链接属性包括 ExternalLinkage、InternalLinkage、AvailableExternallyLinkage 等。
	函数名称 (llvm::Twine)
	函数的名称，通常以字符串形式表示，可以使用 llvm::Twine 类型来表示。
	所属模块 (llvm::Module*)
	函数所属的 LLVM 模块，即将该函数添加到哪个模块中。
	*/
	printFunc = llvm::Function::Create(printType, llvm::Function::ExternalLinkage,
									   llvm::Twine("printf"), TheModule);
	printFunc->setCallingConv(llvm::CallingConv::C);//：设置函数的调用约定为C调用约定。

	std::vector<std::string> ArgNames;//创建一个空的字符串向量，用于存储函数参数的名称。
	//使用名称 "printf" 作为键，将下面创建的函数原型对象存储到 FunctionProtos 中。
	//表示这个函数已经创建，不能再创建了
	FunctionProtos["printf"] = std::move(llvm::make_unique<PrototypeAST>("printf", std::move(ArgNames)));
}

//program ::= function_list
static void MainLoop() {
	DeclarePrintfFunc();
	while(CurTok != TOK_EOF)
		HandleFuncDefinition();

	if (emitIR)
	{
		std::string IRstr;
		FILE *IRFile = fopen("IRCode.ll", "w");
		raw_string_ostream *rawStr = new raw_string_ostream(IRstr);
		TheModule->print(*rawStr, nullptr);
		fprintf(IRFile, "%s\n", rawStr->str().c_str());
	}

	if(!emitObj)
	{
		Function *main = getFunction("main");
		if (!main)
			printf("main is null");
		std::string errStr;
		ExecutionEngine *EE = EngineBuilder(std::move(Owner)).setErrorStr(&errStr).create();
		if (!EE)
		{
			errs() << "Failed to construct ExecutionEngine: " << errStr << "\n";
			return;
		}
		std::vector<GenericValue> noarg;
		GenericValue gv = EE->runFunction(main, noarg);
	}
}
#endif
