

#include <pjmedia/vid_stream.h>
#include <pjmedia/stream.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/log.h>
#include "transport_adapter_nack.h"
#include <pjmedia/rtp.h>
#include <pj/lock.h>

#if defined (__ANDROID__) 

#include <signal.h>
#include <time.h>

#endif 

typedef struct nack_stat
{
    pj_uint32_t rcv;                                
    pj_uint32_t rtr;                                
    pj_uint32_t snd;                                
    pj_uint32_t req;                                
    pj_uint32_t dup;                                
    pj_uint32_t rx_ts_local;                        
    pj_uint32_t tx_ts_local;                        
    pj_uint32_t rx_ts_remote;                        
    pj_uint32_t rx_ts_timer;
    pj_uint32_t tx_ts_timer;
    pj_uint16_t tx_size_timer;
} nack_stat;

enum
{
    RTP_NACK_PT = 127
};



#define SYMBOL_SIZE_MAX    PJMEDIA_MAX_MTU                            
#define RETR_TABLE_SIZE 100                                        
#define BURST_LOSS_SIZE 3                                        
#define BURST_CHECK_DELAY 50                                    

enum
{
    RTCP_SR = 200,                                                
    RTCP_RR = 201,                                                
    RTCP_FIR = 206,                                                
    RTCP_NACK = 205,                                            
    RTCP_ACK = 204                                                
};


enum
{
    ACK_TIMER = 2
};

static char *this_file_tmpl = "tp_nack_%s";


static pj_status_t    transport_get_info        (pjmedia_transport *tp, pjmedia_transport_info *info);

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

static void            transport_retransmit_rtp(void *user_data , const pj_uint32_t ts, pj_uint32_t skew);


static pj_status_t transport_send_rtcp_ack(pjmedia_transport *tp, pj_uint32_t ts);


static struct pjmedia_transport_op nack_adapter_op =
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

typedef struct retr_entry
{
    PJ_DECL_LIST_MEMBER(struct retr_entry);
    pj_uint8_t pkt[SYMBOL_SIZE_MAX];
    pj_uint32_t ts;
    pj_uint16_t size;
} retr_entry;


static int retr_list_comp(void *value, const pj_list_type *node)
{
    
    return !(((retr_entry *)node)->ts == *((pj_uint32_t *)value));
}

static void retr_list_init(retr_entry *list, retr_entry *arr, const pj_uint16_t size)
{
    pj_uint16_t i;
    
    pj_list_init(list);
    
    
    for (i = 0; i < size; i++)
        pj_list_push_back(list, &arr[i]);
}

static void retr_list_push_back(retr_entry *list, const void *pkt, const pj_uint16_t size, const pj_uint32_t ts)
{
    retr_entry *e = list->next;
    
    
    pj_list_erase(e);
    
    
    e->ts = ts;
    e->size = size;
    if (pkt && size)
        memcpy(e->pkt, pkt, size);
    
    
    pj_list_push_back(list, e);
}

static retr_entry *retr_list_search(retr_entry *list, pj_uint32_t ts)
{
    return (retr_entry *)pj_list_search(list, &ts, &retr_list_comp);
}

static retr_entry *retr_list_next(retr_entry *list, retr_entry *e)
{
    
    if (e->next == list)
        return NULL;
    
    return e->next;
}

static retr_entry *retr_list_first(retr_entry *list)
{
    if (list->next == list)
        return NULL;
    
    return list->next;
}

static void retr_list_reset(retr_entry *list)
{
    retr_entry *e = retr_list_first(list);
    for (; e; e = retr_list_next(list, e))
        e->ts = 0;
}


struct nack_adapter
{
    pjmedia_transport    base;
    pj_bool_t            del_base;
    pj_pool_t            *pool;
    pjsip_endpoint        *sip_endpt;
    
    
    void                *stream_user_data;
    void                *stream_ref;
    void                (*stream_rtp_cb)(void *user_data, void *pkt, pj_ssize_t);
    void                (*stream_rtcp_cb)(void *user_data, void *pkt, pj_ssize_t);
    
    
    
