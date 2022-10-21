#include "clang/Analysis/CallGraph.h"
#include "clang/Frontend/ASTUnit.h"

#include <vector>
#include <unordered_set>

using namespace clang;

class FunctionExternalizeFinder
{
  public:
  FunctionExternalizeFinder(ASTUnit *ast, std::vector<std::string> &to_extract, std::vector<std::string> &to_exernalize);

  bool Should_Externalize(CallGraphNode *node);
  bool Should_Externalize(FunctionDecl *decl);

  void Run_Analysis(void);

  std::vector<std::string> Get_To_Externalize(void);

  private:

  bool Analyze_Node(CallGraphNode *);

  bool Externalize_Variables(FunctionDecl *decl);
  bool Externalize_Variables(Stmt *stmt);

  inline bool Must_Not_Externalize(const std::string &name)
  {
    return MustNotExternalize.find(name) != MustNotExternalize.end();
  }

  inline bool Is_Marked_For_Externalization(const std::string &name)
  {
    return MustExternalize.find(name) != MustExternalize.end();
  }

  inline bool Mark_For_Externalization(const std::string &name)
  {
    bool externalized = false;

    if (!Is_Marked_For_Externalization(name)) {
      externalized = true;
      MustExternalize.insert(name);
    }

    return externalized;
  }

  std::unordered_set<std::string> MustExternalize;
  std::unordered_set<std::string> MustNotExternalize;

  ASTUnit *AST;
};