/*
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _DEV_ATH_LEGACY_PS_H_
#define _DEV_ATH_LEGACY_PS_H_

#include "ath_internal.h"

#define ATH_NODE_INACT_WAIT             150     /* inactivity interval (secs) */
#define ATH_NODE_AGE_INTVAL             1000    /* age interval (TU's) */      

/*
 * In tasklet context, we use OS_MALLOC to dynamically alloc PS buffers.
 * This might not work in some OS. Hence, we provide an option here, which can
 * assign the proper malloc function for use.
 * By default, OS_MALLOC/OS_FREE is used.
 */
#define PS_MALLOC_FROM_POOL                0

#if PS_MALLOC_FROM_POOL
#define OS_MALLOC_PS(_dev, _size, _flag)    /* OS defined */
#define OS_FREE_PS(_p)                      /* OS defined */
#else
#define OS_MALLOC_PS(_dev, _size, _flag)    OS_MALLOC(_dev, _size, _flag)
#define OS_FREE_PS(_p)                      OS_FREE(_p)
#endif

/*
 * power save functions for node save queue.
 */
enum ath_node_pwrsaveq_param {
    ATH_NODE_PWRSAVEQ_DATA_Q_LEN,
    ATH_NODE_PWRSAVEQ_MGMT_Q_LEN
};

typedef struct _ath_node_pwrsaveq_info {
    u_int16_t mgt_count;
    u_int16_t data_count;
    u_int16_t mgt_len;
    u_int16_t data_len;
    u_int16_t ps_frame_count; /* frames (null,pspoll) with ps bit set */
} ath_node_pwrsaveq_info;

#if LMAC_SUPPORT_POWERSAVE_QUEUE
typedef struct ath_wbuf {
    wbuf_t                          wbuf;   /* pointer to the actual wbuf */
    ieee80211_tx_control_t          txctl;  /* copy of the txctl passed by UMAC */
    struct ath_wbuf *               next;    
} *ath_wbuf_t;

struct ath_node_pwrsaveq {
    spinlock_t  nsq_lock;
    u_int32_t   nsq_len; /* number of queued frames */
    u_int32_t   nsq_bytes; /* number of bytes queued  */
    ath_wbuf_t  nsq_whead;
    ath_wbuf_t  nsq_wtail;
    u_int16_t   nsq_max_len;
    u_int16_t   nsq_num_ps_frames; /* number of frames with PS bit set */
};  

/* Rather than using this default value, customer platforms can provide a custom value for this constant. 
   Coustomer platform will use the different define value by themself */
#ifndef ATH_NODE_PWRSAVEQ_MAX_LEN
#define ATH_NODE_PWRSAVEQ_MAX_LEN 50
#endif

#define ATH_NODE_PWRSAVEQ_INIT(_nsq) do {             \
        spin_lock_init(&((_nsq)->nsq_lock));             \
        (_nsq)->nsq_len=0;                               \
        (_nsq)->nsq_whead=0;                             \
        (_nsq)->nsq_wtail=0;                             \
        (_nsq)->nsq_bytes = 0;                           \
        (_nsq)->nsq_max_len = ATH_NODE_PWRSAVEQ_MAX_LEN; \
        (_nsq)->nsq_num_ps_frames = 0;                   \
    } while (0)

#define ATH_NODE_PWRSAVEQ_DESTROY(_nsq) do {      \
        spin_lock_destroy(&(_nsq)->nsq_lock);        \
        (_nsq)->nsq_len=0;                                              \
        KASSERT(((_nsq)->nsq_whead == NULL), ("node powersave queue is not empty")); \
    } while (0)

