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

#pragma once

#include <clang/Tooling/Tooling.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Sema/IdentifierResolver.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <unordered_set>

#include "EnumConstTbl.hh"
#include "LLVMMisc.hh"

using namespace clang;

class ClosureSet
{
  public:
  /** Check if a given declaration was already marked as dependency.  */
  inline bool Is_Decl_Marked(Decl *decl)
  { return Dependencies.find(decl) != Dependencies.end(); }

  /** Mark decl as dependencies and all its previous decls versions.  */
  bool Add_Decl_And_Prevs(Decl *decl);

  /** Add a single decl to the set.  */
  void Add_Single_Decl(Decl *decl)
  {
    Dependencies.insert(decl);
  }

  inline std::unordered_set<Decl *> &Get_Set(void)
  {
    return Dependencies;
  }

  inline void Remove_Decl(Decl *decl)
  {
    Dependencies.erase(decl);
  }

  private:
  /** Datastructure holding all Decls required for the functions. This is
      then used to mark which Decls we need to output.

      We use a unordered_set because we need quick lookup, so a hash table
      may be (?) the ideal datastructure for this.  */
  std::unordered_set<Decl*> Dependencies;
};


/// AST visitor for computing the closure of given symbol. From clang:
///
/// A class that does preorder or postorder
/// depth-first traversal on the entire Clang AST and visits each node.
///
/// This class performs three distinct tasks:
///   1. traverse the AST (i.e. go to each node);
///   2. at a given node, walk up the class hierarchy, starting from
///      the node's dynamic type, until the top-most class (e.g. Stmt,
///      Decl, or Type) is reached.
///   3. given a (node, class) combination, where 'class' is some base
///      class of the dynamic type of 'node', call a user-overridable
///      function to actually visit the node.
///
/// These tasks are done by three groups of methods, respectively:
///   1. TraverseDecl(Decl *x) does task #1.  It is the entry point
///      for traversing an AST rooted at x.  This method simply
///      dispatches (i.e. forwards) to TraverseFoo(Foo *x) where Foo
///      is the dynamic type of *x, which calls WalkUpFromFoo(x) and
///      then recursively visits the child nodes of x.
///      TraverseStmt(Stmt *x) and TraverseType(QualType x) work
///      similarly.
///   2. WalkUpFromFoo(Foo *x) does task #2.  It does not try to visit
///      any child node of x.  Instead, it first calls WalkUpFromBar(x)
///      where Bar is the direct parent class of Foo (unless Foo has
///      no parent), and then calls VisitFoo(x) (see the next list item).
///   3. VisitFoo(Foo *x) does task #3.
///
/// These three method groups are tiered (Traverse* > WalkUpFrom* >
/// Visit*).  A method (e.g. Traverse*) may call methods from the same
/// tier (e.g. other Traverse*) or one tier lower (e.g. WalkUpFrom*).
/// It may not call methods from a higher tier.
///
/// Note that since WalkUpFromFoo() calls WalkUpFromBar() (where Bar
/// is Foo's super class) before calling VisitFoo(), the result is
/// that the Visit*() methods for a given node are called in the
/// top-down order (e.g. for a node of type NamespaceDecl, the order will
/// be VisitDecl(), VisitNamedDecl(), and then VisitNamespaceDecl()).
///
/// This scheme guarantees that all Visit*() calls for the same AST
/// node are grouped together.  In other words, Visit*() methods for
/// different nodes are never interleaved.
///
/// Clients of this visitor should subclass the visitor (providing
/// themselves as the template argument, using the curiously recurring
/// template pattern) and override any of the Traverse*, WalkUpFrom*,
/// and Visit* methods for declarations, types, statements,
/// expressions, or other AST nodes where the visitor should customize
/// behavior.  Most users only need to override Visit*.  Advanced
/// users may override Traverse* and WalkUpFrom* to implement custom
/// traversal strategies.  Returning false from one of these overridden
/// functions will abort the entire traversal.
///
/// By default, this visitor tries to visit every part of the explicit
/// source code exactly once.  The default policy towards templates
/// is to descend into the 'pattern' class or function body, not any
/// explicit or implicit instantiations.  Explicit specializations
/// are still visited, and the patterns of partial specializations
/// are visited separately.  This behavior can be changed by
/// overriding shouldVisitTemplateInstantiations() in the derived class
/// to return true, in which case all known implicit and explicit
/// instantiations will be visited at the same time as the pattern
/// from which they were produced.
///
/// By default, this visitor preorder traverses the AST. If postorder traversal
/// is needed, the \c shouldTraversePostOrder method needs to be overridden
/// to return \c true.
class DeclClosureVisitor : public RecursiveASTVisitor<DeclClosureVisitor>
{
  public:
  DeclClosureVisitor(ASTUnit *ast)
    : RecursiveASTVisitor(),
      AST(ast),
      EnumTable(ast)
  {
  }

  /* ---------------- Administrative stuff --------------- */

  /** Tell the RecursiveASTVisitor that we want to visit template instantiations.  */
  bool shouldVisitTemplateInstantiations(void) const //override
  {
    return true;
  }

