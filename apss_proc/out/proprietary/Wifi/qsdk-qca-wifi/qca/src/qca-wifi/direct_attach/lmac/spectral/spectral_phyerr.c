/* Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

/*
 * Copyright (c) 2002-2009 Atheros Communications, Inc.
 * All Rights Reserved.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#include <osdep.h>
#include "spectral.h"
#if WLAN_SPECTRAL_ENABLE

/*
 * Function     : print_buf
 * Description  : Prints given buffer for given length
 * Input        : Pointer to buffer and length
 * Output       : Void
 *
 */
void print_buf(u_int8_t* pbuf, int len)
{
    int i = 0;
    for (i = 0; i < len; i++) {
        printk("%02X ", pbuf[i]);
        if (i % 32 == 31) {
            printk("\n");
        }
    }
}

/*
 * Function     : spectral_dump_fft
 * Description  : Dump Spectral FFT
 * Input        : Pointer to Spectral Phyerr FFT
 * Output       : Success/Failure
 *
 */
int spectral_dump_fft(u_int8_t* pfft, int fftlen)
{
    int i = 0;

    /* TODO : Do not delete the following print
     *        The scripts used to validate Spectral depend on this Print
     */
    printk("SPECTRAL : FFT Length is 0x%x (%d)\n", fftlen, fftlen);

    printk("fft_data # ");
    for (i = 0; i < fftlen; i++) {
        printk("%d ", pfft[i]);
#if 0
        if (i % 32 == 31)
            printk("\n");
#endif
    }
    printk("\n");
    return 0;
}

/*
 * Function     : spectral_send_tlv_to_host
 * Description  : Send the TLV information to Host
 * Input        : Pointer to the TLV
 * Output       : Success/Failure
 *
 */

#ifdef HOST_OFFLOAD
extern void
atd_spectral_msg_send(struct net_device *dev, struct spectral_samp_msg *msg, uint16_t msg_len);
#endif

int spectral_send_tlv_to_host(struct ath_spectral* spectral, u_int8_t* data, u_int32_t datalen)
{

    int status = AH_TRUE;
    spectral_prep_skb(spectral);
    if (spectral->spectral_skb != NULL) {
        spectral->spectral_nlh = (struct nlmsghdr*)spectral->spectral_skb->data;
        memcpy(NLMSG_DATA(spectral->spectral_nlh), data, datalen);
        spectral_bcast_msg(spectral);
    } else {
        status = AH_FALSE;
    }
#ifdef HOST_OFFLOAD
    atd_spectral_msg_send(spectral->ic->ic_osdev->netdev,
            data,
            datalen);
#endif
    return status;
}EXPORT_SYMBOL(spectral_send_tlv_to_host);

/*
 * Function     : dbg_print_SAMP_param
 * Description  : Print contents of SAMP struct
 * Input        : Pointer to SAMP message
 * Output       : Void
 *
 */
void dbg_print_SAMP_param(struct samp_msg_params* p)
{
    printk("\nSAMP Packet : -------------------- START --------------------\n");
    printk("Freq        = %d\n", p->freq);
    printk("RSSI        = %d\n", p->rssi);
    printk("Bin Count   = %d\n", p->pwr_count);
    printk("Timestamp   = %d\n", p->tstamp);
    printk("SAMP Packet : -------------------- END -----------------------\n");
}

/*
 * Function     : dbg_print_SAMP_msg
 * Description  : Print contents of SAMP Message
 * Input        : Pointer to SAMP message
 * Output       : Void
 *
 */
void dbg_print_SAMP_msg(struct spectral_samp_msg* ss_msg)
{
    int i = 0;

    struct spectral_samp_data *p = &ss_msg->samp_data;
    struct spectral_classifier_params *pc = &p->classifier_params;
    struct interf_src_rsp  *pi = &p->interf_list;

    line();
    printk("Spectral Message\n");
    line();
    printk("Signature   :   0x%x\n", ss_msg->signature);
    printk("Freq        :   %d\n", ss_msg->freq);
    printk("Freq load   :   %d\n", ss_msg->freq_loading);
    printk("Intfnc type :   %d\n", ss_msg->int_type);
    line();
    printk("Spectral Data info\n");
    line();
    printk("data length     :   %d\n", p->spectral_data_len);
    printk("rssi            :   %d\n", p->spectral_rssi);
    printk("combined rssi   :   %d\n", p->spectral_combined_rssi);
    printk("upper rssi      :   %d\n", p->spectral_upper_rssi);
    printk("lower rssi      :   %d\n", p->spectral_lower_rssi);
    printk("bw info         :   %d\n", p->spectral_bwinfo);
    printk("timestamp       :   %d\n", p->spectral_tstamp);
    printk("max index       :   %d\n", p->spectral_max_index);
    printk("max exp         :   %d\n", p->spectral_max_exp);
    printk("max mag         :   %d\n", p->spectral_max_mag);
    printk("last timstamp   :   %d\n", p->spectral_last_tstamp);
    printk("upper max idx   :   %d\n", p->spectral_upper_max_index);
    printk("lower max idx   :   %d\n", p->spectral_lower_max_index);
    printk("bin power count :   %d\n", p->bin_pwr_count);
    line();
    printk("Classifier info\n");
    line();
    printk("20/40 Mode      :   %d\n", pc->spectral_20_40_mode);
    printk("dc index        :   %d\n", pc->spectral_dc_index);
    printk("dc in MHz       :   %d\n", pc->spectral_dc_in_mhz);
    printk("upper channel   :   %d\n", pc->upper_chan_in_mhz);
    printk("lower channel   :   %d\n", pc->lower_chan_in_mhz);
    line();
    printk("Interference info\n");
    line();
    printk("inter count     :   %d\n", pi->count);

    for (i = 0; i < pi->count; i++) {
        printk("inter type  :   %d\n", pi->interf[i].interf_type);
        printk("min freq    :   %d\n", pi->interf[i].interf_min_freq);
        printk("max freq    :   %d\n", pi->interf[i].interf_max_freq);
    }


}

/*
 * Function     : get_offset_swar_sec80
 * Description  : Get offset for SWAR according to the channel width
 * Input        : Channel width
 * Output       : Offset for SWAR algorithm
 */
uint32_t get_offset_swar_sec80(uint32_t channel_width)
{
    uint32_t offset = 0;
    switch (channel_width)
    {
        case IEEE80211_CWM_WIDTH20:
            offset = OFFSET_CH_WIDTH_20;
            break;
        case IEEE80211_CWM_WIDTH40:
            offset = OFFSET_CH_WIDTH_40;
            break;
        case IEEE80211_CWM_WIDTH80:
            offset = OFFSET_CH_WIDTH_80;
            break;
        case IEEE80211_CWM_WIDTH160:
            offset = OFFSET_CH_WIDTH_160;
            break;
        default:
            offset = OFFSET_CH_WIDTH_80;
            break;
    }
    return offset;
}

#endif  /* WLAN_SPECTRAL_ENABLE */
