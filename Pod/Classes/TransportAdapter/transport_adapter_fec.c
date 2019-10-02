


#include <pjmedia/vid_stream.h>
#include <pjmedia/stream.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/log.h>
#include "transport_adapter_fec.h"
#include <time.h>
#include <pjmedia/rtp.h>




#define shift_floor(arg)    ((int)(arg + 32768.) - 32768)
#define shift_ceil(arg)        (32768 - (int)(32768. - arg))


typedef struct fec_ext_hdr
{
    pj_uint32_t                sn;                        
    pj_uint16_t                esi;                    
    pj_uint16_t                k;                        
    pj_uint16_t                n;                        
    pj_uint16_t                esl;                    
    pj_uint16_t                s;                        
    pj_uint16_t                rsrv;                    
} fec_ext_hdr;



const char *fec_sdp_attrs [] =
{
    "FEC-declaration",
    "FEC-redundancy-level",
    "FEC",
    "FEC-OTI-extension",
    
    
    
    
    
    
    NULL
};


#define K_MIN                4                                    
#define K_MAX                20                                    
#define CODE_RATE_MAX        .667                                
#define N_MAX                shift_ceil(K_MAX / CODE_RATE_MAX)    

#if defined(K_MIN) && defined(K_MAX) && (K_MIN >= K_MAX)
#error Check redundancy parameters K_MAX, K_MIN
#endif


#define SYMBOL_SIZE_MAX    PJMEDIA_MAX_MTU


#define RTP_VERSION    2                                            
#define RTCP_SR        200                                            
#define RTCP_RR        201                                            
#define RTCP_FIR    206                                            
#define RTP_EXT_PT  127                                            


#define THIS_FILE   "tp_adap_fec"


static pj_status_t    transport_get_info        (pjmedia_transport *tp, pjmedia_transport_info *info);
static void            transport_rtp_cb        (void * user_data, void * pkt, pj_ssize_t size);

static pj_status_t    transport_attach        (pjmedia_transport *tp,    void *user_data, const pj_sockaddr_t *rem_addr,    const pj_sockaddr_t *rem_rtcp, unsigned addr_len, void(*rtp_cb)(void*, void*, pj_ssize_t), void(*rtcp_cb)(void*, void*, pj_ssize_t));
#if defined(PJ_VERSION_NUM_MAJOR) && (PJ_VERSION_NUM_MAJOR == 2) && defined(PJ_VERSION_NUM_MINOR) && (PJ_VERSION_NUM_MINOR >= 6)
static pj_status_t    transport_attach2        (pjmedia_transport *tp, pjmedia_transport_attach_param *att_prm);
#endif
static void            transport_detach        (pjmedia_transport *tp, void *strm);
static pj_status_t    transport_send_rtp        (pjmedia_transport *tp, const void *pkt, pj_size_t size);
static pj_status_t    transport_send_rtcp        (pjmedia_transport *tp, const void *pkt, pj_size_t size);
static pj_status_t    transport_send_rtcp2    (pjmedia_transport *tp, const pj_sockaddr_t *addr, unsigned addr_len, const void *pkt, pj_size_t size);
static pj_status_t    transport_media_create    (pjmedia_transport *tp, pj_pool_t *sdp_pool, unsigned options, const pjmedia_sdp_session *rem_sdp, unsigned media_index);
static pj_status_t    transport_encode_sdp    (pjmedia_transport *tp, pj_pool_t *sdp_pool, pjmedia_sdp_session *local_sdp, const pjmedia_sdp_session *rem_sdp, unsigned media_index);
static pj_status_t    transport_media_start    (pjmedia_transport *tp, pj_pool_t *pool, const pjmedia_sdp_session *local_sdp, const pjmedia_sdp_session *rem_sdp, unsigned media_index);
static pj_status_t    transport_media_stop    (pjmedia_transport *tp);
static pj_status_t    transport_simulate_lost    (pjmedia_transport *tp, pjmedia_dir dir, unsigned pct_lost);
static pj_status_t    transport_destroy        (pjmedia_transport *tp);




static void*        fec_dec_cb                (void *context , pj_uint32_t size, pj_uint32_t esi);

static void*        fec_dec_hdr                (const void *pkt, fec_ext_hdr *fec_hdr);

static pj_uint16_t    fec_dec_pkt                (void * const dst, void *pkt, pj_ssize_t size, pj_bool_t rtp);

static pj_status_t    fec_dec_reset            (void *user_data , pj_uint16_t k, pj_uint16_t n, pj_uint16_t len);

static pj_status_t    fec_dec_len                (void *user_data );


static pj_status_t    fec_enc_pkt                (void *user_data , const void *pkt, pj_size_t size);

static pj_uint16_t    fec_enc_src                (void *dst, const void *pkt, pj_uint16_t size, const fec_ext_hdr *fec_hdr, const pjmedia_rtp_ext_hdr *ext_hdr);

static pj_uint16_t  fec_enc_rpr                (void *dst, pjmedia_rtp_session *ses, int ts_len, const void *payload, pj_uint16_t payload_len, const fec_ext_hdr *fec_hdr, const pjmedia_rtp_ext_hdr *ext_hdr);

static pj_status_t    fec_enc_reset            (void *user_data , pj_uint16_t n, pj_uint16_t len);

static pj_status_t    fec_enc_len                (void *user_data );


static pj_status_t    transport_send_rtcp_fir(pjmedia_transport *tp);


static struct pjmedia_transport_op tp_adapter_op =
{
    &transport_get_info,
#if defined(PJ_VERSION_NUM_MAJOR) && (PJ_VERSION_NUM_MAJOR == 2) && defined(PJ_VERSION_NUM_MINOR) && (PJ_VERSION_NUM_MINOR <= 6)
    &transport_attach,
#else
    NULL,
#endif
    &transport_detach,
    &transport_send_rtp,
    &transport_send_rtcp,
    &transport_send_rtcp2,
    &transport_media_create,
    &transport_encode_sdp,
    &transport_media_start,
    &transport_media_stop,
    &transport_simulate_lost,
    &transport_destroy,
#if defined(PJ_VERSION_NUM_MAJOR) && (PJ_VERSION_NUM_MAJOR == 2) && defined(PJ_VERSION_NUM_MINOR) && (PJ_VERSION_NUM_MINOR >= 6)
    &transport_attach2,
#endif
};


