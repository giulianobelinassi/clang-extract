/* { dg-options "-DCE_EXTRACT_FUNCTIONS=g -DCE_NO_EXTERNALIZATION" }*/

/* Same as c++-11.cpp, but for a global variable.  */

#include <cwchar>

std::mbstate_t g;

/* { dg-final { scan-tree-dump "typedef __mbstate_t mbstate_t;" } } */
/* { dg-final { scan-tree-dump "namespace std {\n.*\n *using ::mbstate_t;" } } */
/* { dg-final { scan-tree-dump "std::mbstate_t g;" } } */
