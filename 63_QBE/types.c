#include "defs.h"
#include "data.h"
#include "decl.h"

// Types and type handling
// Copyright (c) 2019 Warren Toomey, GPL3

// Return true if a type is an int type
// of any size, false otherwise
/*该函数用于判断给定的类型是否是整数类型。*/
int inttype(int type) {
  return (((type & 0xf) == 0) && (type >= P_CHAR && type <= P_LONG));
}

// Return true if a type is of pointer type
//返回变量的类型
int ptrtype(int type) {
  return ((type & 0xf) != 0);
}

// Given a primitive type, return
// the type which is a pointer to it
/*其作用是给定一个基本类型，返回一个指向该基本类型的指针类型。*/
int pointer_to(int type) {
  if ((type & 0xf) == 0xf)
    fatald("Unrecognised in pointer_to: type", type);
  return (type + 1);
}

// Given a primitive pointer type, return
// the type which it points to
/*这函数的作用是给定一个原始的指针类型，返回它所指向的类型。*/
int value_at(int type) {
  if ((type & 0xf) == 0x0)
    fatald("Unrecognised in value_at: type", type);
  return (type - 1);
}

// Given a type and a composite type pointer, return
// the size of this type in bytes
int typesize(int type, struct symtable *ctype) {
  if (type == P_STRUCT || type == P_UNION)
    /*这里检查 type 是否为结构体（P_STRUCT）或联合体（P_UNION）。
    如果是的话，函数直接返回 ctype 中存储的复合类型的大小（ctype->size）。这是因为结构体和联合体的大小由其成员的大小累加而来。*/
    return (ctype->size);
  return (genprimsize(type));
}

// Given an AST tree and a type which we want it to become,
// possibly modify the tree by widening or scaling so that
// it is compatible with this type. Return the original tree
// if no changes occurred, a modified tree, or NULL if the
// tree is not compatible with the given type.
// If this will be part of a binary operation, the AST op is not zero.
/*确保左的类型相同*/
struct ASTnode *modify_type(struct ASTnode *tree, int rtype,
			    struct symtable *rctype, int op) {
  int ltype;
  int lsize, rsize;

  ltype = tree->type;
  /*对于逻辑运算符 A_LOGOR 和 A_LOGAND，左右操作数必须是整数或指针类型，*/
  // For A_LOGOR and A_LOGAND, both types have to be int or pointer types
  if (op == A_LOGOR || op == A_LOGAND) {
    /*// 如果左操作数不是整数类型且不是指针类型，则返回 NULL*/
    if (!inttype(ltype) && !ptrtype(ltype))
      return (NULL);
    /*// 如果右操作数不是整数类型且不是指针类型，则返回 NULL*/
    if (!inttype(rtype) && !ptrtype(rtype))
      return (NULL);
    return (tree);
  }
  // XXX No idea on these yet
  /*// 如果左操作数的类型是结构体或联合体，或者右操作数的类型是结构体或联合体*/
  if (ltype == P_STRUCT || ltype == P_UNION)
    fatal("Don't know how to do this yet");
  if (rtype == P_STRUCT || rtype == P_UNION)
    fatal("Don't know how to do this yet");

  // Compare scalar int types
  if (inttype(ltype) && inttype(rtype)) {

    // Both types same, nothing to do
    if (ltype == rtype)
      return (tree);

    // Get the sizes for each type
    lsize = typesize(ltype, NULL);
    rsize = typesize(rtype, NULL);

    // The tree's type size is too big and we can't narrow
    if (lsize > rsize)
      return (NULL);

    // Widen to the right
    if (rsize > lsize)
      return (mkastunary(A_WIDEN, rtype, NULL, tree, NULL, 0));
  }
  // For pointers
  if (ptrtype(ltype) && ptrtype(rtype)) {
    // We can compare them
    if (op >= A_EQ && op <= A_GE)
      return (tree);

    // A comparison of the same type for a non-binary operation is OK,
    // or when the left tree is of  `void *` type.
    if (op == 0 && (ltype == rtype || ltype == pointer_to(P_VOID)))
      return (tree);
  }
  // We can scale only on add and subtract operations
  if (op == A_ADD || op == A_SUBTRACT || op == A_ASPLUS || op == A_ASMINUS) {

    // Left is int type, right is pointer type and the size
    // of the original type is >1: scale the left
    if (inttype(ltype) && ptrtype(rtype)) {
      rsize = genprimsize(value_at(rtype));
      if (rsize > 1)
	return (mkastunary(A_SCALE, rtype, rctype, tree, NULL, rsize));/*它将返回一个包含 A_SCALE 操作的新 AST 节点，该节点表示对左操作数进行缩放，并且缩放因子为右操作数指向的数据类型的大小。*/
      else
	// No need to scale, but we need to widen to pointer size
	return (mkastunary(A_WIDEN, rtype, NULL, tree, NULL, 0));
    }
  }
  // If we get here, the types are not compatible
  return (NULL);
}
