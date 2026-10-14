/* { dg-options "-DCE_EXTRACT_FUNCTIONS=f -DCE_NO_EXTERNALIZATION" }*/

/* Check that the namespaces enclosing a decl are output, even if they are not
   referenced in the code, as the inline namespace `v1` below.  */

namespace n {
  inline namespace v1 {
    int g(void) { return 1; }
  }
}

int f(void)
{
  return n::g();
}

/* { dg-final { scan-tree-dump "namespace n {" } } */
/* { dg-final { scan-tree-dump "inline namespace v1 {" } } */
/* { dg-final { scan-tree-dump "int g\(void\) { return 1; }" } } */