struct tp_adapter
{
    pjmedia_transport    base;
    pj_bool_t            del_base;
    pj_pool_t            *pool;
    
    
    void                *stream_user_data;
    void                *stream_ref;
    void                (*stream_rtp_cb)(void *user_data, void *pkt, pj_ssize_t);
    void                (*stream_rtcp_cb)(void *user_data, void *pkt, pj_ssize_t);
    
    pjmedia_type        stream_type;
    
    
    pjmedia_transport        *slave_tp;                    
    
    
    of_codec_id_t            codec_id;                    
    
    of_session_t            *dec_ses;                    
    
    of_parameters_t            *enc_params;                
    of_parameters_t            *dec_params;                
    
    of_rs_2_m_parameters_t    enc_rs_params;                
    of_ldpc_parameters_t    enc_ldps_params;            
    
    of_rs_2_m_parameters_t    dec_rs_params;                
    of_ldpc_parameters_t    dec_ldps_params;            
    
    pjmedia_rtp_session        *rtp_tx_ses;                
    pjmedia_rtcp_session    *rtcp_ses;                    
    
    pjmedia_rtp_hdr            rtp_hdr;                    
    pjmedia_rtp_ext_hdr        ext_hdr;                    
    fec_ext_hdr                fec_hdr;                    
    
    
    pj_uint16_t                snd_k_max;                    
    pj_uint16_t                snd_k;                        
    pj_uint32_t                snd_sn;                        
    pj_uint16_t                snd_len;                    
    pj_bool_t                snd_ready;
    
    pj_uint16_t                rcv_k;                        
    pj_uint32_t                rcv_sn;                        
    
    pj_uint8_t                rtcp_fir_sn;                
    
    double                    tx_loss;                    
    double                    tx_code_rate;                
    
    
    void*        enc_symbols_ptr[N_MAX];                        
    pj_uint8_t    enc_symbols_buf[SYMBOL_SIZE_MAX * N_MAX];    
    pj_uint16_t    enc_symbols_size[N_MAX];                    
    pj_uint8_t    enc_symbol_buf[SYMBOL_SIZE_MAX];            
    
    
    void*        dec_symbols_ptr[N_MAX];                        
    pj_uint8_t    dec_symbols_buf[SYMBOL_SIZE_MAX * N_MAX];    
    pj_uint16_t    dec_symbols_size[N_MAX];                    
    
#if 1
    pj_bool_t    old_client;
#endif 
    
};


static void dump_pkt(const void *buf, const pj_uint32_t size, pj_uint16_t esi, const char *type)
{
    char    *ptr;
    pj_uint16_t    n = size;
    char str[SYMBOL_SIZE_MAX * 3] = { '\0' }, *p = str;
    
    p += sprintf(p, "%s_%03u size=%u: ", type, esi, size);
    p += sprintf(p, "0x");
    for (ptr = (char *)buf; n > 0; n--, ptr++)
    {
        p += sprintf(p, "%hhX", *ptr);
    }
    p += sprintf(p, "\n");
    
    PJ_LOG(4, (THIS_FILE, str));
}


static pj_status_t fec_enc_reset(void *user_data, pj_uint16_t n, pj_uint16_t len)
{
    struct tp_adapter *a = (struct tp_adapter *)user_data;
    pj_uint16_t esi;
    
    if (!a)
        return PJ_EINVAL;
    
    
    for (esi = 0; esi < n; esi++)
    {
        memset(a->enc_symbols_ptr[esi], 0, len);
        
        a->enc_symbols_size[esi] = 0;
    }
    
    
    a->snd_k = 0;
    a->snd_len = 0;
    a->snd_ready = PJ_FALSE;
    
    return PJ_SUCCESS;
}


static pj_status_t fec_dec_reset(void *user_data, pj_uint16_t k, pj_uint16_t n, pj_uint16_t len)
{
    struct tp_adapter *a = (struct tp_adapter *)user_data;
    pj_status_t status = PJ_SUCCESS;
    pj_uint16_t esi;
    
    if (!a)
        return PJ_EINVAL;
    
    
    if (a->dec_ses)
        of_release_codec_instance(a->dec_ses);
    
    
    esi = a->dec_params->nb_source_symbols + a->dec_params->nb_repair_symbols;
    while (esi--)
    {
        memset(a->dec_symbols_ptr[esi], 0, a->dec_params->encoding_symbol_length);
        
        a->dec_symbols_size[esi] = 0;
    }
    
    a->rcv_k = 0;
    a->dec_params->nb_source_symbols = k;
    a->dec_params->nb_repair_symbols = n - k;
    a->dec_params->encoding_symbol_length = len;
    
    
    if (of_create_codec_instance(&a->dec_ses, a->codec_id, OF_DECODER, 2 ) != OF_STATUS_OK)
    {
        status = PJ_EINVAL;
        PJ_LOG(4, (THIS_FILE, "Create decoder instance failed"));
    }
    
    if (status == PJ_SUCCESS && of_set_fec_parameters(a->dec_ses, a->dec_params) != OF_STATUS_OK)
    {
        status = PJ_EINVAL;
        PJ_LOG(4, (THIS_FILE, "Set parameters failed for decoder codec_id=%d", a->codec_id));
    }
    
    
    if (of_set_callback_functions(a->dec_ses, &fec_dec_cb, NULL, a) != OF_STATUS_OK)
    {
        status = PJ_EINVAL;
        PJ_LOG(4, (THIS_FILE, "Set callback functions failed for decoder with codec_id=%d", a->codec_id));
    }
    
    
    
    if (status != PJ_SUCCESS && a->dec_ses)
        of_release_codec_instance(a->dec_ses);
    
    return status;
}

static void* fec_dec_len_cb(void *context, unsigned size, unsigned esi)
{
    pj_uint16_t *dec_len_tab = (pj_uint16_t *)context;
    
    return &dec_len_tab[esi];
}

