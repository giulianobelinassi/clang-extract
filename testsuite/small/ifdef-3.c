/* { dg-options "-DCE_EXTRACT_FUNCTIONS=f -DCE_NO_EXTERNALIZATION" }*/

/* Check that __has_include evaluates the same way when the output, which
   have all headers expanded, is parsed again.  */

#include <sys/single_threaded.h>

int f(void)
{
#if __has_include(<sys/single_threaded.h>)
  return __libc_single_threaded;
#else
  return undeclared_x;
#endif
}

/* { dg-final { scan-tree-dump "extern char __libc_single_threaded;" } } */
/* { dg-final { scan-tree-dump "#if __has_include\(<sys/single_threaded.h>\)" } } */
