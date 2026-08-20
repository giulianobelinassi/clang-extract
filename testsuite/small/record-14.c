/* { dg-options "-DCE_EXTRACT_FUNCTIONS=ossl_cmp_calc_protection -DCE_NO_EXTERNALIZATION" }*/

typedef struct ossl_cmp_pkiheader_st OSSL_CMP_PKIHEADER;
typedef struct ossl_cmp_msg_st OSSL_CMP_MSG;

struct ossl_cmp_msg_st {
    OSSL_CMP_PKIHEADER *header;
};

struct ossl_cmp_msg_st;

typedef struct ossl_cmp_ctx_st OSSL_CMP_CTX;

typedef struct ossl_cmp_protectedpart_st {
    OSSL_CMP_PKIHEADER *header;
} OSSL_CMP_PROTECTEDPART;

void *ossl_cmp_calc_protection(const OSSL_CMP_CTX *ctx,
                                          const OSSL_CMP_MSG *msg)
{
    OSSL_CMP_PROTECTEDPART prot_part;
    prot_part.header = msg->header;
    return ((void*)0);
}

/* { dg-final { scan-tree-dump "struct ossl_cmp_msg_st {\n *OSSL_CMP_PKIHEADER \*header;" } } */