static pj_status_t    fec_dec_len(void *user_data)
{
    struct tp_adapter        *a = (struct tp_adapter *)user_data;
    of_session_t            *dec_ses;
    pj_uint16_t                esi, k, n;
    of_rs_2_m_parameters_t    dec_params;
    
    if (of_create_codec_instance(&dec_ses, OF_CODEC_REED_SOLOMON_GF_2_M_STABLE, OF_DECODER, 2) != OF_STATUS_OK)
        return PJ_EINVAL;
    
    dec_params.nb_source_symbols = a->dec_params->nb_source_symbols;
    dec_params.nb_repair_symbols = a->dec_params->nb_repair_symbols;
    dec_params.encoding_symbol_length = sizeof(pj_uint16_t);
    dec_params.m = 8;
    
    if (of_set_fec_parameters(dec_ses, (of_parameters_t *)&dec_params) != OF_STATUS_OK
        || of_set_callback_functions(dec_ses, &fec_dec_len_cb, NULL, (void *)a->dec_symbols_size) != OF_STATUS_OK)
    {
        PJ_LOG(4, (THIS_FILE, "Set parameters failed for length decoder codec_id=%d", a->codec_id));
        
        of_release_codec_instance(dec_ses);
        
        return PJ_EINVAL;
    }
    
    k = dec_params.nb_source_symbols;
    n = dec_params.nb_source_symbols + dec_params.nb_repair_symbols;
    
    for (esi = 0; esi < n; esi++)
    {
        if(a->dec_symbols_size[esi])
            of_decode_with_new_symbol(dec_ses, (void *)&a->dec_symbols_size[esi], esi);
        
        if (of_is_decoding_complete(dec_ses))
            break;
    }
    
    of_release_codec_instance(dec_ses);
    
    return PJ_SUCCESS;
}

static pj_status_t    fec_enc_len(void *user_data)
{
    struct tp_adapter        *a = (struct tp_adapter *)user_data;
    of_session_t            *enc_ses;
    pj_uint16_t                esi, n, k;
    of_rs_2_m_parameters_t    enc_params;
    void*                    enc_symbols_tab[N_MAX];
    
    if (of_create_codec_instance(&enc_ses, OF_CODEC_REED_SOLOMON_GF_2_M_STABLE, OF_ENCODER, 2) != OF_STATUS_OK)
        return PJ_EINVAL;
    
    enc_params.nb_source_symbols = a->enc_params->nb_source_symbols;
    enc_params.nb_repair_symbols = a->enc_params->nb_repair_symbols;
    enc_params.encoding_symbol_length = sizeof(pj_uint16_t);
    enc_params.m = 8;
    
    
    if (of_set_fec_parameters(enc_ses, (of_parameters_t *)&enc_params) != OF_STATUS_OK)
    {
        PJ_LOG(4, (THIS_FILE, "Set parameters failed for length encoder codec_id=%d", a->codec_id));
        
        of_release_codec_instance(enc_ses);
        
        return PJ_EINVAL;
    }
    
    k = enc_params.nb_source_symbols;
    n = enc_params.nb_source_symbols + enc_params.nb_repair_symbols;
    
    for (esi = 0; esi < n; esi++)
        enc_symbols_tab[esi] = (void *)&a->enc_symbols_size[esi];
    
    for (esi = k; esi < n; esi++)
        of_build_repair_symbol(enc_ses, enc_symbols_tab, esi);
    
    of_release_codec_instance(enc_ses);
    
    return PJ_SUCCESS;
}

static pj_status_t fec_enc_pkt(void *user_data, const void *pkt, pj_size_t size)
{
    struct tp_adapter    *a = (struct tp_adapter *)user_data;
    pj_status_t            status = PJ_SUCCESS;
    pj_uint16_t            esi, n, k, len;
    of_session_t        *enc_ses;
    
    
    
    if (a->snd_k < a->snd_k_max)
    {
        
        memcpy(a->enc_symbols_ptr[a->snd_k], pkt, size);
        
        
        a->enc_symbols_size[a->snd_k] = (pj_uint16_t)size;
        
        a->snd_k++;
        
        
        if (size > a->snd_len)
            a->snd_len = (pj_uint16_t)size;
    }
    
    pjmedia_rtp_hdr *rtp_hdr = (pjmedia_rtp_hdr *)pkt;
    
    
    
    if ((a->snd_k < a->snd_k_max && !rtp_hdr->m) || (rtp_hdr->m && a->snd_k < K_MIN))
        return PJ_SUCCESS;
    
    k = a->snd_k;
    
    n = shift_ceil(k / a->tx_code_rate);
    len = a->snd_len;
    
    
    a->snd_sn++;
    
    
    a->enc_params->nb_source_symbols = k;
    a->enc_params->nb_repair_symbols = n - k;
    a->enc_params->encoding_symbol_length = len;
    
    
    if (of_create_codec_instance(&enc_ses, a->codec_id, OF_ENCODER, 2 ) != OF_STATUS_OK)
    {
        PJ_LOG(4, (THIS_FILE, "Create encoder instance failed"));
        
        return PJ_EINVAL;
    }
    
    
    if (of_set_fec_parameters(enc_ses, a->enc_params) != OF_STATUS_OK)
    {
        PJ_LOG(4, (THIS_FILE, "Set parameters failed for encoder codec_id=%d", a->codec_id));
        
        
        of_release_codec_instance(enc_ses);
        
        return PJ_EINVAL;
    }
    
    
    for (esi = k; esi < n; esi++)
    {
        if (of_build_repair_symbol(enc_ses, a->enc_symbols_ptr, esi) != OF_STATUS_OK)
        {
            PJ_LOG(4, (THIS_FILE, "Build repair symbol failed for esi=%u", esi));
        }
        else
        {
            
            a->enc_symbols_size[esi] = len;
        }
    }
    
    a->snd_ready = PJ_TRUE;
    
    
    of_release_codec_instance(enc_ses);
    
    return status;
}


static void* fec_dec_cb(void *context, pj_uint32_t size, pj_uint32_t esi)
{
    struct tp_adapter *a = (struct tp_adapter *)context;
    
    
    return a->dec_symbols_ptr[esi];
}


