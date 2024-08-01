#include "defs.h"
#include "data.h"
#include "decl.h"

// AST tree functions
// Copyright (c) 2019 Warren Toomey, GPL3

// Build and return a generic AST node
/*
op：表示该节点的操作符（operator）。操作符的值取自一组预定义的常量，用于标识节点表示的具体操作，如加法、减法、赋值等。

type：表示节点的数据类型。这个值指定了节点所表示的数据的类型，例如整数、字符、指针等。类型的值取自一组预定义的常量。

ctype：表示节点的类别（category）。这个值指定了节点表示的数据的具体类别，例如变量、数组、函数等。类别的值取自一组预定义的常量。

left、mid、right：分别表示节点的左子节点、中间子节点和右子节点。这些子节点构成了整个抽象语法树的结构，用于表示复杂的表达式和语句。

sym：表示节点相关的符号表条目（symbol table entry）。这个值通常用于表示节点关联的标识符，如变量、函数等。可以是 NULL，表示该节点没有关联的符号表条目。

intvalue：表示节点的整数值。这个值在一些节点类型中存储具体的整数常量值。

linenum：表示节点所在的源代码行号。
*/
struct ASTnode *mkastnode(int op, int type,
			  struct symtable *ctype,
			  struct ASTnode *left,
			  struct ASTnode *mid,
			  struct ASTnode *right,
			  struct symtable *sym, int intvalue) {
  struct ASTnode *n;

  // Malloc a new ASTnode
  n = (struct ASTnode *) malloc(sizeof(struct ASTnode));
  if (n == NULL)
    fatal("Unable to malloc in mkastnode()");

  // Copy in the field values and return it
  n->op = op;
  n->type = type;
  n->ctype = ctype;
  n->left = left;
  n->mid = mid;
  n->right = right;
  n->sym = sym;
  n->a_intvalue = intvalue;
  n->linenum = 0;
  return (n);
}


// Make an AST leaf node
/*
这段代码定义了一个辅助函数 mkastleaf，用于创建 AST（抽象语法树）中的叶节点。
op: 操作码（Operation Code），表示 AST 节点的类型或操作。
type: AST 节点的数据类型。
ctype: 符号表中的类型信息，通常与 type 相关。
sym: 符号表中的符号信息，表示与该节点关联的符号。
intvalue: 整数值，用于存储节点的值，例如常量节点的值。
*/
struct ASTnode *mkastleaf(int op, int type,
			  struct symtable *ctype,
			  struct symtable *sym, int intvalue) {
  return (mkastnode(op, type, ctype, NULL, NULL, NULL, sym, intvalue));
}

// Make a unary AST node: only one child
/*这段代码实现了一个用于创建一元AST节点的函数mkastunary
int op: 表示一元操作的类型或标志。
int type: 表示AST节点的类型。
struct symtable *ctype: 表示节点的符号表项的类型信息。
struct ASTnode *left: 表示节点的左子树。
struct symtable *sym: 表示与节点相关联的符号表项。
int intvalue: 表示节点的整数值（如果适用）。*/
struct ASTnode *mkastunary(int op, int type,
			   struct symtable *ctype,
			   struct ASTnode *left,
			   struct symtable *sym, int intvalue) {
  return (mkastnode(op, type, ctype, left, NULL, NULL, sym, intvalue));
}

// Generate and return a new label number
// just for AST dumping purposes
static int dumpid = 1;
static int gendumplabel(void) {
  return (dumpid++);
}

// List of AST node names
static char *astname[] = { NULL,
  "ASSIGN", "ASPLUS", "ASMINUS", "ASSTAR",
  "ASSLASH", "ASMOD", "TERNARY", "LOGOR",
  "LOGAND", "OR", "XOR", "AND", "EQ", "NE", "LT",
  "GT", "LE", "GE", "LSHIFT", "RSHIFT",
  "ADD", "SUBTRACT", "MULTIPLY", "DIVIDE", "MOD",
  "INTLIT", "STRLIT", "IDENT", "GLUE",
  "IF", "WHILE", "FUNCTION", "WIDEN", "RETURN",
  "FUNCCALL", "DEREF", "ADDR", "SCALE",
  "PREINC", "PREDEC", "POSTINC", "POSTDEC",
  "NEGATE", "INVERT", "LOGNOT", "TOBOOL", "BREAK",
  "CONTINUE", "SWITCH", "CASE", "DEFAULT", "CAST"
};

