#include "defs.h"
#include "data.h"
#include "decl.h"

// Code generator for x86-64 using the QBE intermediate language.
// Copyright (c) 2019 Warren Toomey, GPL3

// Switch to the text segment
/*将代码生成切换到文本段。*/
void cgtextseg() {
}

// Switch to the data segment
/*将代码生成切换到数据段。*/
void cgdataseg() {
}

// Given a scalar type value, return the
// character that matches the QBE type.
// Because chars are stored on the stack,
// we can return 'w' for P_CHAR.
/*这段代码是一个函数，根据给定的标量类型值（type参数），返回与QBE类型匹配的字符。*/
char cgqbetype(int type) {
  if (ptrtype(type))
    return ('l');
  switch (type) {
    case P_VOID:
      return (' ');
    case P_CHAR:
      return ('w');
    case P_INT:
      return ('w');
    case P_LONG:
      return ('l');
    default:
      fatald("Bad type in cgqbetype:", type);
  }
  return (0);			// Keep -Wall happy
}

// Given a scalar type value, return the
// size of the QBE type in bytes.
//返回类型大小
int cgprimsize(int type) {
  if (ptrtype(type))
    return (8);
  switch (type) {
    case P_CHAR:
      return (1);
    case P_INT:
      return (4);
    case P_LONG:
      return (8);
    default:
      fatald("Bad type in cgprimsize:", type);
  }
  return (0);			// Keep -Wall happy
}

// Given a scalar type, an existing memory offset
// (which hasn't been allocated to anything yet)
// and a direction (1 is up, -1 is down), calculate
// and return a suitably aligned memory offset
// for this scalar type. This could be the original
// offset, or it could be above/below the original
/*这段代码的作用是根据给定的标量类型（scalar type）、内存偏移（尚未分配给任何内容），
以及一个方向（1 表示向上，-1 表示向下），计算并返回一个适当对齐的内存偏移。*/
/*函数接受三个参数，分别是标量类型 type、内存偏移 offset 和方向 direction。*/
int cgalign(int type, int offset, int direction) {
  int alignment;

  // We don't need to do this on x86-64, but let's
  // align chars on any offset and align ints/pointers
  // on a 4-byte alignment
  switch (type) {
    case P_CHAR:
      break;
    default:
      // Align whatever we have now on a 4-byte alignment.
      // I put the generic code here so it can be reused elsewhere.
      alignment = 4;
      /*计算新的对齐偏移。该表达式的含义是：其目的是为了与4对其*/
      offset = (offset + direction * (alignment - 1)) & ~(alignment - 1);
  }
  return (offset);
}

// Allocate a QBE temporary
/*这个函数的作用是生成QBE编译器所需的唯一临时变量标识符。*/
static int nexttemp = 0;
int cgalloctemp(void) {
  return (++nexttemp);
}

// Print out the assembly preamble
// for one output file
/*用于输出汇编文件的前导部分。*/
void cgpreamble(char *filename) {
}

// Nothing to do for the end of a file
/*用于输出汇编文件的后导部分。*/
void cgpostamble() {
}

// Boolean flag: has there been a switch statement
// in this function yet?
static int used_switch;

