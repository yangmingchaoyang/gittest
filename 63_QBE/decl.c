#include "defs.h"
#include "data.h"
#include "decl.h"

// Parsing of declarations
// Copyright (c) 2019 Warren Toomey, GPL3

static struct symtable *composite_declaration(int type);
static int typedef_declaration(struct symtable **ctype);
static int type_of_typedef(char *name, struct symtable **ctype);
static void enum_declaration(void);

// Parse the current token and return a primitive type enum value,
// a pointer to any composite type and possibly modify
// the class of the type.
/*这段代码实现了一个函数 parse_type，其目的是解析当前的标记（token）并返回相应的基本类型枚举值，
一个指向任何复合类型的指针，并且可能修改类型的类别（class）。*/
/*标识符是由程序员定义的变量、函数、结构体等命名时使用的名称。
给变量定义类型，且给结构体和联合以及枚举类型装载*/
int parse_type(struct symtable **ctype, int *class) {
  int type = 0, exstatic = 1;

  // See if the class has been changed to extern or static
  while (exstatic) {
    switch (Token.token) {//判断这个词牌是extern，还是static
      case T_EXTERN:
	if (*class == C_STATIC)//extern与static不能共用
	  fatal("Illegal to have extern and static at the same time");
	*class = C_EXTERN;
	scan(&Token);
	break;
      case T_STATIC:
	if (*class == C_LOCAL)
	  fatal("Compiler doesn't support static local declarations");//提示编译器不支持静态局部变量的声明。
	if (*class == C_EXTERN)
	  fatal("Illegal to have extern and static at the same time");//提示在同一时间不能同时具有 extern 和 static 存储类别。
	*class = C_STATIC;
	scan(&Token);
	break;
      default:
	exstatic = 0;
    }
  }

  // Now work on the actual type keyword
  switch (Token.token) {
    case T_VOID:
      type = P_VOID;
      scan(&Token);
      break;
    case T_CHAR:
      type = P_CHAR;
      scan(&Token);
      break;
    case T_INT:
      type = P_INT;
      scan(&Token);
      break;
    case T_LONG:
      type = P_LONG;
      scan(&Token);
      break;

      // For the following, if we have a ';' after the
      // parsing then there is no type, so return -1.
      // Example: struct x {int y; int z};
    case T_STRUCT:
      type = P_STRUCT;
      *ctype = composite_declaration(P_STRUCT);
      if (Token.token == T_SEMI)
  //表示这个联合体的定义后面没有成员声明，因此将 type 设为 -1。设置为-1后，最后根据-1进行处理。
	type = -1;
      break;
    case T_UNION:
      type = P_UNION;
      *ctype = composite_declaration(P_UNION);
      if (Token.token == T_SEMI)
	type = -1;
      break;
    case T_ENUM:
      type = P_INT;		// Enums are really ints
      enum_declaration();
      if (Token.token == T_SEMI)
	type = -1;
      break;
    case T_TYPEDEF:
      type = typedef_declaration(ctype);
      if (Token.token == T_SEMI)
	type = -1;
      break;
    case T_IDENT:
      type = type_of_typedef(Text, ctype);
      break;
    default:
      fatals("Illegal type, token", Token.tokstr);
  }
  return (type);
}

// Given a type parsed by parse_type(), scan in any following
// '*' tokens and return the new type
/*其作用是在给定的基本类型 type 后，扫描并解析任何后续的 '*' 标记，然后返回新的类型。*/
int parse_stars(int type) {

  while (1) {
    if (Token.token != T_STAR)
      break;
    type = pointer_to(type);
    scan(&Token);
  }
  return (type);//可能会变为指针类型
}

// Parse a type which appears inside a cast
int parse_cast(struct symtable **ctype) {
  int type = 0, class = 0;

  // Get the type inside the parentheses
  type = parse_stars(parse_type(ctype, &class));

  // Do some error checking. I'm sure more can be done
  if (type == P_STRUCT || type == P_UNION || type == P_VOID)
    fatal("Cannot cast to a struct, union or void type");
  return (type);
}

