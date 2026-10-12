/* { dg-options "-DCE_EXTRACT_FUNCTIONS=g -DCE_NO_EXTERNALIZATION" }*/

/* Check that out-of-line definitions of a nested class and of a method of a
   class template are output with their template header.  */

template<typename T>
  struct A
  {
    struct B;
    int f(void);
  };

template<typename T>
  struct A<T>::B
  {
    T x;
  };

template<typename T>
  int A<T>::f(void)
  {
    B b;
    return 0;
  }

int g(void)
{
  A<int> a;
  return a.f();
}

/* { dg-final { scan-tree-dump "template<typename T>\n *struct A<T>::B\n *{\n *T x;" } } */
/* { dg-final { scan-tree-dump "template<typename T>\n *int A<T>::f\(void\)\n *{\n *B b;" } } */
/* { dg-final { scan-tree-dump-not "Full definition was removed" } } */
