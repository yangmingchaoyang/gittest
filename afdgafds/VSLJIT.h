#pragma once
//===- VSLJIT.h - A simple JIT for VSL --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Contains a simple JIT definition for use in the VSL tutorials.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_ORC_VSLJIT_H
#define LLVM_EXECUTIONENGINE_ORC_VSLJIT_H

#include "llvm/ADT/iterator_range.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/LambdaResolver.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace llvm {
	namespace orc {

		class VSLJIT {
		public:
			using ObjLayerT = RTDyldObjectLinkingLayer;
			using CompileLayerT = IRCompileLayer<ObjLayerT, SimpleCompiler>;
			using ModuleHandleT = CompileLayerT::ModuleHandleT;

			VSLJIT()
				: TM(EngineBuilder().selectTarget()), DL(TM->createDataLayout()),
				ObjectLayer([]() { return std::make_shared<SectionMemoryManager>(); }),
				CompileLayer(ObjectLayer, SimpleCompiler(*TM)) {
				llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
			}

			TargetMachine &getTargetMachine() { return *TM; }

			ModuleHandleT addModule(std::unique_ptr<Module> M) {
				// We need a memory manager to allocate memory and resolve symbols for this
				// new module. Create one that resolves symbols by looking back into the
				// JIT.
				auto Resolver = createLambdaResolver(
					[&](const std::string &Name) {
					if (auto Sym = findMangledSymbol(Name))
						return Sym;
					return JITSymbol(nullptr);
				},
					[](const std::string &S) { return nullptr; });
				auto H = cantFail(CompileLayer.addModule(std::move(M),
					std::move(Resolver)));

				ModuleHandles.push_back(H);
				return H;
			}

			void removeModule(ModuleHandleT H) {
				ModuleHandles.erase(find(ModuleHandles, H));
				cantFail(CompileLayer.removeModule(H));
			}

			JITSymbol findSymbol(const std::string Name) {
				return findMangledSymbol(mangle(Name));
			}

		private:
			std::string mangle(const std::string &Name) {
				std::string MangledName;
				{
					raw_string_ostream MangledNameStream(MangledName);
					Mangler::getNameWithPrefix(MangledNameStream, Name, DL);
				}
				return MangledName;
			}

			JITSymbol findMangledSymbol(const std::string &Name) {
#ifdef LLVM_ON_WIN32
				// The symbol lookup of ObjectLinkingLayer uses the SymbolRef::SF_Exported
				// flag to decide whether a symbol will be visible or not, when we call
				// IRCompileLayer::findSymbolIn with ExportedSymbolsOnly set to true.
				//
				// But for Windows COFF objects, this flag is currently never set.
				// For a potential solution see: https://reviews.llvm.org/rL258665
				// For now, we allow non-exported symbols on Windows as a workaround.
				const bool ExportedSymbolsOnly = false;
#else
				const bool ExportedSymbolsOnly = true;
#endif

				// Search modules in reverse order: from last added to first added.
				// This is the opposite of the usual search order for dlsym, but makes more
				// sense in a REPL where we want to bind to the newest available definition.
				for (auto H : make_range(ModuleHandles.rbegin(), ModuleHandles.rend()))
					if (auto Sym = CompileLayer.findSymbolIn(H, Name, ExportedSymbolsOnly))
						return Sym;

				// If we can't find the symbol in the JIT, try looking in the host process.
				if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(Name))
					return JITSymbol(SymAddr, JITSymbolFlags::Exported);

#ifdef LLVM_ON_WIN32
				// For Windows retry without "_" at beginning, as RTDyldMemoryManager uses
				// GetProcAddress and standard libraries like msvcrt.dll use names
				// with and without "_" (for example "_itoa" but "sin").
				if (Name.length() > 2 && Name[0] == '_')
					if (auto SymAddr =
						RTDyldMemoryManager::getSymbolAddressInProcess(Name.substr(1)))
						return JITSymbol(SymAddr, JITSymbolFlags::Exported);
#endif

				return nullptr;
			}

			std::unique_ptr<TargetMachine> TM;//这是一个指向目标机器 (TargetMachine) 的智能指针。
			const DataLayout DL;//DataLayout 是一个描述目标平台数据布局的对象。
			ObjLayerT ObjectLayer;//ObjectLayer 是实际的对象链接层实例，负责管理和链接编译后的对象文件。它处理对象文件的加载、内存分配和符号解析。
			CompileLayerT CompileLayer;//负责将 LLVM IR 编译成目标机器代码。它使用对象链接层 (ObjectLayer) 和简单编译器 (SimpleCompiler) 进行编译和链接。
			std::vector<ModuleHandleT> ModuleHandles;//ModuleHandles 是一个存储所有已添加模块句柄的向量。每个句柄代表一个已编译并加载到 JIT 编译器中的模块。
		};

	} // end namespace orc
} // end namespace llvm

#endif // LLVM_EXECUTIONENGINE_ORC_VSLJIT_H
