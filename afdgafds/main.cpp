#include <cstdio>
#include <cstdlib>
#include "llvm/Support/TargetSelect.h"//这是LLVM（低级虚拟机）的头文件，提供了目标选择支持，用于设置LLVM环境以生成针对特定硬件架构的代码。
#include "llvm/Target/TargetMachine.h"//这个LLVM头文件定义了 TargetMachine 类，它负责为特定的目标机器（硬件）生成代码。
#ifdef linux    //linux
#include "KaleidoscopeJIT.h"
#else  //windows
#include "../include/KaleidoscopeJIT.h"
#endif

static int emitIR = 0;
static int emitObj = 0; //如果添加了-obj选项，则只生成.o文件，不运行代码
static char *inputFileName;
#include "Lexer.h"
#include "AST.h"
#include "Parser.h"

void usage()
{
    printf("usage: VSL inputFile [-r] [-h] [-obj]\n");
    printf("-r: emit IR code to IRcode.ll file\n");
    printf("-h: show help information\n");
    printf("-obj: emit obj file of the input file\n");

    exit(EXIT_FAILURE);
}

void getArgs(int argc, char *argv[])
{
    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0] == '-' && argv[i][1] == 'r')
        {
            emitIR = 1;
        }else if (argv[i][0] == '-' && argv[i][1] == 'h')
        {
            usage();
        }
        else if (argv[i][0] == '-' && argv[i][1] == 'o')
        {
            if (strlen(argv[i]) > 3 && 
                (argv[i][2] == 'b' && argv[i][3] == 'j'))
                emitObj = 1;
            else
                usage();
        }else
        {
            inputFileName = argv[i];
        }
    }
}

int main(int argc, char *argv[]) {
    if(argc < 2)
        usage();
    getArgs(argc, argv);
    inputFile = fopen(inputFileName, "r");
    if(!inputFile){
        printf("%s open error!\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    //这些函数初始化LLVM的目标（target）库，使得程序可以生成机器代码和汇编代码
    InitializeNativeTarget();//这一步使LLVM库能够生成和处理当前运行平台的机器代码。换句话说，它为当前运行的机器架构（如x86, ARM等）配置生成代码的能力。
    InitializeNativeTargetAsmPrinter();//初始化本地目标的汇编打印支持。
    InitializeNativeTargetAsmParser();//初始化本地目标的汇编解析支持。

    BinopPrecedence['+'] = 10;
    BinopPrecedence['-'] = 10;
    BinopPrecedence['*'] = 40;
    BinopPrecedence['/'] = 40;

    TheJIT = llvm::make_unique<KaleidoscopeJIT>();
    //初始化LLVM模块和Pass管理器，用于管理和优化生成的代码。
    InitializeModuleAndPassManager();

    // Run the main "interpreter loop" now.
    getNextToken();
    MainLoop();

    if(emitObj)
    {
        // Initialize the target registry etc.
        InitializeAllTargetInfos();//初始化所有目标的信息。目标信息包括目标平台的基本描述，如名称和特性。
        InitializeAllTargets();//初始化所有目标。目标是指LLVM支持的所有硬件架构（如x86, ARM等）。
        InitializeAllTargetMCs();//初始化所有目标的机器代码生成（MC）组件。机器代码生成组件负责将LLVM IR（中间表示）转换为机器代码。
        InitializeAllAsmParsers();//初始化所有汇编解析器。汇编解析器负责将汇编代码解析回LLVM的中间表示或机器代码。
        InitializeAllAsmPrinters();//初始化所有汇编打印器。汇编打印器负责将机器代码打印成汇编代码。

        //获取并设置目标三元组，三元组通常包含架构、供应商和操作系统信息（例如 x86_64-pc-linux-gnu）。
        auto TargetTriple = sys::getDefaultTargetTriple();

        TheModule->setTargetTriple(TargetTriple);
        //查找与目标三元组对应的目标信息。这个函数在目标注册表中查找与指定三元组匹配的目标。如果找不到匹配的目标，错误信息将存储在Error字符串中。
        std::string Error;
        auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);

        // Print an error and exit if we couldn't find the requested target.
        // This generally occurs if we've forgotten to initialise the
        // TargetRegistry or we have a bogus target triple.
        //根据目标三元组查找目标。
        if (!Target)
        {
            errs() << Error;
            return 1;
        }

        auto CPU = "generic";//设定目标机器的CPU类型为通用类型（generic）。这意味着生成的代码应该能够在所有目标架构的CPU上运行，而无需特定的优化。
        auto Features = "";//设定目标机器的CPU特性为空字符串。这表示不启用任何特定的CPU特性或扩展指令集。

        //创建一个目标机器（Target Machine）实例。
        TargetOptions opt;//TargetOptions 是 LLVM 中用于存储目标机器选项的类。它通常用于配置目标机器的特定选项，例如优化级别、代码生成选项、调试信息生成选项等。
        //重定位模型定义了编译器如何处理生成的目标代码中的地址引用，以确保代码能够在运行时正确地定位到内存中的实际位置。选择合适的重定位模型取决于应用程序的需求和目标平台的特性。
        auto RM = Optional<Reloc::Model>();
        //TheTargetMachine 实例包含了目标平台（Target）的所有信息，包括硬件架构、操作系统、优化选项等。
        auto TheTargetMachine =
            Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);//根据目标三元组、CPU、特性和选项创建目标机器
        // 调用目标机器实例的 createDataLayout 方法，创建一个数据布局（DataLayout）对象。这个对象描述了目标平台的内存布局规则，包括数据对齐方式、类型大小等。
        TheModule->setDataLayout(TheTargetMachine->createDataLayout());//为模块设置数据布局。

        //这段代码的作用是创建一个文件输出流，并将生成的目标文件写入到指定的文件（output.o）。
        auto Filename = "output.o";
        std::error_code EC;
        raw_fd_ostream dest(Filename, EC, sys::fs::F_None);

        if (EC)
        {
            errs() << "Could not open file: " << EC.message();
            return 1;
        }

        //创建一个旧版的Pass管理器实例。
        //在 LLVM 中，Pass（传递）是一种用于转换、优化或分析代码的模块化单元。Passes 用于处理 LLVM 的中间表示（IR），这种表示在编译器的各个阶段都可以使用。每个 Pass 都执行特定的任务，例如优化代码、生成机器代码、分析程序结构等。
        legacy::PassManager pass;
        //指定文件类型为目标文件。
        auto FileType = TargetMachine::CGFT_ObjectFile;
        //向Pass管理器添加必要的Pass，以生成目标文件。如果失败，打印错误信息并退出程序。
        if (TheTargetMachine->addPassesToEmitFile(pass, dest, FileType))
        {
            errs() << "TheTargetMachine can't emit a file of this type";
            return 1;
        }
        //运行Pass管理器，处理模块并生成目标文件。
        pass.run(*TheModule);
        //刷新输出流，确保所有数据都写入文件。
        dest.flush();

        outs() << "Wrote " << Filename << "\n";
    }

    return 0;
}
