//===- ExtendedPreprocessingRecord.hh - Implement the missing features of PreprocessingRecord *- C++ -*-===//
//
// This project is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// The PreprocessingRecord is a class in clang that logs the definition of
/// preprocessing events, such as macro defintion, expansion, and file
/// inclusion.  This class relies on clang's parser Callbacks API (PPCallbacks)
/// and it is able to hook on many other things, such as change of files, which
/// are useful for our analysis.  So instead of than rewriting the entire
/// PreprocessingRecord, we hijack the class by replacing some tokens during
/// compilation time and extends it so we can implement the missing pieces.
///
/// This also saves us from working directly with clang's sourcecode, which
/// takes a lot of time to compile.
//
//===----------------------------------------------------------------------===//

/* Author: Giuliano Belinassi  */

#pragma once


/* includes from clang/Lex/PreprocessingRecord.h.  */
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/PPCallbacks.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/iterator.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/Compiler.h"
#include "clang/Frontend/CompilerInstance.h"
#include <cassert>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

/** Foward declaration of our ExpandedPreprocessingRecord.*/
namespace clang {
  class ExtendedPreprocessingRecord;
};

/** If this include has the following macro defined, then we hijack the
  * PreprocessingRecord class to set the ExtendedPreprocessingRecord class as
  * a friend so we can access its private members.
  */
#ifdef __HIJACK_PREPROCESSING_RECORD
/* If the PreprocessingRecord .h headerguard is set we can't continue, as the
   definition of the class would differ.  So error out.  */
# ifdef LLVM_CLANG_LEX_PREPROCESSINGRECORD_H
#  error "Code injection failed: PreprocessingRecord is already defined."
# endif

/** Now set our hack.  In PreprocessingRecord.h, it defines a method prototype
  * named isEntityInFileID.  We will use this as our hack entry point by
  * expanding this token into three things:
  *  - An array of 0 elem to swallow the `bool` without touching the class size;
  *  - the friend class declaration;
  *  - the definition of the method.
  *
  * this works because the macros are handled by the preprocessor, and not
  * by the parser itself.
  */
# define isEntityInFileID \
  ___v[0]; \
  friend class ExtendedPreprocessingRecord; \
  bool isEntityInFileID
# include "clang/Lex/PreprocessingRecord.h"
# undef isEntityInFileID
#else
# include "clang/Lex/PreprocessingRecord.h"
#endif

#include "clang/Frontend/FrontendAction.h"

namespace clang {

/** The Expended Preprocessing Record class.  Accesses to private members of
    the base PreprocessingRecord should be done in .cpp file, and not here,
    as we try to control the effect of out hack.  */
class ExtendedPreprocessingRecord : public clang::PreprocessingRecord {
  public:
  ExtendedPreprocessingRecord(Preprocessor &pp, SourceManager &sm)
    :  PreprocessingRecord(sm), PP(pp)
  {}

  /* Hijack change of file event.  */
  void FileChanged(SourceLocation Loc, PPCallbacks::FileChangeReason Reason,
                   SrcMgr::CharacteristicKind FileType,
                   FileID PrevFID = FileID()) override;

  /* Hijack #include event.  */
  void InclusionDirective(SourceLocation HashLoc, const Token &IncludeTok,
                          StringRef FileName, bool IsAngled,
                          CharSourceRange FilenameRange,
                          OptionalFileEntryRef File, StringRef SearchPath,
                          StringRef RelativePath, const Module *Imported,
                          SrcMgr::CharacteristicKind FileType) override;

  private:

  /* Reference to the Preprocessor used during the compilation.  */
  Preprocessor &PP;
};


/** The ASTFrontendAction implementation to provide an interface to our
  * ExtendedPreprocessingRecord.  This can be provided to
  * ASTUnit::LoadFromCompilerInvocation for it to call our methods in
  * the ExtendedPreprocessingRecord class.  */
class PPHooksAction : public ASTFrontendAction {
public:
  PPHooksAction(void)
  {}

  /** Get the ExtendedPreprocessingRecord object created by
      CreateASTCustomer.  */
  inline ExtendedPreprocessingRecord *getExtendedPreprocessingRecord(void)
  {
    return RECObj;
  }

protected:
  /* To comply with the ASTFrontendAction API.  */
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef InFile) override;

  /* Called when the input file is depleted.  */
  void EndSourceFileAction(void) override;

  /* ExtendedPreprocessingRecord instance created by CreateASTCustomer.  */
  ExtendedPreprocessingRecord *RECObj;
};
};