// Print out a function preamble
/*这个函数用于生成QBE汇编代码，打印函数的前导部分（preamble）。
函数的前导通常包括导出声明、函数名称、参数声明、函数起始标签以及一些初始化工作。*/
void cgfuncpreamble(struct symtable *sym) {
  char *name = sym->name;// 获取函数名
  struct symtable *parm, *locvar;// 声明参数和局部变量的符号表项
  int size, bigsize;// 用于处理参数和局部变量的大小
  int label;// 函数标签

  // Output the function's name and return type
  if (sym->class == C_GLOBAL)
    fprintf(Outfile, "export ");// 如果函数是全局的，输出导出声明
  /*// 输出函数声明，包括返回类型、函数名等
  // 使用 cgqbetype 函数获取 QBE 类型表示（字符）并输出返回类型// 输出函数名*/
  fprintf(Outfile, "function %c $%s(", cgqbetype(sym->type), name);

  // Output the parameter names and types. For any parameters which
  // need addresses, change their name as we copy their value below
  /*使用循环遍历函数的参数列表（sym->member），其中 parm 是指向参数符号表项的指针。*/
  for (parm = sym->member; parm != NULL; parm = parm->next) {
    /*对于需要地址的参数（parm->st_hasaddr == 1），输出参数类型和名称，并在名称前添加 %.p。*/
    if (parm->st_hasaddr == 1)
      fprintf(Outfile, "%c %%.p%s, ", cgqbetype(parm->type), parm->name);
    else
      fprintf(Outfile, "%c %%%s, ", cgqbetype(parm->type), parm->name);
  }
  /*在循环结束后，输出右括号和函数体的开始大括号 {。fprintf(Outfile, ") {\n");*/
  fprintf(Outfile, ") {\n");

  // Get a label for the function start
  /*使用 genlabel 生成一个函数起始的标签。*/
  label = genlabel();
  cglabel(label);

  // For any parameters which need addresses, allocate memory
  // on the stack for them. QBE won't let us do alloc1, so
  // we allocate 4 bytes for chars. Copy the value from the
  // parameter to the new memory location.
  // of the parameter
  /*使用循环遍历函数的参数列表（sym->member），其中 parm 是指向参数符号表项的指针。*/
  for (parm = sym->member; parm != NULL; parm = parm->next) {
    if (parm->st_hasaddr == 1) {
      size = cgprimsize(parm->type);
      bigsize = (size == 1) ? 4 : size;
      fprintf(Outfile, "  %%%s =l alloc%d 1\n", parm->name, bigsize);

      // Copy to the allocated memory
      /*
      根据参数大小选择合适的 store 命令将参数的值复制到新分配的内存位置。
      对于1字节的参数，使用 storeb。
      对于4字节的参数，使用 storew。
      对于8字节的参数，使用 storel。
      */
      switch (size) {
	case 1:
	  fprintf(Outfile, "  storeb %%.p%s, %%%s\n", parm->name, parm->name);
	  break;
	case 4:
	  fprintf(Outfile, "  storew %%.p%s, %%%s\n", parm->name, parm->name);
	  break;
	case 8:
	  fprintf(Outfile, "  storel %%.p%s, %%%s\n", parm->name, parm->name);
      }
    }
  }

  // Allocate memory for any local variables that need to be on the
  // stack. There are two reasons for this. The first is for locals
  // where their address is used. The second is for char variables
  // We need to do this as QBE can only truncate down to 8 bits
  // for locations in memory
  /*// 为需要在栈上分配内存的局部变量分配内存。有两个原因需要这样做。第一个是用于地址引用的本地变量。
// 第二个是用于字符变量。我们需要这样做是因为 QBE 只能将位置内存截断到 8 位，以确保它们在 8 字节边界上对齐。*/
  for (locvar = Loclhead; locvar != NULL; locvar = locvar->next) {
    if (locvar->st_hasaddr == 1) {
      // Get the total size for all elements (if an array).
      // Round up to the nearest multiple of 8, to ensure that
      // pointers are aligned on 8-byte boundaries
      /*
       // 对于需要使用地址的本地变量，计算以字节为单位的总大小（考虑数组），
       // 并四舍五入到最接近的 8 的倍数，以确保在 8 字节边界上对齐。
      */
      size = locvar->size * locvar->nelems;
      size = (size + 7) >> 3;
      fprintf(Outfile, "  %%%s =l alloc8 %d\n", locvar->name, size);
    } else if (locvar->type == P_CHAR) {
      /*
      // 对于字符变量，在栈上分配 4 字节的内存，
      // 并将 st_hasaddr 设置为 1，表示已分配地址。
      */
      locvar->st_hasaddr = 1;
      fprintf(Outfile, "  %%%s =l alloc4 1\n", locvar->name);
    }
  }

  used_switch = 0;		// We haven't output the switch handling code yet
}

// Print out a function postamble
/*这个函数的作用是输出函数的结束标签以及根据函数类型是否为 void 输出相应的返回指令。*/
void cgfuncpostamble(struct symtable *sym) {
   // 输出函数结束的标签
  cglabel(sym->st_endlabel);

  // Return a value if the function's type isn't void
   // 如果函数的类型不是 void，则返回一个值
  if (sym->type != P_VOID)
    fprintf(Outfile, "  ret %%.ret\n}\n");
  else
    fprintf(Outfile, "  ret\n}\n");
}

// Load an integer literal value into a temporary.
// Return the number of the temporary.
//这段代码用于将整数字面值加载到一个临时变量中，并返回该临时变量的编号。
int cgloadint(int value, int type) {
  // Get a new temporary
  // 获取一个新的临时变量
  int t = cgalloctemp();//将字面值放入一个寄存器中，然后堆这个寄存器进行操作。

  fprintf(Outfile, "  %%.t%d =%c copy %d\n", t, cgqbetype(type), value);
  return (t);
}

// Load a value from a variable into a temporary.
// Return the number of the temporary. If the
// operation is pre- or post-increment/decrement,
// also perform this action.
/*这段代码是为了生成汇编代码，从变量（或数组元素）中加载值到一个新的临时寄存器中，
并且如果有前缀或后缀的增减操作，也会进行相应的计算。*/

