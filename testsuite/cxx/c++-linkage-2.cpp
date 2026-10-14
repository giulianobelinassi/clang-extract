/* { dg-options "-DCE_EXTRACT_FUNCTIONS=f -DCE_NO_EXTERNALIZATION" }*/
/* { dg-xfail } */
/* Fails until the closure supports declarations inside of
   `extern "C" { ... }` blocks.  */

extern "C" void *gVariable;

void *f(void)
{
  return gVariable;
}

/* { dg-final { scan-tree-dump "extern \"C\" void \*gVariable;" } } */