static pj_uint16_t fec_enc_src(void *dst, const void *pkt, pj_uint16_t size, const fec_ext_hdr *fec_hdr, const pjmedia_rtp_ext_hdr *ext_hdr)
{
    unsigned num;
    pj_uint8_t *ptr = (pj_uint8_t *)dst;
    
    
    pjmedia_rtp_hdr *rtp_hdr = (pjmedia_rtp_hdr *)pkt;
    
    
    if (rtp_hdr->v != RTP_VERSION || rtp_hdr->x)
    {
        PJ_LOG(4, (THIS_FILE, "RTP packet header decode failed esi=%u, sn=%u", fec_hdr->esi, fec_hdr->sn));
        return 0;
    }
    
    
    rtp_hdr->x = 1;
    
    
    
    num = sizeof(pjmedia_rtp_hdr) + rtp_hdr->cc * sizeof(pj_uint32_t);
    memcpy(ptr, pkt, num);
    ptr += num;
    
    
    pkt = (pj_uint8_t *)pkt + num;
    size -= num;
    
    
    num = sizeof(pjmedia_rtp_ext_hdr);
    memcpy(ptr, ext_hdr, num);
    ptr += num;
    
    
    
    num = pj_ntohs(ext_hdr->length) * sizeof(pj_uint32_t);
    memcpy(ptr, fec_hdr, num);
    ptr += num;
    
    
    memcpy(ptr, pkt, size);
    ptr += size;
    
    return (ptr - (pj_uint8_t *)dst);
}


static pj_uint16_t fec_enc_rpr(void *dst, pjmedia_rtp_session *ses, int ts_len, const void *payload, pj_uint16_t payload_len, const fec_ext_hdr *fec_hdr, const pjmedia_rtp_ext_hdr *ext_hdr)
{
    int num;
    pj_uint8_t *ptr = (pj_uint8_t *)dst;
    pjmedia_rtp_hdr *hdr;
    
    
    pjmedia_rtp_encode_rtp(ses, ses->out_pt, 0, payload_len, ts_len, &hdr, &num);
    
    
    memcpy(ptr, hdr, num);
    ((pjmedia_rtp_hdr *)dst)->x = 1;
    ptr += num;
    
    
    num = sizeof(pjmedia_rtp_ext_hdr);
    memcpy(ptr, ext_hdr, num);
    ptr += num;
    
    
    
    num = pj_ntohs(ext_hdr->length) * sizeof(pj_uint32_t);
    memcpy(ptr, fec_hdr, num);
    ptr += num;
    
    
    memcpy(ptr, payload, payload_len);
    ptr += payload_len;
    
    return (ptr - (pj_uint8_t *)dst);
}

static void* fec_dec_hdr(const void *pkt, fec_ext_hdr *fec_hdr)
{
    unsigned num;
    pj_uint8_t *ptr = (pj_uint8_t *)pkt;
    
    
    pjmedia_rtp_hdr *rtp_hdr = (pjmedia_rtp_hdr *)pkt;
    
    
    
    
    
    if (rtp_hdr->v != RTP_VERSION || !rtp_hdr->x)
    {
        PJ_LOG(4, (THIS_FILE, "FEC header in RTP packet decode failed"));
        return 0;
    }
    
    
    num = sizeof(pjmedia_rtp_hdr) + rtp_hdr->cc * sizeof(pj_uint32_t) + sizeof(pjmedia_rtp_ext_hdr);
    ptr += num;
    
    fec_hdr->sn = pj_ntohl(*(pj_uint32_t *)ptr);
    ptr += sizeof(pj_uint32_t);
    fec_hdr->esi = pj_ntohs(*(pj_uint16_t *)ptr);
    ptr += sizeof(pj_uint16_t);
    fec_hdr->k = pj_ntohs(*(pj_uint16_t *)ptr);
    ptr += sizeof(pj_uint16_t);
    fec_hdr->n = pj_ntohs(*(pj_uint16_t *)ptr);
    ptr += sizeof(pj_uint16_t);
    fec_hdr->esl = pj_ntohs(*(pj_uint16_t *)ptr);
    ptr += sizeof(pj_uint16_t);
    fec_hdr->s = pj_ntohs(*(pj_uint16_t *)ptr);
    ptr += sizeof(pj_uint16_t);
    fec_hdr->rsrv = pj_ntohs(*(pj_uint16_t *)ptr);
    ptr += sizeof(pj_uint16_t);
    
    return ptr;
}

static pj_uint16_t fec_dec_pkt(void * const dst, void *pkt, pj_ssize_t size, pj_bool_t rtp)
{
    unsigned num;
    pj_uint8_t *ptr = (pj_uint8_t *)dst;
    pjmedia_rtp_hdr *rtp_hdr = (pjmedia_rtp_hdr *)pkt;
    
    
    
    
    
    num = sizeof(pjmedia_rtp_hdr) + rtp_hdr->cc * sizeof(pj_uint32_t);
    
    
    if (rtp)
    {
        rtp_hdr->x = 0;
        
        memcpy(ptr, pkt, num);
        ptr += num;
    }
    
    
    num += sizeof(pjmedia_rtp_ext_hdr);
    
    num += pj_ntohs(((pjmedia_rtp_ext_hdr *)((pj_uint8_t *)pkt + sizeof(pjmedia_rtp_hdr)))->length) * sizeof(pj_uint32_t);
    
    
    pkt = (pj_uint8_t *)pkt + num;
    size -= num;
    
    memcpy(ptr, pkt, size);
    ptr += size;
    
    return (ptr - (pj_uint8_t *)dst);
}


