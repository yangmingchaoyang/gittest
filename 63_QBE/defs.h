#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "incdir.h"

// Structure and enum definitions
// Copyright (c) 2019 Warren Toomey, GPL3

enum {
  TEXTLEN = 512			// Length of identifiers in input
  //定义了标识符在输入中的最大长度为512。这个枚举用于指定输入中标识符的字符数上限。
};

// Commands and default filenames
#define AOUT "a.out"//默认可执行文件名，设置为 "a.out"。
#define ASCMD "as -g -o "//汇编器命令，用于将汇编代码转换为目标文件，包括调试信息 ("-g")。命令字符串是 "as -g -o "。
#define QBECMD "qbe -o "// QBE（一个C语言编译器）命令，用于将中间代码转换为汇编代码。命令字符串是 "qbe -o "。
#define LDCMD "cc -g -no-pie -o "//链接器命令，用于将目标文件链接为可执行文件，包括调试信息 ("-g") 和禁用位置独立执行 ("-no-pie")。命令字符串是 "cc -g -no-pie -o "。
#define CPPCMD "cpp -nostdinc -isystem "//C预处理器命令，用于预处理源文件，禁用标准头文件 ("-nostdinc") 并指定系统头文件目录 ("-isystem")。命令字符串是 "cpp -nostdinc -isystem "。

// Token types
//符号定义
enum {
  T_EOF,//表示文件结束。

  // Binary operators
  /*T_ASSIGN: 赋值操作符 (=)。
    T_ASPLUS: 赋值加法 (+=)。
    T_ASMINUS: 赋值减法 (-=)。
    T_ASSTAR: 赋值乘法 (*=)。
    T_ASSLASH: 赋值除法 (/=)。
    T_ASMOD: 赋值取模 (%=)。
    T_QUESTION: 三元条件运算符 (?)。
    T_LOGOR: 逻辑或 (||)。
    T_LOGAND: 逻辑与 (&&)*/
  T_ASSIGN, T_ASPLUS, T_ASMINUS,		// 1
  T_ASSTAR, T_ASSLASH, T_ASMOD,			// 4
  T_QUESTION, T_LOGOR, T_LOGAND,		// 7
  T_OR, T_XOR, T_AMPER,				// 10按位或、按位异或、按位与。
  T_EQ, T_NE,					// 13相等和不等比较操作符。
  T_LT, T_GT, T_LE, T_GE,			// 15 小于、大于、小于等于、大于等于比较操作符。
  T_LSHIFT, T_RSHIFT,				// 19左移和右移位操作符。
  T_PLUS, T_MINUS, T_STAR, T_SLASH, T_MOD,	// 21加法、减法、乘法、除法、取模操作符。

  // Other operators
  T_INC, T_DEC, T_INVERT, T_LOGNOT,		// 26自增、自减、按位取反、逻辑非操作符。

  // Type keywords
  T_VOID, T_CHAR, T_INT, T_LONG,		// 30表示不同的基本数据类型。

  // Other keywords
  T_IF, T_ELSE, T_WHILE, T_FOR, T_RETURN,	// 34控制流关键字，如条件语句、循环语句、返回语句等。
  T_STRUCT, T_UNION, T_ENUM, T_TYPEDEF,		// 39结构体、联合体、枚举、类型定义关键字。
  T_EXTERN, T_BREAK, T_CONTINUE, T_SWITCH,	// 43外部变量、循环控制关键字、开关语句关键字。
  T_CASE, T_DEFAULT, T_SIZEOF, T_STATIC,	// 47开关语句中的标签、sizeof 操作符、静态变量关键字。

  // Structural tokens
  T_INTLIT, T_STRLIT, T_SEMI, T_IDENT,		// 51整数字面值、字符串字面值、分号、标识符。
  T_LBRACE, T_RBRACE, T_LPAREN, T_RPAREN,	// 55大括号、小括号。
  T_LBRACKET, T_RBRACKET, T_COMMA, T_DOT,	// 59方括号、逗号、点号。
  T_ARROW, T_COLON				// 63箭头操作符、冒号。
};

// Token structure
//符号代表
struct token {
  int token;			// Token type, from the enum list above//该字段表示标记的类型，使用了一个枚举类型中定义的整数值。
  char *tokstr;			// String version of the token// 该字段包含标记的字符串表示。对于关键字、操作符和标识符等，这个字段保存相应的字符串。
  int intvalue;			// For T_INTLIT, the integer value//仅对于整数字面量标记 T_INTLIT 有效，表示整数字面量的实际整数值。
};

