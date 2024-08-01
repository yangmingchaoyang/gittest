#include "defs.h"
#include "data.h"
#include "decl.h"

// Symbol table functions
// Copyright (c) 2019 Warren Toomey, GPL3

// Append a node to the singly-linked list pointed to by head or tail
//将结构体加入链表中
void appendsym(struct symtable **head, struct symtable **tail,
	       struct symtable *node) {

  // Check for valid pointers
  if (head == NULL || tail == NULL || node == NULL)
    fatal("Either head, tail or node is NULL in appendsym");

  // Append to the list
  if (*tail) {
    (*tail)->next = node;
    *tail = node;
  } else
    *head = *tail = node;
  node->next = NULL;
}

// Create a symbol node to be added to a symbol table list.
// Set up the node's:
// + type: char, int etc.
// + ctype: composite type pointer for struct/union
// + structural type: var, function, array etc.
// + size: number of elements, or endlabel: end label for a function
// + posn: Position information for local symbols
// Return a pointer to the new node
//这个函数名为 newsym，用于创建一个新的符号表节点（symbol node），该节点可以被添加到符号表链表中。
/*
char *name: 字符串，表示符号的名称（标识符）。可以为 NULL，表示匿名符号。
int type: 整数，表示符号的基本类型。可能是 char、int 等。
struct symtable *ctype: 符号的复合类型指针，用于表示结构体或联合体的成员。
int stype: 整数，表示符号的结构类型。例如，变量、函数、数组等。
int class: 整数，表示符号的存储类别。例如，全局、静态、局部等。
int nelems: 整数，表示符号的元素数量。对于数组来说，表示数组的大小。
int posn: 整数，表示符号的位置信息。通常在局部符号表中，用于记录局部变量的位置。
*/
//newsym(name, P_STRUCT, NULL, 0, C_STRUCT, 0, 0);
struct symtable *newsym(char *name, int type, struct symtable *ctype,
			int stype, int class, int nelems, int posn) {

  // Get a new node
  struct symtable *node = (struct symtable *) malloc(sizeof(struct symtable));
  if (node == NULL)
    fatal("Unable to malloc a symbol table node in newsym");

  // Fill in the values
  if (name == NULL)
    node->name = NULL;
  else
    node->name = strdup(name);
  node->type = type;
  node->ctype = ctype;
  node->stype = stype;
  node->class = class;
  node->nelems = nelems;

  // For pointers and integer types, set the size
  // of the symbol. structs and union declarations
  // manually set this up themselves.
  /* 使用 ptrtype 和 inttype 函数检查符号的类型是否是指针类型或整数类型。如果是指针类型或整数类型，*/
  if (ptrtype(type) || inttype(type))
    node->size = nelems * typesize(type, ctype);

  node->st_posn = posn;
  node->next = NULL;
  node->member = NULL;
  node->initlist = NULL;
  return (node);
}

// Add a symbol to the global symbol list
struct symtable *addglob(char *name, int type, struct symtable *ctype,
			 int stype, int class, int nelems, int posn) {
  struct symtable *sym =
    newsym(name, type, ctype, stype, class, nelems, posn);
  // For structs and unions, copy the size from the type node
  if (type == P_STRUCT || type == P_UNION)
    sym->size = ctype->size;
  appendsym(&Globhead, &Globtail, sym);
  return (sym);
}

// Add a symbol to the local symbol list
struct symtable *addlocl(char *name, int type, struct symtable *ctype,
			 int stype, int nelems) {
  struct symtable *sym = newsym(name, type, ctype, stype, C_LOCAL, nelems, 0);

  // For structs and unions, copy the size from the type node
  if (type == P_STRUCT || type == P_UNION)
    sym->size = ctype->size;
  appendsym(&Loclhead, &Locltail, sym);
  return (sym);
}

// Add a symbol to the parameter list
struct symtable *addparm(char *name, int type, struct symtable *ctype,
			 int stype) {
  struct symtable *sym = newsym(name, type, ctype, stype, C_PARAM, 1, 0);
  appendsym(&Parmhead, &Parmtail, sym);
  return (sym);
}

// Add a symbol to the temporary member list
struct symtable *addmemb(char *name, int type, struct symtable *ctype,
			 int stype, int nelems) {
  struct symtable *sym =
    newsym(name, type, ctype, stype, C_MEMBER, nelems, 0);

  // For structs and unions, copy the size from the type node
  if (type == P_STRUCT || type == P_UNION)
    sym->size = ctype->size;
  appendsym(&Membhead, &Membtail, sym);
  return (sym);
}

// Add a struct to the struct list
//将一个新的结构体加入结构体链表中
struct symtable *addstruct(char *name) {
  // 创建一个新的结构体符号表节点（struct symtable）
  struct symtable *sym = newsym(name, P_STRUCT, NULL, 0, C_STRUCT, 0, 0);
  appendsym(&Structhead, &Structtail, sym);
  return (sym);
}