PJ_DEF(pj_status_t) pjmedia_fec_adapter_create(pjmedia_endpt *endpt, const char *name, pjmedia_transport *transport, pj_bool_t del_base, pjmedia_transport **p_tp)
{
    pj_pool_t *pool;
    struct tp_adapter *a;
    
    if (name == NULL)
        name = "tpad%p";
    
    
    pool = pjmedia_endpt_create_pool(endpt, name, 512, 512);
    a = PJ_POOL_ZALLOC_T(pool, struct tp_adapter);
    a->pool = pool;
    pj_ansi_strncpy(a->base.name, pool->obj_name, sizeof(a->base.name));
    a->base.type = (pjmedia_transport_type)(PJMEDIA_TRANSPORT_TYPE_USER + 1);
    a->base.op = &tp_adapter_op;
    
    
    a->slave_tp = transport;
    a->del_base = del_base;
    
    a->tx_code_rate = CODE_RATE_MAX;
    a->snd_k_max = K_MAX;
    a->old_client = 0;
    
    
    if (N_MAX <= 255)
    {
        a->codec_id = OF_CODEC_REED_SOLOMON_GF_2_M_STABLE;
        
        a->enc_rs_params.m = 8;
        a->enc_params = (of_parameters_t *)&a->enc_rs_params;
        
        a->dec_rs_params.m = 8;
        a->dec_params = (of_parameters_t *)&a->dec_rs_params;
    }
    else
    {
        a->codec_id = OF_CODEC_LDPC_STAIRCASE_STABLE;
        
        a->enc_ldps_params.prng_seed = rand();
        a->enc_ldps_params.N1 = 7;
        a->enc_params = (of_parameters_t *)&a->enc_ldps_params;
        
        a->dec_ldps_params.prng_seed = rand();
        a->dec_ldps_params.N1 = 7;
        a->dec_params = (of_parameters_t *)&a->dec_ldps_params;
    }
    
    
    *p_tp = &a->base;
    return PJ_SUCCESS;
}



static pj_status_t transport_get_info(pjmedia_transport *tp, pjmedia_transport_info *info)
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    
    
    return pjmedia_transport_get_info(a->slave_tp, info);
}

#if 1 

static pj_uint16_t fec_symbol_size_old(const void * const pkt, const pj_uint32_t symbol_size)
{
    
    pj_uint8_t *ptr = (pj_uint8_t *)pkt + symbol_size - 1;
    pj_uint16_t size = symbol_size;
    
    while (!*ptr-- && size)
        --size;
    
    return size;
}
#endif





static void transport_rtp_cb(void *user_data, void *pkt, pj_ssize_t size)
{
    struct tp_adapter *a = (struct tp_adapter*)user_data;
    pj_uint16_t esi, k, n, len;
    fec_ext_hdr fec_hdr;
    pjmedia_rtp_hdr *rtp_hdr = (pjmedia_rtp_hdr *)pkt;
    
    if (a->codec_id == OF_CODEC_NIL)
    {
        a->stream_rtp_cb(a->stream_user_data, pkt, size);
        return;
    }
    
    a->rcv_k++;
    
    
    fec_dec_hdr(pkt, &fec_hdr);
    
    
    
    
    
    
    
    
    
    
    
    if (fec_hdr.sn < a->rcv_sn)
    {
        PJ_LOG(5, (THIS_FILE, "Too late sn=%u received in a packet while decoder sn=%u, drop packet", fec_hdr.sn, a->rcv_sn));
        return;
    }
    
    else if (fec_hdr.sn > a->rcv_sn)
    {
        if (a->dec_ses && !of_is_decoding_complete(a->dec_ses))
        {
            k = a->dec_params->nb_source_symbols;
            n = a->dec_params->nb_source_symbols + a->dec_params->nb_repair_symbols;
            
            PJ_LOG(5, (THIS_FILE, "Decoding incomplete for sn=%u k=%u n=%u len=%u rcv=%u, reset for new sn=%u",
                       a->rcv_sn,
                       k,
                       n,
                       a->dec_params->encoding_symbol_length,
                       a->rcv_k,
                       fec_hdr.sn));
            
            
            if (a->stream_type == PJMEDIA_TYPE_VIDEO)
                transport_send_rtcp_fir(user_data);
            
            if (a->stream_type == PJMEDIA_TYPE_AUDIO)
            {
                
                for (esi = 0; esi < k; esi++)
                {
                    if (a->dec_symbols_size[esi])
                        a->stream_rtp_cb(a->stream_user_data, a->dec_symbols_ptr[esi], a->dec_symbols_size[esi]);
                }
            }
        }
        
        
        
        
        a->rcv_sn = fec_hdr.sn;
        
        if (fec_dec_reset(a, fec_hdr.k, fec_hdr.n, fec_hdr.esl) != PJ_SUCCESS)
        {
            PJ_LOG(4, (THIS_FILE, "Decoder instance creation failed for sn=%u k=%u n=%u len=%u",
                       fec_hdr.sn,
                       fec_hdr.k,
                       fec_hdr.n,
                       fec_hdr.esl));
        }
    }
    
    else if (of_is_decoding_complete(a->dec_ses))
    {
        
        
        
        if (fec_hdr.sn != a->rcv_sn)
            PJ_LOG(5, (THIS_FILE, "Decoding already complete for sn=%u, but decoder sn=%u, drop packet", fec_hdr.sn, a->rcv_sn));
        
        return;
    }
    
    
    len = fec_dec_pkt(a->dec_symbols_ptr[fec_hdr.esi], pkt, size, fec_hdr.esi < fec_hdr.k ? PJ_TRUE : PJ_FALSE);
    
    
#if 1
    a->dec_symbols_size[fec_hdr.esi] = a->old_client ? len : fec_hdr.s;
#else
    a->dec_symbols_size[fec_hdr.esi] = fec_hdr.s;
#endif
    
    
    
    
    
    if (of_decode_with_new_symbol(a->dec_ses, a->dec_symbols_ptr[fec_hdr.esi], fec_hdr.esi) == OF_STATUS_ERROR)
        PJ_LOG(4, (THIS_FILE, "Decode with new symbol failed esi=%u, len=%u", fec_hdr.esi, a->dec_params->encoding_symbol_length));
    
    
    if (!of_is_decoding_complete(a->dec_ses))
        return;
    
    
    
    k = a->dec_params->nb_source_symbols;
    n = a->dec_params->nb_source_symbols + a->dec_params->nb_repair_symbols;
    
    
#if 1
    if(!a->old_client)
        fec_dec_len(a);
#else
    fec_dec_len(a);
#endif
    
    
    for (esi = 0; esi < k; esi++)
    {
#if 1 
        
        if (!a->dec_symbols_size[esi])
            a->dec_symbols_size[esi] = fec_symbol_size_old(a->dec_symbols_ptr[esi], a->dec_params->encoding_symbol_length);
#endif
        
        a->stream_rtp_cb(a->stream_user_data, a->dec_symbols_ptr[esi], a->dec_symbols_size[esi]);
    }
    
    
    
    
}


