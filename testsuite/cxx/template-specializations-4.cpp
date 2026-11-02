/* { dg-options "-DCE_EXTRACT_FUNCTIONS=main -DCE_NO_EXTERNALIZATION" }*/
/* Fails until the closure analyzes the template declarations and their
   explicit and partial specializations.  */

/* Check that explicit specializations are output when they are only selected
   by the default template arguments of another template, as in
   std::is_integral and std::make_unsigned.  */

template<typename T>
  struct is_wide
  {
    static constexpr bool value = false;
  };

template<>
  struct is_wide<wchar_t>
  {
    static constexpr bool value = true;
  };

template<typename T, bool Wide = is_wide<T>::value>
  struct pick
  {
    static constexpr int v = 0;
  };

template<typename T>
  struct pick<T, true>
  {
    static constexpr int v = 1;
  };

int main()
{
  return pick<wchar_t>::v;
}

/* { dg-final { scan-tree-dump "struct is_wide<wchar_t>\n *{\n *static constexpr bool value = true;" } } */
/* { dg-final { scan-tree-dump "struct pick<T, true>\n *{\n *static constexpr int v = 1;" } } */