// Given a type, parse an expression of literals and ensure
// that the type of this expression matches the given type.
// Parse any type cast that precedes the expression.
// If an integer literal, return this value.
// If a string literal, return the label number of the string.
/*这段代码用于解析字面值表达式，并确保该表达式的类型与给定的类型匹配。*/
int parse_literal(int type) {
  struct ASTnode *tree;

  // Parse the expression and optimise the resulting AST tree
  tree = optimise(binexpr(0));/*设置折叠整数字面值*/

  // If there's a cast, get the child and
  // mark it as having the type from the cast
  /*检查是否有强制类型转换 (A_CAST)： 首先，代码检查 AST 树的根节点 tree 是否是一个类型转换节点 (A_CAST)。
  如果是的话，它获取类型转换节点的子节点，并将该子节点的类型设置为与类型转换节点相同的类型。
  这是因为初始化表达式中可能包含了类型转换，而初始化的全局变量需要使用转换后的类型。*/
  if (tree->op == A_CAST) {
    tree->left->type = tree->type;
    tree = tree->left;
  }
  // The tree must now have an integer or string literal
  /*代码检查经过可能的类型转换后的节点 (tree) 的类型。初始化的全局变量只能使用整数或字符串字面量来进行初始化。
  因此，代码检查节点的类型是否为整数字面量 (A_INTLIT) 或字符串字面量 (A_STRLIT)。*/
  if (tree->op != A_INTLIT && tree->op != A_STRLIT)
    fatal("Cannot initialise globals with a general expression");

  // If the type is char * and
  /*代码首先检查变量的类型是否为字符指针（type == pointer_to(P_CHAR)）。*/
  if (type == pointer_to(P_CHAR)) {
    // We have a string literal, return the label number
    if (tree->op == A_STRLIT)
      return (tree->a_intvalue);
    // We have a zero int literal, so that's a NULL
    if (tree->op == A_INTLIT && tree->a_intvalue == 0)
      return (0);
  }
  // We only get here with an integer literal. The input type
  // is an integer type and is wide enough to hold the literal value
  /*通过调用 inttype(type) 检查变量的类型是否为整数类型,检查类型是否足够宽以容纳字面量值*/
  if (inttype(type) && typesize(type, NULL) >= typesize(tree->type, NULL))
    return (tree->a_intvalue);

  fatal("Type mismatch: literal vs. variable");
  return (0);			// Keep -Wall happy
}

// Given a pointer to a symbol that may already exist
// return true if this symbol doesn't exist. We use
// this function to convert externs into globals
/*这段函数的作用是检查给定的符号表条目（symbol table entry）是否代表一个新的符号，
具体而言，它检查一个符号是否不存在，以便将 extern 转换为 global。*/
static int is_new_symbol(struct symtable *sym, int class,
			 int type, struct symtable *ctype) {

  // There is no existing symbol, thus is new
  if (sym == NULL)
    return (1);

  // global versus extern: if they match that it's not new
  // and we can convert the class to global
  /*如果 sym 不为 NULL，则检查符号的类别（class）是否为 C_GLOBAL，而传入的 class 是否为 C_EXTERN，*/
  if ((sym->class == C_GLOBAL && class == C_EXTERN)
      || (sym->class == C_EXTERN && class == C_GLOBAL)) {

    // If the types don't match, there's a problem
    /*如果符号存在，还需要检查类型是否匹配。如果类型不匹配，函数会报告类型不匹配的错误。*/
    if (type != sym->type)
      fatals("Type mismatch between global/extern", sym->name);

    // Struct/unions, also compare the ctype
    /*如果符号的类型是结构体或联合体，还需要比较它们的类型信息（ctype）。如果类型信息不匹配，同样会报告类型不匹配的错误。*/
    if (type >= P_STRUCT && ctype != sym->ctype)
      fatals("Type mismatch between global/extern", sym->name);

    // If we get to here, the types match, so mark the symbol
    // as global
    /*如果类型和类型信息都匹配，将符号的类别修改为 C_GLOBAL，表示这是一个全局变量。*/
    sym->class = C_GLOBAL;
    // Return that symbol is not new
    return (0);
  }
  // It must be a duplicate symbol if we get here
  fatals("Duplicate global variable declaration", sym->name);
  return (-1);			// Keep -Wall happy
}

// Given the type, name and class of a scalar variable,
// parse any initialisation value and allocate storage for it.
// Return the variable's symbol table entry.
/*这段代码实现了对标量变量的声明和初始化操作。*/
static struct symtable *scalar_declaration(char *varname, int type,
					   struct symtable *ctype,
					   int class, struct ASTnode **tree) {
  struct symtable *sym = NULL;
  struct ASTnode *varnode, *exprnode;
  *tree = NULL;

  // Add this as a known scalar
  switch (class) {
    case C_STATIC:
    case C_EXTERN:
    case C_GLOBAL:
      // See if this variable is new or already exists
      sym = findglob(varname);
      if (is_new_symbol(sym, class, type, ctype))
	sym = addglob(varname, type, ctype, S_VARIABLE, class, 1, 0);
      break;
    case C_LOCAL:
      sym = addlocl(varname, type, ctype, S_VARIABLE, 1);
      break;
    case C_PARAM:
      sym = addparm(varname, type, ctype, S_VARIABLE);
      break;
    case C_MEMBER:
      sym = addmemb(varname, type, ctype, S_VARIABLE, 1);
      break;
  }

  // The variable is being initialised
  if (Token.token == T_ASSIGN) {/*检查是否有赋值操作*/
    // Only possible for a global or local
    /*变量的类别必须是全局（C_GLOBAL）、局部（C_LOCAL）或静态（C_STATIC）之一，否则报错。*/
    if (class != C_GLOBAL && class != C_LOCAL && class != C_STATIC)
      fatals("Variable can not be initialised", varname);
    scan(&Token);

    // Globals must be assigned a literal value
    /*
    如果变量是全局变量或静态变量，那么它必须被赋予一个字面值。
    为变量的initlist字段分配内存，该字段用于存储初始化值。
    调用parse_literal(type)解析一个字面值，并将其存储在initlist中。
    */
    if (class == C_GLOBAL || class == C_STATIC) {
      // Create one initial value for the variable and
      // parse this value
      sym->initlist = (int *) malloc(sizeof(int));
      sym->initlist[0] = parse_literal(type);
    }
    if (class == C_LOCAL) {
      /*
      如果变量是局部变量，那么需要构建对应的AST（抽象语法树）表示赋值操作。
      使用mkastleaf创建一个A_IDENT的AST节点，表示变量。
      使用binexpr(0)获取赋值语句右侧的表达式，并将其标记为rvalue。
      使用modify_type确保表达式的类型与变量的类型匹配。
      创建一个A_ASSIGN的AST节点，表示赋值操作，连接变量节点和表达式节点。
      */
      // Make an A_IDENT AST node with the variable
      varnode = mkastleaf(A_IDENT, sym->type, sym->ctype, sym, 0);

      // Get the expression for the assignment, make into a rvalue
      exprnode = binexpr(0);
      exprnode->rvalue = 1;

      // Ensure the expression's type matches the variable
      exprnode = modify_type(exprnode, varnode->type, varnode->ctype, 0);
      if (exprnode == NULL)
	fatal("Incompatible expression in assignment");

      // Make an assignment AST tree
      /*进行变量赋值操作。*/
      *tree = mkastnode(A_ASSIGN, exprnode->type, exprnode->ctype, exprnode,
			NULL, varnode, NULL, 0);
    }
  }
  // Generate any global space
  if (class == C_GLOBAL || class == C_STATIC)
    genglobsym(sym);

  return (sym);
}

