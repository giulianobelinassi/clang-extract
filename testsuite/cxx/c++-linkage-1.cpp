/* { dg-options "-DCE_EXTRACT_FUNCTIONS=f -DCE_NO_EXTERNALIZATION" }*/
/* { dg-xfail } */
/* Fails until the closure supports declarations inside of
   `extern "C" { ... }` blocks.  */

extern "C" void *memcpy(void *a, void *b, unsigned long n);

void *f(void *a, void *b, unsigned long n)
{
  return memcpy(a, b, n);
}

/* { dg-final { scan-tree-dump "extern \"C\" void \*memcpy" } } */