    pj_time_val                delay;                        
    pj_timer_entry            timer_entry;
    
    pj_bool_t                rtp_flag;                    
    
    pj_lock_t                *timer_lock;                
    pj_lock_t                *rtp_lock;                    
    
    char                    this_file[15];                
    
    
    pjmedia_transport        *slave_tp;                    
    
    pjmedia_rtcp_session    *rtcp_ses;                    
    pjmedia_rtp_session        *rtp_tx_ses;                
    pjmedia_rtp_session        *rtp_rx_ses;                
    
    unsigned                ts_delay;
    unsigned                ts_step;
    
    
    retr_entry                retr_arr[RETR_TABLE_SIZE], retr_list;
    
    
    nack_stat                stat;
    
#if defined (__ANDROID__) 
    timer_t                    posix_timer_id;
#endif 
    
};

#if defined (__ANDROID__) 

#define CLOCKID CLOCK_REALTIME
#define SIG SIGRTMIN

static void posix_timer_cb(int sig, siginfo_t *si, void *uc)
{
    struct nack_adapter *a = (struct nack_adapter *)si->si_value.sival_ptr;
    pj_status_t status;
    
    
    pj_lock_acquire(a->timer_lock);
    
    PJ_LOG(5, (a->this_file, "NACK timer callback rx_ts_local=%lu rx_ts_timer=%lu", a->stat.rx_ts_local, a->stat.rx_ts_timer));
    
    
    if (a->stat.rx_ts_local && a->rtp_flag == PJ_FALSE)
    {
        status = transport_send_rtcp_ack(si->si_value.sival_ptr, a->stat.rx_ts_local);
        
        
    }
    
    
    a->stat.rx_ts_timer = a->stat.rx_ts_local;
    
    a->rtp_flag = PJ_FALSE;
    
    pj_lock_release(a->timer_lock);
}

static void posix_timer_schedule(void *user_data , long sec, long msec)
{
    struct sigevent sev;
    struct itimerspec its;
    struct sigaction sa;
    struct nack_adapter *a = (struct nack_adapter *)user_data;
    
    
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = posix_timer_cb;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIG, &sa, NULL) == -1)
    {
        PJ_LOG(4, (a->this_file, "NACK Establish handler for timer signal failed"));
        return;
    }
    
    
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIG;
    sev.sigev_value.sival_ptr = user_data;
    if (timer_create(CLOCKID, &sev, &a->posix_timer_id) == -1)
    {
        PJ_LOG(4, (a->this_file, "NACK Create the timer failed"));
        return;
    }
    
    
    its.it_value.tv_sec = sec;
    its.it_value.tv_nsec = msec * 1000000;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;
    if (timer_settime(a->posix_timer_id, 0, &its, NULL) == -1)
    {
        PJ_LOG(4, (a->this_file, "NACK Start the timer id=0x%lx failed", (long)a->posix_timer_id));
        return;
    }
    
    PJ_LOG(4, (a->this_file, "NACK Timer id=0x%lx started sec=%ld msec=%ld", (long)a->posix_timer_id, sec, msec));
}

static void posix_timer_cancel(void *user_data )
{
    struct nack_adapter *a = (struct nack_adapter *)user_data;
    
    if (a->posix_timer_id == -1)
        return;
    
    if (!timer_delete(a->posix_timer_id))
    {
        PJ_LOG(4, (a->this_file, "NACK Timer id=0x%lx stopped", (long)a->posix_timer_id));
    }
    else
    {
        PJ_LOG(4, (a->this_file, "NACK Timer id=0x%lx stop failed", (long)a->posix_timer_id));
    }
    
    a->posix_timer_id = -1;
}
#endif 


