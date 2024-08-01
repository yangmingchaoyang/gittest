#include "defs.h"
#include "data.h"
#include "decl.h"

// Parsing of statements
// Copyright (c) 2019 Warren Toomey, GPL3

// Prototypes
static struct ASTnode *single_statement(void);

// compound_statement:          // empty, i.e. no statement
//      |      statement
//      |      statement statements
//      ;
//
// statement: declaration
//      |     expression_statement
//      |     function_call
//      |     if_statement
//      |     while_statement
//      |     for_statement
//      |     return_statement
//      ;


// if_statement: if_head
//      |        if_head 'else' statement
//      ;
//
// if_head: 'if' '(' true_false_expression ')' statement  ;
//
// Parse an IF statement including any
// optional ELSE clause and return its AST
//如何解析if
static struct ASTnode *if_statement(void) {
  struct ASTnode *condAST, *trueAST, *falseAST = NULL;

  // Ensure we have 'if' '('
  match(T_IF, "if");
  lparen();

  // Parse the following expression
  // and the ')' following. Force a
  // non-comparison to be boolean
  // the tree's operation is a comparison.
  /*
  调用这个函数解析条件表达式，返回相应的AST（抽象语法树）。
  这里的0表示最低的运算符优先级，即允许解析所有的运算符。
  检查条件是否为比较操作：

  if (condAST->op < A_EQ || condAST->op > A_GE)：检查条件表达式的操作符是否是比较操作符（小于、小于等于、大于、大于等于）。
  将非比较操作转换为布尔值：

  如果条件表达式的操作符不是比较操作符，说明它是一个非比较的表达式。
  */
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    condAST =
      mkastunary(A_TOBOOL, condAST->type, condAST->ctype, condAST, NULL, 0);
  rparen();

  // Get the AST for the statement
  trueAST = single_statement();

  // If we have an 'else', skip it
  // and get the AST for the statement
  if (Token.token == T_ELSE) {
    scan(&Token);
    falseAST = single_statement();
  }
  // Build and return the AST for this statement
  return (mkastnode(A_IF, P_NONE, NULL, condAST, trueAST, falseAST, NULL, 0));
}


// while_statement: 'while' '(' true_false_expression ')' statement  ;
//
// Parse a WHILE statement and return its AST
static struct ASTnode *while_statement(void) {
  struct ASTnode *condAST, *bodyAST;

  // Ensure we have 'while' '('
  match(T_WHILE, "while");
  lparen();

  // Parse the following expression
  // and the ')' following. Force a
  // non-comparison to be boolean
  // the tree's operation is a comparison.
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    condAST =
      mkastunary(A_TOBOOL, condAST->type, condAST->ctype, condAST, NULL, 0);
  rparen();

  // Get the AST for the statement.
  // Update the loop depth in the process
  Looplevel++;
  bodyAST = single_statement();
  Looplevel--;

  // Build and return the AST for this statement
  return (mkastnode(A_WHILE, P_NONE, NULL, condAST, NULL, bodyAST, NULL, 0));
}

// for_statement: 'for' '(' expression_list ';'
//                          true_false_expression ';'
//                          expression_list ')' statement  ;
//
// Parse a FOR statement and return its AST
static struct ASTnode *for_statement(void) {
  struct ASTnode *condAST, *bodyAST;
  struct ASTnode *preopAST, *postopAST;
  struct ASTnode *tree;

  // Ensure we have 'for' '('
  match(T_FOR, "for");
  lparen();

  // Get the pre_op expression and the ';'
  preopAST = expression_list(T_SEMI);
  semi();

  // Get the condition and the ';'.
  // Force a non-comparison to be boolean
  // the tree's operation is a comparison.
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    condAST =
      mkastunary(A_TOBOOL, condAST->type, condAST->ctype, condAST, NULL, 0);
  semi();

  // Get the post_op expression and the ')'
  postopAST = expression_list(T_RPAREN);
  rparen();

  // Get the statement which is the body
  // Update the loop depth in the process
  Looplevel++;
  bodyAST = single_statement();
  Looplevel--;

  // Glue the statement and the postop tree
  /*使用A_GLUE节点将bodyAST（循环体）和postopAST（循环后操作）整合在一起，形成一个新的AST节点。*/
  tree = mkastnode(A_GLUE, P_NONE, NULL, bodyAST, NULL, postopAST, NULL, 0);

  // Make a WHILE loop with the condition and this new body
  tree = mkastnode(A_WHILE, P_NONE, NULL, condAST, NULL, tree, NULL, 0);

  // And glue the preop tree to the A_WHILE tree
  return (mkastnode(A_GLUE, P_NONE, NULL, preopAST, NULL, tree, NULL, 0));
}

// return_statement: 'return' '(' expression ')'  ;
//
// Parse a return statement and return its AST
static struct ASTnode *return_statement(void) {
  struct ASTnode *tree = NULL;

