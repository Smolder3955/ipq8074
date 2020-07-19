/*****************************************************************************/
/*! \file ath_desc.c
**  \brief ATH Descriptor Processing
**
**  This file contains the functionality for management of descriptor queues
**  in the ATH object.
**
** Copyright (c) 2009, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
**
*/

#include "ath_internal.h"

/******************************************************************************/
/*!
**  \brief Set up DMA descriptors
**
**  This function will allocate both the DMA descriptor structure, and the
**  buffers it contains.  These are used to contain the descriptors used
**  by the system.
**
**  \param sc Pointer to ATH object ("this" pointer)
**  \param dd Pointer to DMA descriptor structure being allocated
**  \param head Pointer to queue head that manages the DMA descriptors
**  \param name String that names the dma descriptor object
**  \param nbuf Number of buffers to allocate for this object
**  \param ndesc Number of descriptors used for each buffer.  Used for fragmentation.
**
**  \return -ENOMEM if unable to allocate memory
**  \return   0 upon success
*/

int
ath_descdma_setup(
    struct ath_softc *sc,
    struct ath_descdma *dd, ath_bufhead *head,
    const char *name, int nbuf, int ndesc, int is_tx, int frag_per_msdu)
{
#define    DS2PHYS(_dd, _ds) \
    ((_dd)->dd_desc_paddr + ((caddr_t)(_ds) - (caddr_t)(_dd)->dd_desc))
#define ATH_DESC_4KB_BOUND_CHECK(_daddr, _len) ((((u_int32_t)(_daddr) & 0xFFF) > (0x1000 - (_len))) ? 1 : 0)
#define ATH_DESC_4KB_BOUND_NUM_SKIPPED(_len) ((_len) / 4096)

    u_int8_t *ds;
    struct ath_buf *bf;
    int i, bsize, error, desc_len;

    uint32_t nbuf_left, nbuf_alloc, alloc_size, buf_arr_size;
    int j, k;
    struct ath_buf **bf_arr;

    if (is_tx) {
        desc_len = sc->sc_txdesclen;

        /* legacy tx descriptor needs to be allocated for each fragment */
        if (sc->sc_num_txmaps == 1) {
            nbuf = nbuf * frag_per_msdu; /*ATH_FRAG_PER_MSDU;*/
        }
    } else {
        desc_len = sizeof(struct ath_desc);
    }

    DPRINTF(sc, ATH_DEBUG_RESET, "%s: %s DMA: %u buffers %u desc/buf %d desc_len\n",
             __func__, name, nbuf, ndesc, desc_len);

    /* ath_desc must be a multiple of DWORDs */
    if ((desc_len % 4) != 0) {
        DPRINTF(sc, ATH_DEBUG_FATAL, "%s: ath_desc not DWORD aligned\n",
                __func__ );
        ASSERT((desc_len % 4) == 0);
        error = -ENOMEM;
        goto fail;
    }

    dd->dd_name = name;
    dd->dd_desc_len = desc_len * nbuf * ndesc;

    /*
     * WAR for bug 30982 (Merlin)
     * Need additional DMA memory because we can't use descriptors that cross the
     * 4K page boundary. Assume one skipped descriptor per 4K page.
     */

    if (sc->sc_ah && !ath_hal_has4kbsplittrans(sc->sc_ah)) {
        int numdescpage = 4096/(desc_len*ndesc);
        dd->dd_desc_len = (nbuf/numdescpage + 1 ) * 4096;
    }
#if ATH_SUPPORT_DESC_CACHABLE
    if ((strcmp(name, "tx") == 0) ||
        ((strcmp(name, "rx") == 0) && !sc->sc_enhanceddmasupport)) { 
        dd->dd_desc = (void *)OS_MALLOC_NONCONSISTENT(sc->sc_osdev,
                                       dd->dd_desc_len, &dd->dd_desc_paddr,
                                       OS_GET_DMA_MEM_CONTEXT(dd, dd_dmacontext),
                                       sc->sc_reg_parm.shMemAllocRetry);
    }
    else 
#endif
    {
        /* allocate descriptors */
        dd->dd_desc = (void *)OS_MALLOC_CONSISTENT(sc->sc_osdev,
                                       dd->dd_desc_len, &dd->dd_desc_paddr,
                                       OS_GET_DMA_MEM_CONTEXT(dd, dd_dmacontext),
                                       sc->sc_reg_parm.shMemAllocRetry);
    }
    if (dd->dd_desc == NULL) {
        error = -ENOMEM;
        goto fail;
    }
    ds = dd->dd_desc;
    DPRINTF(sc, ATH_DEBUG_RESET, "%s: %s DMA map: %pK (%u) -> %llx (%u)\n",
            __func__, dd->dd_name, ds, (u_int32_t) dd->dd_desc_len,
            ito64(dd->dd_desc_paddr), /*XXX*/ (u_int32_t) dd->dd_desc_len);

    /* allocate buffers */
    bsize = sizeof(struct ath_buf) * nbuf;

    buf_arr_size = ((nbuf * sizeof(struct ath_buf))/MAX_BUF_MEM_ALLOC_SIZE + 2) ;   /* one extra element is reserved at end to indicate the end of array */
    bf_arr = (struct ath_buf **)OS_MALLOC(sc->sc_osdev, 
                                        (sizeof(struct ath_buf *) * buf_arr_size), GFP_KERNEL);
    if(!bf_arr)
    {
        error = -ENOMEM;
        goto fail2;
    }
    OS_MEMZERO(bf_arr, (buf_arr_size * sizeof(struct ath_buf *)));

    TAILQ_INIT(head);

    nbuf_left = nbuf;
    nbuf_alloc = (MAX_BUF_MEM_ALLOC_SIZE / (sizeof(struct ath_buf)));

    for(j = 0; (nbuf_left && (j < (buf_arr_size-1))); j++)
    {
        nbuf_alloc = MIN(nbuf_left, nbuf_alloc);
        alloc_size = (nbuf_alloc * sizeof(struct ath_buf));

        bf_arr[j] = (struct ath_buf *)OS_MALLOC(sc->sc_osdev, alloc_size, GFP_KERNEL);
        if(bf_arr[j] == NULL)
        {
            for(k = 0; k < j; k++)
            {
                OS_FREE(bf_arr[k]);
                bf_arr[k] = NULL;
            }
            error = -ENOMEM;
            goto fail2;
        }
        else
        {
            OS_MEMZERO(bf_arr[j], alloc_size);
            nbuf_left -= nbuf_alloc;
            bf = bf_arr[j];
            for (i = 0; i < nbuf_alloc; i++, bf++, ds += (desc_len * ndesc)) {
                bf->bf_desc = ds;
                bf->bf_daddr = DS2PHYS(dd, ds);
                if (sc->sc_ah && !ath_hal_has4kbsplittrans(sc->sc_ah)) {
                    /*
                    * WAR for bug 30982.
                    * Skip descriptor addresses which can cause 4KB boundary crossing (addr + length)
                    * with a 32 dword descriptor fetch.
                    */
                    if (ATH_DESC_4KB_BOUND_CHECK(bf->bf_daddr, desc_len * ndesc)) {
                        ds += 0x1000 - (bf->bf_daddr & 0xFFF);    /* start from the next page */
                        bf->bf_desc = ds;
                        bf->bf_daddr = DS2PHYS(dd, ds);
                    }
                }
                TAILQ_INSERT_TAIL(head, bf, bf_list);
            }
        }
    }

    dd->dd_bufptr = bf_arr;

#ifdef ATH_DEBUG_MEM_LEAK
    dd->dd_num_buf = nbuf;
#endif

    /* 
     * For OS's that need to allocate dma context to be used to 
     * send down to hw, do that here. (BSD is the only one that needs
     * it currently.)
     */ 
    ALLOC_DMA_CONTEXT_POOL(sc->sc_osdev, name, nbuf);

    return 0;
fail2:

#if ATH_SUPPORT_DESC_CACHABLE
    if ((strcmp(name, "tx") == 0) ||
        ((strcmp(name, "rx") == 0) && !sc->sc_enhanceddmasupport)) { 
        OS_FREE_NONCONSISTENT(sc->sc_osdev, dd->dd_desc_len,
                       dd->dd_desc, dd->dd_desc_paddr,
                       OS_GET_DMA_MEM_CONTEXT(dd, dd_dmacontext));
    }else 
#endif
    {
        OS_FREE_CONSISTENT(sc->sc_osdev, dd->dd_desc_len,
                       dd->dd_desc, dd->dd_desc_paddr,
                       OS_GET_DMA_MEM_CONTEXT(dd, dd_dmacontext));
    }
    if (bf_arr) {
        OS_FREE(bf_arr);
    }
fail:
    OS_MEMZERO(dd, sizeof(*dd));
    return error;
#undef ATH_DESC_4KB_BOUND_CHECK
#undef ATH_DESC_4KB_BOUND_NUM_SKIPPED
#undef DS2PHYS
}

