//------------------------------------------------------------------------------
// Copyright (c) 2004-2010 Atheros Communications Inc.
// All rights reserved.
//
// $ATH_LICENSE_HOSTSDK0_C$
//
// Author(s): ="Atheros"
//------------------------------------------------------------------------------
/*
 *  Copyright (c) 2014 Qualcomm Atheros, Inc.  All rights reserved. 
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners. 
 */

//#include <a_config.h>
#include "athdefs.h"
#include "a_types.h"
#include "a_osapi.h"
#include "htc_packet.h"

#define AR6000_DATA_OFFSET    64

void a_netbuf_enqueue(A_NETBUF_QUEUE_T *q, void *pkt)
{
    skb_queue_tail((struct sk_buff_head *) q, (struct sk_buff *) pkt);
}

void a_netbuf_prequeue(A_NETBUF_QUEUE_T *q, void *pkt)
{
    skb_queue_head((struct sk_buff_head *) q, (struct sk_buff *) pkt);
}

void *a_netbuf_dequeue(A_NETBUF_QUEUE_T *q)
{
    return((void *) skb_dequeue((struct sk_buff_head *) q));
}

int a_netbuf_queue_size(A_NETBUF_QUEUE_T *q)
{
    return(skb_queue_len((struct sk_buff_head *) q));
}

int a_netbuf_queue_empty(A_NETBUF_QUEUE_T *q)
{
    return(skb_queue_empty((struct sk_buff_head *) q));
}

void a_netbuf_queue_init(A_NETBUF_QUEUE_T *q)
{
    skb_queue_head_init((struct sk_buff_head *) q);
}

#ifdef AR6K_ALLOC_DEBUG
void *
a_netbuf_alloc(int size, const char *func, int lineno)
#else
void *
a_netbuf_alloc(int size)
#endif
{
    struct sk_buff *skb;
    size += 2 * (A_GET_CACHE_LINE_BYTES()); /* add some cacheline space at front and back of buffer */
    skb = dev_alloc_skb(AR6000_DATA_OFFSET + sizeof(HTC_PACKET) + size);
    if (skb) {
        skb_reserve(skb, AR6000_DATA_OFFSET + sizeof(HTC_PACKET) + A_GET_CACHE_LINE_BYTES());    
#ifdef AR6K_ALLOC_DEBUG
        __a_meminfo_add(skb, size, func, lineno);
#endif
    }
    return ((void *)skb);
}

/*
 * Allocate an SKB w.o. any encapsulation requirement.
 */
#ifdef AR6K_ALLOC_DEBUG
void *
a_netbuf_alloc_raw(int size, const char *func, int lineno)
#else
void *
a_netbuf_alloc_raw(int size)
#endif
{
    struct sk_buff *skb;

    skb = dev_alloc_skb(size);
#ifdef AR6K_ALLOC_DEBUG
    __a_meminfo_add(skb, size, func, lineno);
#endif
    return ((void *)skb);
}

#ifdef AR6K_ALLOC_DEBUG
void a_netbuf_manage(void *bufPtr, const char *func, int lineno)
{
    struct sk_buff *skb = (struct sk_buff *)bufPtr;
    
    __a_meminfo_add(skb, A_NETBUF_LEN(skb), func, lineno);
}

void a_netbuf_unmanage(void *bufPtr)
{
    a_meminfo_del(bufPtr);
}
#endif

void
a_netbuf_free(void *bufPtr)
{
    struct sk_buff *skb = (struct sk_buff *)bufPtr;
#ifdef AR6K_ALLOC_DEBUG
    a_meminfo_del(bufPtr);
#endif
    dev_kfree_skb(skb);
}

A_UINT32
a_netbuf_to_len(void *bufPtr)
{
    return (((struct sk_buff *)bufPtr)->len);
}

void *
a_netbuf_to_data(void *bufPtr)
{
    return (((struct sk_buff *)bufPtr)->data);
}

/*
 * Add len # of bytes to the beginning of the network buffer
 * pointed to by bufPtr
 */
A_STATUS
a_netbuf_push(void *bufPtr, A_INT32 len)
{
    skb_push((struct sk_buff *)bufPtr, len);

    return A_OK;
}

