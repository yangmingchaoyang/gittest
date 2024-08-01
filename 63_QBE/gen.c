#include "defs.h"
#include "data.h"
#include "decl.h"

// Generic code generator
// Copyright (c) 2019 Warren Toomey, GPL3

// Generate and return a new label number
/*这是一个静态变量，它在函数调用之间保留其值。它被初始化为1，表示第一个标签的编号为1。*/
static int labelid = 1;
/*返回一个标签*/
int genlabel(void) {
  return (labelid++);
}

/*段代码定义了一个函数 update_line，其作用是在生成汇编代码时，
检查 AST 节点的行号是否发生变化，如果发生变化，则输出新的行号信息到汇编代码中。*/
static void update_line(struct ASTnode *n) {
  // Output the line into the assembly if we've
  // changed the line number in the AST node
  if (n->linenum != 0 && Line != n->linenum) {
    Line = n->linenum;
    /*调用 cglinenum 函数，将新的行号信息输出到汇编代码中。*/
    cglinenum(Line);
  }
}

// Generate the code for an IF statement
// and an optional ELSE clause.
static int genIF(struct ASTnode *n, int looptoplabel, int loopendlabel) {
  int Lfalse, Lend = 0;
  int r, r2;

  // Generate two labels: one for the
  // false compound statement, and one
  // for the end of the overall IF statement.
  // When there is no ELSE clause, Lfalse _is_
  // the ending label!
  /*生成两个标签，一个用于表示假条件的复合语句（Lfalse），另一个用于整个IF语句的结束（Lend）。*/
  Lfalse = genlabel();
  if (n->right)
    Lend = genlabel();

  // Generate the condition code
  /*生成条件表达式的代码，并将结果存储在寄存器r中。如果条件为假，将跳转到Lfalse标签。*/
  r = genAST(n->left, Lfalse, NOLABEL, NOLABEL, n->op);
  // Test to see if the condition is true. If not, jump to the false label
  /*使用cgcompare_and_jump比较r和常数1，如果相等（条件为假），则跳转到Lfalse标签。*/
  r2 = cgloadint(1, P_INT);
  cgcompare_and_jump(A_EQ, r, r2, Lfalse, P_INT);

  // Generate the true compound statement
  /*生成真条件的复合语句的代码。*/
  genAST(n->mid, NOLABEL, looptoplabel, loopendlabel, n->op);

  // If there is an optional ELSE clause,
  // generate the jump to skip to the end
  /*如果存在可选的ELSE子句，生成一个新的标签，然后跳转到整个IF语句的结束Lend标签。*/
  if (n->right) {
    // QBE doesn't like two jump instructions in a row, and
    // a break at the end of a true IF section causes this. The
    // solution is to insert a label before the IF jump.
    cglabel(genlabel());
    cgjump(Lend);
  }
  // Now the false label
  cglabel(Lfalse);

  // Optional ELSE clause: generate the
  // false compound statement and the
  // end label
  /*生成假条件的复合语句和可选的ELSE子句的代码。如果存在ELSE子句，最终生成整个IF语句的结束标签Lend。*/
  if (n->right) {
    genAST(n->right, NOLABEL, NOLABEL, loopendlabel, n->op);
    cglabel(Lend);//结束标签一定要在最后
  }

  return (NOREG);
}

