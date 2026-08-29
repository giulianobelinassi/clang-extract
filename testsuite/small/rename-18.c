/* { dg-options "-DCE_EXTRACT_FUNCTIONS=f -DCE_EXPORT_SYMBOLS=h,g -DCE_NO_STRONG_EXT_RENAME" }*/

int g(int);

int h(int x)
{
  return g(x);
}

int g(int x)
{
  return h(x);
}

int f(void)
{
  return g(3);
}

int g(int);

/* { dg-final { scan-tree-dump "return g\(3\);" } } */
/* { dg-final { scan-tree-dump "static int \(\*klpe_g\)\(int\) __attribute__\(\(used\)\);|__attribute__\(\(used\)\) static int \(\*klpe_g\)\(int\);" } } */
/* { dg-final { scan-tree-dump "#define g \(\*klpe_g\)" } } */
