#include "defs.h"
#include "data.h"
#include "decl.h"

// Parsing of expressions
// Copyright (c) 2019 Warren Toomey, GPL3

// expression_list: <null>
//        | expression
//        | expression ',' expression_list
//        ;

// Parse a list of zero or more comma-separated expressions and
// return an AST composed of A_GLUE nodes with the left-hand child
// being the sub-tree of previous expressions (or NULL) and the right-hand
// child being the next expression. Each A_GLUE node will have size field
// set to the number of expressions in the tree at this point. If no
// expressions are parsed, NULL is returned
/*这段代码实现了一个函数 expression_list，用于解析零个或多个由逗号分隔的表达式，并返回一个由 A_GLUE 节点构成的语法树。*/
struct ASTnode *expression_list(int endtoken) {
  struct ASTnode *tree = NULL;
  struct ASTnode *child = NULL;
  int exprcount = 0;

  // Loop until the end token
  /*循环解析表达式*/
  while (Token.token != endtoken) {

    // Parse the next expression and increment the expression count
    /*调用 binexpr 函数解析下一个表达式，并递增 exprcount 记录表达式数量。*/
    child = binexpr(0);
    exprcount++;

    // Build an A_GLUE AST node with the previous tree as the left child
    // and the new expression as the right child. Store the expression count.
    /*使用 mkastnode 函数构建一个 A_GLUE 节点，该节点的左子树是之前表达式的语法树（可能为 NULL），右子树是新解析的表达式的语法树。节点的 size 字段存储表达式数量。*/
    tree =
      mkastnode(A_GLUE, P_NONE, NULL, tree, NULL, child, NULL, exprcount);

    // Stop when we reach the end token
    if (Token.token == endtoken)
      break;

    // Must have a ',' at this point
    match(T_COMMA, ",");
  }

  // Return the tree of expressions
  return (tree);
}

// Parse a function call and return its AST
static struct ASTnode *funccall(void) {
  struct ASTnode *tree;
  struct symtable *funcptr;

  // Check that the identifier has been defined as a function,
  // then make a leaf node for it.
  /*检查函数是否已定义*/
  if ((funcptr = findsymbol(Text)) == NULL || funcptr->stype != S_FUNCTION) {
    fatals("Undeclared function", Text);
  }
  // Get the '('
  lparen();

  // Parse the argument expression list
  /*解析参数表达式列表：*/
  tree = expression_list(T_RPAREN);

  // XXX Check type of each argument against the function's prototype

  // Build the function call AST node. Store the
  // function's return type as this node's type.
  // Also record the function's symbol-id
  tree =
    mkastunary(A_FUNCCALL, funcptr->type, funcptr->ctype, tree, funcptr, 0);

  // Get the ')'
  rparen();
  return (tree);
}

// Parse the index into an array and return an AST tree for it
/*这段代码用于解析数组的索引操作，返回对应的AST树。*/
/*这个数组不能初始化*/
static struct ASTnode *array_access(struct ASTnode *left) {
  struct ASTnode *right;

  // Check that the sub-tree is a pointer
  /*这部分代码检查传入的左子树是否是指针类型（或者数组类型）。如果不是，说明左子树无法进行索引操作，因此会发出致命错误。*/
  if (!ptrtype(left->type))
    fatal("Not an array or pointer");

  // Get the '['
  scan(&Token);

  // Parse the following expression
  /*这部分代码用于获取数组索引。*/
  right = binexpr(0);

  // Get the ']'
  match(T_RBRACKET, "]");

  // Ensure that this is of int type
  /*这部分代码检查数组索引的类型是否为整数类型。如果不是，说明数组索引必须是整数类型，否则发出致命错误。*/
  if (!inttype(right->type))
    fatal("Array index is not of integer type");

  // Make the left tree an rvalue
  left->rvalue = 1;

  // Scale the index by the size of the element's type
  //这段代码的作用是将数组的索引按照数组元素的大小进行缩放。
  right = modify_type(right, left->type, left->ctype, A_ADD);