// Generate the code for a WHILE statement
//对于判断而言，先进行判断后生成结果1，0，装入寄存器中，之后再用结果进行跳转的判断。
static int genWHILE(struct ASTnode *n) {
  int Lstart, Lend;
  int r, r2;

  // Generate the start and end labels
  // and output the start label
  //生成唯一的标签，分别用于标记循环的开始和结束。
  Lstart = genlabel();
  //输出开始标签，这是循环的入口点。
  Lend = genlabel();
  cglabel(Lstart);

  // Generate the condition code
  //genAST() 函数生成 n->left（循环条件）的代码。这个函数递归地处理 AST 节点，并能处理不同的代码路径和操作。
  r = genAST(n->left, Lend, Lstart, Lend, n->op);
  // Test to see if the condition is true. If not, jump to the end label
  r2 = cgloadint(1, P_INT);
  // 比较 r（条件表达式的结果）和 r2。如果条件为假（即不等于 1），跳转到 Lend（结束循环）。
  cgcompare_and_jump(A_EQ, r, r2, Lend, P_INT);

  // Generate the compound statement for the body
  genAST(n->right, NOLABEL, Lstart, Lend, n->op);

  // Finally output the jump back to the condition,
  // and the end label
  // 在循环体执行完后跳回循环的开始，重新评估条件。
  cgjump(Lstart);
  //输出循环的结束标签，这是退出循环的出口点。
  cglabel(Lend);
  return (NOREG);
}

// Generate the code for a SWITCH statement
/*这段代码用于生成SWITCH语句的汇编代码。由于QBE尚不支持跳转表（jump tables），因此代码采用逐一比较的方式实现SWITCH语句的效果。*/
static int genSWITCH(struct ASTnode *n) {
  int *caselabel;
  int Lend;
  int Lcode = 0;
  int i, reg, r2, type;
  struct ASTnode *c;

  // Create an array for the case labels
  /*创建case标签数组*/
  caselabel = (int *) malloc((n->a_intvalue + 1) * sizeof(int));
  if (caselabel == NULL)
    fatal("malloc failed in genSWITCH");

  // Because QBE doesn't yet support jump tables,
  // we simply evaluate the switch condition and
  // then do successive comparisons and jumps,
  // just like we were doing successive if/elses

  // Generate a label for the end of the switch statement.
  Lend = genlabel();

  // Generate labels for each case. Put the end label in
  // as the entry after all the cases
  /*生成case标签和条件判断代码：*/
  for (i = 0, c = n->right; c != NULL; i++, c = c->right)
    caselabel[i] = genlabel();
  caselabel[i] = Lend;

  // Output the code to calculate the switch condition
  /*生成case比较和跳转代码：*/
  reg = genAST(n->left, NOLABEL, NOLABEL, NOLABEL, 0);
  type = n->left->type;

  // Walk the right-child linked list to
  // generate the code for each case
  /*
  对于每个case，生成一个标签用于实际代码。如果Lcode为0，表示尚未生成用于实际代码的标签，此时生成一个新标签。
  输出当前case的测试标签。
  对于非default的case，生成比较和跳转代码。如果值匹配，跳转到下一个case，否则跳转到下一个case的测试标签。
  对于有代码的case，生成实际代码。
  */
 //i 是一个计数器，用来追踪当前是第几个 case。
  for (i = 0, c = n->right; c != NULL; i++, c = c->right) {

    // Generate a label for the actual code that the cases will fall down to
    //Lcode 是用来标识执行 case 语句代码块的位置的标签。如果 Lcode 尚未初始化，则生成一个新的标签。
    if (Lcode == 0)
      Lcode = genlabel();

    // Output the label for this case's test
    //为当前的 case 测试输出一个标签，这些标签是提前生成的，每个 case 对应一个。
    cglabel(caselabel[i]);

    // Do the comparison and jump, but not if it's the default case
    /*
    如果当前的 case 不是默认 case（A_DEFAULT），则进行一系列操作。
    cgloadint(c->a_intvalue, type) 加载当前 case 的整型值到一个临时变量 r2。
    cgcompare_and_jump(A_EQ, reg, r2, caselabel[i + 1], type) 比较寄存器 reg（这里假设之前已加载了 switch 表达式的值）与 r2。如果它们不相等，跳转到下一个 case 的标签。
    cgjump(Lcode) 如果值匹配，跳转到执行当前 case 代码的标签 Lcode。
    */
    if (c->op != A_DEFAULT) {
      // Jump to the next case if the value doesn't match the case value
      r2 = cgloadint(c->a_intvalue, type);
      cgcompare_and_jump(A_EQ, reg, r2, caselabel[i + 1], type);

      // Otherwise, jump to the code to handle this case
      cgjump(Lcode);
    }
    // Generate the case code. Pass in the end label for the breaks.
    // If case has no body, we will fall into the following body.
    // Reset Lcode so we will create a new code label on the next loop.
    /*
    如果当前 case 有代码块（c->left 不为空），则：
    输出执行代码的标签 Lcode。
    调用 genAST 生成 case 代码块的代码，传入结束标签 Lend 以处理可能的 break 语句。
    重置 Lcode 以便在下一次循环中重新生成一个新的代码执行标签。
    */
    if (c->left) {
      cglabel(Lcode);
      genAST(c->left, NOLABEL, NOLABEL, Lend, 0);
      Lcode = 0;
    }
  }

  // Now output the end label.
  cglabel(Lend);
  return (NOREG);
}