static void dump_pkt(const void * const buf, const pj_uint16_t size, pj_uint16_t esi, const char *type)
{
    char    *ptr;
    pj_uint16_t    n = size;
    char str[SYMBOL_SIZE_MAX * 3] = { '\0' }, *p = str;
    
    
    PJ_UNUSED_ARG(esi);
    PJ_UNUSED_ARG(type);
    p += sprintf(p, "size=%u: ", size);
    p += sprintf(p, "0x");
    for (ptr = (char *)buf; n > 0; n--, ptr++)
    {
        
        p += sprintf(p, "%02X", (unsigned char)*ptr);
    }
    p += sprintf(p, "\n");
    
    PJ_LOG(5, ("dump_pkt", str));
}

static void timer_cb(pj_timer_heap_t *timer_heap, struct pj_timer_entry *entry)
{
    PJ_UNUSED_ARG(timer_heap);
    struct nack_adapter *a = (struct nack_adapter *)entry->user_data;
    pj_status_t status;
    
    
    pj_lock_acquire(a->timer_lock);
    
    PJ_LOG(5, (a->this_file, "NACK timer callback rx_ts_local=%lu rx_ts_timer=%lu", a->stat.rx_ts_local, a->stat.rx_ts_timer));
    
    
    if (a->stat.rx_ts_local && a->rtp_flag == PJ_FALSE)
    {
        status = transport_send_rtcp_ack(entry->user_data, a->stat.rx_ts_local);
        
        
    }
    
    
    a->stat.rx_ts_timer = a->stat.rx_ts_local;
    
    a->rtp_flag = PJ_FALSE;
    
    pj_lock_release(a->timer_lock);
    
    
    
    
    
    a->timer_entry.id = ACK_TIMER;
    
    pjsip_endpt_schedule_timer(a->sip_endpt, &a->timer_entry, &a->delay);
}


PJ_DEF(pj_status_t) pjmedia_nack_adapter_create(pjsip_endpoint *sip_endpt, pjmedia_endpt *endpt, const char *name, pjmedia_transport *transport, pj_bool_t del_base, pjmedia_transport **p_tp)
{
    pj_pool_t *pool;
    struct nack_adapter *a;
    
    if (name == NULL)
        name = "tpad%p";
    
    
    pool = pjmedia_endpt_create_pool(endpt, name, 512, 512);
    a = PJ_POOL_ZALLOC_T(pool, struct nack_adapter);
    a->pool = pool;
    pj_ansi_strncpy(a->base.name, pool->obj_name, sizeof(a->base.name));
    a->base.type = (pjmedia_transport_type)(PJMEDIA_TRANSPORT_TYPE_USER + 2);
    a->base.op = &nack_adapter_op;
    
    
    a->slave_tp = transport;
    a->del_base = del_base;
    
    a->sip_endpt = sip_endpt;
    
    
    a->timer_lock = NULL;
    a->timer_entry.cb = &timer_cb;
    a->timer_entry.user_data = a;
    a->timer_entry.id = -1;
    
#if defined (__ANDROID__) 
    a->posix_timer_id = -1;
#endif 
    
    retr_list_init(&a->retr_list, a->retr_arr, RETR_TABLE_SIZE);
    
    
    *p_tp = &a->base;
    return PJ_SUCCESS;
}



static pj_status_t transport_get_info(pjmedia_transport *tp, pjmedia_transport_info *info)
{
    struct nack_adapter *a = (struct nack_adapter*)tp;
    
    
    return pjmedia_transport_get_info(a->slave_tp, info);
}


static void transport_rtp_cb(void *user_data, void *pkt, pj_ssize_t size)
{
    struct nack_adapter *a = (struct nack_adapter*)user_data;
    pjmedia_rtp_hdr *hdr = (pjmedia_rtp_hdr *)pkt;
    pj_uint32_t ts = pj_ntohl(hdr->ts);
    
    
    pj_lock_acquire(a->timer_lock);
    
    
    a->rtp_flag = PJ_TRUE;
    
    
    if (hdr->pt == RTP_NACK_PT)
    {
        
        a->stat.dup++;
        
        
        hdr->pt = a->rtp_rx_ses->out_pt;
    }
    
    
    a->stat.rcv++;
    
    
    if (ts > a->stat.rx_ts_local)
        a->stat.rx_ts_local = ts;
    
    pj_lock_release(a->timer_lock);
    
    a->stream_rtp_cb(a->stream_user_data, pkt, size);
    return;
}


