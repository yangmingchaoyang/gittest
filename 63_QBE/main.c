#include "defs.h"
#define extern_
#include "data.h"
#undef extern_
#include "decl.h"
#include <errno.h>
#include <unistd.h>

// Compiler setup and top-level execution
// Copyright (c) 2019 Warren Toomey, GPL3

// Given a string with a '.' and at least a 1-character suffix
// after the '.', change the suffix to be the given character.
// Return the new string or NULL if the original string could
// not be modified
/*这段代码的作用是修改输入字符串中的文件后缀。具体而言，它假设输入字符串包含一个点（'.'），并且在点之后至少有一个字符的后缀。*/
char *alter_suffix(char *str, char suffix) {
  char *posn;
  char *newstr;

  // Clone the string
  /*使用 strdup 复制输入字符串，创建一个新的字符串 newstr。这样做是为了不修改原始字符串。*/
  if ((newstr = strdup(str)) == NULL)
    return (NULL);

  // Find the '.'
  /*使用 strrchr 函数查找 newstr 中的最后一个点（'.'）的位置，并将其保存在 posn 指针中。*/
  if ((posn = strrchr(newstr, '.')) == NULL)
    return (NULL);

  // Ensure there is a suffix
  /* 确保找到点后存在后缀字符。如果点后没有字符，返回 NULL，表示无法修改。*/
  posn++;
  if (*posn == '\0')
    return (NULL);

  // Change the suffix and NUL-terminate the string
  /*将点后的字符修改为给定的后缀字符，并在新的后缀字符后添加字符串终止符（'\0'）。*/
  *posn = suffix;
  posn++;
  *posn = '\0';
  /*返回修改后的新字符串 newstr。*/
  return (newstr);
}

// Given an input filename, compile that file
// down to assembly code. Return the new file's name
/*这段代码是一个函数，用于编译给定的输入文件（C语言源文件）*/
static char *do_compile(char *filename) {
  char cmd[TEXTLEN];

  // Change the input file's suffix to .q
  /* 使用 alter_suffix 函数将输入文件名的后缀更改为 'q'，
  并将结果存储在 Outfilename 变量中。这里假设输入文件的后缀是可修改的，如果无法修改，输出错误信息并退出。*/
  Outfilename = alter_suffix(filename, 'q');
  if (Outfilename == NULL) {
    fprintf(stderr, "Error: %s has no suffix, try .c on the end\n", filename);
    exit(1);
  }
  // Generate the pre-processor command
  /*使用 snprintf 函数生成预处理器的命令字符串，其中包括预处理器的路径、包含目录等信息。*/
  /*cpp -nostdinc -isystem /tmp/include/filename*/
  /*
  -nostdinc: 禁止使用标准系统头文件目录，即禁用默认的标准头文件搜索路径。
  -isystem /tmp/include: 指定一个系统头文件目录，即告诉预处理器在 /tmp/include 目录中寻找系统头文件。这通常用于指定非标准的头文件目录，以便在编译时使用特定的头文件。
  */
  snprintf(cmd, TEXTLEN, "%s %s %s", CPPCMD, INCDIR, filename);

  // Open up the pre-processor pipe
  /*
  popen 函数打开一个管道，允许从一个子进程读取输出。
  它的第一个参数是要执行的命令（在这里是 cmd，即 C 预处理器的命令字符串），第二个参数是模式（在这里是 "r"，表示只读）。
  如果 popen 打开管道成功，它将返回一个文件指针（FILE *），该文件指针可以用于读取来自子进程的输出。
  //读取了预处理的文件
  */
  if ((Infile = popen(cmd, "r")) == NULL) {
    fprintf(stderr, "Unable to open %s: %s\n", filename, strerror(errno));
    exit(1);
  }
  Infilename = filename;

  // Create the output file
  /*fopen 函数用于打开文件，它的第一个参数是文件路径（在这里是 Outfilename，表示输出文件的路径），
  第二个参数是文件打开模式（在这里是 "w"，表示以写入方式打开文件）。
  如果 fopen 打开文件成功，它将返回一个文件指针（FILE *），该文件指针可以用于写入文件。*/
  if ((Outfile = fopen(Outfilename, "w")) == NULL) {
    fprintf(stderr, "Unable to create %s: %s\n", Outfilename,
	    strerror(errno));
    exit(1);
  }

  Line = 1;			// Reset the scanner将当前行号初始化为 1。
  Linestart = 1;//表示在新的一行的开头。
  Putback = '\n';//将 Putback 设置为换行符，表示扫描器的前一个字符是换行符。
  clear_symtable();		// Clear the symbol table// 清空符号表，以确保符号表是空的。
  if (O_verbose)
    printf("compiling %s\n", filename);
  scan(&Token);			// Get the first token from the input调用 scan 函数，从输入文件中获取第一个令牌（token）。这是编译器开始解析源代码的一部分。
  Peektoken.token = 0;		// and set there is no lookahead token设置没有预读令牌。这将在后续的代码中用于处理预读的令牌。
  genpreamble(filename);	// Output the preamble/*输出编译的前导部分。这可能包括一些汇编代码，用于初始化程序的一些全局设置等。
  global_declarations();	// Parse the global declarations/*解析全局声明。这一部分负责解析源文件中的全局变量和函数声明。在这里进行Token的运动
  genpostamble();		// Output the postamble输出编译的后导部分。这可能包括一些汇编代码，用于结束程序的执行等。
  fclose(Outfile);		// Close the output file关闭输出文件。这是编译器生成的目标文件或汇编文件。

  // Dump the symbol table if requested
  /*通过检查 O_dumpsym 的值判断是否需要打印符号表。*/
  if (O_dumpsym) {
    printf("Symbols for %s\n", filename);
    dumpsymtables();
    fprintf(stdout, "\n\n");
  }

  /*
freestaticsyms 函数的作用是释放文件中的所有静态符号（static symbols）。
在C语言中，static 关键字可以用于函数内的局部变量或全局变量，
以指示它们具有静态生存期，即在整个程序执行期间都存在。*/
  freestaticsyms();		// Free any static symbols in the file
  return (Outfilename);
}