// Generate the code for an
// A_LOGAND or A_LOGOR operation
/*

这段代码是为逻辑与（A_LOGAND）或逻辑或（A_LOGOR）操作生成代码。在逻辑运算中，如果左侧表达式的结果可以确定整个逻辑表达式的值，
那么就无需计算右侧表达式。这样可以通过生成相应的跳转指令来优化逻辑运算。
*/
/*这段代码是为处理逻辑与（&&）和逻辑或（||）操作的抽象语法树（AST）节点生成目标代码的函数。
函数 gen_logandor 对两个逻辑表达式进行计算，并根据它们的真值执行短路运算，这意味着在某些情况下不需要计算第二个表达式的值。*/
static int gen_logandor(struct ASTnode *n) {
  // Generate two labels
  /*首先，生成两个标签 Lfalse 和 Lend，其中 Lfalse 用于标记逻辑表达式为假时的跳转位置，而 Lend 则是整个逻辑表达式结束的标签。*/
  int Lfalse = genlabel();
  int Lend = genlabel();
  int reg;
  int type;

  // Generate the code for the left expression
  // followed by the jump to the false label
  /*生成左侧表达式的代码*/
  reg = genAST(n->left, NOLABEL, NOLABEL, NOLABEL, 0);
  type = n->left->type;
  cgboolean(reg, n->op, Lfalse, type);

  // Generate the code for the right expression
  // followed by the jump to the false label
  /*生成右侧表达式的代码*/
  reg = genAST(n->right, NOLABEL, NOLABEL, NOLABEL, 0);
  type = n->right->type;
  cgboolean(reg, n->op, Lfalse, type);

  // We didn't jump so set the right boolean value
  /*如果没有进行跳转，即左侧表达式的值确定整个逻辑表达式的值，
  那么根据是逻辑与还是逻辑或，生成相应的代码：如果是逻辑与，将右侧表达式的值设为真，否则设为假。*/
  if (n->op == A_LOGAND) {
    cgloadboolean(reg, 1, type);
    cgjump(Lend);
    cglabel(Lfalse);
    cgloadboolean(reg, 0, type);
  } else {
    cgloadboolean(reg, 0, type);
    cgjump(Lend);
    cglabel(Lfalse);
    cgloadboolean(reg, 1, type);
  }
  cglabel(Lend);
  return (reg);
}