int
ath_txstatus_setup(
    struct ath_softc *sc,
    struct ath_descdma *dd,
    const char *name, int ndesc)
{
    int error;

    /* allocate transmit status ring */
    dd->dd_desc = (void *)OS_MALLOC_CONSISTENT(sc->sc_osdev,
                                       (sc->sc_txstatuslen * ndesc), &dd->dd_desc_paddr,
                                       OS_GET_DMA_MEM_CONTEXT(dd, dd_dmacontext),
                                       sc->sc_reg_parm.shMemAllocRetry);
    if (dd->dd_desc == NULL) {
        error = -ENOMEM;
        goto fail;
    }

    dd->dd_desc_len = sc->sc_txstatuslen * ndesc;
    dd->dd_name = name;

    DPRINTF(sc, ATH_DEBUG_RESET, "%s: %s DMA map: %pK (%u) -> %llx (%u)\n",
            __func__, dd->dd_name, dd->dd_desc, (u_int32_t) dd->dd_desc_len,
            ito64(dd->dd_desc_paddr), (u_int32_t) dd->dd_desc_len);
    /*
     * For OS's that need to allocate dma context to be used to
     * send down to hw, do that here. (BSD is the only one that needs
     * it currently.)
     */
    ALLOC_DMA_CONTEXT_POOL(sc->sc_osdev, name, 1);

    return 0;

fail:
	OS_MEMZERO(dd, sizeof(*dd));
	return error;
}