static void transport_rtcp_cb(void *user_data, void *pkt, pj_ssize_t size)
{
    struct tp_adapter *a = (struct tp_adapter*)user_data;
    pjmedia_rtcp_common *common = (pjmedia_rtcp_common*)pkt;
    const pjmedia_rtcp_rr *rr = NULL;
    const pjmedia_rtcp_sr *sr = NULL;
    
    if (a->codec_id == OF_CODEC_NIL)
    {
        a->stream_rtcp_cb(a->stream_user_data, pkt, size);
        return;
    }
    
    
    if (common->pt == RTCP_FIR && a->stream_type == PJMEDIA_TYPE_VIDEO )
    {
        pjmedia_vid_stream_send_keyframe(a->stream_ref);
        return;
    }
    
    
    if (common->pt == RTCP_SR)
    {
        sr = (pjmedia_rtcp_sr*)(((char*)pkt) + sizeof(pjmedia_rtcp_common));
        if (common->count > 0 && size >= (sizeof(pjmedia_rtcp_sr_pkt)))
            rr = (pjmedia_rtcp_rr*)(((char*)pkt) + (sizeof(pjmedia_rtcp_common) + sizeof(pjmedia_rtcp_sr)));
    }
    else if (common->pt == RTCP_RR && common->count > 0)
    {
        rr = (pjmedia_rtcp_rr*)(((char*)pkt) + sizeof(pjmedia_rtcp_common));
    }
    
    
    if (rr)
    {
        
        
        
        
        a->tx_loss = a->tx_code_rate + rr->fract_lost / 256.0 - 1;
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        PJ_LOG(5, (THIS_FILE, "Current TX tx_loss=%3.3f -> code_rate=%1.3f", a->tx_loss, a->tx_code_rate));
    }
    
    
    
    
    
    a->stream_rtcp_cb(a->stream_user_data, pkt, size);
}

#if defined(PJ_VERSION_NUM_MAJOR) && (PJ_VERSION_NUM_MAJOR == 2) && defined(PJ_VERSION_NUM_MINOR) && (PJ_VERSION_NUM_MINOR < 6)

static pj_status_t transport_attach(pjmedia_transport *tp,
                                    void *user_data,
                                    const pj_sockaddr_t *rem_addr,
                                    const pj_sockaddr_t *rem_rtcp,
                                    unsigned addr_len,
                                    void(*rtp_cb)(void*, void*, pj_ssize_t),
                                    void(*rtcp_cb)(void*, void*, pj_ssize_t))
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    pj_status_t status;
    
    
    
    a->stream_user_data = user_data;
    a->stream_rtp_cb = rtp_cb;
    a->stream_rtcp_cb = rtcp_cb;
    
    a->stream_ref = user_data;
    
    
    if (a->stream_type == PJMEDIA_TYPE_VIDEO)
        pjmedia_vid_stream_get_rtp_session_tx(a->stream_ref, &a->rtp_tx_ses);
    
    else
        return PJ_EINVALIDOP;
    
    
    
    a->rtcp_ses = NULL;
    
    rtp_cb = &transport_rtp_cb;
    rtcp_cb = &transport_rtcp_cb;
    user_data = a;
    
    status = pjmedia_transport_attach(a->slave_tp, user_data, rem_addr, rem_rtcp, addr_len, rtp_cb, rtcp_cb);
    if (status != PJ_SUCCESS)
    {
        a->stream_user_data = NULL;
        a->stream_rtp_cb = NULL;
        a->stream_rtcp_cb = NULL;
        a->stream_ref = NULL;
        return status;
    }
    
    return PJ_SUCCESS;
}
#endif

#if defined(PJ_VERSION_NUM_MAJOR) && (PJ_VERSION_NUM_MAJOR == 2) && defined(PJ_VERSION_NUM_MINOR) && (PJ_VERSION_NUM_MINOR >= 6)

static pj_status_t transport_attach2(pjmedia_transport *tp, pjmedia_transport_attach_param *att_param)
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    pj_status_t status;
    
    
    
    
    a->stream_user_data = att_param->user_data;
    a->stream_rtp_cb = att_param->rtp_cb;
    a->stream_rtcp_cb = att_param->rtcp_cb;
    a->stream_ref = att_param->stream;
    a->stream_type = att_param->media_type;
    
    
    pjmedia_stream_rtp_sess_info session_info;
    
    switch (att_param->media_type)
    {
        case PJMEDIA_TYPE_VIDEO:
        {
#if 1 
            if (a->codec_id == OF_CODEC_NIL)
            {
                
                a->old_client = 1;
                a->codec_id = OF_CODEC_REED_SOLOMON_GF_2_M_STABLE;
                a->snd_k_max = 10;
                a->ext_hdr.length = pj_htons(sizeof(fec_ext_hdr) / sizeof(pj_uint32_t) - 1);
            }
            else
                a->snd_k_max = 20;
#else
            a->snd_k_max = 20;
#endif
            pjmedia_vid_stream_get_rtp_session_info(a->stream_ref, &session_info);
            break;
        }
        case PJMEDIA_TYPE_AUDIO:
        {
            a->snd_k_max = 10;
            pjmedia_stream_get_rtp_session_info(a->stream_ref, &session_info);
            break;
        }
        default:
            return PJ_EINVALIDOP;
    }
    
    
    
    
    a->rtp_tx_ses = session_info.tx_rtp;
    a->rtcp_ses = session_info.rtcp;
    
    att_param->rtp_cb = &transport_rtp_cb;
    att_param->rtcp_cb = &transport_rtcp_cb;
    att_param->user_data = a;
    
    status = pjmedia_transport_attach2(a->slave_tp, att_param);
    if (status != PJ_SUCCESS)
    {
        a->stream_user_data = NULL;
        a->stream_rtp_cb = NULL;
        a->stream_rtcp_cb = NULL;
        a->stream_ref = NULL;
        return status;
    }
    
    return PJ_SUCCESS;
}
#endif


static void transport_detach(pjmedia_transport *tp, void *strm)
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    
    PJ_UNUSED_ARG(strm);
    
    if (a->stream_user_data != NULL)
    {
        pjmedia_transport_detach(a->slave_tp, a);
        a->stream_user_data = NULL;
        a->stream_rtp_cb = NULL;
        a->stream_rtcp_cb = NULL;
        a->stream_ref = NULL;
    }
}