// Generate the code to calculate the arguments of a
// function call, then call the function with these
// arguments. Return the temoprary that holds
// the function's return value.
/*这段代码用于生成函数调用的汇编代码。*/
static int gen_funccall(struct ASTnode *n) {
  struct ASTnode *gluetree;
  int i = 0, numargs = 0;
  int *arglist = NULL;
  int *typelist = NULL;

  // Determine the actual number of arguments
  /*计算实际参数数量
  通过遍历参数列表，计算函数调用的实际参数数量。*/
  for (gluetree = n->left; gluetree != NULL; gluetree = gluetree->left) {
    numargs++;
  }

  // Allocate memory to hold the list of argument temporaries.
  // We need to walk the list of arguments to determine the size
  /*为参数列表分配内存*/
  for (i = 0, gluetree = n->left; gluetree != NULL; gluetree = gluetree->left)
    i++;

  if (i != 0) {
    arglist = (int *) malloc(i * sizeof(int));
    if (arglist == NULL)
      fatal("malloc failed in gen_funccall");
    typelist = (int *) malloc(i * sizeof(int));
    if (typelist == NULL)
      fatal("malloc failed in gen_funccall");
  }
  // If there is a list of arguments, walk this list
  // from the last argument (right-hand child) to the first.
  // Also cache the type of each expression
  /*遍历参数列表，计算每个参数的值，并将值和类型保存到相应的数组中。*/
  for (i = 0, gluetree = n->left; gluetree != NULL; gluetree = gluetree->left) {
    // Calculate the expression's value
    arglist[i] =
      genAST(gluetree->right, NOLABEL, NOLABEL, NOLABEL, gluetree->op);
    typelist[i++] = gluetree->right->type;
  }

  // Call the function and return its result
  return (cgcall(n->sym, numargs, arglist, typelist));
}

// Generate code for a ternary expression
static int gen_ternary(struct ASTnode *n) {
  int Lfalse, Lend;
  int reg, expreg;
  int r, r2;

  // Generate two labels: one for the
  // false expression, and one for the
  // end of the overall expression
  Lfalse = genlabel();
  Lend = genlabel();

  // Generate the condition code
  r = genAST(n->left, Lfalse, NOLABEL, NOLABEL, n->op);
  // Test to see if the condition is true. If not, jump to the false label
  r2 = cgloadint(1, P_INT);
  cgcompare_and_jump(A_EQ, r, r2, Lfalse, P_INT);

  // Get a temporary to hold the result of the two expressions
  reg = cgalloctemp();

  // Generate the true expression and the false label.
  // Move the expression result into the known temporary.
  expreg = genAST(n->mid, NOLABEL, NOLABEL, NOLABEL, n->op);
  /*将一个临时寄存器的值移动到另一个临时寄存器。*/
  cgmove(expreg, reg, n->mid->type);
  cgjump(Lend);
  cglabel(Lfalse);

  // Generate the false expression and the end label.
  // Move the expression result into the known temporary.
  expreg = genAST(n->right, NOLABEL, NOLABEL, NOLABEL, n->op);
  cgmove(expreg, reg, n->right->type);
  cglabel(Lend);
  return (reg);
}