// Given the type, name and class of an array variable, parse
// the size of the array, if any. Then parse any initialisation
// value and allocate storage for it.
// Return the variable's symbol table entry.
/*这段代码负责解析数组声明，处理可能的数组初始化，并生成相应的符号表条目。*/
static struct symtable *array_declaration(char *varname, int type,
					  struct symtable *ctype, int class) {

  struct symtable *sym = NULL;	// New symbol table entry/* 用于表示新的符号表条目。在函数执行期间，该指针将指向数组的符号表条目。
  int nelems = -1;		// Assume the number of elements won't be given 用于存储数组的元素数量。初始值设置为 -1，表示假设数组大小不会被指定。
  int maxelems;			// The maximum number of elements in the init list 用于表示初始化列表中的元素的最大数量。这个值在处理初始化列表时可能会动态增加。
  int *initlist;		// The list of initial elements 用作计数器，表示初始化列表中的当前元素位置。
  int i = 0, j;

  // Skip past the '['
  scan(&Token);

  // See if we have an array size
  if (Token.token != T_RBRACKET) {
    nelems = parse_literal(P_INT);/*解析字面值的大小*/
    if (nelems <= 0)
      fatald("Array size is illegal", nelems);
  }
  // Ensure we have a following ']'
  match(T_RBRACKET, "]");

  // Add this as a known array. We treat the
  // array as a pointer to its elements' type
  switch (class) {
    case C_STATIC:
    case C_EXTERN:
    case C_GLOBAL:
      // See if this variable is new or already exists
      sym = findglob(varname);//在全局变量中找到定义
      if (is_new_symbol(sym, class, pointer_to(type), ctype))
	sym = addglob(varname, pointer_to(type), ctype, S_ARRAY, class, 0, 0);
      break;
    case C_LOCAL:
      // Add the array to the local symbol table. Mark it as having an address
      /*本地变量只有指针，没有初始化类型*/
      sym = addlocl(varname, pointer_to(type), ctype, S_ARRAY, 0);
      sym->st_hasaddr = 1;//这个是是否有取地址操作的标识符
      break;
    default:
      fatal("Declaration of array parameters is not implemented");
  }

  // Array initialisation
  if (Token.token == T_ASSIGN) {
    /*为只有全局变量和静态变量才能在声明时进行初始化。如果变量不是全局变量或静态变量，就会调用 fatals 函数报告错误，*/
    if (class != C_GLOBAL && class != C_STATIC)
      fatals("Variable can not be initialised", varname);
    scan(&Token);

    // Get the following left curly bracket
    match(T_LBRACE, "{");

#define TABLE_INCREMENT 10

    // If the array already has nelems, allocate that many elements
    // in the list. Otherwise, start with TABLE_INCREMENT.
    /*如果数组已经有指定的元素数量 nelems，那么 maxelems 就等于 nelems。
    否则，如果数组没有指定元素数量，maxelems 就初始化为 TABLE_INCREMENT。*/
    if (nelems != -1)
      maxelems = nelems;
    else
      maxelems = TABLE_INCREMENT;
    initlist = (int *) malloc(maxelems * sizeof(int));

    // Loop getting a new literal value from the list
    while (1) {

      // Check we can add the next value, then parse and add it
      /*如果数组有指定的元素数量 nelems，并且已经解析的值数量 i 等于 nelems，则调用 fatal 报错，因为初始化列表中的值过多。*/
      if (nelems != -1 && i == maxelems)
	fatal("Too many values in initialisation list");

      /*解析下一个值，并将其添加到初始化列表中。*/
      initlist[i++] = parse_literal(type);

      // Increase the list size if the original size was
      // not set and we have hit the end of the current list
      /*如果数组没有指定元素数量，并且已经解析的值数量 i 等于当前列表空间大小 maxelems，则通过 realloc 增加列表的空间。*/
      if (nelems == -1 && i == maxelems) {
	maxelems += TABLE_INCREMENT;
	initlist = (int *) realloc(initlist, maxelems * sizeof(int));
      }
      // Leave when we hit the right curly bracket
      if (Token.token == T_RBRACE) {
	scan(&Token);
	break;
      }
      // Next token must be a comma, then
      comma();
    }

    // Zero any unused elements in the initlist.
    // Attach the list to the symbol table entry
    /*遍历初始化列表中未使用的元素，并将它们置为零。这是因为数组可能没有完全填充初始化列表，
    但它的大小（sym->nelems）是根据指定的元素数量或实际初始化值的数量来计算的。*/
    for (j = i; j < sym->nelems; j++)
      initlist[j] = 0;

    /*如果实际解析的初始化值数量 i 大于指定的元素数量 nelems，则更新 nelems 为 i。这是为了确保 nelems 取值为数组实际的初始化值数量。
    将初始化列表 initlist 附加到符号表条目 sym 中，以便后续使用。*/
    if (i > nelems)
      nelems = i;
    sym->initlist = initlist;
  }
  // Set the size of the array and the number of elements
  // Only externs can have no elements.
  //如果数组的类别不是 C_EXTERN 且元素数量 nelems 小于等于 0，则发出错误消息，因为数组必须具有非零的元素数量。
  if (class != C_EXTERN && nelems <= 0)
    fatals("Array must have non-zero elements", sym->name);

  sym->nelems = nelems;
  sym->size = sym->nelems * typesize(type, ctype);
  // Generate any global space
  /*该段代码调用 genglobsym(sym) 函数，该函数负责生成符号 sym 对应的全局存储空间。这可能包括为已初始化的全局变量分配空间，
  并将初始化的值存储在相应的位置，或者为未初始化的全局变量分配 BSS 段中的空间，将其初始值设置为零。*/
  if (class == C_GLOBAL || class == C_STATIC)
    genglobsym(sym);
  return (sym);
}