  // Return an AST tree where the array's base has the offset added to it,
  // and dereference the element. Still an lvalue at this point.
  /*这部分代码构建了一个AST树，其中将数组的基地址与索引相加，然后对其进行解引用操作。
  最终得到的AST树表示数组索引的值，但在这一点仍然是左值。*/
  /*基地址加索引来解引用*/
  left =
    mkastnode(A_ADD, left->type, left->ctype, left, NULL, right, NULL, 0);
  left =
    mkastunary(A_DEREF, value_at(left->type), left->ctype, left, NULL, 0);
  return (left);
}

// Parse the member reference of a struct or union
// and return an AST tree for it. If withpointer is true,
// the access is through a pointer to the member.
static struct ASTnode *member_access(struct ASTnode *left, int withpointer) {
  struct ASTnode *right;
  struct symtable *typeptr;
  struct symtable *m;

  // Check that the left AST tree is a pointer to struct or union
  /*检查左子树类型*/
  /*如果 left->type 既不是指向结构体的指针 (pointer_to(P_STRUCT)) 也不是指向联合体的指针 (pointer_to(P_UNION))，则表示 left 不是有效的指向结构体/联合体的指针。*/
  if (withpointer && left->type != pointer_to(P_STRUCT)
      && left->type != pointer_to(P_UNION))
    fatal("Expression is not a pointer to a struct/union");

  // Or, check that the left AST tree is a struct or union.
  // If so, change it from an A_IDENT to an A_ADDR so that
  // we get the base address, not the value at this address.
  /*转换左子树为地址（A_ADDR）*/
  if (!withpointer) {
    if (left->type == P_STRUCT || left->type == P_UNION)
    /*这段代码通过将操作符转换为 A_ADDR，确保在处理直接访问结构体/联合体成员时，能够正确地获取其基地址，并为后续的成员偏移计算和代码生成提供正确的基础。*/
      left->op = A_ADDR;
    else
      fatal("Expression is not a struct/union");
  }
  // Get the details of the composite type
  /*获取结构体或联合体的类型信息：*/
  typeptr = left->ctype;

  // Skip the '.' or '->' token and get the member's name
  /*跳过.或->标记，然后获取成员的名字*/
  scan(&Token);
  ident();

  // Find the matching member's name in the type
  // Die if we can't find it
  /*这部分代码在结构体或联合体的成员列表中查找与给定名字匹配的成员。如果找不到匹配的成员，发出致命错误。*/
  for (m = typeptr->member; m != NULL; m = m->next)
    if (!strcmp(m->name, Text))
      break;
  if (m == NULL)
    fatals("No member found in struct/union: ", Text);

  // Make the left tree an rvalue
  /*标记左子树为右值*/
  left->rvalue = 1;

  // Build an A_INTLIT node with the offset
  /*构建表示成员偏移的AST树*/
  right = mkastleaf(A_INTLIT, P_LONG, NULL, NULL, m->st_posn);

  // Add the member's offset to the base of the struct/union
  // and dereference it. Still an lvalue at this point
  /*将成员偏移添加到结构体或联合体的基地址，然后解引用*/
  left =
    mkastnode(A_ADD, pointer_to(m->type), m->ctype, left, NULL, right, NULL,
	      0);
  left = mkastunary(A_DEREF, m->type, m->ctype, left, NULL, 0);
  return (left);
}

// Parse a parenthesised expression and
// return an AST node representing it.
/*这段代码实现了解析括号表达式（带有括号的表达式）的功能，并返回表示该表达式的AST节点。*/
static struct ASTnode *paren_expression(int ptp) {
  struct ASTnode *n;
  int type = 0;
  struct symtable *ctype = NULL;

  // Beginning of a parenthesised expression, skip the '('.
  scan(&Token);