/******************************************************************************/
/*!
**  \brief Cleanup DMA descriptors
**
**  This function will free the DMA block that was allocated for the descriptor
**  pool.  Since this was allocated as one "chunk", it is freed in the same
**  manner.
**
**  \param sc Pointer to ATH object ("this" pointer)
**  \param dd Pointer to dma descriptor object that is being freed
**  \param head Pointer to queue head containing the pointer to the queue.
**
**  \return N/A
*/

void
ath_descdma_cleanup(
    struct ath_softc *sc,
    struct ath_descdma *dd,
    ath_bufhead *head)
{
    int i;
    struct ath_buf **bf_arr = dd->dd_bufptr;
#if ATH_SUPPORT_DESC_CACHABLE
    const char * name = dd->dd_name;
#endif

    /* Free memory associated with descriptors */

#if ATH_SUPPORT_DESC_CACHABLE
    if ((strcmp(name, "tx") == 0) ||
        ((strcmp(name, "rx") == 0) && !sc->sc_enhanceddmasupport)) { 
        OS_FREE_NONCONSISTENT(sc->sc_osdev, dd->dd_desc_len,
                       dd->dd_desc, dd->dd_desc_paddr,
                       OS_GET_DMA_MEM_CONTEXT(dd, dd_dmacontext));

    }else
#endif
    {
        OS_FREE_CONSISTENT(sc->sc_osdev, dd->dd_desc_len,
                       dd->dd_desc, dd->dd_desc_paddr,
                       OS_GET_DMA_MEM_CONTEXT(dd, dd_dmacontext));

    }

    TAILQ_INIT(head);

    for(i = 0; bf_arr[i]; i++)
    {
        OS_FREE(bf_arr[i]);
        bf_arr[i] = NULL;
    }

    OS_FREE(dd->dd_bufptr);

    /* 
     * For OS's that need to allocate dma context to be used to 
     * send down to hw, free them here. (BSD is the only one that needs
     * it currently.)
     */ 
    FREE_DMA_CONTEXT_POOL(sc->sc_osdev, dd->dd_name);

    OS_MEMZERO(dd, sizeof(*dd));
}

void
ath_txstatus_cleanup(
    struct ath_softc *sc,
    struct ath_descdma *dd)
{
    /* Free memory associated with descriptors */
    OS_FREE_CONSISTENT(sc->sc_osdev, dd->dd_desc_len,
                       dd->dd_desc, dd->dd_desc_paddr,
                       OS_GET_DMA_MEM_CONTEXT(dd, dd_dmacontext));

    /* 
     * For OS's that need to allocate dma context to be used to 
     * send down to hw, free them here. (BSD is the only one that needs
     * it currently.)
     */ 
    FREE_DMA_CONTEXT_POOL(sc->sc_osdev, dd->dd_name);
}

/******************************************************************************/
/*!
**  \brief Endian Swap for transmit descriptor
**
**  Since the hardware expects all 32 bit values and pointers to be in little
**  endian mode, this function is called in case the cpu is in big endian mode.
**  The AH_NEED_DESC_SWAP define is used to flag this condition.
**
**  \param ds Pointer to descriptor to be "fixed"
**
**  \return N/A
*/

void
ath_desc_swap(struct ath_desc *ds)
{
#ifdef AH_NEED_DESC_SWAP
    ds->ds_link = cpu_to_le32(ds->ds_link);
    ds->ds_data = cpu_to_le32(ds->ds_data);
    ds->ds_ctl0 = cpu_to_le32(ds->ds_ctl0);
    ds->ds_ctl1 = cpu_to_le32(ds->ds_ctl1);
    ds->ds_hw[0] = cpu_to_le32(ds->ds_hw[0]);
    ds->ds_hw[1] = cpu_to_le32(ds->ds_hw[1]);
#endif
}
