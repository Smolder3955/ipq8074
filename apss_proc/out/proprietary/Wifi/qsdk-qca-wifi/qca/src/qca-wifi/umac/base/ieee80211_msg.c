/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2008 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#include <osdep.h>
#include <ieee80211_var.h>

void ieee80211com_note(struct ieee80211com *ic, int msgtype, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE], *tmp;
     va_list                ap;

     tmp = tmp_buf + snprintf(tmp_buf, OS_TEMP_BUF_SIZE, "[%s] ", msg_type_to_str(msgtype));
     va_start(ap, fmt);
     vsnprintf (tmp, (OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s\n", tmp_buf);

     /* Ignore trace messages sent before IC is fully initialized. */
     if (ic->ic_log_text != NULL) {
         ic->ic_log_text(ic, tmp_buf);
     }
}

/*
 * assume vsnprintf and snprintf are supported by the platform.
 */

void ieee80211_note(struct ieee80211vap *vap, int msgtype, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE], *tmp;
     va_list                ap;
     if(vap){
         struct ieee80211com    *ic = vap->iv_ic;
         tmp = tmp_buf + snprintf(tmp_buf, OS_TEMP_BUF_SIZE, "[%s] vap-%d(%s):",
                         msg_type_to_str(msgtype), vap->iv_unit, vap->iv_netdev_name);
         va_start(ap, fmt);
         vsnprintf (tmp, (OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
         va_end(ap);
         printk("%s\n", tmp_buf);
         ic->ic_log_text(ic, tmp_buf);
         OS_LOG_DBGPRINT("%s\n", tmp_buf);
     }
}

void ieee80211_note_mac(struct ieee80211vap *vap, int msgtype, u_int8_t *mac,const  char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE], *tmp;
     va_list                ap;
     struct ieee80211com    *ic = vap->iv_ic;
     tmp = tmp_buf + snprintf(tmp_buf, OS_TEMP_BUF_SIZE, "[%s] vap-%d(%s): [%s]",
                     msg_type_to_str(msgtype), vap->iv_unit, vap->iv_netdev_name,
                     ether_sprintf(mac));
     va_start(ap, fmt);
     vsnprintf (tmp, (OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s\n", tmp_buf);
     ic->ic_log_text(ic, tmp_buf);
     OS_LOG_DBGPRINT("%s\n", tmp_buf);
}

void ieee80211_note_frame(struct ieee80211vap *vap, int msgtype,
                                    struct ieee80211_frame *wh,const  char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE], *tmp;
     va_list                ap;
     struct ieee80211com    *ic = vap->iv_ic;
     tmp = tmp_buf + snprintf(tmp_buf, OS_TEMP_BUF_SIZE, "[%s] vap-%d(%s): [%s]",
                     msg_type_to_str(msgtype), vap->iv_unit, vap->iv_netdev_name,
                     ether_sprintf(ieee80211vap_getbssid(vap, wh)));
     va_start(ap, fmt);
     vsnprintf (tmp, (OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s\n", tmp_buf);
     ic->ic_log_text(ic, tmp_buf);
     OS_LOG_DBGPRINT("%s\n", tmp_buf);
}


void ieee80211_discard_frame(struct ieee80211vap *vap, int msgtype,
                                    const struct ieee80211_frame *wh, const char *type, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE], *tmp;
     va_list                ap;
     struct ieee80211com    *ic = vap->iv_ic;
     tmp = tmp_buf + snprintf(tmp_buf, OS_TEMP_BUF_SIZE, "[%s] [vap-%d(%s): %s]"
                    " discard %s frame ", msg_type_to_str(msgtype),
                    vap->iv_unit, vap->iv_netdev_name,
                    ether_sprintf(ieee80211vap_getbssid(vap, wh)), type);
     va_start(ap, fmt);
     vsnprintf (tmp, (OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s\n", tmp_buf);
     ic->ic_log_text(ic, tmp_buf);
}

void ieee80211_discard_ie(struct ieee80211vap *vap, int msgtype, const char *type, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE], *tmp;
     va_list                ap;
     struct ieee80211com    *ic = vap->iv_ic;
     tmp = tmp_buf + snprintf(tmp_buf, OS_TEMP_BUF_SIZE, "[%s] vap-%d(%s): "
                     "discard %s information element ", msg_type_to_str(msgtype),
                     vap->iv_unit, vap->iv_netdev_name, type);
     va_start(ap, fmt);
     vsnprintf (tmp, (OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s\n", tmp_buf);
     ic->ic_log_text(ic, tmp_buf);
}


void ieee80211_discard_mac(struct ieee80211vap *vap, int msgtype, u_int8_t *mac, const char *type, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE], *tmp;
     va_list                ap;
     struct ieee80211com    *ic = vap->iv_ic;
     tmp = tmp_buf + snprintf(tmp_buf, OS_TEMP_BUF_SIZE, "[%s] [vap-%d(%s): %s] "
                     "discard %s frame ", msg_type_to_str(msgtype), vap->iv_unit, vap->iv_netdev_name,
                     ether_sprintf(mac), type);
     va_start(ap, fmt);
     vsnprintf (tmp, (OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s\n", tmp_buf);
     ic->ic_log_text(ic, tmp_buf);
     OS_LOG_DBGPRINT("%s\n", tmp_buf);
}


void wlan_vap_note(struct ieee80211vap *vap, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE], *tmp;
     va_list                ap;
     struct ieee80211com    *ic = vap->iv_ic;
     tmp = tmp_buf + snprintf(tmp_buf, OS_TEMP_BUF_SIZE, "vap-%d(%s): ", vap->iv_unit, vap->iv_netdev_name);
     va_start(ap, fmt);
     vsnprintf (tmp, (OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
     va_end(ap);
     printk("%s\n", tmp_buf);
     ic->ic_log_text(ic, tmp_buf);
}

