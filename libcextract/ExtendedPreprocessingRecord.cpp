//===- ExtendedPreprocessingRecord.cpp - Implement the missing features of PreprocessingRecord *- C++ -*-===//
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
/// and it is able to hook on many other things, such as change of files, that
/// is useful for our analysis.  So rather than rewriting the entire
/// PreprocessingRecord, we hijack the class by replacing some tokens during
/// compilation time and extends it so we can implement the missing pieces.
///
/// This also saves us from working directly with clang's sourcecode, which
/// takes a lot of time to compile.
//
//===----------------------------------------------------------------------===//

/* Author: Giuliano Belinassi  */

/* include the PreprocessingRecord but hijacking the private members of the
   class.  */
#define __HIJACK_PREPROCESSING_RECORD
#include "ExtendedPreprocessingRecord.hh"

namespace clang {
std::unique_ptr<ASTConsumer> PPHooksAction::CreateASTConsumer(CompilerInstance &CI,
                                                              StringRef InfFile)
{
  Preprocessor &pp = CI.getPreprocessor();
  SourceManager &sm = CI.getSourceManager();
  auto h = std::make_unique<ExtendedPreprocessingRecord>(pp, sm);
  RECObj = h.get();

  pp.addPPCallbacks(std::move(h));
  return std::make_unique<ASTConsumer>();
}

void PPHooksAction::EndSourceFileAction(void)
{

}

void ExtendedPreprocessingRecord::FileChanged(SourceLocation Loc,
                                              PPCallbacks::FileChangeReason Reason,
                                              SrcMgr::CharacteristicKind FileType,
                                              FileID PrevFID)
{
  const char *reason[] = {
    "EnterFile",
    "ExitFile",
    "SystemHeaderPragma",
    "RenameFile",
  };

  llvm::outs() << "File Changed: " << reason[Reason] << "\n";
  Loc.dump(getSourceManager());
}

void ExtendedPreprocessingRecord::InclusionDirective(SourceLocation HashLoc,
                        const Token &IncludeTok,
                        StringRef FileName, bool IsAngled,
                        CharSourceRange FilenameRange,
                        OptionalFileEntryRef File, StringRef SearchPath,
                        StringRef RelativePath, const Module *Imported,
                        SrcMgr::CharacteristicKind FileType)
{
  /* Call private member.  */
  PreprocessingRecord::InclusionDirective(HashLoc,
                        IncludeTok,
                        FileName, IsAngled,
                        FilenameRange,
                        File, SearchPath,
                        RelativePath, Imported,
                        FileType);
}
};