// Given a pointer to the new function being declared and
// a possibly NULL pointer to the function's previous declaration,
// parse a list of parameters and cross-check them against the
// previous declaration. Return the count of parameters
/*这段代码处理函数参数列表的解析，其中包括对参数类型和数量的检查。*/
static int param_declaration_list(struct symtable *oldfuncsym,
				  struct symtable *newfuncsym) {
  int type, paramcnt = 0;
  struct symtable *ctype;
  struct symtable *protoptr = NULL;
  struct ASTnode *unused;

  // Get the pointer to the first prototype parameter
  if (oldfuncsym != NULL)
    protoptr = oldfuncsym->member;

  // Loop getting any parameters
  while (Token.token != T_RPAREN) {

    // If the first token is 'void'
    /*如果当前标记是 T_VOID，表示函数的参数列表以 void 开始，这可能意味着函数没有参数。*/
    if (Token.token == T_VOID) {
      // Peek at the next token. If a ')', the function
      // has no parameters, so leave the loop.
      scan(&Peektoken);/*获取下一个标记（Peektoken）。*/
      if (Peektoken.token == T_RPAREN) {/*如果下一个标记是右括号，说明函数没有参数。*/
	// Move the Peektoken into the Token
	paramcnt = 0;
	scan(&Token);
	break;
      }
    }
    // Get the type of the next parameter
    /*调用 declaration_list 函数，解析下一个参数的类型和声明。如果返回值为-1，表示出现了错误，发出致命错误。*/
    //函数参数的树的列表中，在这里可以完成变量的定义。
    type = declaration_list(&ctype, C_PARAM, T_COMMA, T_RPAREN, &unused);//对里面的参数一个一个分析。
    if (type == -1)
      fatal("Bad type in parameter list");

    // Ensure the type of this parameter matches the prototype
    /*如果存在函数原型，检查当前参数的类型是否与原型中的类型匹配。*/
    if (protoptr != NULL) {
      if (type != protoptr->type)
	fatald("Type doesn't match prototype for parameter", paramcnt + 1);
      protoptr = protoptr->next;
    }
    paramcnt++;/*移动到下一个原型参数。*/

    // Stop when we hit the right parenthesis
    if (Token.token == T_RPAREN)
      break;
    // We need a comma as separator
    comma();
  }
  /*检查当前函数参数的数量是否与之前的函数原型中声明的参数数量不匹配。*/
  if (oldfuncsym != NULL && paramcnt != oldfuncsym->nelems)
    fatals("Parameter count mismatch for function", oldfuncsym->name);

  // Return the count of parameters
  return (paramcnt);
}

