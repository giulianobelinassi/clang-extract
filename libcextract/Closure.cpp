//===- Closure.hh - Compute dependencies of a given symbol *- C++ -*-===//
//
// This project is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Compute a set of Decls that is necessary to compile a given symbol.
//
//===----------------------------------------------------------------------===//

/* Author: Giuliano Belinassi  */

#include "Closure.hh"

/** Add a decl to the Dependencies set and all its previous declarations in the
    AST. A function can have multiple definitions but its body may only be
    defined later.  */
bool ClosureSet::Add_Decl_And_Prevs(Decl *decl)
{
  bool inserted = false;

  /* Do not insert builtin decls.  */
  if (Is_Builtin_Decl(decl)) {
    return false;
  }

  while (decl) {
    if (!Is_Decl_Marked(decl)) {
      Dependencies.insert(decl);
      Add_Enclosing_Contexts(decl);
      inserted = true;
    }

    decl = decl->getPreviousDecl();
  }
  return inserted;
}

void DeclClosureVisitor::Compute_Closure_Of_Symbols(const std::vector<std::string> &names,
                                          std::unordered_set<std::string> *matched_names)
{
  /* FIXME: clang has a mechanism (DeclContext::lookup) which is SUPPOSED TO
     return the list of all Decls that matches the lookup name.  However, this
     method doesn't work as intented.  In kernel (see github issue #20) it
     results in the lookup_result set not having the Decl that has the body of
     the function, which is the most important!

     Hence, do our own thing here.  Create a set and sweep the top level decls,
     analyzing the symbol that matches the name in the set.  */

  std::unordered_set<std::string> setof_names(std::make_move_iterator(names.begin()),
                                              std::make_move_iterator(names.end()));

  for (auto it = AST->top_level_begin(); it != AST->top_level_end(); ++it) {
    NamedDecl *decl = dyn_cast<NamedDecl>(*it);

    if (!decl) {
      /* Decl does not have a name, thus skip it.  */
      continue;
    }

    const std::string &decl_name = decl->getNameAsString();
    /* If the symbol name is in the set...   */
    if (setof_names.find(decl_name) != setof_names.end()) {
      /* Mark that name as matched.  */
      if (matched_names)
        matched_names->insert(decl_name);
      /* Find its dependencies.  */
      TraverseDecl(decl);
    }
  }
}

/* ------ DeclClosureVisitor methods ------ */

#define TRY_TO(CALL_EXPR)                    \
  if ((CALL_EXPR) == VISITOR_STOP)           \
    return VISITOR_STOP

#define DO_NOT_RUN_IF_ALREADY_ANALYZED(decl) \
  if (Already_Analyzed(decl) == true)        \
    return VISITOR_CONTINUE

bool DeclClosureVisitor::TraverseDecl(Decl *decl)
{
  DO_NOT_RUN_IF_ALREADY_ANALYZED(decl);
  Mark_As_Analyzed(decl);
  Stack.push_back(decl);
  bool ret = RecursiveASTVisitor::TraverseDecl(decl);
  Stack.pop_back();
  return ret;
}