// Add a struct to the union list
//将一个新的联合体加入链表中
struct symtable *addunion(char *name) {
  struct symtable *sym = newsym(name, P_UNION, NULL, 0, C_UNION, 0, 0);
  appendsym(&Unionhead, &Uniontail, sym);
  return (sym);
}

// Add an enum type or value to the enum list.
// Class is C_ENUMTYPE or C_ENUMVAL.
// Use posn to store the int value.
struct symtable *addenum(char *name, int class, int value) {
  struct symtable *sym = newsym(name, P_INT, NULL, 0, class, 0, value);
  appendsym(&Enumhead, &Enumtail, sym);
  return (sym);
}

// Add a typedef to the typedef list
struct symtable *addtypedef(char *name, int type, struct symtable *ctype) {
  struct symtable *sym = newsym(name, type, ctype, 0, C_TYPEDEF, 0, 0);
  appendsym(&Typehead, &Typetail, sym);
  return (sym);
}

// Search for a symbol in a specific list.
// Return a pointer to the found node or NULL if not found.
// If class is not zero, also match on the given class
/*
char *s: 这是一个字符串，表示要查找的符号的名称（标识符）。函数将遍历符号表链表，比较每个符号表节点的名称与这个参数是否匹配。
struct symtable *list: 这是一个指向符号表链表的头部的指针。函数将在这个链表中查找符号。
int class: 这是一个整数，表示符号表节点的存储类别。如果 class 不为零，
函数将在查找时同时匹配给定的存储类别。如果 class 为零，函数将忽略存储类别，仅匹配符号名称。
*/
static struct symtable *findsyminlist(char *s, struct symtable *list,
				      int class) {
  for (; list != NULL; list = list->next)
    if ((list->name != NULL) && !strcmp(s, list->name))
      if (class == 0 || class == list->class)
	      return (list);
  return (NULL);
}

// Determine if the symbol s is in the global symbol table.
// Return a pointer to the found node or NULL if not found.
struct symtable *findglob(char *s) {
  return (findsyminlist(s, Globhead, 0));
}

// Determine if the symbol s is in the local symbol table.
// Return a pointer to the found node or NULL if not found.
/*
这段代码定义了一个函数 findlocl，其作用是在局部符号表（Local Symbol Table）中查找符号（变量或参数）。
函数首先检查是否在函数体内，如果是，则尝试在当前函数的参数列表中查找符号。
如果在参数列表中找到了符号，则返回对应的符号表节点；否则，继续在整个局部符号表中查找。
*/
struct symtable *findlocl(char *s) {
  struct symtable *node;

  // Look for a parameter if we are in a function's body
  if (Functionid) {
    node = findsyminlist(s, Functionid->member, 0);
    if (node)
      return (node);
  }
  return (findsyminlist(s, Loclhead, 0));
}

// Determine if the symbol s is in the symbol table.
// Return a pointer to the found node or NULL if not found.
/*该函数会根据作用域的不同在不同的符号表中查找符号，首先在函数参数列表中查找，然后在本地符号表，最后在全局符号表。*/
struct symtable *findsymbol(char *s) {
  struct symtable *node;

  // Look for a parameter if we are in a function's body
  if (Functionid) {
    node = findsyminlist(s, Functionid->member, 0);
    if (node)
      return (node);
  }
  // Otherwise, try the local and global symbol lists
  node = findsyminlist(s, Loclhead, 0);
  if (node)
    return (node);
  return (findsyminlist(s, Globhead, 0));
}

// Find a member in the member list
// Return a pointer to the found node or NULL if not found.

struct symtable *findmember(char *s) {
  return (findsyminlist(s, Membhead, 0));
}

// Find a struct in the struct list
// Return a pointer to the found node or NULL if not found.
/*在结构体列表中找当前结构体*/
struct symtable *findstruct(char *s) {
  return (findsyminlist(s, Structhead, 0));
}

// Find a struct in the union list
// Return a pointer to the found node or NULL if not found.
/*在联合体列表中找到当前的联合体*/
struct symtable *findunion(char *s) {
  return (findsyminlist(s, Unionhead, 0));
}

// Find an enum type in the enum list
// Return a pointer to the found node or NULL if not found.
struct symtable *findenumtype(char *s) {
  return (findsyminlist(s, Enumhead, C_ENUMTYPE));
}

// Find an enum value in the enum list
// Return a pointer to the found node or NULL if not found.
struct symtable *findenumval(char *s) {
  return (findsyminlist(s, Enumhead, C_ENUMVAL));
}

// Find a type in the tyedef list
// Return a pointer to the found node or NULL if not found.
/*findtypedef 函数的作用是在 typedef 列表中查找指定名称的类型，并返回找到的类型的符号表节点指针。如果找不到匹配的类型，返回 NULL。*/
struct symtable *findtypedef(char *s) {
  return (findsyminlist(s, Typehead, 0));
}

