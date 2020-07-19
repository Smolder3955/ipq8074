/*
 * Copyright (c) 2014,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _OSIF_RAWMODE_SIM__H_
#define _OSIF_RAWMODE_SIM__H_

#include <qdf_nbuf.h>           /* qdf_nbuf_t */
#include <ieee80211_var.h>      /* Generally required constructs */

/* Raw Mode simulation - conversion between Raw 802.11 format and other
 * formats.
 */

/* Max MSDU length supported in the simulation */
#define MAX_RAWSIM_80211_MSDU_LEN                  (1508)

/* Number of MSDUs to place in A-MSDU */
#define NUM_RAWSIM_80211_MSDUS_IN_AMSDU            (2)

#define L_GET_LLC_ETHTYPE(ptr) \
    (((struct llc*)((ptr)))->llc_un.type_snap.ether_type)

#define L_LLC_ETHTYPE_OFFSET \
    ((u_int8_t*)&(((struct llc*)(0))->llc_un.type_snap.ether_type) - \
     (u_int8_t*)0)

#define L_ETH_ETHTYPE_SIZE \
    (sizeof(((struct ether_header*)(0))->ether_type))

#define GET_UNCONSUMED_CNT(is_frag, psctx, nonfragcnt)  \
    ((is_frag)? (psctx)->unconsumed_cnt_total:(nonfragcnt))

#define RAWSIM_PRINT_TXRXLEN_ERR_CONDITION            (0)

/* Fragment stream processing */

/* 
 * We keep a limit on the peek offset and number of bytes, for simplicity
 * so that we don't have to cross more than one nbuf boundary.
 */
#define OSIF_RAW_RX_FRAGSTREAM_PEEK_OFFSET_MAX   \
                                (sizeof(struct ieee80211_qosframe_addr4) + \
                                 sizeof(struct ether_header) + \
                                 sizeof(struct llc))

#define OSIF_RAW_RX_FRAGSTREAM_PEEK_NBYTES_MAX   (16)

/* 
 * Context for processing read and peek operations on an nbuf fragment stream
 * corresponding to an MPDU.
 */
typedef struct _osif_raw_rx_fragstream_ctx
{
    /* Whether this context is valid. */
    u_int8_t is_valid;
    
    /* Head nbuf for fragment stream */
    qdf_nbuf_t list_head;
    
    /* Total 802.11 header size. To be determined by user of context. */
    u_int16_t headersize;
    
    /* Total 802.11 trailer size. To be determined by user of context. */
    u_int16_t trailersize;
    
    /* Current nbuf being used */
    qdf_nbuf_t currnbuf;
    
    /* 
     * Position in current nbuf from where next read consumption/peek should
     * start
     */
    u_int8_t *currnbuf_ptr;

    /* Next nbuf to be used */
    qdf_nbuf_t nextnbuf;
    
    /* Count of unconsumed bytes in nbuf currently being processed */
    u_int32_t unconsumed_cnt_curr;

    /* Count of unconsumed bytes in all fragment nbufs put together */
    u_int32_t unconsumed_cnt_total;
} osif_raw_rx_fragstream_ctx;


#define RAWSIM_PKT_HEXDUMP(_buf, _len) do {                  \
    int i, mod;                                              \
    unsigned char ascii[17];                                 \
    unsigned char *pc = (_buf);                              \
                                                             \
    for (i = 0; i < (_len); i++) {                           \
        mod = i % 16;                                        \
        if (mod == 0) {                                      \
            if (i != 0)                                      \
                printk("  %s\n", ascii);               \
        }                                                    \
        printk(" %02x", pc[i]);                        \
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))                \
            ascii[mod] = '.';                                \
        else                                                 \
            ascii[mod] = pc[i];                              \
        ascii[(mod) + 1] = '\0';                             \
    }                                                        \
    while ((i % 16) != 0) {                                  \
        printk("   ");                                 \
        i++;                                                 \
    }                                                        \
    printk("  %s\n", ascii);                           \
} while (0)


#define RAWSIM_TXRX_LIST_APPEND(head, tail, elem)            \
do {                                                         \
    if (!(head)) {                                           \
        (head) = (elem);                                     \
    } else {                                                 \
        qdf_nbuf_set_next((tail), (elem));                   \
    }                                                        \
    (tail) = (elem);                                         \
} while (0)

/* Delete a sub linked list starting at subhead and ending at subtail, from in
 * within a main linked list starting at head and ending at tail, with prev
 * pointing to the nbuf prior to subhead.  For efficiency, it is the
 * responsibility of the caller to ensure that the arguments are valid. tail's
 * next must be set to NULL. If subhead is equal to head, then prev must be
 * NULL.
 */
#define RAWSIM_TXRX_SUBLIST_DELETE(_head, _tail, _prev, _subhead, _subtail)  \
do {                                                                         \
    qdf_nbuf_t next = NULL;                                                  \
    qdf_nbuf_t nbuf = (_subhead);                                            \
                                                                             \
    if ((_head) == (_subhead)) {                                             \
        (_head) = qdf_nbuf_next((_subtail));                                 \
    }                                                                        \
                                                                             \
    if (!(_head)) {                                                          \
        (_tail) = NULL;                                                      \
    } else if ((_tail) == (_subtail)) {                                      \
        (_tail) = (_prev);                                                   \
        if ((_tail)) {                                                       \
            qdf_nbuf_set_next((_tail), NULL);                                \
        }                                                                    \
    } else if ((_prev)) {                                                    \
        qdf_nbuf_set_next((_prev), qdf_nbuf_next((_subtail)));               \
    }                                                                        \
                                                                             \
    while(nbuf != (_subtail))                                                \
    {                                                                        \
        next = qdf_nbuf_next(nbuf);                                          \
        qdf_nbuf_free(nbuf);                                                 \
        nbuf = next;                                                         \
    }                                                                        \
                                                                             \
    qdf_nbuf_free((_subtail));                                               \
} while (0)

/* Delete nbuf from in within a linked list starting at head and ending at tail,
 * with prev pointing to the element prior to nbuf.  For efficiency, it is the
 * responsibility of the caller to ensure that the arguments are valid. tail's
 * next must be set to NULL. If nbuf is equal to head, then prev must be NULL.
 */
#define RAWSIM_TXRX_NODE_DELETE(_head, _tail, _prev, _nbuf)                  \
do {                                                                         \
    if ((_head) == (_nbuf)) {                                                \
        (_head) = qdf_nbuf_next((_nbuf));                                    \
    }                                                                        \
                                                                             \
    if (!(_head)) {                                                          \
        (_tail) = NULL;                                                      \
    } else if ((_tail) == (_nbuf)) {                                         \
        (_tail) = (_prev);                                                   \
        if ((_tail)) {                                                       \
            qdf_nbuf_set_next((_tail), NULL);                                \
        }                                                                    \
    } else if ((_prev)) {                                                    \
        qdf_nbuf_set_next((_prev), qdf_nbuf_next((_nbuf)));                  \
    }                                                                        \
                                                                             \
    qdf_nbuf_free((_nbuf));                                                  \
} while (0)

#endif /* _OSIF_RAWMODE_SIM__H_ */