  // If the token after is a type identifier, this is a cast expression
  switch (Token.token) {
    /*如果括号内的第一个标记是类型标识符，那么这是一个类型转换表达式。
    根据标记的类型，获取类型信息，并跳过右括号 )，然后解析括号内的表达式。*/
    case T_IDENT:
      // We have to see if the identifier matches a typedef.
      // If not, treat it as an expression.
      if (findtypedef(Text) == NULL) {
	n = binexpr(0);		// ptp is zero as expression inside ( )
	break;
      }
    case T_VOID:
    case T_CHAR:
    case T_INT:
    case T_LONG:
    case T_STRUCT:
    case T_UNION:
    case T_ENUM:
      // Get the type inside the parentheses
      /*括号内指定一个目标类型，并将一个表达式强制转换为该目标类型。*/
      type = parse_cast(&ctype);

      // Skip the closing ')' and then parse the following expression
      rparen();

    default:
      /*如果不是类型标识符，那么这是一个普通的表达式。调用 binexpr 函数解析括号内的表达式。*/
      n = binexpr(ptp);		// Scan in the expression. We pass in ptp
      // as the cast doesn't change the
      // expression's precedence
  }

  // We now have at least an expression in n, and possibly a non-zero type
  // in type if there was a cast. Skip the closing ')' if there was no cast.
  /*在这一步，检查是否有类型转换。如果没有类型转换，说明是普通的表达式，跳过右括号 )。
  如果有类型转换，创建一个用于类型转换的一元AST节点，并返回该节点。*/
  if (type == 0)
    rparen();
  else
    // Otherwise, make a unary AST node for the cast
    n = mkastunary(A_CAST, type, ctype, n, NULL, 0);
  return (n);
}

// Parse a primary factor and return an
// AST node representing it.
/*
这段代码实现了对主要因子（Primary Factor）的解析，生成对应的抽象语法树（AST）节点。
主要因子是构成表达式的基本元素，可以是变量、常量、函数调用、表达式等。
*/
static struct ASTnode *primary(int ptp) {
  struct ASTnode *n;
  struct symtable *enumptr;
  struct symtable *varptr;
  int id;
  int type = 0;
  int size, class;
  struct symtable *ctype;