#define ATH_NODE_PWRSAVEQ_DATAQ(_an)     (&_an->an_dataq)
#define ATH_NODE_PWRSAVEQ_MGMTQ(_an)     (&_an->an_mgmtq)
#define ATH_NODE_PWRSAVEQ_QLEN(_nsq)     (_nsq->nsq_len)
#define ATH_NODE_PWRSAVEQ_BYTES(_nsq)    (_nsq->nsq_bytes)
#define ATH_NODE_PWRSAVEQ_LOCK(_nsq)      spin_lock(&_nsq->nsq_lock)
#define ATH_NODE_PWRSAVEQ_UNLOCK(_nsq)    spin_unlock(&_nsq->nsq_lock)
#define ATH_NODE_PWRSAVEQ_FULL(_nsq)     ((_nsq)->nsq_len >= (_nsq)->nsq_max_len)
#define ATH_NODE_PWRSAVEQ_DEQUEUE(_nsq, _w) do {   \
    _w = _nsq->nsq_whead;                       \
    if (_w) {                                    \
        _nsq->nsq_whead = (_w)->next;            \
        (_w)->next = NULL;                       \
        if ( _nsq->nsq_whead ==  NULL)           \
            _nsq->nsq_wtail =  NULL;             \
        --_nsq->nsq_len;                         \
        _nsq->nsq_bytes -= wbuf_get_pktlen((_w)->wbuf);   \
    }                                            \
} while (0)

#define ATH_NODE_PWRSAVEQ_ENQUEUE(_nsq, _w, _qlen, _age) do {\
    (_w)->next = NULL;                             \
    if (_nsq->nsq_wtail != NULL) {                       \
        _age -= wbuf_get_age((_nsq->nsq_whead)->wbuf);   \
        (_nsq->nsq_wtail)->next = _w;                    \
        _nsq->nsq_wtail = _w;                            \
    } else {                                             \
        _nsq->nsq_whead =  _nsq->nsq_wtail =  _w;        \
    }                                                    \
    _qlen=++_nsq->nsq_len;                               \
    _nsq->nsq_bytes +=  wbuf_get_pktlen((_w)->wbuf);     \
    wbuf_set_age((_w)->wbuf, _age);                      \
} while (0)

/* queue frame without changing the age */
#define ATH_NODE_PWRSAVEQ_ADD(_nsq, _w) do {\
    (_w)->next = NULL;                                   \
    if (_nsq->nsq_wtail != NULL) {                       \
        (_nsq->nsq_wtail)->next = _w;                    \
        _nsq->nsq_wtail = _w;                            \
    } else {                                             \
        _nsq->nsq_whead =  _nsq->nsq_wtail =  _w;        \
    }                                                    \
    ++_nsq->nsq_len;                                     \
    _nsq->nsq_bytes +=  wbuf_get_pktlen((_w)->wbuf);     \
} while (0)

#define ATH_NODE_PWRSAVEQ_POLL(_nsq, _w)  (_w = _nsq->nsq_whead)
    
#define  ATH_NODE_PWRSAVEQ(_q)  struct ath_node_pwrsaveq _q;
    
void ath_node_pwrsaveq_attach(struct ath_node *an);
void ath_node_pwrsaveq_detach(struct ath_node *an);
int ath_node_pwrsaveq_send(ath_node_t node, u_int8_t frame_type);
int ath_node_pwrsaveq_drain(ath_node_t node);
int ath_node_pwrsaveq_age(ath_node_t node);
void ath_node_pwrsaveq_queue(ath_node_t node, ath_wbuf_t athwbuf, u_int8_t frame_type);
void ath_node_pwrsaveq_flush(ath_node_t node);
void ath_node_pwrsaveq_get_info(ath_node_t node, void *info);
void ath_node_pwrsaveq_set_param(ath_node_t node, u_int8_t param, u_int32_t val);

#else /* LMAC_SUPPORT_POWERSAVE_QUEUE */

#define ATH_NODE_PWRSAVEQ(_q)                      /* */
#define ath_node_pwrsaveq_attach(an)                         /* */
#define ath_node_pwrsaveq_detach(an)                         /* */
#define ath_node_pwrsaveq_send(an,frame_type)                 0
#define ath_node_pwrsaveq_drain(an)                           0
#define ath_node_pwrsaveq_age(an)                            /* */
#define ath_node_pwrsaveq_queue(an, athwbuf,  frame_type)    /* */
#define ath_node_pwrsaveq_flush(an)                          /* */
#define ath_node_pwrsaveq_get_info(an,info)                  /* */
#define ath_node_pwrsaveq_set_param(an,param,val)            /* */

#endif /* LMAC_SUPPORT_POWERSAVE_QUEUE */




#endif /* _DEV_ATH_LEGACY_PS_H_ */