// Given an AST tree, print it out and follow the
// traversal of the tree that genAST() follows
/*这段代码是一个用于遍历并打印抽象语法树（AST）的函数 dumpAST。
该函数接受一个 AST 节点 n，以及一些标签和级别信息，然后根据节点的类型和属性递归地打印 AST 的结构。*/
/*它接受一个AST节点指针 n，一个标签 label，和一个表示缩进级别的整数 level。*/
void dumpAST(struct ASTnode *n, int label, int level) {
  int Lfalse, Lstart, Lend;
  int i;

  if (n == NULL)
    fatal("NULL AST node");
  /*if (n->op > A_CAST) 检查 AST 节点的操作码是否大于 A_CAST。A_CAST 是一个用于类型转换的 AST 操作码。如果节点的操作码大于 A_CAST，则调用 fatald 函数，
  该函数用于输出带有描述信息的致命错误。这个检查的目的是确保 dumpAST 函数中对于所有操作码的处理都是正确的，*/
  if (n->op > A_CAST)
    fatald("Unknown dumpAST operator", n->op);

  // Deal with IF and WHILE statements specifically
  switch (n->op) {
    case A_IF:
      Lfalse = gendumplabel();/*为假时需要跳转的标签*/
      for (i = 0; i < level; i++)
	fprintf(stdout, " ");/*这行代码的作用是将一个空格字符写入标准输出流（stdout）。*/
      fprintf(stdout, "IF");
      if (n->right) {
	Lend = gendumplabel();/*结束时需要跳转的标签*/
	fprintf(stdout, ", end L%d", Lend);
      }
      fprintf(stdout, "\n");
      dumpAST(n->left, Lfalse, level + 2);
      dumpAST(n->mid, NOLABEL, level + 2);
      if (n->right)
	dumpAST(n->right, NOLABEL, level + 2);
      return;
    case A_WHILE:
      Lstart = gendumplabel();/*开始的标签*/
      for (i = 0; i < level; i++)
	fprintf(stdout, " ");
      fprintf(stdout, "WHILE, start L%d\n", Lstart);
      Lend = gendumplabel();/*结束的标签*/
      dumpAST(n->left, Lend, level + 2);
      if (n->right)
	dumpAST(n->right, NOLABEL, level + 2);
      return;
  }

  // Reset level to -2 for A_GLUE nodes
  if (n->op == A_GLUE) {
    level -= 2;
  } else {

    // General AST node handling
    for (i = 0; i < level; i++)
      fprintf(stdout, " ");
    fprintf(stdout, "%s", astname[n->op]);
    switch (n->op) {
      case A_FUNCTION:
      case A_FUNCCALL:
      case A_ADDR:
      case A_PREINC:
      case A_PREDEC:
	if (n->sym != NULL)
	  fprintf(stdout, " %s", n->sym->name);
	break;
      case A_INTLIT:
	fprintf(stdout, " %d", n->a_intvalue);
	break;
      case A_STRLIT:
	fprintf(stdout, " rval label L%d", n->a_intvalue);
	break;
      case A_IDENT:
	if (n->rvalue)
	  fprintf(stdout, " rval %s", n->sym->name);
	else
	  fprintf(stdout, " %s", n->sym->name);
	break;
      case A_DEREF:
	if (n->rvalue)
	  fprintf(stdout, " rval");
	break;
      case A_SCALE:
	fprintf(stdout, " %d", n->a_size);
	break;
      case A_CASE:
	fprintf(stdout, " %d", n->a_intvalue);
	break;
      case A_CAST:
	fprintf(stdout, " %d", n->type);
	break;
    }
    fprintf(stdout, "\n");
  }
  // General AST node handling
  /*向下继续延申*/
  if (n->left)
    dumpAST(n->left, NOLABEL, level + 2);
  if (n->mid)
    dumpAST(n->mid, NOLABEL, level + 2);
  if (n->right)
    dumpAST(n->right, NOLABEL, level + 2);
}