  /** Check if the Decl was already analyized.  */
  inline bool Already_Analyzed(Decl *decl)
  {
    return AnalyzedDecls.find(decl) != AnalyzedDecls.end();
  }

  inline void Mark_As_Analyzed(Decl *decl)
  {
    AnalyzedDecls.insert(decl);
  }

  /** Check if we can get a partial declaration of `decl` rather than a full
      declaration.  */
  inline TagDecl *Maybe_Partial_Decl(TagDecl *decl)
  {
    if (decl->isCompleteDefinitionRequired()) {
      /* We need the full definition of this struct, enum, or union.  */
      return decl;
    } else {
      /* We don't need the full defintion.  Hide the body for the PrettyPrint
         class.  */
      decl->setCompleteDefinition(false);
      /* FIXME: The PrettyPrint class will attempt to write this declaration
         as the user wrote, that means WITH a body.  To avoid this, we set
         the StartLocation to equal the End of location, which will trigger
         the "clang got confused" mechanism in PrettyPrint and force it to
         be output as a tree dump instead of what the user wrote.  The
         correct way of doing this would be update the source location to
         the correct range.  */
      decl->setLocStart(decl->getEndLoc());
      return decl;
    }
  }

  enum {
    VISITOR_CONTINUE = true,   // Return this for the AST transversal to continue;
    VISITOR_STOP     = false,  // Return this for the AST tranversal to stop completely;
  };

#define TRY_TO(CALL_EXPR)                    \
  if ((CALL_EXPR) == VISITOR_STOP)           \
    return VISITOR_STOP

#define DO_NOT_RUN_IF_ALREADY_ANALYZED(decl) \
  if (Already_Analyzed(decl) == true)        \
    return VISITOR_CONTINUE

  /* Special Traversal functions which marks if a Decl was already analyzed.
     This macro generates them.  */
#define DEF_MARKING_TRAVERSE_DECL(DECL)               \
  bool Traverse##DECL(DECL *decl)                     \
  {                                                   \
    DO_NOT_RUN_IF_ALREADY_ANALYZED(decl);             \
    Mark_As_Analyzed(decl);                           \
    return RecursiveASTVisitor::Traverse##DECL(decl); \
  }

  /* Override of TraverseDecl that marks that the given Decl was analyzed.  So
     far it seems we only need this function for now.  */
  DEF_MARKING_TRAVERSE_DECL(Decl);

  /* -------- C Declarations ----------------- */

  bool VisitFunctionDecl(FunctionDecl *decl)
  {
    if (decl->getBuiltinID() != 0) {
      /* Do not redeclare compiler builtin functions.  */
      return VISITOR_CONTINUE;
    }

    /* If this function is actually a template function specialization, then
       look into it.  */
    if (FunctionTemplateSpecializationInfo *ftsi =
                decl->getTemplateSpecializationInfo()) {
      TRY_TO(VisitFunctionTemplateDecl(ftsi->getTemplate()));
    }

    /* Mark function for output.  */
    FunctionDecl *to_mark = decl;
    if (decl->isStatic()) {
      FunctionDecl *definition = decl->getDefinition();
      /* FIXME: Declared static function in closure without a body?  */
      if (definition != nullptr) {
        to_mark = definition;
        /* Make sure we parse the function version with a body.  */
        TRY_TO(TraverseDecl(to_mark));
      }
    }

    Closure.Add_Decl_And_Prevs(to_mark);

    return VISITOR_CONTINUE;
  }

  bool VisitValueDecl(ValueDecl *decl)
  {
    TRY_TO(TraverseType(decl->getType()));
    return VISITOR_CONTINUE;
  }