//
// function_declaration: type identifier '(' parameter_list ')' ;
//      | type identifier '(' parameter_list ')' compound_statement   ;
//
// Parse the declaration of function.
/*这段代码负责解析函数声明，包括参数列表和函数体，并将相关信息添加到符号表中。最后，它构建并生成函数的 AST，并进行一些优化。*/
/*生成的函数代码只需要保存关键信息，具体的函数已经生成了汇编，可以直接跳转对所需执行的汇编部分*/
static struct symtable *function_declaration(char *funcname, int type,
					     struct symtable *ctype,
					     int class) {
  struct ASTnode *tree, *finalstmt;
  struct symtable *oldfuncsym, *newfuncsym = NULL;
  int endlabel = 0, paramcnt;
  int linenum = Line;

  // Text has the identifier's name. If this exists and is a
  // function, get the id. Otherwise, set oldfuncsym to NULL.
  /*这段代码用于检查符号表中是否存在给定名称的符号，并判断该符号是否为函数类型。*/
  if ((oldfuncsym = findsymbol(funcname)) != NULL)
    if (oldfuncsym->stype != S_FUNCTION)
      oldfuncsym = NULL;

  // If this is a new function declaration, get a
  // label-id for the end label, and add the function
  // to the symbol table,
  /*说明这是一个新的函数声明，因为还没有与该函数名关联的符号表条目。*/
  if (oldfuncsym == NULL) {
    //用于生成一个新的标签，该标签可能用于表示函数的结束位置或者返回地址。
    endlabel = genlabel();
    // Assumption: functions only return scalar types, so NULL below
    //使用 addglob 函数向全局符号表中添加一个新的函数符号表条目。
    newfuncsym =
      addglob(funcname, type, NULL, S_FUNCTION, class, 0, endlabel);/*加入全局的符号表*/
  }
  // Scan in the '(', any parameters and the ')'.
  // Pass in any existing function prototype pointer
  lparen();
  paramcnt = param_declaration_list(oldfuncsym, newfuncsym);/*设置参数列表的参数*/
  rparen();

  // If this is a new function declaration, update the
  // function symbol entry with the number of parameters.
  // Also copy the parameter list into the function's node.
  if (newfuncsym) {
    newfuncsym->nelems = paramcnt;
    newfuncsym->member = Parmhead;
    oldfuncsym = newfuncsym;
  }
  // Clear out the parameter list
  Parmhead = Parmtail = NULL;

  // If the declaration ends in a semicolon, only a prototype.
  /*这段代码表示如果函数声明以分号（;）结束，那么这只是一个函数的原型声明，没有实际的函数体。*/
  if (Token.token == T_SEMI)
    return (oldfuncsym);

  // This is not just a prototype.
  // Set the Functionid global to the function's symbol pointer
  Functionid = oldfuncsym;

  // Get the AST tree for the compound statement and mark
  // that we have parsed no loops or switches yet
  Looplevel = 0;/*将循环的嵌套层级标记为 0。这是一个全局变量，用于追踪当前代码中循环的嵌套层级。*/
  Switchlevel = 0;/* 将 switch 语句的嵌套层级标记为 0。同样是一个全局变量，用于追踪当前代码中 switch 语句的嵌套层级。*/
  lbrace();
  tree = compound_statement(0);
  rbrace();

  // If the function type isn't P_VOID ...
  if (type != P_VOID) {/*表示如果函数的返回类型不是P_VOID。*/

    // Error if no statements in the function
    if (tree == NULL)
      fatal("No statements in function with non-void type");/*如果函数体为空，则调用fatal函数报告错误，表示非void类型的函数内没有语句。*/

    // Check that the last AST operation in the
    // compound statement was a return statement
    /*根据复合语句的结构，确定最后一个语句。如果复合语句是由A_GLUE操作连接的，
    那么最后一个语句是A_GLUE节点的右子树；否则，最后一个语句就是整个复合语句。*/
    finalstmt = (tree->op == A_GLUE) ? tree->right : tree;
    /*检查finalstmt是否为空或者最后一个语句的操作符不是A_RETURN。如果其中任一条件成立，说明非void类型的函数缺少返回语句。*/
    if (finalstmt == NULL || finalstmt->op != A_RETURN)
      fatal("No return for function with non-void type");
  }
  // Build the A_FUNCTION node which has the function's symbol pointer
  // and the compound statement sub-tree
  /*创建一个新的AST节点，表示函数定义。这个节点的操作符是A_FUNCTION，
  表示这是一个函数。参数包括函数的返回类型type，函数的类型（可能是函数指针类型）ctype，
  函数体的AST子树tree，指向函数符号表条目的指针oldfuncsym，以及表示函数结束位置的标签endlabel。
  这个新的AST节点将成为整个函数定义的根节点。*/
  tree = mkastunary(A_FUNCTION, type, ctype, tree, oldfuncsym, endlabel);
  /*将新创建的AST节点的行号字段设置为当前行号linenum。*/
  tree->linenum = linenum;

  // Do optimisations on the AST tree
  /*对整个函数定义的AST进行优化。这一步可能包括删除不必要的节点、简化表达式等，以提高生成的代码的效率。*/
  tree = optimise(tree);

  // Dump the AST tree if requested
  /*检查编译器选项 O_dumpAST 是否开启，即是否要求在生成代码之前将AST（抽象语法树）转储到标准输出。
  如果开启了这个选项，就执行下面的操作；否则，跳过这一部分。*/
  if (O_dumpAST) {
    dumpAST(tree, NOLABEL, 0);
    fprintf(stdout, "\n\n");
  }
  // Generate the assembly code for it
  /*调用 genAST 函数，生成函数体的汇编代码。*/
  genAST(tree, NOLABEL, NOLABEL, NOLABEL, 0);

  // Now free the symbols associated with this function
  /*释放与当前函数关联的局部符号表条目。这是一个清理步骤，确保在处理一个函数后释放相应的资源。*/
  freeloclsyms();
  return (oldfuncsym);
}