// Given an AST, an optional label, and the AST op
// of the parent, generate assembly code recursively.
// Return the temporary id with the tree's final value.
/*
n：AST节点，表示当前要处理的语法树节点。

iflabel：整数，表示在处理条件语句（如A_IF）时使用的标签。对于非条件语句，可以用NOLABEL表示不使用标签。

looptoplabel：整数，表示在处理循环语句时使用的循环起始标签。对于非循环语句，可以用NOLABEL表示不使用标签。

loopendlabel：整数，表示在处理循环语句时使用的循环结束标签。对于非循环语句，可以用NOLABEL表示不使用标签。

parentASTop：整数，表示当前AST节点的父节点的操作符。这是为了在处理某些特殊情况时能够执行适当的优化或处理。
函数的目的是根据AST节点的类型和内容生成相应的汇编代码。
在递归调用过程中，该函数会遍历整个AST树，并根据每个节点的操作符（op）和相关信息生成相应的汇编代码。
函数的返回值是一个整数，表示生成的汇编代码的结果保存在哪个寄存器中，或者如果不需要结果则返回NOREG。

在函数内部，通过递归调用处理AST的左右子树，同时根据不同的操作符执行不同的代码生成逻辑。
函数还处理了一些特殊的AST节点类型，如条件语句、循环语句、函数调用等，以确保生成的汇编代码符合相应的语义规则。*/
int genAST(struct ASTnode *n, int iflabel, int looptoplabel,
	   int loopendlabel, int parentASTop) {
  int leftreg = NOREG, rightreg = NOREG;/*// 临时寄存器用于保存左右子树的结果*/
  int lefttype = P_VOID, type = P_VOID; // 左右子树的类型
  struct symtable *leftsym = NULL;// 左子树的符号表

  // Empty tree, do nothing
  if (n == NULL)
    return (NOREG);

  // Update the line number in the output
   // 更新输出中的行号
  update_line(n);

  // We have some specific AST node handling at the top
  // so that we don't evaluate the child sub-trees immediately
  // 处理一些特殊的AST节点类型，避免立即评估子树
  switch (n->op) {
    case A_IF:
      return (genIF(n, looptoplabel, loopendlabel));
    case A_WHILE:
      return (genWHILE(n));
    case A_SWITCH:
      return (genSWITCH(n));
    case A_FUNCCALL:
      return (gen_funccall(n));//需要知道传的参数是什么，并使用call进行函数调用
    case A_TERNARY://用于处理三元运算符（ternary operator）
      return (gen_ternary(n));
    case A_LOGOR://表示逻辑或（||）和逻辑与（&&）运算符的常量或枚举值。
      return (gen_logandor(n));
    case A_LOGAND:
      return (gen_logandor(n));
    case A_GLUE:
      // Do each child statement, and free the
      // temporaries after each child
      /*这段代码处理AST节点类型为A_GLUE的情况。
      A_GLUE节点用于表示两个语句或表达式的序列，即将两个子节点连接起来的节点。
      在代码生成过程中，它的左右子节点分别表示两个语句或表达式。*/
      //用于处理两个相邻的语句或表达式。A_GLUE 节点在 AST 中表示两个语句或表达式的顺序执行。
      if (n->left != NULL)
	genAST(n->left, iflabel, looptoplabel, loopendlabel, n->op);
      if (n->right != NULL)
	genAST(n->right, iflabel, looptoplabel, loopendlabel, n->op);
      return (NOREG);
    case A_FUNCTION:
      // Generate the function's preamble before the code
      // in the child sub-tree
      /*该部分专门处理函数定义节点（A_FUNCTION），负责生成函数的前导代码（preamble）、函数体的代码以及函数的尾部代码（postamble）。*/
      cgfuncpreamble(n->sym);//在传参时，由于参数过小，可以通过一次性传多个参数进行传参的优化。
      genAST(n->left, NOLABEL, NOLABEL, NOLABEL, n->op);
      cgfuncpostamble(n->sym);
      return (NOREG);
  }

  // General AST node handling below

  // Get the left and right sub-tree values. Also get the type
  /*这段代码用于获取AST节点 n 的左右子树的值和类型，并存储在相应的变量中。*/
  if (n->left) {
    lefttype = type = n->left->type;
    leftsym = n->left->sym;
    leftreg = genAST(n->left, NOLABEL, NOLABEL, NOLABEL, n->op);
  }
  if (n->right) {
    type = n->right->type;
    rightreg = genAST(n->right, NOLABEL, NOLABEL, NOLABEL, n->op);
  }

  switch (n->op) {
    case A_ADD:
      return (cgadd(leftreg, rightreg, type));
    case A_SUBTRACT:
      return (cgsub(leftreg, rightreg, type));
    case A_MULTIPLY:
      return (cgmul(leftreg, rightreg, type));
    case A_DIVIDE:
      return (cgdivmod(leftreg, rightreg, A_DIVIDE, type));
    case A_MOD:
      return (cgdivmod(leftreg, rightreg, A_MOD, type));
    case A_AND:
      return (cgand(leftreg, rightreg, type));
    case A_OR:
      return (cgor(leftreg, rightreg, type));
    case A_XOR:
      return (cgxor(leftreg, rightreg, type));
    case A_LSHIFT:
      return (cgshl(leftreg, rightreg, type));
    case A_RSHIFT:
      return (cgshr(leftreg, rightreg, type));
    case A_EQ:
    case A_NE:
    case A_LT:
    case A_GT:
    case A_LE:
    case A_GE:
      return (cgcompare_and_set(n->op, leftreg, rightreg, lefttype));
    case A_INTLIT:
    //cgloadint(n->a_intvalue, n->type)：加载整数字面值到寄存器。
      return (cgloadint(n->a_intvalue, n->type));
    case A_STRLIT:
    //cgloadglobstr(n->a_intvalue)：加载字符串字面值的全局标识符或地址。
      return (cgloadglobstr(n->a_intvalue));
    case A_IDENT:
      // Load our value if we are an rvalue
      // or we are being dereferenced
      /*n->rvalue 表示当前标识符节点是否是右值。如果是右值，说明需要获取变量的值，因此条件成立。
      parentASTop == A_DEREF 表示当前标识符节点是否在被解引用（dereference）的上下文中。
      如果是，说明需要获取指针所指的值，同样条件成立。*/
      if (n->rvalue || parentASTop == A_DEREF) {
	return (cgloadvar(n->sym, n->op));
      } else
	return (NOREG);
    case A_ASPLUS:
    case A_ASMINUS:
    case A_ASSTAR:
    case A_ASSLASH:
    case A_ASMOD:
    case A_ASSIGN:

      // For the '+=' and friends operators, generate suitable code
      // and get the temporary with the result. Then take the left child,
      // make it the right child so that we can fall into the assignment code.
      switch (n->op) {
	case A_ASPLUS:
	  leftreg = cgadd(leftreg, rightreg, type);
	  n->right = n->left;/*将左子树 (n->left) 赋给右子树 (n->right)。这是为了将结果传递给下一步的赋值操作。*/
	  break;
	case A_ASMINUS:
	  leftreg = cgsub(leftreg, rightreg, type);
	  n->right = n->left;
	  break;
	case A_ASSTAR:
	  leftreg = cgmul(leftreg, rightreg, type);
	  n->right = n->left;
	  break;
	case A_ASSLASH:
	  leftreg = cgdivmod(leftreg, rightreg, A_DIVIDE, type);
	  n->right = n->left;
	  break;
	case A_ASMOD:
	  leftreg = cgdivmod(leftreg, rightreg, A_MOD, type);
	  n->right = n->left;
	  break;
      }

      // Now into the assignment code
      // Are we assigning to an identifier or through a pointer?
      /*如果要赋值的右子树是一个标识符，代码检查这个标识符是全局的、外部的还是静态的，
      然后调用不同的函数进行处理。cgstorglob 处理全局变量，cgstorlocal 处理局部变量。*/
      switch (n->right->op) {
	case A_IDENT:
  /*全局变量、外部变量或静态变量:
如果 n->right->sym->class 是 C_GLOBAL、C_EXTERN 或 C_STATIC，这表明变量是全局的、外部链接的或静态存储的。
对这些类型的变量进行赋值时，需要调用 cgstorglob 函数，该函数负责生成将值从寄存器（leftreg）存储到全局或静态内存位置的代码。
这通常涉及到生成如汇编语言中的全局标签引用和存储指令。*/
	  if (n->right->sym->class == C_GLOBAL ||
	      n->right->sym->class == C_EXTERN ||
	      n->right->sym->class == C_STATIC)
	    return (cgstorglob(leftreg, n->right->sym));
	  else
	    return (cgstorlocal(leftreg, n->right->sym));
	case A_DEREF:
  /*如果变量不是全局、外部或静态的，那么它被视为局部变量。
对局部变量的赋值通过 cgstorlocal 函数处理，这个函数生成将寄存器内容存储到栈上相应局部变量位置的代码。*/
	  return (cgstorderef(leftreg, rightreg, n->right->type));
	default:
    /*如果右子树不是标识符也不是解引用操作，就会调用 fatald 函数，报告错误并终止程序运行，因为不应该出现不能处理的情况。*/
	  fatald("Can't A_ASSIGN in genAST(), op", n->op);
      }
    case A_WIDEN:
      // Widen the child's type to the parent's type
      return (cgwiden(leftreg, lefttype, n->type));
    case A_RETURN:
      cgreturn(leftreg, Functionid);
      return (NOREG);
    case A_ADDR:
      // If we have a symbol, get its address. Otherwise,
      // the left temporary already has the address because
      // it's a member access
      /*如果 sym 不为空，说明这是对一个符号取地址的操作。
      于是，它调用 cgaddress 函数来生成取地址的目标代码。这个函数会返回一个临时寄存器，其中存放着符号的地址。*/
      if (n->sym != NULL)
	return (cgaddress(n->sym));
      else
	return (leftreg);
    case A_DEREF:
      // If we are an rvalue, dereference to get the value we point at,
      // otherwise leave it for A_ASSIGN to store through the pointer
      if (n->rvalue)
	return (cgderef(leftreg, lefttype));
      else
	return (leftreg);
  //位移地址的位置
    case A_SCALE:
      // Small optimisation: use shift if the
      // scale value is a known power of two
      switch (n->a_size) {
	case 2:
	  return (cgshlconst(leftreg, 1, type));
	case 4:
	  return (cgshlconst(leftreg, 2, type));
	case 8:
	  return (cgshlconst(leftreg, 3, type));
	default:
	  // Load a temporary with the size and
	  // multiply the leftreg by this size
	  rightreg = cgloadint(n->a_size, P_INT);
	  return (cgmul(leftreg, rightreg, type));
      }
    case A_POSTINC:
    case A_POSTDEC:
      // Load and decrement the variable's value into a temporary
      // and post increment/decrement it
      return (cgloadvar(n->sym, n->op));
    case A_PREINC:
    case A_PREDEC:
      // Load and decrement the variable's value into a temporary
      // and pre increment/decrement it
      return (cgloadvar(leftsym, n->op));
    case A_NEGATE:
      return (cgnegate(leftreg, type));
    case A_INVERT:
      return (cginvert(leftreg, type));
    case A_LOGNOT:
      return (cglognot(leftreg, type));
    case A_TOBOOL:
      // If the parent AST node is an A_IF or A_WHILE, generate
      // a compare followed by a jump. Otherwise, set the temporary
      // to 0 or 1 based on it's zeroeness or non-zeroeness
      return (cgboolean(leftreg, parentASTop, iflabel, type));
    case A_BREAK:
      cgjump(loopendlabel);
      return (NOREG);
    case A_CONTINUE:
      cgjump(looptoplabel);
      return (NOREG);
    case A_CAST:
      return (cgcast(leftreg, lefttype, n->type));
    default:
      fatald("Unknown AST operator", n->op);
  }
  return (NOREG);		// Keep -Wall happy
}

/*汇编文件的前导部分*/
void genpreamble(char *filename) {
  cgpreamble(filename);
}
/*汇编文件的后导部分*/
void genpostamble() {
  cgpostamble();
}

//分配空间
void genglobsym(struct symtable *node) {
  cgglobsym(node);
}

// Generate a global string.
// If append is true, append to
// previous genglobstr() call.
/*
函数首先调用 genlabel 生成一个唯一的标签（label），然后调用 cgglobstr 函数生成相应的全局字符串汇编代码。
cgglobstr 函数通常由编译器的后端（code generation）部分实现，
负责生成目标平台的汇编代码，以便在程序运行时将字符串存储在全局数据区。
*/
int genglobstr(char *strvalue, int append) {
  int l = genlabel();
  cgglobstr(l, strvalue, append);
  return (l);
}
void genglobstrend(void) {
  cgglobstrend();
}
/*其作用是根据给定的基本类型 type，调用一个可能是 cgprimsize 的辅助函数来返回该基本类型在字节中的大小*/
int genprimsize(int type) {
  return (cgprimsize(type));
}
int genalign(int type, int offset, int direction) {
  return (cgalign(type, offset, direction));
}