int cgloadvar(struct symtable *sym, int op) {
  int r, posttemp, offset = 1;
  char qbeprefix;

  // Get a new temporary
  //获取新的临时寄存器
  r = cgalloctemp();

  // If the symbol is a pointer, use the size
  // of the type that it points to as any
  // increment or decrement. If not, it's one.
  /*计算增减操作的偏移量。如果变量是指针类型，则使用其指向类型的大小；如果是前缀或后缀的减法操作，将偏移量设为负值。*/
  if (ptrtype(sym->type))
    offset = typesize(value_at(sym->type), sym->ctype);

  // Negate the offset for decrements
  if (op == A_PREDEC || op == A_POSTDEC)
    offset = -offset;

  // Get the relevant QBE prefix for the symbol
  /*确定符号的 QBE 前缀，它取决于符号的存储类别（global、static、extern 使用 $，其他情况使用 %）。*/
  qbeprefix = ((sym->class == C_GLOBAL) || (sym->class == C_STATIC) ||
	       (sym->class == C_EXTERN)) ? '$' : '%';

  // If we have a pre-operation
  /*如果是前缀增减操作，生成相应的汇编代码，包括加载变量值、执行增减操作、存储回变量。*/
  //这段操作是为什么利用++或者--来改变地址或者改变值的大小
  if (op == A_PREINC || op == A_PREDEC) {
    /*如果符号具有地址或者是全局/静态/外部变量（使用$作为前缀），则执行前缀增减的代码。否则，执行普通的赋值语句。*/
    if (sym->st_hasaddr || qbeprefix == '$') {
      // Get a new temporary
      posttemp = cgalloctemp();
      /*
      在前缀增减操作中，首先为存储结果的临时寄存器分配一个新的编号（posttemp）。
      根据变量的大小生成不同的加载指令（loadub、loadsw、loadl），加载变量的值到新的临时寄存器。
      执行加法操作，将偏移量加到加载的值上。
      将结果存储回变量。
      */
      switch (sym->size) {
	case 1:
	  fprintf(Outfile, "  %%.t%d =w loadub %c%s\n", posttemp, qbeprefix,
		  sym->name);
	  fprintf(Outfile, "  %%.t%d =w add %%.t%d, %d\n", posttemp, posttemp,
		  offset);
	  fprintf(Outfile, "  storeb %%.t%d, %c%s\n", posttemp, qbeprefix,
		  sym->name);
	  break;
	case 4:
	  fprintf(Outfile, "  %%.t%d =w loadsw %c%s\n", posttemp, qbeprefix,
		  sym->name);
	  fprintf(Outfile, "  %%.t%d =w add %%.t%d, %d\n", posttemp, posttemp,
		  offset);
	  fprintf(Outfile, "  storew %%.t%d, %c%s\n", posttemp, qbeprefix,
		  sym->name);
	  break;
	case 8:
	  fprintf(Outfile, "  %%.t%d =l loadl %c%s\n", posttemp, qbeprefix,
		  sym->name);
	  fprintf(Outfile, "  %%.t%d =l add %%.t%d, %d\n", posttemp, posttemp,
		  offset);
	  fprintf(Outfile, "  storel %%.t%d, %c%s\n", posttemp, qbeprefix,
		  sym->name);
      }
    } else
      /*将变量的当前值加上或减去偏移量，并将结果存回相同的变量。这就实现了前缀自增和自减的语义。*/
      fprintf(Outfile, "  %c%s =%c add %c%s, %d\n",
	      qbeprefix, sym->name, cgqbetype(sym->type), qbeprefix,
	      sym->name, offset);//使用变量名作为地址名的参数
  }
  // Now load the output temporary with the value
  /*加载变量值到输出的临时寄存器：*/
  if (sym->st_hasaddr || qbeprefix == '$') {
    switch (sym->size) {
      case 1:
	fprintf(Outfile, "  %%.t%d =w loadub %c%s\n", r, qbeprefix,
		sym->name);
	break;
      case 4:
	fprintf(Outfile, "  %%.t%d =w loadsw %c%s\n", r, qbeprefix,
		sym->name);
	break;
      case 8:
	fprintf(Outfile, "  %%.t%d =l loadl %c%s\n", r, qbeprefix, sym->name);
    }
  } else
    fprintf(Outfile, "  %%.t%d =%c copy %c%s\n",
	    r, cgqbetype(sym->type), qbeprefix, sym->name);

  // If we have a post-operation
  if (op == A_POSTINC || op == A_POSTDEC) {
    if (sym->st_hasaddr || qbeprefix == '$') {
      // Get a new temporary
      posttemp = cgalloctemp();
      switch (sym->size) {
	case 1:
	  fprintf(Outfile, "  %%.t%d =w loadub %c%s\n", posttemp, qbeprefix,
		  sym->name);
	  fprintf(Outfile, "  %%.t%d =w add %%.t%d, %d\n", posttemp, posttemp,
		  offset);
	  fprintf(Outfile, "  storeb %%.t%d, %c%s\n", posttemp, qbeprefix,
		  sym->name);
	  break;
	case 4:
	  fprintf(Outfile, "  %%.t%d =w loadsw %c%s\n", posttemp, qbeprefix,
		  sym->name);
	  fprintf(Outfile, "  %%.t%d =w add %%.t%d, %d\n", posttemp, posttemp,
		  offset);
	  fprintf(Outfile, "  storew %%.t%d, %c%s\n", posttemp, qbeprefix,
		  sym->name);
	  break;
	case 8:
	  fprintf(Outfile, "  %%.t%d =l loadl %c%s\n", posttemp, qbeprefix,
		  sym->name);
	  fprintf(Outfile, "  %%.t%d =l add %%.t%d, %d\n", posttemp, posttemp,
		  offset);
	  fprintf(Outfile, "  storel %%.t%d, %c%s\n", posttemp, qbeprefix,
		  sym->name);
      }
    } else
      fprintf(Outfile, "  %c%s =%c add %c%s, %d\n",
	      qbeprefix, sym->name, cgqbetype(sym->type), qbeprefix,
	      sym->name, offset);
  }
  // Return the temporary with the value
  return (r);
}

