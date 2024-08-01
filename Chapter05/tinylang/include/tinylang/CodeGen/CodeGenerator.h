#ifndef TINYLANG_CODEGEN_CODEGENERATOR_H
#define TINYLANG_CODEGEN_CODEGENERATOR_H

#include "tinylang/AST/AST.h"
#include "llvm/Target/TargetMachine.h"
#include <string>

namespace tinylang {

class CodeGenerator {
  llvm::LLVMContext &Ctx;//引用 LLVM 的上下文对象，它在 LLVM 中用于存储全局状态，例如类型信息和常量池。
  llvm::TargetMachine *TM;//指向 LLVM 的目标机器对象。它提供了关于目标架构的信息，用于优化和生成目标特定的代码。
  ModuleDeclaration *CM;//指向 ModuleDeclaration 对象的指针，该对象通常代表要生成代码的模块的声明。模块声明包含了所有的函数、全局变量等信息。

protected:
  CodeGenerator(llvm::LLVMContext &Ctx, llvm::TargetMachine *TM)
      : Ctx(Ctx), TM(TM), CM(nullptr) {}

public:
  static CodeGenerator *create(llvm::LLVMContext &Ctx, llvm::TargetMachine *TM);

  std::unique_ptr<llvm::Module> run(ModuleDeclaration *CM, std::string FileName);
};
} // namespace tinylang
#endif