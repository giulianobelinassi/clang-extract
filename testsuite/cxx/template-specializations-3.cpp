/* { dg-options "-DCE_EXTRACT_FUNCTIONS=main -DCE_NO_EXTERNALIZATION" }*/

/* Check that partial specializations are output when an instantiation comes
   from them.  */

template<typename T, bool IsConst>
  struct selector;

template<typename T>
  struct selector<T, false>
  {
    typedef T type;
  };

int main()
{
  selector<int, false>::type x = 0;
  return x;
}

/* { dg-final { scan-tree-dump "struct selector<T, false>\n *{\n *typedef T type;" } } */