// AST node types. The first few line up
// with the related tokens
//操作类型
enum {
  A_ASSIGN = 1, A_ASPLUS, A_ASMINUS, A_ASSTAR,			//  1赋值操作 (T_ASSIGN) 加法赋值操作+= (T_ASPLUS) 减法赋值操作 (T_ASMINUS) 乘法赋值操作 (T_ASSTAR)
  A_ASSLASH, A_ASMOD, A_TERNARY, A_LOGOR,			//  5 除法赋值操作 (T_ASSLASH) 取模赋值操作 (T_ASMOD) 三元条件表达式 (A_TERNARY) 逻辑或 (T_LOGOR)
  A_LOGAND, A_OR, A_XOR, A_AND, A_EQ, A_NE, A_LT,		//  9逻辑与 (T_LOGAND) 位或 (T_OR)  位异或 (T_XOR) 位与 (A_AND) 相等比较 (T_EQ) 不等比较 (T_NE) 小于比较 (T_LT)
  A_GT, A_LE, A_GE, A_LSHIFT, A_RSHIFT,				// 16 大于比较 (T_GT) 小于等于比较 (T_LE) 大于等于比较 (T_GE) 左移位操作 (T_LSHIFT) 右移位操作 (T_RSHIFT)
  A_ADD, A_SUBTRACT, A_MULTIPLY, A_DIVIDE, A_MOD,		// 21加法 (A_ADD) 减法 (A_SUBTRACT) 乘法 (A_MULTIPLY) 除法 (A_DIVIDE)  取模操作 (T_MOD)
  A_INTLIT, A_STRLIT, A_IDENT, A_GLUE,				// 26 整数字面量 (T_INTLIT) 字符串字面量 (T_STRLIT) 标识符 (T_IDENT) 用于连接两个语句的节点
  A_IF, A_WHILE, A_FUNCTION, A_WIDEN, A_RETURN,			// 30 条件语句 (T_IF)  循环语句 (T_WHILE) 函数定义 类型强制转换 (T_INT, T_LONG, T_CHAR) 返回语句 (T_RETURN)
  A_FUNCCALL, A_DEREF, A_ADDR, A_SCALE,				// 35 函数调用  解引用操作 (T_STAR)  取地址操作 (T_AMPER)  数组索引操作 (T_LBRACKET)
  A_PREINC, A_PREDEC, A_POSTINC, A_POSTDEC,			// 39 前置递增操作 (T_INC) 前置递减操作 (T_DEC) 后置递增操作 (T_INC)  后置递减操作 (T_DEC)
  A_NEGATE, A_INVERT, A_LOGNOT, A_TOBOOL, A_BREAK,		// 43取负操作 (T_MINUS) 按位取反操作 (T_INVERT) 逻辑非操作 (T_LOGNOT) 转为布尔值操作 中断语句 (T_BREAK)
  A_CONTINUE, A_SWITCH, A_CASE, A_DEFAULT, A_CAST		// 48继续语句 (T_CONTINUE) 开关语句 (T_SWITCH)  开关语句的分支 (T_CASE) 开关语句的默认分支 (T_DEFAULT) 类型转换操作
};

// Primitive types. The bottom 4 bits is an integer
// value that represents the level of indirection,
// e.g. 0= no pointer, 1= pointer, 2= pointer pointer etc.
/*
P_NONE: 没有指针，表示一个基本数据类型。
P_VOID: 代表void类型，通常用于表示不返回值的函数或通用指针。
P_CHAR: 代表char类型，通常用于表示字符。
P_INT: 代表int类型，通常用于表示整数。
P_LONG: 代表long类型，通常用于表示长整数。
P_STRUCT: 代表结构体类型，通常用于表示用户自定义的数据结构。
P_UNION: 代表联合类型，通常用于表示共享存储空间的不同数据类型。
*/
//变量类型
enum {
  P_NONE, P_VOID = 16, P_CHAR = 32, P_INT = 48, P_LONG = 64,
  P_STRUCT=80, P_UNION=96
};

// Structural types
/*
S_VARIABLE: 代表变量。这个枚举值可以用于标识源代码中声明的变量。
S_FUNCTION: 代表函数。这个枚举值可以用于标识源代码中声明的函数。
S_ARRAY: 代表数组。这个枚举值可以用于标识源代码中声明的数组。
*/
//段类型
enum {
  S_VARIABLE, S_FUNCTION, S_ARRAY
};

// Storage classes
//变量位置类型
enum {
  C_GLOBAL = 1,			// Globally visible symbol全局可见符号，即在整个程序中都可以访问的符号。
  C_LOCAL,			// Locally visible symbol本地可见符号，即在定义它的块（通常是函数内）中可见的符号。
  C_PARAM,			// Locally visible function parameter函数参数，即函数的参数变量。
  C_EXTERN,			// External globally visible symbol外部全局可见符号，即在其他文件中定义但在当前文件中声明的符号。
  C_STATIC,			// Static symbol, visible in one file静态符号，只在当前文件中可见的符号，具有文件作用域。
  C_STRUCT,			// A struct结构体类型。
  C_UNION,			// A union联合类型。
  C_MEMBER,			// Member of a struct or union结构体或联合体的成员。
  C_ENUMTYPE,			// A named enumeration type枚举类型的名称。
  C_ENUMVAL,			// A named enumeration value枚举值的名称。
  C_TYPEDEF			// A named typedef类型定义的名称。
};