static void transport_rtcp_cb(void *user_data, void *pkt, pj_ssize_t size)
{
    struct nack_adapter *a = (struct nack_adapter*)user_data;
    pjmedia_rtcp_common *hdr = (pjmedia_rtcp_common*)pkt;
    
    
    if (hdr->pt == RTCP_ACK)
    {
        a->stat.rx_ts_remote = pj_ntohl(hdr->ssrc);
        
        transport_retransmit_rtp(a, a->stat.rx_ts_remote, a->stat.tx_ts_local);
        
        return;
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
    struct nack_adapter *a = (struct nack_adapter*)tp;
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
    struct nack_adapter *a = (struct nack_adapter *)tp;
    pj_status_t status;
    pjmedia_transport_attach_param param;
    pjmedia_port *port;
    
    
    
    
    
    a->stream_user_data = att_param->user_data;
    a->stream_rtp_cb = att_param->rtp_cb;
    a->stream_rtcp_cb = att_param->rtcp_cb;
    a->stream_ref = att_param->stream;
    
    
    pjmedia_stream_rtp_sess_info session_info;
    
    switch (att_param->media_type)
    {
        case PJMEDIA_TYPE_AUDIO:
        {
            sprintf(a->this_file, this_file_tmpl, "aud");
            
            pjmedia_stream_get_rtp_session_info(a->stream_ref, &session_info);
            
            status = pjmedia_stream_get_port(a->stream_ref, &port);
            if (status == PJ_SUCCESS)
            {
                a->ts_delay = BURST_CHECK_DELAY * (port->info.fmt.det.aud.clock_rate / 1000);
                a->ts_step = (port->info.fmt.det.aud.frame_time_usec / 1000) * (port->info.fmt.det.aud.clock_rate / 1000);
            }
            else
            {
                PJ_LOG(4, (a->this_file, "NACK get media stream params failed"));
                return PJ_EINVALIDOP;
            }
            
            break;
        }
        default:
        {
            PJ_LOG(4, (a->this_file, "NACK unsupported media type. Attach to media stream failed"));
            return PJ_EINVALIDOP;
        }
    }
    
    a->delay.sec = 0;
    a->delay.msec = BURST_CHECK_DELAY;
    
    a->rtp_tx_ses = (pjmedia_rtp_session *)session_info.tx_rtp;
    a->rtp_rx_ses = (pjmedia_rtp_session *)session_info.rx_rtp;
    a->rtcp_ses = (pjmedia_rtcp_session *)session_info.rtcp;
    
    
    
    
    
    
    status = pj_lock_create_simple_mutex(a->pool, "timer_lock", &a->timer_lock);
    if (status != PJ_SUCCESS)
    {
        PJ_LOG(4, (a->this_file, "NACK timer callback lock creation failed"));
        return PJ_EUNKNOWN;
    }
    
    
    status = pj_lock_create_simple_mutex(a->pool, "rtp_lock", &a->rtp_lock);
    if (status != PJ_SUCCESS)
    {
        PJ_LOG(4, (a->this_file, "NACK rtp send lock creation failed"));
        return PJ_EUNKNOWN;
    }
    
    retr_list_reset(&a->retr_list);
    
    a->rtp_flag = PJ_FALSE;
    
    
    
    
#if defined (__ANDROID__) 
    posix_timer_schedule(a, a->delay.sec, a->delay.msec);
#else
    a->timer_entry.id = ACK_TIMER;
    pjsip_endpt_schedule_timer(a->sip_endpt, &a->timer_entry, &a->delay);
#endif 
    
    
    
    memset(&a->stat, 0, sizeof(a->stat));
    
    
    param = *att_param;
    param.user_data = a;
    param.rtp_cb = &transport_rtp_cb;
    param.rtcp_cb = &transport_rtcp_cb;
    
    status = pjmedia_transport_attach2(a->slave_tp, &param);
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
    struct nack_adapter *a = (struct nack_adapter*)tp;
    
    PJ_UNUSED_ARG(strm);
    
#if defined (__ANDROID__) 
    posix_timer_cancel(a);
    a->posix_timer_id = -1;
#else
    if (a->timer_entry.id > 0)
        pjsip_endpt_cancel_timer(a->sip_endpt, &a->timer_entry);
#endif 
    
    PJ_LOG(4, (a->this_file, "NACK statistics: sent=%lu retransmit=%lu recieve=%lu requested=%lu duplicated=%lu", a->stat.snd, a->stat.rtr, a->stat.rcv, a->stat.req, a->stat.dup));
    
    if (a->stream_user_data != NULL)
    {
        pjmedia_transport_detach(a->slave_tp, a);
        a->stream_user_data = NULL;
        a->stream_rtp_cb = NULL;
        a->stream_rtcp_cb = NULL;
        a->stream_ref = NULL;
    }
}

static void transport_retransmit_rtp(void *user_data, const pj_uint32_t ts_from, pj_uint32_t ts_to)
{
    struct nack_adapter *a = (struct nack_adapter *)user_data;
    pj_status_t status;
    retr_entry *e = NULL;
    pjmedia_rtp_hdr *hdr;
    int len;
    pj_uint16_t rtr = 0;
    pj_uint32_t ts;
    
    if (!ts_from || !ts_to)
        return;
    
    pj_lock_acquire(a->rtp_lock);
    
    
    e = retr_list_search(&a->retr_list, ts_from);
    
    
    ts = ts_to - ts_from;
    
    
    if (ts < a->ts_delay)
        e = NULL;
    else if (e)
    
        e = retr_list_next(&a->retr_list, e);
    
    PJ_LOG(5, (a->this_file, "NACK retransmit request ts_from=%lu ts_to=%lu last_tx=%lu", ts_from, ts_to, a->stat.tx_ts_local));
    
    
    
    ts = ts_from + a->ts_delay + a->ts_step;
    
    
    while (e && e->ts < ts)
    {
        
        status = pjmedia_rtp_encode_rtp(a->rtp_tx_ses, -1, -1, -1, 0, (const void **)&hdr, &len);
        
        
        
        
        
        
        
        ((pjmedia_rtp_hdr *)e->pkt)->seq = hdr->seq;
        
        ((pjmedia_rtp_hdr *)e->pkt)->pt = RTP_NACK_PT;
        
        status = pjmedia_transport_send_rtp(a->slave_tp, e->pkt, e->size);
        
        
        
        
        rtr++;
        
        
        a->stat.rtr++;
        
        e = retr_list_next(&a->retr_list, e);
    }
    
    pj_lock_release(a->rtp_lock);
    
    PJ_LOG(5, (a->this_file, "NACK retransmitted ts_from=%lu ts_to=%lu ts_wnd=%lu count=%u", ts_from, ts_to, ts, rtr));
}



static pj_status_t transport_send_rtp(pjmedia_transport *tp, const void *pkt, pj_size_t size)
{
    struct nack_adapter *a = (struct nack_adapter *)tp;
    pj_status_t status;
    
    pj_lock_acquire(a->rtp_lock);
    
    
    a->stat.tx_ts_local = pj_ntohl(((pjmedia_rtp_hdr *)pkt)->ts);
    
    
    a->stat.snd++;
    
    
    
    retr_list_push_back(&a->retr_list, pkt, (pj_uint16_t)size, a->stat.tx_ts_local);
    
    status = pjmedia_transport_send_rtp(a->slave_tp, pkt, size);
    
    
    
    
    
    a->stat.tx_size_timer++;
    
    pj_lock_release(a->rtp_lock);
    
    return status;
}


static pj_status_t transport_send_rtcp_ack(pjmedia_transport *tp, pj_uint32_t ts)
{
    struct nack_adapter *a = (struct nack_adapter*)tp;
    pj_uint8_t buf[64];
    pjmedia_rtcp_common *hdr = (pjmedia_rtcp_common*)buf;
    pj_size_t len = sizeof(*hdr);
    pj_uint8_t *p;
    
    
    
    if (!a->rtcp_ses)
        return PJ_EINVALIDOP;
    
    
    pj_memcpy(hdr, &a->rtcp_ses->rtcp_sr_pkt.common, len);
    hdr->pt = RTCP_ACK;
    p = (pj_uint8_t *)hdr + len;
    
    
    hdr->ssrc = pj_htonl(ts);
    
    
    
    hdr->length = pj_htons((pj_uint16_t)(len / sizeof(pj_uint32_t) - 1));
    
    
    
    
    
    a->stat.req++;
    
    PJ_LOG(5, (a->this_file, "NACK sending positive request for ts=%lu", ts));
    
    return transport_send_rtcp(tp, (void *)buf, len);
}


static pj_status_t transport_send_rtcp(pjmedia_transport *tp, const void *pkt, pj_size_t size)
{
    struct nack_adapter *a = (struct nack_adapter *)tp;
    
    
    
    
    return pjmedia_transport_send_rtcp(a->slave_tp, pkt, size);
}



static pj_status_t transport_send_rtcp2(pjmedia_transport *tp, const pj_sockaddr_t *addr, unsigned addr_len, const void *pkt, pj_size_t size)
{
    struct nack_adapter *a = (struct nack_adapter *)tp;
    
    
    return pjmedia_transport_send_rtcp2(a->slave_tp, addr, addr_len, pkt, size);
}


static pj_status_t transport_media_create(pjmedia_transport *tp, pj_pool_t *sdp_pool, unsigned options, const pjmedia_sdp_session *rem_sdp, unsigned media_index)
{
    struct nack_adapter *a = (struct nack_adapter *)tp;
    
    
    if (rem_sdp)
    {
        
    }
    
    
    return pjmedia_transport_media_create(a->slave_tp, sdp_pool, options, rem_sdp, media_index);
}


static pj_status_t transport_encode_sdp(pjmedia_transport *tp, pj_pool_t *sdp_pool, pjmedia_sdp_session *local_sdp, const pjmedia_sdp_session *rem_sdp, unsigned media_index)
{
    struct nack_adapter *a = (struct nack_adapter *)tp;
    
    
    if (rem_sdp)
    {
        
    }
    
    else
    {
        
    }
    
    
    return pjmedia_transport_encode_sdp(a->slave_tp, sdp_pool, local_sdp, rem_sdp, media_index);
}


static pj_status_t transport_media_start(pjmedia_transport *tp, pj_pool_t *pool, const pjmedia_sdp_session *local_sdp, const pjmedia_sdp_session *rem_sdp, unsigned media_index)
{
    struct nack_adapter *a = (struct nack_adapter *)tp;
    
    
    if (rem_sdp)
    {
        
    }
    
    
    return pjmedia_transport_media_start(a->slave_tp, pool, local_sdp, rem_sdp, media_index);
}


static pj_status_t transport_media_stop(pjmedia_transport *tp)
{
    struct nack_adapter *a = (struct nack_adapter *)tp;
    
    
    return pjmedia_transport_media_stop(a->slave_tp);
}


static pj_status_t transport_simulate_lost(pjmedia_transport *tp, pjmedia_dir dir, unsigned pct_lost)
{
    struct nack_adapter *a = (struct nack_adapter *)tp;
    
    return pjmedia_transport_simulate_lost(a->slave_tp, dir, pct_lost);
}


static pj_status_t transport_destroy(pjmedia_transport *tp)
{
    struct nack_adapter *a = (struct nack_adapter *)tp;
    
    
    if (a->del_base)
        pjmedia_transport_close(a->slave_tp);
    
    
    pj_pool_release(a->pool);
    
    return PJ_SUCCESS;
}