// Given an input filename, run QBE on the file and
// produce an assembly file. Return the object filename
/*这段代码的目的是给定一个输入的文件名，运行 QBE（Quick Backend）编译器处理该文件，并生成一个相应的汇编文件。*/
char *do_qbe(char *filename) {
  char cmd[TEXTLEN];
  int err;
  /*: 使用 alter_suffix 函数将输入的文件名的后缀修改为 's'，表示汇编文件。*/
  char *outfilename = alter_suffix(filename, 's');
  if (outfilename == NULL) {
    fprintf(stderr, "Error: %s has no suffix, try .qbe on the end\n",
	    filename);
    exit(1);
  }
  // Build the QBE command and run it
  /*qbe -o filename.s filename
  其作用是将输入的源文件（filename）编译成汇编语言，并将输出保存为一个汇编文件（filename.s）。*/
  snprintf(cmd, TEXTLEN, "%s %s %s", QBECMD, outfilename, filename);
  if (O_verbose)
    printf("%s\n", cmd);
  err = system(cmd);//调用系统命令运行 QBE 命令，system 函数返回运行结果。
  if (err != 0) {
    fprintf(stderr, "QBE translation of %s failed\n", filename);
    exit(1);
  }
  return (outfilename);//返回生成的汇编文件名。
}

// Given an input filename, assemble that file
// down to object code. Return the object filename
/*这段代码的作用是使用汇编器（as）将输入的汇编文件（filename）汇编成目标文件（object code）。*/
char *do_assemble(char *filename) {
  char cmd[TEXTLEN];
  int err;

  /*生成一个.o后缀的文件*/
  char *outfilename = alter_suffix(filename, 'o');
  if (outfilename == NULL) {
    fprintf(stderr, "Error: %s has no suffix, try .s on the end\n", filename);
    exit(1);
  }
  // Build the assembly command and run it
  /*as -g -o filename.o filename.s
  这个命令使用了汇编器（as）来将汇编文件（filename.s）转换成目标文件*/
  snprintf(cmd, TEXTLEN, "%s %s %s", ASCMD, outfilename, filename);
  if (O_verbose)
    printf("%s\n", cmd);
  err = system(cmd);
  if (err != 0) {
    fprintf(stderr, "Assembly of %s failed\n", filename);
    exit(1);
  }
  return (outfilename);
}

// Given a list of object files and an output filename,
// link all of the object filenames together.
/*这段代码实现了链接（linking）的功能。链接是将多个目标文件（object files）合并成一个可执行文件或者共享库的过程。*/
void do_link(char *outfilename, char **objlist) {
  int cnt, size = TEXTLEN;
  char cmd[TEXTLEN], *cptr;
  int err;

  // Start with the linker command and the output file
  cptr = cmd;
  /*cc -g -no-pie -o outfilename*/
  /*
  cc: 这是C语言编译器的命令。
  -g: 表示生成包含调试信息的可执行文件，以便进行调试。
  -no-pie: 指定生成的可执行文件不使用位置独立可执行（Position Independent Executable，PIE）格式。
  PIE是一种可执行文件格式，其中代码和数据段是相对于基址的，这有助于增强安全性。在这里，-no-pie的选项表示不使用PIE。
  -o outfilename: 指定生成的可执行文件的名称为 outfilename。
  因此，这个命令的作用是编译和链接C代码，生成一个名为 outfilename 的可执行文件。
  */
  cnt = snprintf(cptr, size, "%s %s ", LDCMD, outfilename);
  cptr += cnt;
  size -= cnt;

  // Now append each object file
  while (*objlist != NULL) {//链接全部可执行文件
    cnt = snprintf(cptr, size, "%s ", *objlist);
    cptr += cnt;
    size -= cnt;
    objlist++;
  }

  if (O_verbose)
    printf("%s\n", cmd);
  err = system(cmd);
  if (err != 0) {
    fprintf(stderr, "Linking failed\n");
    exit(1);
  }
}