bool DeclClosureVisitor::VisitFunctionDecl(FunctionDecl *decl)
{
  if (decl->getBuiltinID() != 0) {
    /* Do not redeclare compiler builtin functions.  */
    return VISITOR_CONTINUE;
  }

  /* In case this is an implicit instantiation of a template, then we need
     the function it was instantiated from, which is what is written in the
     source code.  */
  if (isTemplateInstantiation(decl->getTemplateSpecializationKind())) {
    TRY_TO(TraverseDecl(decl->getTemplateInstantiationPattern()));
  }

  /* If this function is actually a template function specialization, then
     look into it.  */
  if (FunctionTemplateSpecializationInfo *ftsi =
              decl->getTemplateSpecializationInfo()) {
    TRY_TO(TraverseDecl(ftsi->getTemplate()));
  }

  /* Mark function for output.  Functions inside of templates must have its
     definition visible in order to be instantiated.  */
  FunctionDecl *to_mark = decl;
  if (decl->isStatic() || decl->isTemplated()) {
    FunctionDecl *definition = decl->getDefinition();
    /* FIXME: Declared static function in closure without a body?  */
    if (definition != nullptr) {
      to_mark = definition;
      /* Make sure we parse the function version with a body.  */
      TRY_TO(TraverseDecl(to_mark));
    }
  }

  /* Check if the return type of the function is a complete struct
     definition, which in this case clang seems to ignore.  */
  const clang::Type *ret_type = to_mark->getReturnType().getTypePtr();
  if (ret_type->isRecordType()) {
    if (TagDecl *tag = ret_type->getAsTagDecl()) {
      tag->setCompleteDefinitionRequired(true);
    }
  }

  Closure.Add_Single_Decl(to_mark);

  /* Also analyze the previous version of this function to make sure we are
     not losing the version with body.  */
  TRY_TO(AnalyzePreviousDecls(to_mark));

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitDeclaratorDecl(DeclaratorDecl *decl)
{
  /* There are some types in which we lose the the typedef reference, for
   * example:
   *
   *   typedef void (*XtErrorHandler)(char *);
   *   extern void XtSetErrorHandler(XtErrorHandler __attribute((noreturn)));
   *
   *  The type dump for this is:
   *
   *   FunctionProtoType 0x5210000720e0 'void (void (*)(char *) __attribute__((noreturn)))' cdecl
   *   |-BuiltinType 0x521000014170 'void'
   *   `-PointerType 0x521000071fd0 'void (*)(char *) __attribute__((noreturn))'
   *     `-ParenType 0x521000071f70 'void (char *) __attribute__((noreturn))' sugar
   *       `-FunctionProtoType 0x521000071f30 'void (char *) __attribute__((noreturn))' noreturn cdecl
   *         |-BuiltinType 0x521000014170 'void'
   *         `-PointerType 0x521000014d10 'char *'
   *           `-BuiltinType 0x5210000141b0 'char'
   *
   *  Notice that there is no reference for XtErrorHandler.  Somehow clang was
   *  not able to identify that XtErrorHandler __attribute((noreturn)) is a
   *  XtErrorHandler.  OTOH, if we drop __attribute((noreturn)) we get:
   *
   *   FunctionProtoType 0x521000071ff0 'void (XtErrorHandler)' cdecl
   *   |-BuiltinType 0x521000014170 'void'
   *   `-ElaboratedType 0x521000071f00 'XtErrorHandler' sugar
   *     `-TypedefType 0x521000071ed0 'XtErrorHandler' sugar
   *       |-Typedef 0x521000071e78 'XtErrorHandler'
   *       `-PointerType 0x521000071e10 'void (*)(char *)'
   *         `-ParenType 0x521000071db0 'void (char *)' sugar
   *           `-FunctionProtoType 0x521000071d70 'void (char *)' cdecl
   *             |-BuiltinType 0x521000014170 'void'
   *             `-PointerType 0x521000014d10 'char *'
   *               `-BuiltinType 0x5210000141b0 'char'
   *
   *  which is correct.  Hence we get the SourceText and try to match to any
   *  decl that is in the symbol table with that name.
   */

  SourceManager &sm = AST->getSourceManager();
  const LangOptions &lo = AST->getLangOpts();

  const TypeSourceInfo *typeinfo = decl->getTypeSourceInfo();
  const TypeLoc &tl = typeinfo->getTypeLoc();

  /* Get the range of the token which we expect is the type of it.  */
  SourceLocation tok_begin = Lexer::GetBeginningOfToken(tl.getBeginLoc(), sm, lo);
  SourceLocation tok_end   = Lexer::getLocForEndOfToken(tl.getBeginLoc(), 0, sm, lo);

  StringRef text = PrettyPrint::Get_Source_Text({tok_begin, tok_end});

  /* Lookup in the symbol table for any Decl matching it.  */
  DeclContextLookupResult decls = Get_Decl_From_Symtab(AST, text);
  for (auto decl_it : decls) {
    TRY_TO(TraverseDecl(decl_it));
  }

  /* Also analyze the decls that have the same beginloc, for declarations
     comming from comma-separated.  */
  TRY_TO(AnalyzeDeclsWithSameBeginlocHelper(decl));

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitValueDecl(ValueDecl *decl)
{
  TRY_TO(TraverseType(decl->getType()));
  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitRecordDecl(RecordDecl *decl)
{
  /* If this is a C++ record decl, then analyze it as such.  */
  if (CXXRecordDecl *cxxdecl = dyn_cast<CXXRecordDecl>(decl)) {
    TRY_TO(VisitCXXRecordDecl(cxxdecl));
  }

  if (TypedefNameDecl *typedecl = decl->getTypedefNameForAnonDecl()) {
    /* In case this struct is defined without a name, such as:
      struct {
        int a;
      };

      then do not add it to the output.  This can happen in the case where the
      original code was declared as:

      typedef struct { int a; } A;  */
    return VisitTypedefNameDecl(typedecl);
  }

  TRY_TO(ParentRecordDeclHelper(decl));

  Closure.Add_Single_Decl(decl);


  /* In case our RecordDecl was triggered to be in the closure, then also
     analyse the types of any FieldDecl it contains. For example:

      struct quic_channel_st {
          SSL *tls;
          BIO_ADDR cur_peer_addr;
      };

     if we are here because we accessed `tls` attribute of this struct, we also
     need to make sure BIO_ADDR is also included in the closure, otherwise we
     can't know the size of quic_channel_st even if nothing accesses it.  See
     closure-7.c testcase for a case where this happens.

     For some reason, sometimes the struct definition found by clang AST
     visitor when comming from an TypedefType isn't the full definition of the
     struct when we need it!  In this case, lets make sure we also process the
     declaration that have the definition.  */
  if (!decl->isCompleteDefinition()) {
    RecordDecl *complete = decl->getDefinition();
    if (complete != nullptr && complete != decl &&
        complete->isCompleteDefinitionRequired()) {
      TRY_TO(TraverseDecl(complete));
    }
  }

  /* Also analyze the previous version of this decl for any version of it
     that is nested-declared inside another record.  */
  TRY_TO(AnalyzePreviousDecls(decl));

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitEnumDecl(EnumDecl *decl)
{
  if (TypedefNameDecl *typedecl = decl->getTypedefNameForAnonDecl()) {
    /* In case this enum is defined without a name, such as:
      enum {
        CONSTANT = 1,
      };

      then do not add it to the output.  This can happen in the case where the
      original code was declared as:

      typedef enum { CONSTANT = 1 } A;  */
    return VisitTypedefNameDecl(typedecl);
  }

  TRY_TO(ParentRecordDeclHelper(decl));
  Closure.Add_Single_Decl(decl);

  /* Also analyze the previous version of this decl for any version of it
     that is nested-declared inside another record.  */
  TRY_TO(AnalyzePreviousDecls(decl));

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitTagDecl(TagDecl *decl)
{
  DeclContext *parent = decl->getParent();
  if (parent && parent->hasValidDeclKind()) {
    /* Check if parent declaration is a namespace.  If yes, then add it
       as well BUT DO NOT add its children.  */
    if (parent->isNamespace()) {
      NamespaceDecl *nm = cast<NamespaceDecl>(parent);
      TRY_TO(VisitNamespaceDecl(nm));
    }
  }

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitEnumConstantDecl(EnumConstantDecl *decl)
{
  /* Add original EnumDecl it originated.  */
  EnumDecl *enum_decl = dyn_cast<EnumDecl>(decl->getLexicalDeclContext());
  if (enum_decl) {
    return TraverseDecl(enum_decl);
  }

  return VISITOR_CONTINUE;
}

/* Not called automatically by Traverse.  */
bool DeclClosureVisitor::VisitTypedefNameDecl(TypedefNameDecl *decl)
{
  // FIXME: Do we need to analyze the previous decls?
  TRY_TO(TraverseType(decl->getUnderlyingType()));
  Closure.Add_Single_Decl(decl);

  TRY_TO(AnalyzeDeclsWithSameBeginlocHelper(decl));

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitVarDecl(VarDecl *decl)
{
  /* In case this is an instantiation of a variable template or of a static
     member of a class template, then we need the template rather than the
     instantiation, which is not written anywhere.  */
  if (isTemplateInstantiation(decl->getTemplateSpecializationKind())) {
    if (VarTemplateSpecializationDecl *vtsd =
        dyn_cast<VarTemplateSpecializationDecl>(decl)) {
      auto specialized = vtsd->getSpecializedTemplateOrPartial();
      if (auto partial = specialized.dyn_cast<VarTemplatePartialSpecializationDecl *>()) {
        TRY_TO(TraverseDecl(partial));
      }
      return TraverseDecl(vtsd->getSpecializedTemplate());
    }

    if (VarDecl *pattern = decl->getInstantiatedFromStaticDataMember()) {
      return TraverseDecl(pattern);
    }
  }

  /* Avoid adding variables that are not global.  */
  // FIXME: Do we need to analyze every previous decl?
  if (decl->hasGlobalStorage()) {
    Closure.Add_Single_Decl(decl);
  }

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitFunctionTemplateDecl(FunctionTemplateDecl *decl)
{
  Closure.Add_Decl_And_Prevs(decl);

  /* The template may have been forward declared and defined later.  We need
     the definition in order to instantiate it.  */
  if (FunctionDecl *def = decl->getTemplatedDecl()->getDefinition()) {
    TRY_TO(TraverseDecl(def->getDescribedFunctionTemplate()));
  }

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitTemplateDecl(TemplateDecl *decl)
{
  Closure.Add_Decl_And_Prevs(decl);

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitNamespaceDecl(NamespaceDecl *decl)
{
  Closure.Add_Decl_And_Prevs(decl);

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitCXXRecordDecl(CXXRecordDecl *decl)
{
  /* Records inside of templates (e.g. out-of-line definitions of nested
     classes) are needed in full to instantiate the template.  */
  if (decl->isDependentContext()) {
    decl->setCompleteDefinitionRequired(true);
    if (CXXRecordDecl *def = decl->getDefinition()) {
      def->setCompleteDefinitionRequired(true);
    }
  }

  /* In case this is an instantiation of a member class of a class template,
     we need the class it was instantiated from.  */
  if (decl->getTemplateSpecializationKind() == TSK_ImplicitInstantiation &&
      !isa<ClassTemplateSpecializationDecl>(decl)) {
    if (CXXRecordDecl *pattern = decl->getTemplateInstantiationPattern()) {
      pattern->setCompleteDefinitionRequired(true);
      TRY_TO(TraverseDecl(pattern));
    }
  }

  /* In case this is a ClassTemplateSpecialDecl, then analyze it as such.  */
  if (ClassTemplateSpecializationDecl *ctsd =
      dyn_cast<ClassTemplateSpecializationDecl>(decl)) {
    TRY_TO(VisitClassTemplateSpecializationDecl(ctsd));
  }

  Closure.Add_Decl_And_Prevs(decl);

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitClassTemplateDecl(ClassTemplateDecl *decl)
{
  Closure.Add_Decl_And_Prevs(decl);

  /* The template may have been forward declared and defined later.  We need
     the definition in order to instantiate it.  */
  if (CXXRecordDecl *def = decl->getTemplatedDecl()->getDefinition()) {
    def->setCompleteDefinitionRequired(true);
    TRY_TO(TraverseDecl(def->getDescribedClassTemplate()));
  }

  /* Any of the partial specializations may be selected by an instantiation of
     this template, so add them as well.  */
  SmallVector<ClassTemplatePartialSpecializationDecl *, 4> partials;
  decl->getPartialSpecializations(partials);
  for (ClassTemplatePartialSpecializationDecl *partial : partials) {
    /* clang doesn't mark those as requiring a complete definition.  */
    partial->setCompleteDefinitionRequired(true);
    TRY_TO(TraverseDecl(partial));
  }

  /* The same goes for explicit specializations, which may be selected in a
     way we can't track, e.g. by SFINAE.  */
  for (ClassTemplateSpecializationDecl *spec : decl->specializations()) {
    if (spec->getSpecializationKind() == TSK_ExplicitSpecialization) {
      spec->setCompleteDefinitionRequired(true);
      TRY_TO(TraverseDecl(spec));
    }
  }

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitClassTemplateSpecializationDecl(
                            ClassTemplateSpecializationDecl *decl)
{
  /* Assume
    template<T>
    class V{
      T x;
    };
    struct A {int x; }

  V<A> u;
  This call will add the A to the closure.  */
  TRY_TO(TraverseTemplateArguments(decl->getTemplateArgs().asArray()));

  /* In case this specializes a partial specialization, we also need it.  */
  auto specialized = decl->getSpecializedTemplateOrPartial();
  if (auto partial = specialized.dyn_cast<ClassTemplatePartialSpecializationDecl *>()) {
    partial->setCompleteDefinitionRequired(true);
    TRY_TO(TraverseDecl(partial));
  }

  /* This call will add the V to the closure.  Traverse it so that the
     dependencies of the template pattern are also added.  */
  return TraverseDecl(decl->getSpecializedTemplate());
}

bool DeclClosureVisitor::VisitUsingDecl(UsingDecl *decl)
{
  Closure.Add_Single_Decl(decl);

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitUsingShadowDecl(UsingShadowDecl *decl)
{
  /* Assume
    namespace std {
      using ::mbstate_t;
    }

    std::mbstate_t x;
  The reference to `std::mbstate_t` points to the UsingShadowDecl, so we
  need both the `using` declaration and the original declaration.

  Not called automatically by Traverse because UsingShadowDecls are
  implicit.  */
  TRY_TO(TraverseDecl(decl->getIntroducer()));
  TRY_TO(TraverseDecl(decl->getTargetDecl()));

  return VISITOR_CONTINUE;
}

/* ----------- Statements -------------- */

bool DeclClosureVisitor::VisitMemberExpr(MemberExpr *expr)
{
  /* Make sure we grow the closure towards the RecordDecl that contains
     the FieldDecl accessed by the MemberExpr,  for example, if clang see:

     a->x;

     it should grow the closure to include:

     struct AA {
      int x;
     }  */

  if (FieldDecl *field = dyn_cast<FieldDecl>(expr->getMemberDecl())) {
    RecordDecl *record = field->getParent();
    return TraverseDecl(record);
  }

  /* For C++ the member can also be a method, which may be defined outside of
     the class.  */
  if (CXXMethodDecl *method = dyn_cast<CXXMethodDecl>(expr->getMemberDecl())) {
    return TraverseDecl(method);
  }

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitDeclRefExpr(DeclRefExpr *expr)
{
  TRY_TO(TraverseDecl(expr->getDecl()));
  /* For C++ we also need to analyze the name specifier.  */
#if CLANG_VERSION_MAJOR >= 22
  TRY_TO(TraverseNestedNameSpecifier(expr->getQualifier()));
#else
  if (NestedNameSpecifier *nns = expr->getQualifier()) {
    TRY_TO(TraverseNestedNameSpecifier(nns));
  }
#endif

  /* In case the decl was found through a `using` declaration, we also need
     to analyze it.  */
  if (UsingShadowDecl *shadow = dyn_cast<UsingShadowDecl>(expr->getFoundDecl())) {
    TRY_TO(VisitUsingShadowDecl(shadow));
  }

  /* Analyze the decl it references to.  */
  return TraverseDecl(expr->getDecl());
}

bool DeclClosureVisitor::VisitOverloadExpr(OverloadExpr *expr)
{
  /* In a dependent context we do not know which declaration will be selected,
     so analyze all candidates.  */
  for (NamedDecl *decl : expr->decls()) {
    if (UsingShadowDecl *shadow = dyn_cast<UsingShadowDecl>(decl)) {
      TRY_TO(VisitUsingShadowDecl(shadow));
    } else {
      TRY_TO(TraverseDecl(decl));
    }
  }

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitOffsetOfExpr(OffsetOfExpr *expr)
{
  /* Handle the following case:
   *
   * typedef struct {
   *   int x;
   *   int y;
   * } Head;
   *
   * int x = __builtin_offsetof(Head, y);
   */
  unsigned num_components = expr->getNumComponents();
  for (unsigned i = 0; i < num_components; i++) {
    /* Get the RecordDecl referenced in the builtin_offsetof and add to the
       dependency list.  */
    const OffsetOfNode &component = expr->getComponent(i);
    if (component.getKind() == OffsetOfNode::Field) {
      const FieldDecl *field = component.getField();

      RecordDecl *record = (RecordDecl *)field->getParent();
      TRY_TO(TraverseDecl(record));
    }
  }

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitConvertVectorExpr(const ConvertVectorExpr *expr)
{
  const TypeSourceInfo *tsi = expr->getTypeSourceInfo();
  TRY_TO(TraverseTypeLoc(tsi->getTypeLoc()));

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitUnaryOperator(const UnaryOperator *expr)
{
  /* Fix cases where a copy of a struct is necessary but
   * CompleteDefinitionRequired is somehow set to false by clang itself.
   * see small/record-9.c for an example.
   */
  const clang::Type *type = expr->getType().getTypePtr();
  if (TagDecl *tag = type->getAsTagDecl()) {
    tag->setCompleteDefinitionRequired(true);
  }

  return VISITOR_CONTINUE;
}

/* --------- Attributes ----------- */

bool DeclClosureVisitor::VisitCleanupAttr(const CleanupAttr *attr)
{
  TRY_TO(TraverseDecl(attr->getFunctionDecl()));
  return VISITOR_CONTINUE;
}

/* -------- Types ----------------- */

bool DeclClosureVisitor::VisitTagType(const TagType *type)
{
  TRY_TO(TraverseDecl(type->getDecl()));
  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitTypedefType(const TypedefType *type)
{
  TRY_TO(TraverseDecl(type->getDecl()));
  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::VisitUsingType(const UsingType *type)
{
  /* UsingShadowDecls are implicit, so TraverseDecl would ignore them.  */
#if CLANG_VERSION_MAJOR >= 22
  TRY_TO(VisitUsingShadowDecl(type->getDecl()));
#else
  TRY_TO(VisitUsingShadowDecl(type->getFoundDecl()));
#endif
  return VISITOR_CONTINUE;
}

/* -------- C++ Types ------------- */

bool DeclClosureVisitor::VisitTemplateSpecializationType(
                    const TemplateSpecializationType *type)
{
  /* For some reason the Traverse do not run on the original template
     C++ Record, only on its specializations.  Hence do it here.  */
  TRY_TO(TraverseDecl(type->getAsCXXRecordDecl()));

  /* In case the type is dependent there is no specialization to look into,
     so get the template from its name.  */
  return TraverseDecl(type->getTemplateName().getAsTemplateDecl());
}

bool DeclClosureVisitor::VisitDeducedTemplateSpecializationType(
                         const DeducedTemplateSpecializationType *type)
{
  TemplateName template_name = type->getTemplateName();
  return TraverseDecl(template_name.getAsTemplateDecl());
}

/* ----------- Other C++ stuff ----------- */

#if CLANG_VERSION_MAJOR >= 22
bool DeclClosureVisitor::TraverseNestedNameSpecifier(NestedNameSpecifier nns)
{
  switch (nns.getKind()) {
    case NestedNameSpecifier::Kind::Namespace: {
      NamespaceBaseDecl *base =
        const_cast<NamespaceBaseDecl *>(nns.getAsNamespaceAndPrefix().Namespace);
      if (NamespaceDecl *nm = dyn_cast<NamespaceDecl>(base)) {
        /* Do not traverse to avoid adding unecessary childs.  */
        TRY_TO(VisitNamespaceDecl(nm));
      } else {
        TRY_TO(TraverseDecl(base));
      }
      break;
    }

    case NestedNameSpecifier::Kind::MicrosoftSuper:
      /* Something regarding MSVC, but lets put this here.  */
      TRY_TO(TraverseDecl(nns.getAsMicrosoftSuper()));
      break;

    default:
      break;
  }
  return RecursiveASTVisitor::TraverseNestedNameSpecifier(nns);
}
#else
bool DeclClosureVisitor::TraverseNestedNameSpecifier(NestedNameSpecifier *nns)
{
  switch (nns->getKind()) {
    case NestedNameSpecifier::Namespace:
      /* Do not traverse to avoid adding unecessary childs.  */
      TRY_TO(VisitNamespaceDecl(nns->getAsNamespace()));
      break;
    case NestedNameSpecifier::NamespaceAlias:
      TRY_TO(TraverseDecl(nns->getAsNamespaceAlias()));
      break;

    case NestedNameSpecifier::Super:
      /* Something regarding MSVC, but lets put this here.  */
      TRY_TO(TraverseDecl(nns->getAsRecordDecl()));
      break;

    default:
      break;
  }
  return RecursiveASTVisitor::TraverseNestedNameSpecifier(nns);
}
#endif

bool DeclClosureVisitor::TraverseCXXBaseSpecifier(const CXXBaseSpecifier base)
{
  TRY_TO(TraverseType(base.getType()));
  return RecursiveASTVisitor::TraverseCXXBaseSpecifier(base);
}

/* ------ Helper Functions ------ */

bool DeclClosureVisitor::ParentRecordDeclHelper(TagDecl *decl)
{
  /* In case this references a struct/union defined inside a struct (nested
     struct), then we also need to analyze the parent struct.  */
  RecordDecl *parent = dyn_cast<RecordDecl>(decl->getLexicalDeclContext());
  Decl *analysis_parent = nullptr;

  /* Check if we are not coming from an analysis from the RecordDecl, i.e. from
     a stack perspective the Decl that is at the bottom of the Decl.  */
  if (Stack.size() >= 2) {
    analysis_parent = Stack[Stack.size() - 2];
  }

  if (parent && analysis_parent != parent) {
    /* If the parent struct is flagged as not needing a complete definition
       then we need to set it to true, else the nested struct won't be
       output as of only a partial definition of the parent struct is
       output. */
    parent->setCompleteDefinitionRequired(true);

    /* Analyze parent struct.  */
    TRY_TO(TraverseDecl(parent));
  }

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::AnalyzeDeclsWithSameBeginlocHelper(Decl *decl)
{
  SourceManager &SM = AST->getSourceManager();
  ArrayRef<Decl *> decls = Get_Toplev_Decls_With_Same_Beginloc(AST,
                           SM.getExpansionLoc(decl->getBeginLoc()));

  for (auto it = decls.begin(); it != decls.end(); ++it) {
    TRY_TO(TraverseDecl(*it));
  }

  return VISITOR_CONTINUE;
}

bool DeclClosureVisitor::AnalyzePreviousDecls(Decl *decl)
{
  Decl *prev = decl->getPreviousDecl();
  while (prev) {
    TRY_TO(TraverseDecl(prev));
    Closure.Add_Single_Decl(prev);
    prev = prev->getPreviousDecl();
  }

  return VISITOR_CONTINUE;
}
