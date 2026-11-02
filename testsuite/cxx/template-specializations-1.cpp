/* { dg-options "-DCE_EXTRACT_FUNCTIONS=main -DCE_NO_EXTERNALIZATION" }*/

/* Check that templates referenced in the default template arguments of
   another template are output.  */

template<typename T>
  struct is_small
  {
    static constexpr bool value = sizeof(T) < 4;
  };

template<typename T, bool Small = is_small<T>::value>
  struct storage
  {
    static constexpr int size = Small ? 1 : 2;
  };

int main()
{
  return storage<char>::size;
}

/* { dg-final { scan-tree-dump "struct is_small\n *{\n *static constexpr bool value = sizeof\(T\) < 4;" } } */
/* { dg-final { scan-tree-dump "template<typename T, bool Small = is_small<T>::value>" } } */