// Parse composite type declarations: structs or unions.
// Either find an existing struct/union declaration, or build
// a struct/union symbol table entry and return its pointer.
/*
这段代码实现了解析复合类型声明（结构体或联合体声明）的功能。
它主要用于在编译器中处理结构体和联合体的定义，包括成员的声明和计算结构体或联合体的大小和成员的偏移量。
*/
static struct symtable *composite_declaration(int type) {
  struct symtable *ctype = NULL;
  struct symtable *m;
  struct ASTnode *unused;
  int offset;
  int t;

  // Skip the struct/union keyword
  scan(&Token);//下一个字符

  // See if there is a following struct/union name
  /*判断下一个标记是否是标识符（identifier），即结构体或联合体的名称。如果是标识符，说明可能存在结构体或联合体的名称。*/
  if (Token.token == T_IDENT) {
    // Find any matching composite type
    //观察是否被重复定义。
    if (type == P_STRUCT)
      ctype = findstruct(Text);
    else
      ctype = findunion(Text);
    scan(&Token);
  }
  // If the next token isn't an LBRACE , this is
  // the usage of an existing struct/union type.
  // Return the pointer to the type.
  if (Token.token != T_LBRACE) {//不等于大括号，表示错误
    if (ctype == NULL)
      fatals("unknown struct/union type", Text);
    return (ctype);
  }
  // Ensure this struct/union type hasn't been
  // previously defined
  if (ctype)//如果ctype存在，就发生重复定义的错误。
    fatals("previously defined struct/union", Text);

  // Build the composite type and skip the left brace
  if (type == P_STRUCT)
    ctype = addstruct(Text);//ctype是一个结构体的symtable
  else
    ctype = addunion(Text);
  scan(&Token);

  // Scan in the list of members
  while (1) {
    // Get the next member. m is used as a dummy
    t = declaration_list(&m, C_MEMBER, T_SEMI, T_RBRACE, &unused);
    if (t == -1)
      fatal("Bad type in member list");
    if (Token.token == T_SEMI)
      scan(&Token);
    if (Token.token == T_RBRACE)
      break;
  }

  // Attach to the struct type's node
  rbrace();
  if (Membhead == NULL)
    fatals("No members in struct", ctype->name);
  ctype->member = Membhead;/*将结构体或联合体的成员列表指向 Membhead，该列表包含了所有成员的信息。*/
  Membhead = Membtail = NULL;/*将 Membhead 和 Membtail 设置为 NULL，以便为下一个结构体或联合体的成员重新开始链表。*/

  // Set the offset of the initial member
  // and find the first free byte after it
  m = ctype->member;/*将指针 m 指向结构体或联合体的第一个成员。*/
  m->st_posn = 0;/*设置第一个成员的偏移为 0。*/
  offset = typesize(m->type, m->ctype);/*计算第一个成员的大小，并将结果存储在 offset 变量中。*/

  // Set the position of each successive member in the composite type
  // Unions are easy. For structs, align the member and find the next free byte
  /*遍历结构体或联合体的成员链表，跳过第一个成员（因为第一个成员已经设置过了）。*/
  /*
  genalign 函数的作用是根据指定的对齐值，计算适当对齐的偏移位置。
  在结构体的上下文中，genalign(m->type, offset, 1) 这里的 m->type 代表结构体成员的类型，
  offset 是当前的偏移位置，1 是方向，表示向上对齐。这样就确保了结构体的成员在合适的内存位置开始，
  以满足对齐要求，提高数据访问效率。这一步是结构体布局中的常见操作，以保证结构体在内存中的正确对齐。
  */
  for (m = m->next; m != NULL; m = m->next) {
    // Set the offset for this member
    if (type == P_STRUCT)//这个只针对结构体
      m->st_posn = genalign(m->type, offset, 1);/*为结构体的成员设置适当的对齐偏移。*/
    else
      m->st_posn = 0;

    // Get the offset of the next free byte after this member
    offset += typesize(m->type, m->ctype);
  }

  // Set the overall size of the composite type
  ctype->size = offset;
  return (ctype);
}

