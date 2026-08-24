/* { dg-options "-DCE_EXTRACT_FUNCTIONS=port_make_channel -DCE_NO_EXTERNALIZATION" }*/

typedef struct ssl_st SSL;
typedef union bio_addr_st BIO_ADDR;

typedef struct quic_channel_st QUIC_CHANNEL;

union bio_addr_st {
  int a;
};

struct quic_channel_st {
    SSL *tls;
    BIO_ADDR cur_peer_addr;
};

QUIC_CHANNEL *port_make_channel(SSL *tls)
{
    QUIC_CHANNEL *ch;
    ch->tls = tls;
    return ch;
}

/* { dg-final { scan-tree-dump "union bio_addr_st {" } } */