/*
 * Add len # of bytes to the beginning of the network buffer
 * pointed to by bufPtr and also fill with data
 */
A_STATUS
a_netbuf_push_data(void *bufPtr, char *srcPtr, A_INT32 len)
{
    skb_push((struct sk_buff *) bufPtr, len);
    A_MEMCPY(((struct sk_buff *)bufPtr)->data, srcPtr, len);

    return A_OK;
}

/*
 * Add len # of bytes to the end of the network buffer
 * pointed to by bufPtr
 */
A_STATUS
a_netbuf_put(void *bufPtr, A_INT32 len)
{
    skb_put((struct sk_buff *)bufPtr, len);

    return A_OK;
}

/*
 * Add len # of bytes to the end of the network buffer
 * pointed to by bufPtr and also fill with data
 */
A_STATUS
a_netbuf_put_data(void *bufPtr, char *srcPtr, A_INT32 len)
{
    char *start = (char*)(((struct sk_buff *)bufPtr)->data +
        ((struct sk_buff *)bufPtr)->len);
    skb_put((struct sk_buff *)bufPtr, len);
    A_MEMCPY(start, srcPtr, len);

    return A_OK;
}


/*
 * Trim the network buffer pointed to by bufPtr to len # of bytes 
 */
A_STATUS
a_netbuf_setlen(void *bufPtr, A_INT32 len)
{
    skb_trim((struct sk_buff *)bufPtr, len);

    return A_OK;
}

/*
 * Chop of len # of bytes from the end of the buffer.
 */
A_STATUS
a_netbuf_trim(void *bufPtr, A_INT32 len)
{
    skb_trim((struct sk_buff *)bufPtr, ((struct sk_buff *)bufPtr)->len - len);

    return A_OK;
}

/*
 * Chop of len # of bytes from the end of the buffer and return the data.
 */
A_STATUS
a_netbuf_trim_data(void *bufPtr, char *dstPtr, A_INT32 len)
{
    char *start = (char*)(((struct sk_buff *)bufPtr)->data +
        (((struct sk_buff *)bufPtr)->len - len));
    
    A_MEMCPY(dstPtr, start, len);
    skb_trim((struct sk_buff *)bufPtr, ((struct sk_buff *)bufPtr)->len - len);

    return A_OK;
}


/*
 * Returns the number of bytes available to a a_netbuf_push()
 */
A_INT32
a_netbuf_headroom(void *bufPtr)
{
    return (skb_headroom((struct sk_buff *)bufPtr));
}

/*
 * Removes specified number of bytes from the beginning of the buffer
 */
A_STATUS
a_netbuf_pull(void *bufPtr, A_INT32 len)
{
    skb_pull((struct sk_buff *)bufPtr, len);

    return A_OK;
}

/*
 * Removes specified number of bytes from the beginning of the buffer
 * and return the data
 */
A_STATUS
a_netbuf_pull_data(void *bufPtr, char *dstPtr, A_INT32 len)
{
    A_MEMCPY(dstPtr, ((struct sk_buff *)bufPtr)->data, len);
    skb_pull((struct sk_buff *)bufPtr, len);

    return A_OK;
}
 
#ifdef AR6K_ALLOC_DEBUG 
struct sk_buff* __adf_nbuf_alloc_debug(size_t size, int reserve, int align, char *func, int line)
{
    struct sk_buff *skb;
    unsigned long offset;

    if(align)
        size += (align - 1);

    skb = dev_alloc_skb(size);

    if (!skb) {
        printk("ERROR:NBUF alloc failed\n");
        return NULL;
    }
    
    __a_meminfo_add(skb, size, func, line);
    
    if(align){
        offset = ((unsigned long) skb->data) % align;
        if(offset)
            skb_reserve(skb, align - offset);
    }

    skb_reserve(skb, reserve);

    return skb;
}
#endif

#ifdef EXPORT_HCI_BRIDGE_INTERFACE
EXPORT_SYMBOL(a_netbuf_to_data);
EXPORT_SYMBOL(a_netbuf_put);
EXPORT_SYMBOL(a_netbuf_pull);
EXPORT_SYMBOL(a_netbuf_alloc);
EXPORT_SYMBOL(a_netbuf_free);
#endif