  switch (Token.token) {
    case T_STATIC:
    case T_EXTERN:
      /*这两个标识符暂时不支持局部声明，因此调用 fatal 函数发出错误消息。*/
      fatal("Compiler doesn't support static or extern local declarations");
    case T_SIZEOF:
      // Skip the T_SIZEOF and ensure we have a left parenthesis
      /*处理 sizeof 运算符。跳过 sizeof，确保其后有左括号 (。然后获取括号内的类型，计算其大小，生成一个整数字面量的叶节点。*/
      scan(&Token);
      if (Token.token != T_LPAREN)
	fatal("Left parenthesis expected after sizeof");
      scan(&Token);

      // Get the type inside the parentheses
      type = parse_stars(parse_type(&ctype, &class));/*返回该函数类型，并判断是否是指针类型*/

      // Get the type's size
      size = typesize(type, ctype);
      rparen();

      // Make a leaf node int literal with the size
      return (mkastleaf(A_INTLIT, P_INT, NULL, NULL, size));/*生成一个整数字面量的叶节点。*/

    case T_INTLIT:
      // For an INTLIT token, make a leaf AST node for it.
      // Make it a P_CHAR if it's within the P_CHAR range
      /*对于整数字面量，生成一个整数字面量的叶节点，根据大小判断其类型是字符还是整数。*/
      if (Token.intvalue >= 0 && Token.intvalue < 256)
	n = mkastleaf(A_INTLIT, P_CHAR, NULL, NULL, Token.intvalue);
      else
	n = mkastleaf(A_INTLIT, P_INT, NULL, NULL, Token.intvalue);
      break;

    /*在编译过程中，字符串字面量通常被存储为全局数据，然后在汇编代码中生成适当的表示。
    这段代码的目的是处理字符串字面量，生成相应的汇编代码，并构建一个抽象语法树 (AST) 节点以代表这个字符串字面量。*/
    case T_STRLIT:
      /*对于字符串字面量，调用 genglobstr 生成字符串的汇编代码，
      同时处理后续的字符串字面量，生成全局字符串的结束标记，然后生成一个字符串字面量的叶节点。*/
      // For a STRLIT token, generate the assembly for it.
      /*生成对应的汇编代码。这可能包括在汇编中定义一个全局字符串，并返回字符串的标签*/
      id = genglobstr(Text, 0);/*返回字符串的标签（label）。*/

      // For successive STRLIT tokens, append their contents
      // to this one
      while (1) {/*查看下一个 Token 是否也是字符串字面量（T_STRLIT）。*/
	scan(&Peektoken);
	if (Peektoken.token != T_STRLIT)
	  break;
	genglobstr(Text, 1);/*如果下一个 Token 是字符串字面量，调用 genglobstr 函数，将其内容追加到先前字符串的末尾。然后使用 scan(&Token) 跳过当前字符串字面量。*/
	scan(&Token);		// To skip it properly
      }

      // Now make a leaf AST node for it. id is the string's label.
      genglobstrend();/*使用 genglobstrend 函数生成全局字符串的结束标记。*/
      /*此节点包含了字符串字面量的类型（指向字符的指针）和标签（字符串在汇编中的标识符）。*/
      n = mkastleaf(A_STRLIT, pointer_to(P_CHAR), NULL, NULL, id);
      break;

    case T_IDENT:
      // If the identifier matches an enum value,
      // return an A_INTLIT node
      /*如果标识符匹配一个枚举值（enum value），则创建一个整数字面量（A_INTLIT）的叶子节点，其值为该枚举值的位置。*/
      if ((enumptr = findenumval(Text)) != NULL) {
	n = mkastleaf(A_INTLIT, P_INT, NULL, NULL, enumptr->st_posn);
	break;
      }
      // See if this identifier exists as a symbol. For arrays, set rvalue to 1.
      /*如果标识符不是枚举值，检查该标识符是否存在于符号表中。如果不存在，通过 fatals 函数发出错误，表示未知的变量或函数。*/
      if ((varptr = findsymbol(Text)) == NULL)
	fatals("Unknown variable or function", Text);
      switch (varptr->stype) {
  /*对于标量变量（S_VARIABLE），创建一个标识符节点（A_IDENT）。*/
	case S_VARIABLE:
	  n = mkastleaf(A_IDENT, varptr->type, varptr->ctype, varptr, 0);
	  break;
	case S_ARRAY:
	  n = mkastleaf(A_ADDR, varptr->type, varptr->ctype, varptr, 0);
	  n->rvalue = 1;/*创建一个地址节点（A_ADDR），并将 rvalue 标记设置为1，表示这是一个右值。*/
	  break;
	case S_FUNCTION:
	  // Function call, see if the next token is a left parenthesis
	  scan(&Token);
	  if (Token.token != T_LPAREN)
	    fatals("Function name used without parentheses", Text);
	  return (funccall());/*调用 funccall 处理函数调用，该函数将返回一个表示函数调用的语法树节点。*/
	default:
	  fatals("Identifier not a scalar or array variable", Text);
      }

      break;

    case T_LPAREN:/*如果遇到左括号，说明可能是一个括号表达式，调用 paren_expression 处理括号内的表达式。*/
      return (paren_expression(ptp));

    default:
      fatals("Expecting a primary expression, got token", Token.tokstr);
  }

  // Scan in the next token and return the leaf node
  scan(&Token);
  return (n);
}

// Parse a postfix expression and return
// an AST node representing it. The
// identifier is already in Text.
/*
这段代码实现了对后缀表达式的解析，生成对应的抽象语法树（AST）节点。后缀表达式是指在变量名或表达式之后跟随的一系列操作符，
例如数组访问、结构体成员访问、后缀递增（post-increment）和后缀递减（post-decrement）。
*/
/*
基本表达式是构建更复杂表达式的基础： 后缀表达式是在基本表达式的基础上进行的操作。基本表达式可以包括标识符（变量名或函数名）、常量（整数、字符、字符串等）以及括号中的表达式。这些基本表达式是语法树的叶子节点。

后缀运算符依赖于基本表达式： 后缀运算符，如数组访问、结构体成员访问、后缀递增和后缀递减等，通常是在基本表达式之后应用的。例如，对数组的访问是对数组变量（基本表达式）的后续操作。

递增和递减运算符的语法： 递增（++）和递减（--）运算符可以作为后缀表达式的一部分，但它们在解析时需要考虑运算符的位置和性质。这包括检查是否重复递增或递减，以及在右值（rvalue）上是否允许递增或递减。
*/
static struct ASTnode *postfix(int ptp) {
  struct ASTnode *n;

