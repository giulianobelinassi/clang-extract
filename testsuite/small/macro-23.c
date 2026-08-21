/* { dg-options "-DCE_EXTRACT_FUNCTIONS=f -DCE_EXPORT_SYMBOLS=OSSL_CMP_PROTECTEDPART_it" }*/
/* { dg-xfail }*/

# define DECLARE_ASN1_FUNCTIONS_attr(attr, type)                           \
   DECLARE_ASN1_FUNCTIONS_name_attr(attr, type, type)
# define DECLARE_ASN1_FUNCTIONS(type)                                      \
   DECLARE_ASN1_FUNCTIONS_attr(extern, type)

# define DECLARE_ASN1_FUNCTIONS_name_attr(attr, type, name)                \
   DECLARE_ASN1_ALLOC_FUNCTIONS_name_attr(attr, type, name)                \
   DECLARE_ASN1_ENCODE_FUNCTIONS_name_attr(attr, type, name)

# define DECLARE_ASN1_FUNCTIONS_attr(attr, type)                            \
    DECLARE_ASN1_FUNCTIONS_name_attr(attr, type, type)
# define DECLARE_ASN1_FUNCTIONS(type)                                       \
    DECLARE_ASN1_FUNCTIONS_attr(extern, type)

# define DECLARE_ASN1_ALLOC_FUNCTIONS_attr(attr, type)                      \
    DECLARE_ASN1_ALLOC_FUNCTIONS_name_attr(attr, type, type)
# define DECLARE_ASN1_ALLOC_FUNCTIONS(type)                                 \
    DECLARE_ASN1_ALLOC_FUNCTIONS_attr(extern, type)

# define DECLARE_ASN1_FUNCTIONS_name_attr(attr, type, name)                 \
    DECLARE_ASN1_ALLOC_FUNCTIONS_name_attr(attr, type, name)                \
    DECLARE_ASN1_ENCODE_FUNCTIONS_name_attr(attr, type, name)
# define DECLARE_ASN1_FUNCTIONS_name(type, name)                            \
    DECLARE_ASN1_FUNCTIONS_name_attr(extern, type, name)

# define DECLARE_ASN1_ENCODE_FUNCTIONS_attr(attr, type, itname, name)       \
    DECLARE_ASN1_ENCODE_FUNCTIONS_only_attr(attr, type, name)               \
    DECLARE_ASN1_ITEM_attr(attr, itname)
# define DECLARE_ASN1_ENCODE_FUNCTIONS(type, itname, name)                  \
    DECLARE_ASN1_ENCODE_FUNCTIONS_attr(extern, type, itname, name)

# define DECLARE_ASN1_ENCODE_FUNCTIONS_name_attr(attr, type, name)          \
    DECLARE_ASN1_ENCODE_FUNCTIONS_attr(attr, type, name, name)
# define DECLARE_ASN1_ENCODE_FUNCTIONS_name(type, name) \
    DECLARE_ASN1_ENCODE_FUNCTIONS_name_attr(extern, type, name)

# define DECLARE_ASN1_ENCODE_FUNCTIONS_only_attr(attr, type, name)          \
    attr type *d2i_##name(type **a, const unsigned char **in, long len);    \
    attr int i2d_##name(const type *a, unsigned char **out);
# define DECLARE_ASN1_ENCODE_FUNCTIONS_only(type, name)                     \
    DECLARE_ASN1_ENCODE_FUNCTIONS_only_attr(extern, type, name)

# define DECLARE_ASN1_NDEF_FUNCTION_attr(attr, name)                        \
    attr int i2d_##name##_NDEF(const name *a, unsigned char **out);
# define DECLARE_ASN1_NDEF_FUNCTION(name)                                   \
    DECLARE_ASN1_NDEF_FUNCTION_attr(extern, name)

# define DECLARE_ASN1_ALLOC_FUNCTIONS_name_attr(attr, type, name)           \
    attr type *name##_new(void);                                            \
    attr void name##_free(type *a);
# define DECLARE_ASN1_ALLOC_FUNCTIONS_name(type, name)                      \
    DECLARE_ASN1_ALLOC_FUNCTIONS_name_attr(extern, type, name)

# define DECLARE_ASN1_DUP_FUNCTION_attr(attr, type)                         \
    DECLARE_ASN1_DUP_FUNCTION_name_attr(attr, type, type)
# define DECLARE_ASN1_DUP_FUNCTION(type)                                    \
    DECLARE_ASN1_DUP_FUNCTION_attr(extern, type)

# define DECLARE_ASN1_DUP_FUNCTION_name_attr(attr, type, name)              \
    attr type *name##_dup(const type *a);
# define DECLARE_ASN1_DUP_FUNCTION_name(type, name)                         \
    DECLARE_ASN1_DUP_FUNCTION_name_attr(extern, type, name)

# define DECLARE_ASN1_PRINT_FUNCTION_attr(attr, stname)                     \
    DECLARE_ASN1_PRINT_FUNCTION_fname_attr(attr, stname, stname)
# define DECLARE_ASN1_PRINT_FUNCTION(stname)                                \
    DECLARE_ASN1_PRINT_FUNCTION_attr(extern, stname)

# define DECLARE_ASN1_PRINT_FUNCTION_fname_attr(attr, stname, fname)        \
    attr int fname##_print_ctx(BIO *out, const stname *x, int indent,       \
                               const ASN1_PCTX *pctx);
# define DECLARE_ASN1_PRINT_FUNCTION_fname(stname, fname)                   \
    DECLARE_ASN1_PRINT_FUNCTION_fname_attr(extern, stname, fname)

# define DECLARE_ASN1_ITEM_attr(attr, name)                                 \
    attr const ASN1_ITEM * name##_it(void);
# define DECLARE_ASN1_ITEM(name)                                            \
    DECLARE_ASN1_ITEM_attr(extern, name)


#define ASN1_ITEM_rptr(ref) (ref##_it())

typedef struct asn_item ASN1_ITEM;

typedef struct pkiheader OSSL_CMP_PKIHEADER;
typedef struct pkibody   OSSL_CMP_PKIBODY;

typedef struct ossl_cmp_protectedpart_st {
    OSSL_CMP_PKIHEADER *header;
    OSSL_CMP_PKIBODY *body;
} OSSL_CMP_PROTECTEDPART;

DECLARE_ASN1_FUNCTIONS(OSSL_CMP_PROTECTEDPART);

void *f(void)
{
  return (void *) ASN1_ITEM_rptr(OSSL_CMP_PROTECTEDPART);
}

/* { dg-final { scan-tree-dump "\(\*klpe_OSSL_CMP_PROTECTEDPART_it\)\(\)" } } */
