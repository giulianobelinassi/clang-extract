/* { dg-options "-DCE_EXTRACT_FUNCTIONS=main -DCE_NO_EXTERNALIZATION" }*/

/* Check that templates referenced in the body of a template are output, even
   if that part of the template is never instantiated.  The whole template is
   output, so `helper` is needed to compile `unused`.  */

template<typename T>
  struct helper
  {
    static T get() { return T(); }
  };

template<typename T>
  struct wrapper
  {
    T value;
    T unused() { return helper<T>::get(); }
  };

int main()
{
  wrapper<int> w;
  w.value = 0;
  return w.value;
}

/* { dg-final { scan-tree-dump "struct helper\n *{\n *static T get\(\) { return T\(\); }" } } */
/* { dg-final { scan-tree-dump "T unused\(\) { return helper<T>::get\(\); }" } } */