// Reset the contents of the symbol table
//清空符号表
void clear_symtable(void) {
  Globhead = Globtail = NULL;
  Loclhead = Locltail = NULL;
  Parmhead = Parmtail = NULL;
  Membhead = Membtail = NULL;
  Structhead = Structtail = NULL;
  Unionhead = Uniontail = NULL;
  Enumhead = Enumtail = NULL;
  Typehead = Typetail = NULL;
}

// Clear all the entries in the local symbol table
void freeloclsyms(void) {
  Loclhead = Locltail = NULL;
  Parmhead = Parmtail = NULL;
  Functionid = NULL;
}

// Remove all static symbols from the global symbol table
void freestaticsyms(void) {
  // g points at current node, prev at the previous one
  struct symtable *g, *prev = NULL;

  // Walk the global table looking for static entries
  for (g = Globhead; g != NULL; g = g->next) {
    if (g->class == C_STATIC) {/*找到符号表中具有 C_STATIC 类别的条目，并将其从链表中删除。*/

      // If there's a previous node, rearrange the prev pointer
      // to skip over the current node. If not, g is the head,
      // so do the same to Globhead
      /* 如果存在前一个节点，将前一个节点的 next 指针指向当前节点的下一个节点，从而绕过当前节点。*/
      if (prev != NULL)
	prev->next = g->next;
      else
	Globhead->next = g->next;

      // If g is the tail, point Globtail at the previous node
      // (if there is one), or Globhead
      if (g == Globtail) {
	if (prev != NULL)
	  Globtail = prev;
	else
	  Globtail = Globhead;
      }
    }
  }

  // Point prev at g before we move up to the next node
  //prev是联系前一个节点。
  prev = g;
}

// Dump a single symbol
static void dumpsym(struct symtable *sym, int indent) {
  int i;

  for (i = 0; i < indent; i++)
    printf(" ");
  switch (sym->type & (~0xf)) {
    case P_VOID:
      printf("void ");
      break;
    case P_CHAR:
      printf("char ");
      break;
    case P_INT:
      printf("int ");
      break;
    case P_LONG:
      printf("long ");
      break;
    case P_STRUCT:
      if (sym->ctype != NULL)
	printf("struct %s ", sym->ctype->name);
      else
	printf("struct %s ", sym->name);
      break;
    case P_UNION:
      if (sym->ctype != NULL)
	printf("union %s ", sym->ctype->name);
      else
	printf("union %s ", sym->name);
      break;
    default:
      printf("unknown type ");
  }

  for (i = 0; i < (sym->type & 0xf); i++)
    printf("*");
  printf("%s", sym->name);

  switch (sym->stype) {
    case S_VARIABLE:
      break;
    case S_FUNCTION:
      printf("()");
      break;
    case S_ARRAY:
      printf("[]");
      break;
    default:
      printf(" unknown stype");
  }

  switch (sym->class) {
    case C_GLOBAL:
      printf(": global");
      break;
    case C_LOCAL:
      printf(": local");
      break;
    case C_PARAM:
      printf(": param");
      break;
    case C_EXTERN:
      printf(": extern");
      break;
    case C_STATIC:
      printf(": static");
      break;
    case C_STRUCT:
      printf(": struct");
      break;
    case C_UNION:
      printf(": union");
      break;
    case C_MEMBER:
      printf(": member");
      break;
    case C_ENUMTYPE:
      printf(": enumtype");
      break;
    case C_ENUMVAL:
      printf(": enumval");
      break;
    case C_TYPEDEF:
      printf(": typedef");
      break;
    default:
      printf(": unknown class");
  }

  switch (sym->stype) {
    case S_VARIABLE:
      if (sym->class == C_ENUMVAL)
	printf(", value %d\n", sym->st_posn);
      else
	printf(", size %d\n", sym->size);
      break;
    case S_FUNCTION:
      printf(", %d params\n", sym->nelems);
      break;
    case S_ARRAY:
      printf(", %d elems, size %d\n", sym->nelems, sym->size);
      break;
  }

  switch (sym->type & (~0xf)) {
    case P_STRUCT:
    case P_UNION:
      dumptable(sym->member, NULL, 4);
  }

  switch (sym->stype) {
    case S_FUNCTION:
      dumptable(sym->member, NULL, 4);
  }
}

// Dump one symbol table
void dumptable(struct symtable *head, char *name, int indent) {
  struct symtable *sym;

  if (head != NULL && name != NULL)
    printf("%s\n--------\n", name);
  for (sym = head; sym != NULL; sym = sym->next)
    dumpsym(sym, indent);
}

/*打印符号表*/
void dumpsymtables(void) {
  dumptable(Globhead, "Global", 0);
  printf("\n");
  dumptable(Enumhead, "Enums", 0);
  printf("\n");
  dumptable(Typehead, "Typedefs", 0);
}