static pj_status_t transport_send_rtp(pjmedia_transport *tp, const void *pkt, pj_size_t size)
{
    struct tp_adapter *a = (struct tp_adapter *)tp;
    pj_uint16_t esi, len, n, k;
    fec_ext_hdr fec_hdr;
    
    if (a->codec_id == OF_CODEC_NIL)
        return pjmedia_transport_send_rtp(a->slave_tp, pkt, size);
    
    
    
    
    
    
    pj_status_t status = fec_enc_pkt(a, pkt, size);
    
    
    if (status != PJ_SUCCESS)
    {
        PJ_LOG(4, (THIS_FILE, "Encode for send rtp failed with packet size=%u", size));
        return status;
    }
    
    
    if (a->snd_ready == PJ_FALSE)
        return status;
    
    k = a->enc_params->nb_source_symbols;
    n = a->enc_params->nb_repair_symbols + a->enc_params->nb_source_symbols;
    
    
    fec_hdr.sn = pj_htonl(a->snd_sn);
    fec_hdr.k = pj_htons(k);
    fec_hdr.n = pj_htons(n);
    fec_hdr.esl = pj_htons(a->enc_params->encoding_symbol_length);
    
    
#if 1
    if (!a->old_client)
        fec_enc_len(a);
#else
    fec_enc_len(a);
#endif
    
    
    
    
    
    
    
    for (esi = 0; esi < k; esi++)
    {
        
        fec_hdr.esi = pj_htons(esi);
        fec_hdr.s = pj_htons(a->enc_symbols_size[esi]);
        len = fec_enc_src((void *)a->enc_symbol_buf, a->enc_symbols_ptr[esi], a->enc_symbols_size[esi], &fec_hdr, &a->ext_hdr);
        
        if (len)
            status = pjmedia_transport_send_rtp(a->slave_tp, (void *)a->enc_symbol_buf, len);
        
        if (status != PJ_SUCCESS || !len)
            PJ_LOG(4, (THIS_FILE, "Send RTP packet failed sn=%u k=%u n=%u esi=%u len=%u pkt_len=%u", a->snd_sn, k, n, esi, a->enc_params->encoding_symbol_length, len));
        
        
    }
    
    for (; esi < n; esi++)
    {
        
        fec_hdr.esi = pj_htons(esi);
        fec_hdr.s = pj_htons(a->enc_symbols_size[esi]);
        
        len = fec_enc_rpr((void *)a->enc_symbol_buf, a->rtp_tx_ses, 0, a->enc_symbols_ptr[esi], a->snd_len, &fec_hdr, &a->ext_hdr);
        
        if (len)
            status = pjmedia_transport_send_rtp(a->slave_tp, (void *)a->enc_symbol_buf, len);
        
        if (status != PJ_SUCCESS || !len)
            PJ_LOG(4, (THIS_FILE, "Send RTP packet failed sn=%u k=%u n=%u esi=%u len=%u pkt_len=%u", a->snd_sn, k, n, esi, a->enc_params->encoding_symbol_length, len));
        
        
    }
    
    
    
    
    
    
    fec_enc_reset(a, n, a->enc_params->encoding_symbol_length);
    
    return status;
}


static pj_status_t transport_send_rtcp_fir(pjmedia_transport *tp)
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    pj_uint8_t buf[256];
    pjmedia_rtcp_common *hdr = (pjmedia_rtcp_common*)buf;
    pj_uint8_t *p;
    pj_size_t len;
    pj_uint32_t ssrc;
    
    
    if (!a->rtcp_ses)
        return PJ_EINVALIDOP;
    
    
    len = sizeof(*hdr);
    pj_memcpy(hdr, &a->rtcp_ses->rtcp_sr_pkt.common, sizeof(*hdr));
    hdr->pt = RTCP_FIR;
    p = (pj_uint8_t*)hdr + sizeof(*hdr);
    
    
    
    
    ssrc = pj_htonl(a->rtcp_ses->peer_ssrc);
    pj_memcpy(p, &ssrc, sizeof(ssrc));
    len += sizeof(ssrc);
    p += sizeof(ssrc);
    
    
    *p++ = ++a->rtcp_fir_sn;
    len++;
    
    
    while ((p - (pj_uint8_t*)buf) % 4)
    {
        *p++ = 0;
        len++;
    }
    
    hdr->length = len;
    
    
    
    
    return transport_send_rtcp(tp, (void *)buf, len);
}


static pj_status_t transport_send_rtcp(pjmedia_transport *tp, const void *pkt, pj_size_t size)
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    
    
    
    
    return pjmedia_transport_send_rtcp(a->slave_tp, pkt, size);
}



static pj_status_t transport_send_rtcp2(pjmedia_transport *tp, const pj_sockaddr_t *addr, unsigned addr_len, const void *pkt, pj_size_t size)
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    
    return pjmedia_transport_send_rtcp2(a->slave_tp, addr, addr_len, pkt, size);
}


static pj_status_t transport_media_create(pjmedia_transport *tp, pj_pool_t *sdp_pool, unsigned options, const pjmedia_sdp_session *rem_sdp, unsigned media_index)
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    
    
    if (rem_sdp)
    {
        
    }
    
    
    return pjmedia_transport_media_create(a->slave_tp, sdp_pool, options, rem_sdp, media_index);
}


static pjmedia_sdp_attr *fec_sdp_decl_create(pj_pool_t *pool, unsigned ref, unsigned enc_id)
{
    pj_str_t value;
    pjmedia_sdp_attr *attr = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_attr);
    char buf[128];
    
    value.ptr = buf;
    
    
    value.slen = 0;
    value.slen += pj_utoa(ref, buf);
    
    
    pj_strcat2(&value, " encoding-id=");
    value.slen += pj_utoa(enc_id, buf + value.slen);
    
    pj_strdup2(pool, &attr->name, "FEC-declaration");
    pj_strdup(pool, &attr->value, &value);
    
    return attr;
}