// Parse an enum declaration
/*这段代码的目标是解析 enum 的声明，包括类型定义和枚举值的定义。*/
static void enum_declaration(void) {
  struct symtable *etype = NULL;
  char *name = NULL;
  int intval = 0;

  // Skip the enum keyword.
  scan(&Token);

  // If there's a following enum type name, get a
  // pointer to any existing enum type node.
  /*用于检查当前 token 是否为标识符（T_IDENT），如果是的话，表示有一个紧随其后的 enum 类型名称。*/
  if (Token.token == T_IDENT) {
    etype = findenumtype(Text);
    name = strdup(Text);	// As it gets tromped soon
    scan(&Token);
  }
  // If the next token isn't a LBRACE, check
  // that we have an enum type name, then return
  if (Token.token != T_LBRACE) {
    if (etype == NULL)
      fatals("undeclared enum type:", name);
    return;
  }
  // We do have an LBRACE. Skip it
  scan(&Token);

  // If we have an enum type name, ensure that it
  // hasn't been declared before.
  if (etype != NULL)
    fatals("enum type redeclared:", etype->name);
  else
    // Build an enum type node for this identifier
    etype = addenum(name, C_ENUMTYPE, 0);

  // Loop to get all the enum values
  //这段代码的作用是循环解析枚举类型的所有枚举值，并处理每个枚举值的关联整数值（如果存在），同时确保每个枚举值的唯一性。
  while (1) {
    // Ensure we have an identifier
    // Copy it in case there's an int literal coming up
    ident();
    name = strdup(Text);

    // Ensure this enum value hasn't been declared before
    etype = findenumval(name);
    if (etype != NULL)
      fatals("enum value redeclared:", Text);

    // If the next token is an '=', skip it and
    // get the following int literal
    if (Token.token == T_ASSIGN) {/*如果下一个 token 是等号，表示当前枚举值有一个关联的整数值。执行以下操作：*/
      scan(&Token);
      if (Token.token != T_INTLIT)
	fatal("Expected int literal after '='");
      intval = Token.intvalue;
      scan(&Token);
    }
    // Build an enum value node for this identifier.
    // Increment the value for the next enum identifier.
    etype = addenum(name, C_ENUMVAL, intval++);

    // Bail out on a right curly bracket, else get a comma
    if (Token.token == T_RBRACE)
      break;
    comma();
  }
  scan(&Token);			// Skip over the right curly bracket
}

// Parse a typedef declaration and return the type
// and ctype that it represents
/*这段代码是一个函数，用于解析 typedef 声明，并返回所表示的类型和 ctype。*/
static int typedef_declaration(struct symtable **ctype) {
  int type, class = 0;

  // Skip the typedef keyword.
  scan(&Token);

  // Get the actual type following the keyword
  type = parse_type(ctype, &class);
  if (class != 0)
  /*如果 class 不为 0，说明在 typedef 声明中出现了不允许的类型类别，例如 static 或 extern。因为 typedef 不能带有这些修饰符，
  所以调用 fatal 函数，终止程序执行，并打印错误消息，指出不能在 typedef 声明中使用 static 或 extern。*/
    fatal("Can't have static/extern in a typedef declaration");

  // See if the typedef identifier already exists
  //在typedef的链表中存在该变量名
  if (findtypedef(Text) != NULL)
    fatals("redefinition of typedef", Text);

  // Get any following '*' tokens
  type = parse_stars(type);/*判断是否是指针*/

  // It doesn't exist so add it to the typedef list
  addtypedef(Text, type, *ctype);
  scan(&Token);
  return (type);
}

// Given a typedef name, return the type it represents
/*这段代码是一个函数，用于根据给定的 typedef 名称找到其实际代表的类型。*/
static int type_of_typedef(char *name, struct symtable **ctype) {
  struct symtable *t;

  // Look up the typedef in the list
  t = findtypedef(name);
  if (t == NULL)
    fatals("unknown type", name);
  scan(&Token);
  *ctype = t->ctype;
  return (t->type);
}

// Parse the declaration of a variable or function.
// The type and any following '*'s have been scanned, and we
// have the identifier in the Token variable.
// The class argument is the symbol's class.
// Return a pointer to the symbol's entry in the symbol table
/*，symbol_declaration 函数用于解析变量或函数的声明，
包括处理函数声明、检查变量是否已经被声明以及调用相应的处理函数处理数组或标量的声明。最终，函数返回符号表中相应符号的条目。*/
static struct symtable *symbol_declaration(int type, struct symtable *ctype,
					   int class, struct ASTnode **tree) {
  struct symtable *sym = NULL;
  char *varname = strdup(Text);

  // Ensure that we have an identifier. 
  // We copied it above so we can scan more tokens in, e.g.
  // an assignment expression for a local variable.
  ident();//确保该信息就是一个标识符

  // Deal with function declarations
  if (Token.token == T_LPAREN) {//如果出现括号，则定义为函数
    return (function_declaration(varname, type, ctype, class));
  }
  // See if this array or scalar variable has already been declared
  /*这是因为在这个特定的情况下，无论变量是外部的、静态的、全局的、局部的还是参数，都需要检查是否在局部变量的链表中有重复的声明。*/
  switch (class) {
    case C_EXTERN:
    case C_STATIC:
    case C_GLOBAL:
    case C_LOCAL:
    case C_PARAM:
      if (findlocl(varname) != NULL)/*是否在局部变量*/
	fatals("Duplicate local variable declaration", varname);
    case C_MEMBER:
      if (findmember(varname) != NULL)/*是否是组成员*/
	fatals("Duplicate struct/union member declaration", varname);
  }

  // Add the array or scalar variable to the symbol table
  if (Token.token == T_LBRACKET) {//数组变量
    sym = array_declaration(varname, type, ctype, class);
    *tree = NULL;		// Local arrays are not initialised
  } else
    sym = scalar_declaration(varname, type, ctype, class, tree);
  return (sym);
}