  // Get the primary expression
  /*：调用 primary() 函数获取主表达式的 AST 节点，该节点可能表示变量、常量或其他基本表达式。*/
  n = primary(ptp);

  // Loop until there are no more postfix operators
  while (1) {
    switch (Token.token) {
      case T_LBRACKET:
	// An array reference
	n = array_access(n);
	break;

      case T_DOT:
	// Access into a struct or union
	n = member_access(n, 0);
	break;

      case T_ARROW:
	// Pointer access into a struct or union
	n = member_access(n, 1);
	break;

      case T_INC:
	// Post-increment: skip over the token
	if (n->rvalue == 1)
	  fatal("Cannot ++ on rvalue");
	scan(&Token);

	// Can't do it twice
	if (n->op == A_POSTINC || n->op == A_POSTDEC)
	  fatal("Cannot ++ and/or -- more than once");

	// and change the AST operation
	n->op = A_POSTINC;
	break;

      case T_DEC:
	// Post-decrement: skip over the token
	if (n->rvalue == 1)
	  fatal("Cannot -- on rvalue");
	scan(&Token);

	// Can't do it twice
	if (n->op == A_POSTINC || n->op == A_POSTDEC)
	  fatal("Cannot ++ and/or -- more than once");

	// and change the AST operation
	n->op = A_POSTDEC;
	break;

      default:
	return (n);
    }
  }

  return (NULL);		// Keep -Wall happy
}


// Convert a binary operator token into a binary AST operation.
// We rely on a 1:1 mapping from token to AST operation
/*确定操作是什么*/
static int binastop(int tokentype) {
  if (tokentype > T_EOF && tokentype <= T_MOD)
    return (tokentype);
  fatals("Syntax error, token", Tstring[tokentype]);
  return (0);			// Keep -Wall happy
}

// Return true if a token is right-associative,
// false otherwise.
/*用于判断给定的标记类型是否表示右结合的运算符。右结合的运算符是指在相同优先级的情况下，从右到左进行结合的运算符。*/
static int rightassoc(int tokentype) {
  if (tokentype >= T_ASSIGN && tokentype <= T_ASSLASH)
    return (1);
  return (0);
}

// Operator precedence for each token. Must
// match up with the order of tokens in defs.h
/*
赋值操作：T_ASSIGN 和相关操作（如 +=, -=, *=, /=, %=）的优先级为 10。这些操作一般是最低的，因为赋值通常在表达式的最后进行。
条件操作：三目运算符 T_QUESTION 的优先级为 15，介于赋值和逻辑操作之间。
逻辑操作：逻辑或 (||)、逻辑与 (&&) 的优先级相对较低，分别为 20 和 30。
按位操作：按位或、异或、与 (|, ^, &) 的优先级逐渐增高，分别为 40, 50, 60。
比较操作：比较操作 (==, !=, <, >, <=, >=) 的优先级较高，介于 70 到 80 之间。
移位操作：左移和右移 (<<, >>) 的优先级为 90。
加减乘除：加法和减法的优先级为 100，乘法、除法和取模的优先级更高，为 110。
*/
static int OpPrec[] = {
  0, 10, 10,			// T_EOF, T_ASSIGN, T_ASPLUS,
  10, 10,			// T_ASMINUS, T_ASSTAR,
  10, 10,			// T_ASSLASH, T_ASMOD,
  15,				// T_QUESTION,
  20, 30,			// T_LOGOR, T_LOGAND
  40, 50, 60,			// T_OR, T_XOR, T_AMPER 
  70, 70,			// T_EQ, T_NE
  80, 80, 80, 80,		// T_LT, T_GT, T_LE, T_GE
  90, 90,			// T_LSHIFT, T_RSHIFT
  100, 100,			// T_PLUS, T_MINUS
  110, 110, 110			// T_STAR, T_SLASH, T_MOD
};