// Given the label number of a global string,
// load its address into a new temporary
int cgloadglobstr(int label) {
  // Get a new temporary
  int r = cgalloctemp();
  fprintf(Outfile, "  %%.t%d =l copy $L%d\n", r, label);
  return (r);
}

// Add two temporaries together and return
// the number of the temporary with the result
/*两个数在寄存器相加，返回其中一个寄存器*/
int cgadd(int r1, int r2, int type) {
  fprintf(Outfile, "  %%.t%d =%c add %%.t%d, %%.t%d\n",
	  r1, cgqbetype(type), r1, r2);
  return (r1);
}

// Subtract the second temporary from the first and
// return the number of the temporary with the result
/*两个寄存器相减*/
int cgsub(int r1, int r2, int type) {
  fprintf(Outfile, "  %%.t%d =%c sub %%.t%d, %%.t%d\n",
	  r1, cgqbetype(type), r1, r2);
  return (r1);
}

// Multiply two temporaries together and return
// the number of the temporary with the result
/*两个寄存器相乘*/
int cgmul(int r1, int r2, int type) {
  fprintf(Outfile, "  %%.t%d =%c mul %%.t%d, %%.t%d\n",
	  r1, cgqbetype(type), r1, r2);
  return (r1);
}

// Divide or modulo the first temporary by the second and
// return the number of the temporary with the result
/*这段代码是用于生成汇编代码，实现整数的除法或取模操作。*/
int cgdivmod(int r1, int r2, int op, int type) {
  if (op == A_DIVIDE)
    fprintf(Outfile, "  %%.t%d =%c div %%.t%d, %%.t%d\n",
	    r1, cgqbetype(type), r1, r2);
  else
    fprintf(Outfile, "  %%.t%d =%c rem %%.t%d, %%.t%d\n",
	    r1, cgqbetype(type), r1, r2);
  return (r1);
}

// Bitwise AND two temporaries
int cgand(int r1, int r2, int type) {
  fprintf(Outfile, "  %%.t%d =%c and %%.t%d, %%.t%d\n",
	  r1, cgqbetype(type), r1, r2);
  return (r1);
}

// Bitwise OR two temporaries
int cgor(int r1, int r2, int type) {
  fprintf(Outfile, "  %%.t%d =%c or %%.t%d, %%.t%d\n",
	  r1, cgqbetype(type), r1, r2);
  return (r1);
}

// Bitwise XOR two temporaries
int cgxor(int r1, int r2, int type) {
  fprintf(Outfile, "  %%.t%d =%c xor %%.t%d, %%.t%d\n",
	  r1, cgqbetype(type), r1, r2);
  return (r1);
}

// Shift left r1 by r2 bits
/*这函数用于生成汇编代码，实现左移操作。*/
int cgshl(int r1, int r2, int type) {
  fprintf(Outfile, "  %%.t%d =%c shl %%.t%d, %%.t%d\n",
	  r1, cgqbetype(type), r1, r2);
  return (r1);
}

// Shift right r1 by r2 bits
int cgshr(int r1, int r2, int type) {
  fprintf(Outfile, "  %%.t%d =%c shr %%.t%d, %%.t%d\n",
	  r1, cgqbetype(type), r1, r2);
  return (r1);
}

