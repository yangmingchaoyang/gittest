#ifndef extern_
#define extern_ extern
#endif

// Global variables
// Copyright (c) 2019 Warren Toomey, GPL3

extern_ int Line;		     	// Current line number// 当前行号。
extern_ int Linestart;		     	// True if at start of a line// 是否在行的起始位置。
extern_ int Putback;		     	// Character put back by scanner//被扫描器放回的字符。
extern_ struct symtable *Functionid; 	// Symbol ptr of the current function//当前函数的符号表条目。
extern_ FILE *Infile;		     	// Input and output files// 输入文件指针。
extern_ FILE *Outfile;//输出文件指针。
extern_ char *Infilename;		// Name of file we are parsing//当前输入文件的文件名。
extern_ char *Outfilename;		// Name of file we opened as Outfile//当前输出文件的文件名。
extern_ struct token Token;		// Last token scanned//最后一个扫描到的标记。
extern_ struct token Peektoken;		// A look-ahead token//向前看一个标记。
extern_ char Text[TEXTLEN + 1];		// Last identifier scanned//最后一个扫描到的标识符文本。
extern_ int Looplevel;			// Depth of nested loops//嵌套循环的深度。
/*通过维护一个嵌套深度计数器，编译器可以正确地解析和处理嵌套的 switch 语句。当进入一个新的 switch 语句时，深度加一；当离开 switch 语句时，深度减一。*/
extern_ int Switchlevel;		// Depth of nested switches//嵌套开关语句的深度。
extern char *Tstring[];			// List of token strings//一个包含标记字符串的列表。

// Symbol table lists
extern_ struct symtable *Globhead, *Globtail;	  // Global variables and functions全局变量和函数的符号表链表的头和尾。
extern_ struct symtable *Loclhead, *Locltail;	  // Local variables局部变量的符号表链表的头和尾。
extern_ struct symtable *Parmhead, *Parmtail;	  // Local parameters 函数参数的符号表链表的头和尾。
extern_ struct symtable *Membhead, *Membtail;	  // Temp list of struct/union members临时的结构体/联合体成员的符号表链表的头和尾。
extern_ struct symtable *Structhead, *Structtail; // List of struct types结构体类型的符号表链表的头和尾。
extern_ struct symtable *Unionhead, *Uniontail;   // List of union types联合体类型的符号表链表的头和尾。
extern_ struct symtable *Enumhead,  *Enumtail;    // List of enum types and values枚举类型和值的符号表链表的头和尾。
extern_ struct symtable *Typehead,  *Typetail;    // List of typedefs  typedef类型的符号表链表的头和尾。

// Command-line flags
extern_ int O_dumpAST;		// If true, dump the AST trees如果为真，编译器将在编译过程中输出抽象语法树（AST）的信息。抽象语法树是源代码语法结构的一种树状表示，用于中间代码生成。
extern_ int O_dumpsym;		// If true, dump the symbol table如果为真，编译器将在编译过程中输出符号表的信息。符号表包含了源代码中定义的各种符号（如变量、函数等）的信息。
extern_ int O_keepasm;		// If true, keep any assembly files如果为真，编译器将保留生成的汇编文件，而不删除它们。通常在调试或检查生成的汇编代码时使用。
extern_ int O_assemble;		// If true, assemble the assembly files如果为真，编译器将在生成汇编文件后进行汇编，将其转换为目标文件。这个标志通常用于控制是否执行汇编过程。
extern_ int O_dolink;		// If true, link the object files如果为真，编译器将链接生成的目标文件，创建可执行文件。这个标志通常用于控制是否执行链接过程。
extern_ int O_verbose;		// If true, print info on compilation stages如果为真，编译器将输出详细的编译信息，例如正在编译的文件名等。这个标志通常用于控制是否输出更多的编译信息。