// Print out a usage if started incorrectly
/*
这个函数的作用是打印程序的用法信息，用于指导用户如何正确启动程序。
在这里，usage 函数通过标准错误流 (stderr) 打印了程序的使用方式和可用选项的说明。
如果用户启动程序时提供了不正确的参数，可以调用这个函数来显示正确的用法信息，并退出程序。
*/
static void usage(char *prog) {
  fprintf(stderr, "Usage: %s [-vcSTM] [-o outfile] file [file ...]\n", prog);
  fprintf(stderr,
	  "       -v give verbose output of the compilation stages\n");
  fprintf(stderr, "       -c generate object files but don't link them\n");
  fprintf(stderr, "       -S generate assembly files but don't link them\n");
  fprintf(stderr, "       -T dump the AST trees for each input file\n");
  fprintf(stderr, "       -M dump the symbol table for each input file\n");
  fprintf(stderr, "       -o outfile, produce the outfile executable file\n");
  exit(1);
}

// Main program: check arguments and print a usage
// if we don't have an argument. Open up the input
// file and call scanfile() to scan the tokens in it.
enum { MAXOBJ = 100 };
int main(int argc, char **argv) {
  char *outfilename = AOUT;/*存储输出文件的名称。可以通过 -o 命令行选项进行更改，指定生成的可执行文件的名称。*/
  char *qbefile, *asmfile, *objfile;/*存储生成的中间文件（QBE 文件、汇编文件、目标文件）的文件名。*/
  char *objlist[MAXOBJ];/*存储所有生成的目标文件的列表。这个列表将用于链接操作。*/
  int i, j, objcnt = 0;/*跟踪 objlist 数组中存储的目标文件的数量。*/

  // Initialise our variables
  O_dumpAST = 0;/*是否打印 AST（Abstract Syntax Tree）树。*/
  O_dumpsym = 0;/*是否打印符号表。*/
  O_keepasm = 0;/*是否保留汇编文件。*/
  O_assemble = 0;/*是否进行汇编。*/
  O_verbose = 0;/*是否输出详细信息。*/
  O_dolink = 1;/*是否进行链接。*/

  // Scan for command-line options
  for (i = 1; i < argc; i++) {
    // No leading '-', stop scanning for options
    /*如果当前参数不以 - 开头，说明不是选项，就跳出外部循环。因为选项通常以 - 开头。*/
    if (*argv[i] != '-')//到达要编译的文件
      break;

    // For each option in this argument
    /*内部循环用于处理每个选项的字符。*/
    for (j = 1; (*argv[i] == '-') && argv[i][j]; j++) {
      switch (argv[i][j]) {
	case 'o':
  /*设置输出文件名为下一个命令行参数的值，并跳过下一个参数。*/
	  outfilename = argv[++i];	// Save & skip to next argument
	  break;
	case 'T':
	  O_dumpAST = 1;
	  break;
	case 'M':
	  O_dumpsym = 1;
	  break;
	case 'c':
	  O_assemble = 1;
	  O_keepasm = 0;
	  O_dolink = 0;
	  break;
	case 'S':
	  O_keepasm = 1;
	  O_assemble = 0;
	  O_dolink = 0;
	  break;
	case 'v':
	  O_verbose = 1;
	  break;
	default:
	  usage(argv[0]);
      }
    }
  }

  // Ensure we have at lease one input file argument
  /*如果确实没有提供输入文件参数，程序会调用 usage(argv[0]) 函数，该函数会打印出程序的用法信息，并终止程序的执行。*/
  if (i >= argc)
    usage(argv[0]);

  // Work on each input file in turn
  while (i < argc) {
    /*使用 do_compile 函数将源文件编译为 QBE 中间代码文件。*/
    qbefile = do_compile(argv[i]);	// Compile the source file
    /*使用 do_qbe 函数将 QBE 文件转换为汇编语言文件。*/
    asmfile = do_qbe(qbefile);
    /*如果设置了链接标志 O_dolink 或者汇编标志 O_assemble，则使用 do_assemble 函数将汇编文件汇编成目标文件。*/
    if (O_dolink || O_assemble) {
      objfile = do_assemble(asmfile);	// Assemble it to object forma
      if (objcnt == (MAXOBJ - 2)) {
	fprintf(stderr, "Too many object files for the compiler to handle\n");
	exit(1);
      }
      /*将生成的目标文件名添加到对象文件列表 objlist 中。*/
      objlist[objcnt++] = objfile;	// Add the object file's name
      objlist[objcnt] = NULL;	// to the list of object files
    }
    /*如果不需要保留中间文件（QBE 和汇编文件），则使用 unlink 函数删除这些文件。*/
    if (!O_keepasm) {		// Remove the QBE and assembly files
      unlink(qbefile);		// if we don't need to keep them
      unlink(asmfile);
    }
    i++;
  }

  // Now link all the object files together
  /*如果 O_dolink 为真，表示需要进行链接操作，则调用 do_link 函数将所有对象文件链接在一起，生成最终的可执行文件。*/
  if (O_dolink) {
    do_link(outfilename, objlist);

    // If we don't need to keep the object
    // files, then remove them
    if (!O_assemble) {
      for (i = 0; objlist[i] != NULL; i++)
	unlink(objlist[i]);
    }
  }

  return (0);
}