// Negate a temporary's value
int cgnegate(int r, int type) {
  fprintf(Outfile, "  %%.t%d =%c sub 0, %%.t%d\n", r, cgqbetype(type), r);
  return (r);
}

// Invert a temporary's value
int cginvert(int r, int type) {
  fprintf(Outfile, "  %%.t%d =%c xor %%.t%d, -1\n", r, cgqbetype(type), r);
  return (r);
}

// Logically negate a temporary's value
int cglognot(int r, int type) {
  char q = cgqbetype(type);
  fprintf(Outfile, "  %%.t%d =%c ceq%c %%.t%d, 0\n", r, q, q, r);
  return (r);
}

// Load a boolean value (only 0 or 1)
// into the given temporary
//将一个布尔值加载到一个临时寄存器中
void cgloadboolean(int r, int val, int type) {
  fprintf(Outfile, "  %%.t%d =%c copy %d\n", r, cgqbetype(type), val);
}

// Convert an integer value to a boolean value. Jump if
// it's an IF, WHILE, LOGAND or LOGOR operation
/*r 是存储整数值的临时寄存器。
op 是操作类型，可能是 A_IF、A_WHILE、A_LOGAND 或 A_LOGOR。
label 是目标标签，用于跳转。
type 是整数值的类型。*/
int cgboolean(int r, int op, int label, int type) {
  // Get a label for the next instruction
   // 为下一条指令获取一个标签
  int label2 = genlabel();
   // 为比较操作分配一个新的临时寄存器
  // Get a new temporary for the comparison
  int r2 = cgalloctemp();
  // 将临时寄存器的值与零比较，得到布尔值
  // Convert temporary to boolean value
  fprintf(Outfile, "  %%.t%d =l cne%c %%.t%d, 0\n", r2, cgqbetype(type), r);
// 根据操作类型生成相应的条件跳转指令
  switch (op) {
    case A_IF:
    case A_WHILE:
    case A_LOGAND:
      fprintf(Outfile, "  jnz %%.t%d, @L%d, @L%d\n", r2, label2, label);
      break;
    case A_LOGOR:
      fprintf(Outfile, "  jnz %%.t%d, @L%d, @L%d\n", r2, label, label2);
      break;
  }

  // Output the label for the next instruction
  // 输出下一条指令的标签
  cglabel(label2);
  return (r2);
}

// Call a function with the given symbol id.
// Return the temprary with the result
int cgcall(struct symtable *sym, int numargs, int *arglist, int *typelist) {
  int outr;
  int i;

  // Get a new temporary for the return result
  //使用 cgalloctemp 函数获取一个新的临时寄存器，用于存储函数调用的返回结果。
  outr = cgalloctemp();

  // Call the function
  //生成函数调用汇编代码
  if (sym->type == P_VOID)
    fprintf(Outfile, "  call $%s(", sym->name);
  else
    fprintf(Outfile, "  %%.t%d =%c call $%s(", outr, cgqbetype(sym->type),
	    sym->name);

  // Output the list of arguments
  for (i = numargs - 1; i >= 0; i--) {
    fprintf(Outfile, "%c %%.t%d, ", cgqbetype(typelist[i]), arglist[i]);
  }
  fprintf(Outfile, ")\n");

  return (outr);
}

// Shift a temporary left by a constant. As we only
// use this for address calculations, extend the
// type to be a QBE 'l' if required
int cgshlconst(int r, int val, int type) {
  int r2 = cgalloctemp();
  int r3 = cgalloctemp();
  //
  if (cgprimsize(type) < 8) {
    fprintf(Outfile, "  %%.t%d =l extsw %%.t%d\n", r2, r);
    fprintf(Outfile, "  %%.t%d =l shl %%.t%d, %d\n", r3, r2, val);
  } else
    fprintf(Outfile, "  %%.t%d =l shl %%.t%d, %d\n", r3, r, val);
  return (r3);
}

// Store a temporary's value into a global variable
/*这段代码负责将一个临时变量的值存储到一个全局变量中。*/
int cgstorglob(int r, struct symtable *sym) {

  // We can store to bytes in memory
  char q = cgqbetype(sym->type);
  if (sym->type == P_CHAR)
    q = 'b';

  fprintf(Outfile, "  store%c %%.t%d, $%s\n", q, r, sym->name);
  //返回临时寄存器编号： 返回临时寄存器编号 r，这是存储操作的结果。
  return (r);
}

