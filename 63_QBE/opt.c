#include "defs.h"
#include "data.h"
#include "decl.h"

// AST Tree Optimisation Code
// Copyright (c) 2019 Warren Toomey, GPL3

// Fold an AST tree with a binary operator
// and two A_INTLIT children. Return either 
// the original tree or a new leaf node.
/*对常数进行运算*/
static struct ASTnode *fold2(struct ASTnode *n) {
  int val, leftval, rightval;

  // Get the values from each child
  leftval = n->left->a_intvalue;
  rightval = n->right->a_intvalue;

  // Perform some of the binary operations.
  // For any AST op we can't do, return
  // the original tree.
  switch (n->op) {
    case A_ADD:
      val = leftval + rightval;
      break;
    case A_SUBTRACT:
      val = leftval - rightval;
      break;
    case A_MULTIPLY:
      val = leftval * rightval;
      break;
    case A_DIVIDE:
      // Don't try to divide by zero.
      if (rightval == 0)
	return (n);
      val = leftval / rightval;
      break;
    default:
      return (n);
  }

  // Return a leaf node with the new value
  /*返回一个整数字面值的字符*/
  return (mkastleaf(A_INTLIT, n->type, NULL, NULL, val));
}

// Fold an AST tree with a unary operator
// and one INTLIT children. Return either 
// the original tree or a new leaf node.
static struct ASTnode *fold1(struct ASTnode *n) {
  int val;

  // Get the child value. Do the
  // operation if recognised.
  // Return the new leaf node.
  val = n->left->a_intvalue;
  switch (n->op) {
    case A_WIDEN:
      break;
    case A_INVERT:
      val = ~val;
      break;
    case A_LOGNOT:
      val = !val;
      break;
    default:
      return (n);
  }

  // Return a leaf node with the new value
  return (mkastleaf(A_INTLIT, n->type, NULL, NULL, val));
}

// Attempt to do constant folding on
// the AST tree with the root node n
/* 这是一个递归函数，它对给定的 AST 树进行常量折叠。首先，它检查节点是否为 NULL，如果是，则直接返回。
然后，它递归地对左子树和右子树调用自身。最后，如果左子树和右子树都是整数字面量（A_INTLIT），则调用 fold2 或 fold1 进行折叠。*/
static struct ASTnode *fold(struct ASTnode *n) {

  if (n == NULL)
    return (NULL);

  // Fold on the left child, then
  // do the same on the right child
  n->left = fold(n->left);
  n->right = fold(n->right);

  // If both children are A_INTLITs, do a fold2()
  if (n->left && n->left->op == A_INTLIT) {
    if (n->right && n->right->op == A_INTLIT)
      n = fold2(n);
    else
      // If only the left is A_INTLIT, do a fold1()
      n = fold1(n);
  }
  // Return the possibly modified tree
  return (n);
}

// Optimise an AST tree by
// constant folding in all sub-trees
/*它通过调用 fold 函数对 AST 树进行常量折叠（constant folding）。
常量折叠是一种编译器优化技术，旨在在编译时计算常量表达式的值，从而减少运行时的计算开销。*/
struct ASTnode *optimise(struct ASTnode *n) {
  n = fold(n);
  return (n);
}
