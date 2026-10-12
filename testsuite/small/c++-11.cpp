/* { dg-options "-DCE_EXTRACT_FUNCTIONS=f -DCE_NO_EXTERNALIZATION" }*/

/* Check that types found through a `using` declaration are output together
   with the `using` declaration and its enclosing namespace.  */

#include <cwchar>

std::mbstate_t f(void)
{
  std::mbstate_t s = {};
  return s;
}

/* { dg-final { scan-tree-dump "typedef __mbstate_t mbstate_t;" } } */
/* { dg-final { scan-tree-dump "namespace std {\n.*\n *using ::mbstate_t;" } } */
/* { dg-final { scan-tree-dump "std::mbstate_t f\(void\)" } } */