  // Ensure we have 'return'
  match(T_RETURN, "return");

  // See if we have a return value
  if (Token.token == T_LPAREN) {
    // Can't return a value if function returns P_VOID
    if (Functionid->type == P_VOID)
      fatal("Can't return from a void function");

    // Skip the left parenthesis
    lparen();

    // Parse the following expression
    tree = binexpr(0);

    // Ensure this is compatible with the function's type
    tree = modify_type(tree, Functionid->type, Functionid->ctype, 0);
    if (tree == NULL)
      fatal("Incompatible type to return");

    // Get the ')'
    rparen();
  }
  // Add on the A_RETURN node
  tree = mkastunary(A_RETURN, P_NONE, NULL, tree, NULL, 0);

  // Get the ';'
  semi();
  return (tree);
}

// break_statement: 'break' ;
//
// Parse a break statement and return its AST
static struct ASTnode *break_statement(void) {

  if (Looplevel == 0 && Switchlevel == 0)
    fatal("no loop or switch to break out from");
  scan(&Token);
  semi();
  return (mkastleaf(A_BREAK, P_NONE, NULL, NULL, 0));
}

// continue_statement: 'continue' ;
//
// Parse a continue statement and return its AST
static struct ASTnode *continue_statement(void) {

  if (Looplevel == 0)
    fatal("no loop to continue to");
  scan(&Token);
  semi();
  return (mkastleaf(A_CONTINUE, P_NONE, NULL, NULL, 0));
}

// Parse a switch statement and return its AST
static struct ASTnode *switch_statement(void) {
  struct ASTnode *left, *body, *n, *c;
  struct ASTnode *casetree = NULL, *casetail = NULL;
  int inloop = 1, casecount = 0;
  int seendefault = 0;
  int ASTop, casevalue = 0;

  // Skip the 'switch' and '('
  scan(&Token);
  lparen();

  // Get the switch expression, the ')' and the '{'
  left = binexpr(0);
  rparen();
  lbrace();

  // Ensure that this is of int type
  if (!inttype(left->type))
    fatal("Switch expression is not of integer type");

  // Build an A_SWITCH subtree with the expression as
  // the child
  /*构建一个A_SWITCH节点，表示switch语句，并将表达式作为子节点。*/
  n = mkastunary(A_SWITCH, P_NONE, NULL, left, NULL, 0);

  // Now parse the cases
  Switchlevel++;/*增加switch层级计数。*/
  while (inloop) {/*循环解析case和default子句，直到遇到}结束。*/
    switch (Token.token) {
	// Leave the loop when we hit a '}'
      case T_RBRACE:
	if (casecount == 0)
	  fatal("No cases in switch");
	inloop = 0;
	break;
      case T_CASE:
      case T_DEFAULT:
	// Ensure this isn't after a previous 'default'
	if (seendefault)
	  fatal("case or default after existing default");

	// Set the AST operation. Scan the case value if required
  /*
  设置ASTop为A_DEFAULT，表示当前节点是一个default节点。
  将seendefault标记为1，表示已经遇到了一个default。
  使用scan函数跳过default关键字。
  */
	if (Token.token == T_DEFAULT) {
	  ASTop = A_DEFAULT;
	  seendefault = 1;
	  scan(&Token);
	} else {
	  ASTop = A_CASE;
	  scan(&Token);
    /*解析case关键字后的表达式，将结果存储在left中。*/
	  left = binexpr(0);

	  // Ensure the case value is an integer literal
	  if (left->op != A_INTLIT)
	    fatal("Expecting integer literal for case value");
	  casevalue = left->a_intvalue;

	  // Walk the list of existing case values to ensure
	  // that there isn't a duplicate case value
    /*遍历已存在的case值列表（casetree链表），确保没有重复的case值。如果发现重复，发出致命错误。*/
	  for (c = casetree; c != NULL; c = c->right)
	    if (casevalue == c->a_intvalue)
	      fatal("Duplicate case value");
	}

	// Scan the ':' and increment the casecount
	match(T_COLON, ":");
	casecount++;

	// If the next token is a T_CASE, the existing case will fall
	// into the next case. Otherwise, parse the case body.
  /*body = NULL;：将body设置为NULL，表示当前case子句将会落入到下一个case中。*/
	if (Token.token == T_CASE)
	  body = NULL;
	else
	  body = compound_statement(1);

	// Build a sub-tree with any compound statement as the left child
	// and link it in to the growing A_CASE tree
  /*
  如果casetree为空，表示这是第一个case子句，那么构建一个新的AST节点，并将其作为casetree和casetail。
  如果casetree不为空，表示已经有之前的case子句，那么构建一个新的AST节点，并将其连接到casetail的右子树，然后更新casetail为新的节点。
  */
	if (casetree == NULL) {
	  casetree = casetail =
	    mkastunary(ASTop, P_NONE, NULL, body, NULL, casevalue);
	} else {
	  casetail->right =
	    mkastunary(ASTop, P_NONE, NULL, body, NULL, casevalue);
	  casetail = casetail->right;
	}
	break;
      default:
	fatals("Unexpected token in switch", Token.tokstr);
    }
  }
  Switchlevel--;

