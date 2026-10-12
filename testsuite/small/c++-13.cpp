/* { dg-options "-DCE_EXTRACT_FUNCTIONS=f -DCE_NO_EXTERNALIZATION" }*/

/* Check that declarations inside of `extern "C" { ... }` blocks are output,
   but only the ones that are needed.  Here wmemcmp is declared inside such
   block in wchar.h and used by char_traits<wchar_t>::compare.  */

#include <bits/char_traits.h>

int f(const wchar_t *a, const wchar_t *b)
{
  return std::char_traits<wchar_t>::compare(a, b, 3);
}

/* { dg-final { scan-tree-dump "extern \"C\" {" } } */
/* { dg-final { scan-tree-dump "extern int wmemcmp" } } */
/* { dg-final { scan-tree-dump "using ::wmemcmp;" } } */
/* { dg-final { scan-tree-dump-not "wcstok" } } */