// Check that we have a binary operator and
// return its precedence.
/*这段代码的目的是检查给定的二进制运算符标记类型，并返回该运算符的优先级。*/
static int op_precedence(int tokentype) {
  int prec;
  if (tokentype > T_MOD)
    fatals("Token with no precedence in op_precedence:", Tstring[tokentype]);
  prec = OpPrec[tokentype];
  if (prec == 0)
    fatals("Syntax error, token", Tstring[tokentype]);
  return (prec);
}

// prefix_expression: postfix_expression
//     | '*'  prefix_expression
//     | '&'  prefix_expression
//     | '-'  prefix_expression
//     | '++' prefix_expression
//     | '--' prefix_expression
//     ;

// Parse a prefix expression and return 
// a sub-tree representing it.
/*这是解析前缀表达式的函数，参数 ptp 表示前一个操作符的优先级。*/
static struct ASTnode *prefix(int ptp) {
  struct ASTnode *tree = NULL;
  switch (Token.token) {
    /*处理取地址操作符 &。*/
    case T_AMPER:
      // Get the next token and parse it
      // recursively as a prefix expression
      scan(&Token);
      /*通过递归调用 prefix(ptp) 解析 & 后的表达式。*/
      tree = prefix(ptp);

      // Ensure that it's an identifier
      if (tree->op != A_IDENT)/*确保表达式的根节点是标识符（A_IDENT）。*/
	fatal("& operator must be followed by an identifier");

      // Prevent '&' being performed on an array
      if (tree->sym->stype == S_ARRAY)/*防止在数组上执行 & 操作。*/
	fatal("& operator cannot be performed on an array");

      // Now change the operator to A_ADDR and the type to
      // a pointer to the original type. Mark the identifier
      // as needing a real memory address
      tree->op = A_ADDR;/*将根节点的操作符改为 A_ADDR，*/
      tree->type = pointer_to(tree->type);/*类型改为指向原始类型的指针，并*/
      tree->sym->st_hasaddr = 1;/*并标记标识符需要真正的内存地址。,1是作标记*/
      break;
    case T_STAR:
      // Get the next token and parse it
      // recursively as a prefix expression.
      // Make it an rvalue
      scan(&Token);
      tree = prefix(ptp);/*通过递归调用 prefix(ptp) 解析 * 后的表达式。*/
      tree->rvalue = 1;/*将表达式的 rvalue 标志设置为 1，表示它是右值。只是一个地址就为右值*/

      // Ensure the tree's type is a pointer
      if (!ptrtype(tree->type))/*确保表达式的类型是指针类型。*/
	fatal("* operator must be followed by an expression of pointer type");

      // Prepend an A_DEREF operation to the tree
      tree =
	mkastunary(A_DEREF, value_at(tree->type), tree->ctype, tree, NULL, 0);/*在树的顶部添加一个 A_DEREF 操作符。*/
      break;
    case T_MINUS:
    /*理一元负号操作符 -*/
      // Get the next token and parse it
      // recursively as a prefix expression
      scan(&Token);
      tree = prefix(ptp);

      // Prepend a A_NEGATE operation to the tree and
      // make the child an rvalue. Because chars are unsigned,
      // also widen this if needed to int so that it's signed
      tree->rvalue = 1;/*将表达式的 rvalue 标志设置为 1，表示它是右值。*/
      if (tree->type == P_CHAR)/*如果表达式的类型是 char，将其类型更改为 int，以确保它是有符号的。*/
	tree->type = P_INT;
      tree = mkastunary(A_NEGATE, tree->type, tree->ctype, tree, NULL, 0);/*在树的顶部添加一个 A_NEGATE 操作符。*/
      break;
    case T_INVERT:
      /*处理按位取反操作符 ~*/
      // Get the next token and parse it
      // recursively as a prefix expression
      scan(&Token);
      tree = prefix(ptp);

      // Prepend a A_INVERT operation to the tree and
      // make the child an rvalue.
      tree->rvalue = 1;/*将表达式的 rvalue 标志设置为 1，表示它是右值。*/
      tree = mkastunary(A_INVERT, tree->type, tree->ctype, tree, NULL, 0);
      break;
    case T_LOGNOT:
    /*处理逻辑非操作符 !。*/
      // Get the next token and parse it
      // recursively as a prefix expression
      scan(&Token);
      tree = prefix(ptp);

      // Prepend a A_LOGNOT operation to the tree and
      // make the child an rvalue.
      tree->rvalue = 1;/*将表达式的 rvalue 标志设置为 1，表示它是右值。*/
      tree = mkastunary(A_LOGNOT, tree->type, tree->ctype, tree, NULL, 0);
      break;
    case T_INC:
      // Get the next token and parse it
      // recursively as a prefix expression
      scan(&Token);
      tree = prefix(ptp);

      // For now, ensure it's an identifier
      if (tree->op != A_IDENT)
	fatal("++ operator must be followed by an identifier");

      // Prepend an A_PREINC operation to the tree
      tree = mkastunary(A_PREINC, tree->type, tree->ctype, tree, NULL, 0);
      break;
    case T_DEC:
      // Get the next token and parse it
      // recursively as a prefix expression
      scan(&Token);
      tree = prefix(ptp);

      // For now, ensure it's an identifier
      if (tree->op != A_IDENT)
	fatal("-- operator must be followed by an identifier");

      // Prepend an A_PREDEC operation to the tree
      tree = mkastunary(A_PREDEC, tree->type, tree->ctype, tree, NULL, 0);
      break;
    default:
      tree = postfix(ptp);
  }
  return (tree);
}

