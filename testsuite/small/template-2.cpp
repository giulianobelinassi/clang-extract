/* { dg-options "-DCE_EXTRACT_FUNCTIONS=f -DCE_NO_EXTERNALIZATION" }*/

/* Check that a function template inside of a namespace is output, and not
   its instantiation.  */

namespace std {
  template<typename _Tp>
    inline const _Tp&
    min(const _Tp& __a, const _Tp& __b)
    {
      if (__b < __a)
        return __b;
      return __a;
    }
}

int f(int a, int b)
{
  return std::min(a, b);
}

/* { dg-final { scan-tree-dump "namespace std {" } } */
/* { dg-final { scan-tree-dump "template<typename _Tp>\n *inline const _Tp&\n *min\(const _Tp& __a, const _Tp& __b\)" } } */
/* { dg-final { scan-tree-dump-not "min\(const int" } } */