// Symbol table structure
//变量表
struct symtable {
  char *name;			// Name of a symbol符号的名称，通常是一个字符串。
  int type;			// Primitive type for the symbol符号的基本类型，使用之前提到的基本类型的枚举值（如P_NONE, P_VOID, P_CHAR等）来表示。
  struct symtable *ctype;	// If struct/union, ptr to that type如果符号的类型是结构体或联合体（P_STRUCT或P_UNION），则指向相应的结构体或联合体类型。
  int stype;			// Structural type for the symbol 符号的结构类型，使用之前提到的结构类型的枚举值（如S_VARIABLE, S_FUNCTION, S_ARRAY等）来表示。
  int class;			// Storage class for the symbol符号的存储类别，可能表示符号是局部变量、全局变量等。
  int size;			// Total size in bytes of this symbol符号的总大小（以字节为单位）。
  int nelems;			// Functions: # params. Arrays: # elements 对于函数，表示参数的数量；对于数组，表示数组的元素数量。
#define st_endlabel st_posn	// For functions, the end label 对于函数，表示函数的结束标签；
#define st_hasaddr  st_posn	// For locals, 1 if any A_ADDR operation对于局部变量，表示是否有A_ADDR操作。
  int st_posn;			// For struct members, the offset of 对于结构体成员，表示该成员相对于结构体基址的偏移量。
    				// the member from the base of the struct
  int *initlist;		// List of initial values 对于初始化的符号，存储初始值的列表。
  struct symtable *next;	// Next symbol in one list符号表中下一个符号的指针，用于形成符号表的链表。
  struct symtable *member;	// First member of a function, struct,对于函数、结构体、联合体或枚举，指向第一个成员的指针。
};				// union or enum

// Abstract Syntax Tree structure
//操作子树
struct ASTnode {
  int op;			// "Operation" to be performed on this tree表示树节点执行的操作或运算符。这可能是二元运算符（如加法、减法）、一元运算符（如取地址、取反）或其他操作。
  int type;			// Type of any expression this tree generates表示树节点生成的任何表达式的类型。使用之前提到的基本类型的枚举值（如P_NONE, P_VOID, P_CHAR等）来表示。
  struct symtable *ctype;	// If struct/union, ptr to that type如果树节点表示结构体或联合体（P_STRUCT或P_UNION），则指向相应的结构体或联合体类型。
  int rvalue;			// True if the node is an rvalue一个标志，表示节点是否为右值（rvalue）。右值是表达式的计算结果，通常是一个可以赋值给左值的值。如果为true，表示是右值。
  /*分别表示树节点的左子树、中间子树和右子树。这些子树对应于表达式的不同部分。*/
  struct ASTnode *left;		// Left, middle and right child trees
  struct ASTnode *mid;
  struct ASTnode *right;
  struct symtable *sym;		// For many AST nodes, the pointer to对于许多AST节点，这是指向符号表中相应符号的指针。例如，对于变量引用或函数调用，可以通过这个指针找到符号表中的相关信息。
  				// the symbol in the symbol table
#define a_intvalue a_size	// For A_INTLIT, the integer value 对于整数字面量节点(A_INTLIT)，这个成员的值表示整数值。
  int a_size;			// For A_SCALE, the size to scale by对于某些节点，比如A_SCALE，表示一个缩放因子。
  int linenum;			// Line number from where this node comes表示该节点对应的源代码行号。
};

/*在生成汇编代码时，可以检查是否有可用的寄存器或标签，并在必要时使用NOREG或NOLABEL来表示相应的情况。*/
enum {
  /*这个常量用于表示在AST（抽象语法树）生成过程中没有可用的临时寄存器。
  通常，在生成AST时，可能需要使用寄存器来存储临时值或执行某些操作。当没有可用的寄存器时，可以使用NOREG来表示这种情况。*/
  NOREG = -1,			// Use NOREG when the AST generation
  				// functions have no temporary to return
  /*这个常量用于表示在某些情况下没有可用的标签。
  标签通常在汇编语言中用于标识代码中的位置，例如跳转目标或循环起始点。当在某个上下文中没有标签可用时，可以使用NOLABEL来表示这种情况。*/
  NOLABEL = 0			// Use NOLABEL when we have no label to
    				// pass to genAST()
};