// Store a temporary's value into a local variable
//这段代码实现了将临时变量的值存储到局部变量中的逻辑。
int cgstorlocal(int r, struct symtable *sym) {

  // If the variable is on the stack, use store instructions
  if (sym->st_hasaddr) {
    fprintf(Outfile, "  store%c %%.t%d, %%%s\n",
	    cgqbetype(sym->type), r, sym->name);
  } else {
    fprintf(Outfile, "  %%%s =%c copy %%.t%d\n",
	    sym->name, cgqbetype(sym->type), r);
  }
  return (r);
}

// Generate a global symbol but not functions
/*这段代码是一个用于生成全局符号（global symbol）的函数，但不包括函数（functions）。这个函数主要用于处理程序中的全局变量和全局数组。*/
void cgglobsym(struct symtable *node) {
  int size, type;
  int initvalue;
  int i;

  if (node == NULL)
    return;
  if (node->stype == S_FUNCTION)
    return;

  // Get the size of the variable (or its elements if an array)
  // and the type of the variable
  /*如果节点的类型是数组（S_ARRAY），则获取数组元素的类型和大小。*/
  if (node->stype == S_ARRAY) {
    size = typesize(value_at(node->type), node->ctype);
    type = value_at(node->type);
  } else {
    size = node->size;
    type = node->type;
  }

  // Generate the global identity and the label
  /*生成全局标识和标签
  切换到数据段（cgdataseg()）。
如果变量的类别是全局（C_GLOBAL），则在输出中添加 "export "。
如果变量的类型是结构体或联合体，则生成相应的数据标签和对齐信息。*/
  cgdataseg();
  if (node->class == C_GLOBAL)
    fprintf(Outfile, "export ");
  if ((node->type == P_STRUCT) || (node->type == P_UNION))
    fprintf(Outfile, "data $%s = align 8 { ", node->name);
  else
    fprintf(Outfile, "data $%s = align %d { ", node->name, cgprimsize(type));

  // Output space for one or more elements
  /*使用循环遍历数组的每个元素，为每个元素生成相应的空间和初始值。
  根据元素的大小和类型，生成相应的汇编代码。支持1字节（b）、4字节（w）和8字节（l）大小的元素，以及其他大小的元素（z）。*/
  for (i = 0; i < node->nelems; i++) {

    // Get any initial value
    initvalue = 0;
    if (node->initlist != NULL)
      initvalue = node->initlist[i];

    // Generate the space for this type
    switch (size) {
      case 1:
	fprintf(Outfile, "b %d, ", initvalue);
	break;
      case 4:
	fprintf(Outfile, "w %d, ", initvalue);
	break;
      case 8:
	// Generate the pointer to a string literal. Treat a zero value
	// as actually zero, not the label L0
  /*这段代码是处理大小为8字节的情况，通常用于处理指针类型或者长整型等数据。*/
	if (node->initlist != NULL && type == pointer_to(P_CHAR)
	    && initvalue != 0)/*这个条件是判断是否是指针的条件*/
	  fprintf(Outfile, "l $L%d, ", initvalue);
	else
	  fprintf(Outfile, "l %d, ", initvalue);
	break;
      default:
	fprintf(Outfile, "z %d, ", size);
    }
  }
  fprintf(Outfile, "}\n");
}

// Generate a global string and its label.
// Don't output the label if append is true.
/*，用于生成全局字符串以及与之相关联的标签。
data $L1 = { b 72, b 101, b 108, b 108, b 111, b 44, b 32, b 87, b 111, b 114, b 108, b 100 }
*/
void cgglobstr(int l, char *strvalue, int append) {
  char *cptr;
  if (!append)
    fprintf(Outfile, "data $L%d = { ", l);

  for (cptr = strvalue; *cptr; cptr++) {
    fprintf(Outfile, "b %d, ", *cptr);
  }
}

// NUL terminate a global string
/*在之后追加字符*/
void cgglobstrend(void) {
  fprintf(Outfile, " b 0 }\n");
}

// List of comparison instructions,
// in AST order: A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *cmplist[] = { "ceq", "cne", "cslt", "csgt", "csle", "csge" };

// Compare two temporaries and set if true.
int cgcompare_and_set(int ASTop, int r1, int r2, int type) {
  int r3;
  char q = cgqbetype(type);

  // Check the range of the AST operation
  /*确保 ASTop 的范围在 A_EQ 到 A_GE 之间，以防止出现无效的 AST 操作符。*/
  if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_set()");

  // Get a new temporary for the comparison
  /*获取一个新的临时寄存器 r3 用于存储比较的结果。*/
  r3 = cgalloctemp();
  /*使用 fprintf 生成比较操作的汇编代码，将结果存储在 r3 中。
  比较操作的具体内容由 cmplist[ASTop - A_EQ] 给出，而 q 是由 cgqbetype(type) 计算得到的比较操作的类型。*/
  fprintf(Outfile, "  %%.t%d =%c %s%c %%.t%d, %%.t%d\n",
	  r3, q, cmplist[ASTop - A_EQ], q, r1, r2);
  return (r3);
}