// Parse a list of symbols where there is an initial type.
// Return the type of the symbols. et1 and et2 are end tokens.
/*这个函数的作用是解析一个符号列表，其中包含一个初始类型。
函数会返回符号列表的类型，并可以通过参数 gluetree 返回一个 AST（抽象语法树）表示这个符号列表的结构。
struct symtable **ctype: 一个指向指针的指针，用于存储当前符号列表的类型。在函数执行后，ctype 指向的指针会被更新，以反映符号列表的类型。

int class: 表示符号列表中符号的存储类别。这是一个整数，通常表示符号是在全局范围、局部范围还是静态范围内定义的。

int et1, int et2: 两个结束标记，用于指示符号列表的结束。当解析到其中一个结束标记时，函数会终止符号列表的解析。

struct ASTnode **gluetree: 一个指向指针的指针，用于存储生成的 AST，表示整个符号列表的结构。
在函数执行后，gluetree 指向的指针会被更新，以反映生成的 AST。
*/
int declaration_list(struct symtable **ctype, int class, int et1, int et2,
		     struct ASTnode **gluetree) {
  int inittype, type;
  struct symtable *sym;
  struct ASTnode *tree = NULL;
  *gluetree = NULL;

  // Get the initial type. If -1, it was
  // a composite type definition, return this
  /*调用 parse_type 函数获取初始类型 inittype，如果返回值为 -1，则说明是一个复合类型定义，函数直接返回该值。*/
  //是各种void int char 还是union struct typedef 还是typedef生成的字符
  //返回数据的类型，ctype中返回数据的的定义
  if ((inittype = parse_type(ctype, &class)) == -1)//等于-1的是已经定义完成的
    return (inittype);

  // Now parse the list of symbols
  while (1) {
    // See if this symbol is a pointer
    //判断是否要变为指针类型
    type = parse_stars(inittype);//这个是看接下来的字符是否是*

    // Parse this symbol
    //变量声明
    sym = symbol_declaration(type, *ctype, class, &tree);/*定义不在形成的子树中*/

    // We parsed a function, there is no list so leave
    /*说，这段代码用于确保函数定义只能出现在全局或静态的作用域中，
    并且在这种情况下返回函数的类型。如果发现函数定义不在正确的作用域中，将发出致命错误。*/
    if (sym->stype == S_FUNCTION) {
      if (class != C_GLOBAL && class != C_STATIC)
	fatal("Function definition not at global level");
      return (type);
    }
    // Glue any AST tree from a local declaration
    // to build a sequence of assignments to perform
    /*这段代码用于将本地声明的AST树与已有的AST树连接在一起，形成一个执行一系列赋值操作的AST树。*/
    if (*gluetree == NULL)
      *gluetree = tree;
    else
      *gluetree =
	mkastnode(A_GLUE, P_NONE, NULL, *gluetree, NULL, tree, NULL, 0);

    // We are at the end of the list, leave
    /*这段代码的目的是在解析一个表达式列表时，检查是否已经到达列表的末尾。*/
    if (Token.token == et1 || Token.token == et2)
      return (type);

    // Otherwise, we need a comma as separator
    comma();
  }

  return (0);			// Keep -Wall happy
}

// Parse one or more global declarations,
// either variables, functions or structs
void global_declarations(void) {
  struct symtable *ctype = NULL;/*初始化 ctype 为 NULL。ctype 用于表示当前解析的声明的类型，可能是结构体、联合体等。*/
  struct ASTnode *unused;

  // Loop parsing one declaration list until the end of file
  while (Token.token != T_EOF) {/*进入循环，该循环会解析一个声明列表，直到文件结束（Token 为 T_EOF）。*/
    //T_EOF这是一个常量或者枚举值，可能表示文件结束符。
    //T_SEMI在这个函数中，T_SEMI 可能用于指示声明列表的结束
    declaration_list(&ctype, C_GLOBAL, T_SEMI, T_EOF, &unused);

    // Skip any separating semicolons
    /*检查当前 Token 是否为分号 T_SEMI，如果是，调用 scan 函数获取下一个 Token，跳过分号。*/
    if (Token.token == T_SEMI)
      scan(&Token);
  }
}