// Return an AST tree whose root is a binary operator.
// Parameter ptp is the previous token's precedence.
/*
初始化用于构建 AST 的变量，包括左子树 left、右子树 right，
以及用于中间计算的临时变量 ltemp 和 rtemp。还有用于存储 AST 操作类型的 ASTop 变量和表示当前标记类型的 tokentype 变量。
*/
//这个是为了进行二元运算的操作而使用的函数。
struct ASTnode *binexpr(int ptp) {
  struct ASTnode *left, *right;
  struct ASTnode *ltemp, *rtemp;
  int ASTop;
  int tokentype;

  // Get the tree on the left.
  // Fetch the next token at the same time.
  left = prefix(ptp);/*调用 prefix 函数获取左子树，并在同一时间获取下一个标记。*/

  // If we hit one of several terminating tokens, return just the left node
  /*如果当前标记是终止标记之一，直接返回左子树。设置 rvalue 为 1 表示左子树是右值。*/
  tokentype = Token.token;
  if (tokentype == T_SEMI || tokentype == T_RPAREN ||
      tokentype == T_RBRACKET || tokentype == T_COMMA ||
      tokentype == T_COLON || tokentype == T_RBRACE) {
    left->rvalue = 1;
    return (left);
  }
  // While the precedence of this token is more than that of the
  // previous token precedence, or it's right associative and
  // equal to the previous token's precedence
  /*这个循环会一直执行，直到遇到的运算符的优先级小于或等于前一个运算符的优先级。*/
  while ((op_precedence(tokentype) > ptp) ||
	 (rightassoc(tokentype) && op_precedence(tokentype) == ptp)) {
    // Fetch in the next integer literal
    scan(&Token);

    // Recursively call binexpr() with the
    // precedence of our token to build a sub-tree
    /*这种递归的策略允许解析器按照运算符的优先级逐步构建抽象语法树。
    在递归的每一步，它会递归地处理具有更高优先级的运算符，直到遇到具有较低优先级的运算符或者表达式的末尾。*/
    right = binexpr(OpPrec[tokentype]);

    // Determine the operation to be performed on the sub-trees
    /*
    这段代码的作用是确定在子树上要执行的操作，即确定要在语法树上的两个子节点之间执行的操作。
    binastop() 函数通过传入的运算符类型（tokentype）返回相应的抽象语法树操作码（ASTop）。该操作码表示应该在语法树上执行的特定操作*/
    ASTop = binastop(tokentype);

    switch (ASTop) {
      case A_TERNARY:
	// Ensure we have a ':' token, scan in the expression after it
	match(T_COLON, ":");/*如果当前符号位：,去扫描下一个字符*/
	ltemp = binexpr(0);/*调用 binexpr(0) 解析并获取冒号后的表达式，即假值时的表达式。*/

	// Build and return the AST for this statement. Use the middle
	// expression's type as the return type. XXX We should also
	// consider the third expression's type.
  //left是问号之前的数据，right是问号之后位真的数据，ltemp是问号之后为假的数据
	return (mkastnode
		(A_TERNARY, right->type, right->ctype, left, right, ltemp,
		 NULL, 0));//结果的符号

      case A_ASSIGN:
	// Assignment
	// Make the right tree into an rvalue
	right->rvalue = 1;/*将右子树（right）标记为右值（rvalue）。*/

	// Ensure the right's type matches the left
  /*将右子树的类型修改为左子树的类型。这是为了确保赋值操作符两边的类型是兼容的。*/
	right = modify_type(right, left->type, left->ctype, 0);
	if (right == NULL)
	  fatal("Incompatible expression in assignment");

	// Make an assignment AST tree. However, switch
	// left and right around, so that the right expression's 
	// code will be generated before the left expression
  /*创建一个表示赋值操作的AST子树，但在生成AST子树时交换左右操作数的顺序。
  具体来说，它交换了left和right指针的值，以确保生成右操作数的代码在生成左操作数的代码之前执行。*/
  
	ltemp = left;
	left = right;
	right = ltemp;
	break;

      default:
	// We are not doing a ternary or assignment, so both trees should
	// be rvalues. Convert both trees into rvalue if they are lvalue trees
  /*这段代码处理非三元运算符和赋值运算符的情况。*/
	left->rvalue = 1;
	right->rvalue = 1;/*设置左右子树为右值。*/

	// Ensure the two types are compatible by trying
	// to modify each tree to match the other's type.
  /*通过 modify_type 函数，尝试修改左右子树的类型以使它们相容。modify_type 函数用于确保两个表达式的类型兼容。
  如果两者不兼容，会导致致命错误。如果某个子树需要修改，则将新的子树赋给相应的变量。*/
	ltemp = modify_type(left, right->type, right->ctype, ASTop);//ASTop是字符
	rtemp = modify_type(right, left->type, left->ctype, ASTop);
	if (ltemp == NULL && rtemp == NULL)
	  fatal("Incompatible types in binary expression");
	if (ltemp != NULL)
	  left = ltemp;
	if (rtemp != NULL)
	  right = rtemp;
    }

    // Join that sub-tree with ours. Convert the token
    // into an AST operation at the same time.
    /*这段代码的目的是将左子树和右子树连接成一个新的AST节点，并指定该节点的操作符类型。*/
    left =
      mkastnode(binastop(tokentype), left->type, left->ctype, left, NULL,
		right, NULL, 0);

    // Some operators produce an int result regardless of their operands
    switch (binastop(tokentype)) {
      case A_LOGOR:
      case A_LOGAND:
      case A_EQ:
      case A_NE:
      case A_LT:
      case A_GT:
      case A_LE:
      case A_GE:
	left->type = P_INT;/*将 left->type（即左子树的类型）设置为整数类型 P_INT。*/
    }

    // Update the details of the current token.
    // If we hit a terminating token, return just the left node
    tokentype = Token.token;/*更新当前标记类型，将其设置为下一个标记的类型。*/
    if (tokentype == T_SEMI || tokentype == T_RPAREN ||
	tokentype == T_RBRACKET || tokentype == T_COMMA ||
	tokentype == T_COLON || tokentype == T_RBRACE) {
      left->rvalue = 1;
      return (left);
    }
  }

  // Return the tree we have when the precedence
  // is the same or lower
  left->rvalue = 1;
  return (left);
}