  bool VisitRecordDecl(RecordDecl *decl)
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
    } else {
      Closure.Add_Decl_And_Prevs(Maybe_Partial_Decl(decl));
    }

    return VISITOR_CONTINUE;
  }

  bool VisitEnumDecl(EnumDecl *decl)
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
    } else {
      Closure.Add_Decl_And_Prevs(decl);
    }

    return VISITOR_CONTINUE;
  }

  bool VisitTagDecl(TagDecl *decl)
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

  bool VisitEnumConstantDecl(EnumConstantDecl *decl)
  {
    /* Add original EnumDecl it originated.  */
    EnumDecl *enum_decl = EnumTable.Get(decl);
    if (enum_decl) {
      return TraverseDecl(enum_decl);
    }

    return VISITOR_CONTINUE;
  }

  /* Not called automatically by Transverse.  */
  bool VisitTypedefNameDecl(TypedefNameDecl *decl)
  {
    TRY_TO(TraverseType(decl->getUnderlyingType()));
    Closure.Add_Decl_And_Prevs(decl);

    return VISITOR_CONTINUE;
  }

  bool VisitRecordType(const RecordType *type)
  {
    return VISITOR_CONTINUE;
  }

  bool VisitVarDecl(VarDecl *decl)
  {
    /* Avoid adding variables that are not global.  */
    if (decl->hasGlobalStorage()) {
      Closure.Add_Decl_And_Prevs(decl);
    }

    return VISITOR_CONTINUE;
  }

  /* --------- C++ Declarations ---------- */

  bool VisitFunctionTemplateDecl(FunctionTemplateDecl *decl)
  {
    Closure.Add_Decl_And_Prevs(decl);

    return VISITOR_CONTINUE;
  }

  bool VisitTemplateDecl(TemplateDecl *decl)
  {
    Closure.Add_Decl_And_Prevs(decl);

    return VISITOR_CONTINUE;
  }

  bool VisitNamespaceDecl(NamespaceDecl *decl)
  {
    Closure.Add_Decl_And_Prevs(decl);

    return VISITOR_CONTINUE;
  }

  bool VisitCXXRecordDecl(CXXRecordDecl *decl)
  {
    /* In case this is a ClassTemplateSpecialDecl, then analyze it as such.  */
    if (ClassTemplateSpecializationDecl *ctsd =
        dyn_cast<ClassTemplateSpecializationDecl>(decl)) {
      TRY_TO(VisitClassTemplateSpecializationDecl(ctsd));
    }

    Closure.Add_Decl_And_Prevs(decl);

    return VISITOR_CONTINUE;
  }

  bool VisitClassTemplateDecl(ClassTemplateDecl *decl)
  {
    Closure.Add_Decl_And_Prevs(decl);

    return VISITOR_CONTINUE;
  }

  bool VisitClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl *decl)
  {
    /* Assume
      template<T>
      class V{
        T x;
      };
      struct A {int x; }

      V<A> u;
      This call will add the A to the closure.
    */
    TRY_TO(TraverseTemplateArguments(decl->getTemplateArgs().asArray()));

    /* This call will add the V to the closure.  */
    return VisitClassTemplateDecl(decl->getSpecializedTemplate());
  }

  /* ----------- Statements -------------- */

  bool VisitDeclRefExpr(DeclRefExpr *expr)
  {
    TRY_TO(TraverseDecl(expr->getDecl()));
    /* For C++ we also need to analyze the name specifier.  */
    if (NestedNameSpecifier *nns = expr->getQualifier()) {
      TRY_TO(TraverseNestedNameSpecifier(nns));
    }

    /* Analyze the decl it references to.  */
    return TraverseDecl(expr->getDecl());
  }

  bool VisitOffsetOfExpr(OffsetOfExpr *expr)
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

  bool VisitConvertVectorExpr(const ConvertVectorExpr *expr)
  {
    const TypeSourceInfo *tsi = expr->getTypeSourceInfo();
    TRY_TO(TraverseTypeLoc(tsi->getTypeLoc()));

    return VISITOR_CONTINUE;
  }

  /* -------- Types ----------------- */
  bool VisitTagType(const TagType *type)
  {
    TRY_TO(TraverseDecl(type->getDecl()));
    return VISITOR_CONTINUE;
  }

  bool VisitTypedefType(const TypedefType *type)
  {
    TRY_TO(TraverseDecl(type->getDecl()));
    return VISITOR_CONTINUE;
  }

  /* -------- C++ Types ------------- */
  bool VisitTemplateSpecializationType(const TemplateSpecializationType *type)
  {
    /* For some reason the Traverse do not run on the original template
       C++ Record, only on its specializations.  Hence do it here.  */
    return TraverseDecl(type->getAsCXXRecordDecl());
  }

  bool VisitDeducedTemplateSpecializationType(
      const DeducedTemplateSpecializationType *type)
  {
    TemplateName template_name = type->getTemplateName();
    return TraverseDecl(template_name.getAsTemplateDecl());
  }

  bool VisitElaboratedType(const ElaboratedType *type)
  {
    return VISITOR_CONTINUE;
  }

  /* ----------- Other C++ stuff ----------- */
  bool TraverseNestedNameSpecifier(NestedNameSpecifier *nns)
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

  bool TraverseCXXBaseSpecifier(const CXXBaseSpecifier base)
  {
    TRY_TO(TraverseType(base.getType()));
    return RecursiveASTVisitor::TraverseCXXBaseSpecifier(base);
  }

  ClosureSet &Get_Closure(void)
  {
    return Closure;
  }

  const ClosureSet &Get_Closure(void) const
  {
    return Closure;
  }

  void Compute_Closure_Of_Symbols(const std::vector<std::string> &names,
                                 std::unordered_set<std::string> *matched_names = nullptr);

  private:

  /** The ASTUnit object.  */
  ASTUnit *AST;

  /** Datastructure holding all Decls required for the functions. This is
      then used to mark which Decls we need to output.

      We use a unordered_set because we need quick lookup, so a hash table
      may be (?) the ideal datastructure for this.  */
  ClosureSet Closure;

  /** The table maping EnumConstantDecl to its original EnumDecl, used to find
      out where a certain EnumConstantDecl was defined.  */
  EnumConstantTable EnumTable;

  /** The set of all analyzed Decls.  */
  std::unordered_set<Decl *> AnalyzedDecls;
#undef TRY_TO
#undef DO_NOT_RUN_IF_ALREADY_ANALYZED
#undef DEF_MARKING_TRAVERSE_DECL
};
