
#ifndef _OL_ATH_COOKIE__
#define _OL_ATH_COOKIE__

#define MAX_COOKIE_NUM  2048
struct cookie {
    wbuf_t                 PacketContext;    /* Must be first field */
    HTC_PACKET             HtcPkt;       /* HTC packet wrapper */
    struct cookie *arc_list_next;
};
struct ol_ath_cookie {
    struct cookie *cookie_list;
    int cookie_count;
    struct cookie s_cookie_mem[MAX_COOKIE_NUM];
    qdf_spinlock_t cookie_lock;
};
void ol_cookie_init(void *ar);
void ol_cookie_cleanup(void *ar);
void ol_free_cookie(void *ar, struct cookie * cookie);
struct cookie *ol_alloc_cookie(void  *ar);
#endif /*_OL_ATH_COOKIE__*/