// Generate a label
void cglabel(int l) {
  fprintf(Outfile, "@L%d\n", l);
}

// Generate a jump to a label
void cgjump(int l) {
  fprintf(Outfile, "  jmp @L%d\n", l);
}

// List of inverted jump instructions,
// in AST order: A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *invcmplist[] = { "cne", "ceq", "csge", "csle", "csgt", "cslt" };

// Compare two temporaries and jump if false.
/*
ASTop: 表示比较类型的操作符，期望在特定范围内。
r1, r2: 在比较中使用的临时寄存器索引。
label: 如果比较条件为真，则跳转到的标签。
type: 被比较值的数据类型，这会影响比较操作的方式。
*/
int cgcompare_and_jump(int ASTop, int r1, int r2, int label, int type) {
  int label2;
  int r3;
  //这可能将 type 参数转换为表示比较操作中使用的类型限定符的字符。
  char q = cgqbetype(type);

  // Check the range of the AST operation
  if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_set()");

  // Get a label for the next instruction
  //生成用于条件为假时分支的新标签。
  label2 = genlabel();

  // Get a new temporary for the comparison
  //分配新的临时寄存器以存储比较结果。
  /*
  寄存器和临时变量
在编译器的后端，特别是在生成目标代码的阶段，经常需要使用到寄存器来临时存储计算结果或进行数据的中转。
寄存器是有限的资源，而且在多个计算中可能会频繁地需要存储临时数据。因此，编译器需要有效管理这些寄存器的使用。

功能和应用
在函数 cgcompare_and_jump 中，r3 是用来存储两个寄存器 r1 和 r2 之间比较操作的结果的。这个结果是一个布尔值，指示比较的真假，并将用于决定程序接下来的跳转操作。这里的 cgalloctemp() 函数负责为这个结果分配一个新的临时寄存器标识符。

临时寄存器的重要性
命名冲突避免：保证每个临时寄存器的标识符都是唯一的，从而避免在生成的代码中产生命名冲突。
代码优化：使用临时寄存器可以帮助编译器进行更好的代码优化，如寄存器间的数据移动最小化。
控制流管理：在 cgcompare_and_jump 的上下文中，r3 的值将直接影响程序的控制流（通过条件跳转指令），因此必须保证其操作正确无误。
递增生成策略
通过简单的递增策略（每次调用时加一），cgalloctemp 既简单又高效地为每个新的临时需求生成一个唯一的标识符。
这种策略尽管简单，却非常适合快速编译环境，允许编译器轻松跟踪和管理所有已分配的寄存器。
  */
  r3 = cgalloctemp();

  /*
  第一行执行使用经过 ASTop - A_EQ 调整的操作符的 r1 和 r2 之间的比较。结果存储在 r3 中。
  第二行输出条件跳转指令（jnz），如果 r3 非零（真）则跳转到 label，为零（假）则跳转到 label2。
  */
  fprintf(Outfile, "  %%.t%d =%c %s%c %%.t%d, %%.t%d\n",
	  r3, q, invcmplist[ASTop - A_EQ], q, r1, r2);
  //  %.t1 => cmp> %.t2, %.t3
  fprintf(Outfile, "  jnz %%.t%d, @L%d, @L%d\n", r3, label, label2);
  //    jnz %.t1, @L100, @L200
  cglabel(label2);
  return (NOREG);
}

// Widen the value in the temporary from the old
// to the new type, and return a temporary with
// this new value
/*
这段代码是用于执行类型宽化（Type Widening）操作的函数。
类型宽化是将变量的类型从较小的类型扩展到较大的类型，以适应目标类型的过程。*/
int cgwiden(int r, int oldtype, int newtype) {
  char oldq = cgqbetype(oldtype);
  char newq = cgqbetype(newtype);

  // Get a new temporary
  int t = cgalloctemp();

  switch (oldtype) {
    /*
    如果 oldtype 是 P_CHAR，表示输入值是字符类型，那么使用 extub 指令执行零扩展（zero extension），
    将无符号字节转换为目标类型。生成的 QBE 指令为 " %%.t%d =%c extub %%.t%d\n"。
    对于其他类型，使用 exts 指令执行有符号扩展（sign extension），将符号类型转换为目标类型。
    生成的 QBE 指令为 " %%.t%d =%c exts%c %%.t%d\n"。
    */
    case P_CHAR:
      fprintf(Outfile, "  %%.t%d =%c extub %%.t%d\n", t, newq, r);
      break;
    default:
      fprintf(Outfile, "  %%.t%d =%c exts%c %%.t%d\n", t, newq, oldq, r);
  }
  return (t);
}

