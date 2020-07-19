/*
 *  Copyright (c) 2010 Atheros Communications Inc.
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

#include <qdf_module.h>
#include <qdf_types.h>
#include <qdf_util.h>
#include <ieee80211_wow.h>
#include <ieee80211_var.h>
#include <ath_internal.h>
#include <if_athvar.h>
#include <ah_wow.h>


#define wow_dump_pkt(buf, len) { \
        int i=0; \
        for (i=0; i<len;i++) { \
            qdf_print("%x ", ((unsigned char*)buf)[i]); \
            if(i && i%16 ==0) qdf_print("\n"); } \
            qdf_print("\n"); }

void atd_tgt_htc_set_stopflag(uint8_t val);

static void
ieee80211_wow_set_gpio(wlan_if_t vap)
{
    osdev_t                     os_handle = vap->iv_ic->ic_osdev;
    struct net_device           *dev = os_handle->netdev;
    struct ath_softc_net80211   *scn = ath_netdev_priv(dev);
    struct ath_softc            *sc = ATH_DEV_TO_SC(scn->sc_dev);
    struct ath_hal              *ah = sc->sc_ah;

    ah_wow_set_gpio_hi(ah);
}

 /*
  * processes data frames.
  * ieee80211_wow_magic_parser parses the wbuf to check if match magic packet.
  */
void 
ieee80211_wow_magic_parser(struct ieee80211_node *ni, wbuf_t wbuf)
{
    struct ieee80211vap *vap = ni->ni_vap;
    uint8_t    i_addr[IEEE80211_ADDR_LEN];
    int32_t    left_len = wbuf_get_pktlen(wbuf);
    uint8_t    *cur_ptr;
    uint32_t   dup_cnt, is_match = FALSE, is_bcast = FALSE;

    //wow_dump_pkt(wbuf->data, wbuf->len);

    if (left_len < IEEE80211_WOW_MAGIC_PKTLEN)
        return;
        
    cur_ptr = wbuf_header(wbuf);
    IEEE80211_ADDR_COPY(i_addr, vap->iv_myaddr);

    /* parse whole pkt */
    while (cur_ptr && (left_len>0) && (!is_match)) {
        
        /* left len is less than magic pkt size, give up */
        if (left_len < IEEE80211_WOW_MAGIC_PKTLEN)
            break;

        /* to skip continuous 0xFF and to match last 6 bytes of 0xFF */
        while (IEEE80211_IS_BROADCAST(cur_ptr) && (left_len>=IEEE80211_WOW_MAGIC_PKTLEN)) {
            is_bcast = TRUE;
            cur_ptr += IEEE80211_ADDR_LEN;
            left_len -= IEEE80211_ADDR_LEN;
        }

        /* 6 bytes of 0xFF matched, to check if 16 duplications of the IEEE address */
        if (is_bcast) {
            
            dup_cnt = 0;
            
            /* if 0xFF exists at head, skip it */
            while ((cur_ptr[0]==0xFF) && (left_len>=IEEE80211_WOW_MAGIC_DUPLEN)) {
                cur_ptr++;
                left_len--;
            }

            /* left len is less than duplication size, give up */
            if (left_len < IEEE80211_WOW_MAGIC_DUPLEN)
                break;
            
            /* to check if 16 duplications of the IEEE address */
            while (cur_ptr && (left_len>=IEEE80211_ADDR_LEN) && IEEE80211_ADDR_EQ(cur_ptr, i_addr)) {
                cur_ptr += IEEE80211_ADDR_LEN;
                left_len -= IEEE80211_ADDR_LEN;
                dup_cnt++;
                
                if (dup_cnt >= IEEE80211_WOW_MAGIC_DUPCNT) {
                    is_match = TRUE;
                    break;
                }
            }

            /* not match magic pkt, keep parsing */
            is_bcast = FALSE;

        } else {
            cur_ptr++;
            left_len--;
        }        
    }
    
    if (is_match) {
        qdf_print("Magic packet received...");
        ieee80211_wow_set_gpio(vap);
    }
}

int
wlan_get_wow(wlan_if_t vap)
{
    return vap->iv_sw_wow;
}

int
wlan_set_wow(wlan_if_t vap, uint32_t value)
{
    if (value == 0) {   /* wow disable */
        vap->iv_sw_wow = FALSE;
        atd_tgt_htc_set_stopflag(FALSE);
        qdf_print("Exiting WOW...");
    }
    else if (value == 1) {  /* wow enable */
        vap->iv_sw_wow = TRUE;
        atd_tgt_htc_set_stopflag(TRUE);
        qdf_print("Entering WOW...");
    }
    else
        return -EINVAL;
    
    return EOK;
}


/*
 * Module glue.
 */
static	char *dev_info = "sw_wow";

static int __init
init_ieee80211_wow(void)
{
	qdf_print("%s: Copyright (c) 2010 Atheros Communications, Inc, "
        "All Rights Reserved", dev_info);
	return EOK;
}

static void __exit
exit_ieee80211_wow(void)
{
	qdf_print("%s: driver unloaded", dev_info);
}

qdf_virt_module_init(init_ieee80211_wow);
qdf_virt_module_exit(exit_ieee80211_wow);
qdf_virt_module_name(sw_wow);
qdf_export_symbol(ieee80211_wow_magic_parser);
qdf_export_symbol(wlan_get_wow);
qdf_export_symbol(wlan_set_wow);