  // We have a sub-tree with the cases and any default. Put the
  // case count into the A_SWITCH node and attach the case tree.
  /*这段代码用于完成解析switch语句时构建的AST树，
  将case的数量和case子树连接到A_SWITCH节点上，并进行最后的收尾。*/
  n->a_intvalue = casecount;
  n->right = casetree;
  rbrace();

  return (n);
}

// Parse a single statement and return its AST.
/*这段代码是用于解析单个语句并返回其 AST*/
static struct ASTnode *single_statement(void) {
  struct ASTnode *stmt;
  struct symtable *ctype;
  int linenum = Line;

  switch (Token.token) {
    case T_SEMI:
      // An empty statement
      semi();
      break;
    case T_LBRACE:
      /*如果当前标记是左花括号 {，表示复合语句，调用 compound_statement(0); 解析复合语句。*/
      // We have a '{', so this is a compound statement
      lbrace();
      stmt = compound_statement(0);
      stmt->linenum = linenum;
      rbrace();
      return (stmt);
    case T_IDENT:
      // We have to see if the identifier matches a typedef.
      // If not, treat it as an expression.
      // Otherwise, fall down to the parse_type() call.
      /*如果当前标记是标识符，检查是否匹配 typedef。如果不是 typedef，则将其视为表达式并调用 binexpr(0); 进行解析。*/
      if (findtypedef(Text) == NULL) {
	stmt = binexpr(0);/*解析这段代码进行二元运算*/
	stmt->linenum = linenum;
	semi();
	return (stmt);
      }
    case T_CHAR:
    case T_INT:
    case T_LONG:
    case T_STRUCT:
    case T_UNION:
    case T_ENUM:
    case T_TYPEDEF:
      // The beginning of a variable declaration list.
      declaration_list(&ctype, C_LOCAL, T_SEMI, T_EOF, &stmt);/*作定义，此时定义的字符是本地字符，不是全局字符*/
      semi();
      return (stmt);		// Any assignments from the declarations
    case T_IF:
      stmt = if_statement();
      stmt->linenum = linenum;
      return (stmt);
    case T_WHILE:
      stmt = while_statement();
      stmt->linenum = linenum;
      return (stmt);
    case T_FOR:
      stmt = for_statement();
      stmt->linenum = linenum;
      return (stmt);
    case T_RETURN:
      stmt = return_statement();
      stmt->linenum = linenum;
      return (stmt);
    case T_BREAK:
      stmt = break_statement();
      stmt->linenum = linenum;
      return (stmt);
    case T_CONTINUE:
      stmt = continue_statement();
      stmt->linenum = linenum;
      return (stmt);
    case T_SWITCH:
      stmt = switch_statement();
      stmt->linenum = linenum;
      return (stmt);
    default:
      // For now, see if this is an expression.
      // This catches assignment statements.
      /*调用binexpr函数解析一个表达式，返回一个表示该表达式的AST节点。这里假定这个表达式是一个语句，例如赋值语句。*/
      stmt = binexpr(0);
      stmt->linenum = linenum;
      semi();
      return (stmt);
  }
  return (NULL);		// Keep -Wall happy
}

// Parse a compound statement
// and return its AST. If inswitch is true,
// we look for a '}', 'case' or 'default' token
// to end the parsing. Otherwise, look for
// just a '}' to end the parsing.
/*int inswitch 是一个布尔值，表示当前是否处于switch语句中。
如果 inswitch 为真，函数会在查找 case、default 或 } 时结束解析，否则只在找到 } 时结束解析。*/
//解析{   }
struct ASTnode *compound_statement(int inswitch) {
  struct ASTnode *left = NULL;
  struct ASTnode *tree;

  while (1) {
    // Leave if we've hit the end token. We do this first to allow
    // an empty compound statement
    if (Token.token == T_RBRACE)//右括号代表结束
      return (left);
    /*如果 inswitch 为真，同时当前标记是 case 或 default，也表示复合语句结束，函数返回已解析的语法树。*/
    if (inswitch && (Token.token == T_CASE || Token.token == T_DEFAULT))
      return (left);

    // Parse a single statement
    /*如果tree为空则不会加入树中*/
    tree = single_statement();//对每一句代码进行装入ASTnode中

    // For each new tree, either save it in left
    // if left is empty, or glue the left and the
    // new tree together
    /*生成所需代码*/
    if (tree != NULL) {
      if (left == NULL)
	left = tree;
      else
	left = mkastnode(A_GLUE, P_NONE, NULL, left, NULL, tree, NULL, 0);
    }
  }
  return (NULL);		// Keep -Wall happy
}