static pjmedia_sdp_attr *fec_sdp_oti_create(pj_pool_t *pool, unsigned ref, unsigned block_len, unsigned symbol_len)
{
    pj_str_t value;
    pjmedia_sdp_attr *attr = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_attr);
    char buf[128];
    
    value.ptr = buf;
    
    
    value.slen = 0;
    value.slen += pj_utoa(ref, buf);
    
    
    pj_strcat2(&value, " max-block-len=");
    value.slen += pj_utoa(block_len, buf + value.slen);
    
    
    pj_strcat2(&value, " max-symbol-len=");
    value.slen += pj_utoa(symbol_len, buf + value.slen);
    
    pj_strdup2(pool, &attr->name, "FEC-OTI-extension");
    pj_strdup(pool, &attr->value, &value);
    
    return attr;
}

static of_codec_id_t fec_sdp_check(unsigned attr_count, pjmedia_sdp_attr * const *attr_array, unsigned media_index)
{
    pjmedia_sdp_attr *attr;
    unsigned ref, enc_id, block_len, symbol_len;
    
    
    attr = pjmedia_sdp_attr_find2(attr_count, attr_array, "FEC-declaration", NULL);
    
    if (attr && sscanf(attr->value.ptr, "%u encoding-id=%u", &ref, &enc_id) == 2
        && enc_id < OF_CODEC_LDPC_FROM_FILE_ADVANCED)
    {
        
        
        
        
        attr = pjmedia_sdp_attr_find2(attr_count, attr_array, "FEC-OTI-extension", NULL);
        
        if (attr && sscanf(attr->value.ptr, "%u max-block-len=%u max-symbol-len=%u", &ref, &block_len, &symbol_len) == 3
            && block_len <= N_MAX && symbol_len <= SYMBOL_SIZE_MAX)
        {
            
            
            
            return (of_codec_id_t)enc_id;
        }
    }
    
    return OF_CODEC_NIL;
}


static pj_status_t transport_encode_sdp(pjmedia_transport *tp, pj_pool_t *sdp_pool, pjmedia_sdp_session *local_sdp, const pjmedia_sdp_session *rem_sdp, unsigned media_index)
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    pjmedia_sdp_attr *fec_attr;
    pj_status_t status = PJ_EINVAL;
    unsigned i;
    
    
    if (rem_sdp)
    {
        
        a->codec_id = fec_sdp_check(rem_sdp->media[media_index]->attr_count, rem_sdp->media[media_index]->attr, media_index);
        
        
        if (a->codec_id != OF_CODEC_NIL)
        {
            
            status = pjmedia_sdp_attr_add(&local_sdp->media[media_index]->attr_count, local_sdp->media[media_index]->attr, fec_sdp_decl_create(sdp_pool, media_index, a->codec_id));
            
            for (i = 1; fec_sdp_attrs[i]; i++)
            {
                fec_attr = pjmedia_sdp_attr_find2(rem_sdp->media[media_index]->attr_count, rem_sdp->media[media_index]->attr, fec_sdp_attrs[i], NULL);
                
                if (fec_attr)
                    status = pjmedia_sdp_attr_add(&local_sdp->media[media_index]->attr_count, local_sdp->media[media_index]->attr, pjmedia_sdp_attr_clone(sdp_pool, fec_attr));
            }
        }
    }
    
    else
    {
        
        
        
        
        status = pjmedia_sdp_attr_add(&local_sdp->media[media_index]->attr_count, local_sdp->media[media_index]->attr, fec_sdp_decl_create(sdp_pool, media_index, a->codec_id));
        
        
        status = pjmedia_sdp_attr_add(&local_sdp->media[media_index]->attr_count, local_sdp->media[media_index]->attr, fec_sdp_oti_create(sdp_pool, media_index, N_MAX, SYMBOL_SIZE_MAX));
    }
    
    
    return pjmedia_transport_encode_sdp(a->slave_tp, sdp_pool, local_sdp, rem_sdp, media_index);
}


static pj_status_t transport_media_start(pjmedia_transport *tp, pj_pool_t *pool, const pjmedia_sdp_session *local_sdp, const pjmedia_sdp_session *rem_sdp, unsigned media_index)
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    pjmedia_sdp_attr *fec_attr = NULL;
    pj_status_t status = PJ_EINVAL;
    pj_uint16_t esi;
    
    
    if (rem_sdp)
        a->codec_id = fec_sdp_check(rem_sdp->media[media_index]->attr_count, rem_sdp->media[media_index]->attr, media_index);
    
    
    {
        a->dec_ses = NULL;
        a->tx_loss = .0;
        
        
        
        a->ext_hdr.profile_data = pj_htons(RTP_EXT_PT);
        a->ext_hdr.length = pj_htons(sizeof(fec_ext_hdr) / sizeof(pj_uint32_t));
        
        a->rcv_sn = 0;
        
        a->rtcp_fir_sn = 0;
        
        
        for (esi = 0; esi < N_MAX; esi++)
        {
            a->enc_symbols_ptr[esi] = (void *)&a->enc_symbols_buf[esi * SYMBOL_SIZE_MAX];
            a->dec_symbols_ptr[esi] = (void *)&a->dec_symbols_buf[esi * SYMBOL_SIZE_MAX];
        }
        
        PJ_LOG(5, (THIS_FILE, "FEC adapter init for media_index=%u succeed with max values: k=%u n=%u len=%u",
                   media_index,
                   K_MAX,
                   N_MAX,
                   SYMBOL_SIZE_MAX));
    }
    
    
    return pjmedia_transport_media_start(a->slave_tp, pool, local_sdp, rem_sdp, media_index);
}


static pj_status_t transport_media_stop(pjmedia_transport *tp)
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    
    
    if (a->dec_ses)
        of_release_codec_instance(a->dec_ses);
    
    
    return pjmedia_transport_media_stop(a->slave_tp);
}


static pj_status_t transport_simulate_lost(pjmedia_transport *tp, pjmedia_dir dir, unsigned pct_lost)
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    
    return pjmedia_transport_simulate_lost(a->slave_tp, dir, pct_lost);
}


static pj_status_t transport_destroy(pjmedia_transport *tp)
{
    struct tp_adapter *a = (struct tp_adapter*)tp;
    
    
    if (a->del_base)
        pjmedia_transport_close(a->slave_tp);
    
    
    pj_pool_release(a->pool);
    
    return PJ_SUCCESS;
}