// Generate code to return a value from a function
/*
reg: 表示包含返回值的寄存器的编号。NOREG 表示没有返回值。
sym: 函数符号表条目，包含函数的信息
*/
void cgreturn(int reg, struct symtable *sym) {

  // Only return a value if we have a value to return
  if (reg != NOREG)
    fprintf(Outfile, "  %%.ret =%c copy %%.t%d\n", cgqbetype(sym->type), reg);

  cgjump(sym->st_endlabel);
}

// Generate code to load the address of an
// identifier. Return a new temporary
/*取地址的操作*/
int cgaddress(struct symtable *sym) {
  int r = cgalloctemp();
  char qbeprefix = ((sym->class == C_GLOBAL) || (sym->class == C_STATIC) ||
		    (sym->class == C_EXTERN)) ? '$' : '%';

  fprintf(Outfile, "  %%.t%d =l copy %c%s\n", r, qbeprefix, sym->name);
  return (r);
}

// Dereference a pointer to get the value
// it points at into a new temporary
int cgderef(int r, int type) {
  // Get the type that we are pointing to
  int newtype = value_at(type);
  // Now get the size of this type
  int size = cgprimsize(newtype);
  // Get temporary for the return result
  int ret = cgalloctemp();

  switch (size) {
    case 1:
      fprintf(Outfile, "  %%.t%d =w loadub %%.t%d\n", ret, r);
      break;
    case 4:
      fprintf(Outfile, "  %%.t%d =w loadsw %%.t%d\n", ret, r);
      break;
    case 8:
      fprintf(Outfile, "  %%.t%d =l loadl %%.t%d\n", ret, r);
      break;
    default:
      fatald("Can't cgderef on type:", type);
  }
  return (ret);
}

// Store through a dereferenced pointer
/*这段代码实现了通过解引用指针将一个临时变量的值存储到内存中的逻辑。*/
/*

解引用是指通过指针访问指针所指向的内存地址中的值。
在C语言中，使用解引用操作符（*）来执行解引用操作。通过解引用，你可以使用指针来获取或修改指针指向的内存中的数据。

int x = 10;
int *ptr = &x;  // 声明一个整型指针，并将其指向变量 x 的地址

// 使用解引用操作符获取指针所指向的值
int value = *ptr;

// 修改指针所指向的内存中的值
*ptr = 20;
*/
int cgstorderef(int r1, int r2, int type) {
  // Get the size of the type
  int size = cgprimsize(type);

  switch (size) {
    case 1:
      fprintf(Outfile, "  storeb %%.t%d, %%.t%d\n", r1, r2);
      break;
    case 4:
      fprintf(Outfile, "  storew %%.t%d, %%.t%d\n", r1, r2);
      break;
    case 8:
      fprintf(Outfile, "  storel %%.t%d, %%.t%d\n", r1, r2);
      break;
    default:
      fatald("Can't cgstoderef on type:", type);
  }
  return (r1);
}

// Move value between temporaries
void cgmove(int r1, int r2, int type) {
  fprintf(Outfile, "  %%.t%d =%c copy %%.t%d\n", r2, cgqbetype(type), r1);
}

// Output a gdb directive to say on which
// source code line number the following
// assembly code came from
void cglinenum(int line) {
  // fprintf(Outfile, "\t.loc 1 %d 0\n", line);
}

// Change a temporary value from its old
// type to a new type.
int cgcast(int t, int oldtype, int newtype) {
  // Get temporary for the return result
  int ret = cgalloctemp();
  int oldsize, newsize;
  char qnew;

  // If the new type is a pointer
  if (ptrtype(newtype)) {
    // Nothing to do if the old type is also a pointer
    if (ptrtype(oldtype))
      return (t);
    // Otherwise, widen from a primitive type to a pointer
    return (cgwiden(t, oldtype, newtype));
  }

  // New type is not a pointer
  // Get the new QBE type
  // and the type sizes in bytes
  qnew = cgqbetype(newtype);
  oldsize = cgprimsize(oldtype);
  newsize = cgprimsize(newtype);

  // Nothing to do if the two are the same size
  if (newsize == oldsize)
    return (t);

  // If the new size is smaller, we can copy and QBE will truncate it,
  // otherwise use the QBE cast operation
  if (newsize < oldsize)
    fprintf(Outfile, " %%.t%d =%c copy %%.t%d\n", ret, qnew, t);
  else
    fprintf(Outfile, " %%.t%d =%c cast %%.t%d\n", ret, qnew, t);
  return (ret);
}
