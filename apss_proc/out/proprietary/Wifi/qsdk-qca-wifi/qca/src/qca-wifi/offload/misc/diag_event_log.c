/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

/*
 * Copyright (c) 2013-2017 The Linux Foundation. All rights reserved.
 */

/* Diag Event log implementation */

#include <a_debug.h>
#include <wmi_unified_api.h>
#include <wmi.h>
#include "ol_defines.h"
#include <wlan_nlink_srv.h>
#include <wlan_nlink_common.h>
#include "dbglog_id.h"
#include "dbglog.h"
#include <fwlog/dbglog_host.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/vmalloc.h>
#include "ol_if_athvar.h"
#include "fw_dbglog_priv.h"
#include "init_deinit_lmac.h"

#define ATH6KL_FWLOG_PAYLOAD_SIZE  1500

/**
 * nl_srv_bcast_fw_logs() - Wrapper func to send bcast msgs to FW logs mcast grp
 * @skb: sk buffer pointer
 *
 * Sends the bcast message to FW logs multicast group with generic nl socket
 * if CNSS_GENL is enabled. Else, use the legacy netlink socket to send.
 *
 * Return: zero on success, error code otherwise
 */
static int nl_srv_bcast_fw_logs(struct sk_buff *skb)
{
#ifdef CNSS_GENL
    return nl_srv_bcast(skb, CLD80211_MCGRP_FW_LOGS, WLAN_NL_MSG_CNSS_DIAG);
#else
    return nl_srv_bcast(skb);
#endif
}

static int
send_diag_netlink_data(const uint8_t *buffer, A_UINT32 len, A_UINT32 cmd,
                        int wmi_diag_version)
{
    struct sk_buff *skb_out;
    struct nlmsghdr *nlh;
    int res = 0;
    struct dbglog_slot *slot;
    size_t slot_len;
    tAniNlHdr *wnl;
    int radio = 0;

    if (WARN_ON(len > ATH6KL_FWLOG_PAYLOAD_SIZE))
        return -ENODEV;

    if (nl_srv_is_initialized() != 0)
        return -EIO;


    slot_len = sizeof(*slot) + ATH6KL_FWLOG_PAYLOAD_SIZE +
        sizeof(radio);

    skb_out = nlmsg_new(slot_len, GFP_KERNEL);
    if (!skb_out) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                ("Failed to allocate new skb\n"));
        return -ENOMEM;
    }

    nlh = nlmsg_put(skb_out, 0, 0, WLAN_NL_MSG_CNSS_DIAG,
            slot_len, 0);
    if (!nlh) {
        kfree_skb(skb_out);
        return -EMSGSIZE;
    }
    wnl = (tAniNlHdr *)nlh;
    /*
     * TODO: Need to populate the correct radio index
     */
    wnl->radio = radio;
    /* data buffer offset from: nlmsg_hdr + sizeof(int) radio */
    slot = (struct dbglog_slot *) (nlmsg_data(nlh) + sizeof(radio));
    slot->diag_type = cmd;
    slot->timestamp = cpu_to_le32(jiffies);
    slot->length = cpu_to_le32(len);
    /* Version mapped to wmi_diag_version here */
    slot->dropped = wmi_diag_version;
    memcpy(slot->payload, buffer, len);

    res = nl_srv_bcast_fw_logs(skb_out);
    if ((res < 0) && (res != -ESRCH)) {
        AR_DEBUG_PRINTF(ATH_DEBUG_RSVD1,
                ("%s: nl_srv_bcast_fw_logs failed 0x%x\n",
                 __func__, res));
        return res;
    }
    return res;
}

int diag_fw_handler(ol_scn_t sc, uint8_t *data, uint32_t datalen)
{
    wmitlv_cmd_param_info *param_buf;
    uint8_t *datap;
    uint32_t len = 0;
    uint32_t *buffer;
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;

    param_buf = (wmitlv_cmd_param_info *) data;
    if (!param_buf) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                ("Get NULL point message from FW\n"));
        return -EINVAL;
    }

    datap = param_buf->tlv_ptr;
    len = param_buf->num_elements;
    if (!soc->wmi_diag_version) {
        buffer = (uint32_t *) datap;
        buffer++;       /* skip offset */
        if (WLAN_DIAG_TYPE_CONFIG == DIAG_GET_TYPE(*buffer)) {
            buffer++;       /* skip  */
            if (DIAG_VERSION_INFO == DIAG_GET_ID(*buffer)) {
                buffer++;       /* skip  */
                /* get payload */
                soc->wmi_diag_version = *buffer;
            }
        }
    }

    return send_diag_netlink_data((A_UINT8 *) datap,
            len, DIAG_TYPE_FW_MSG, soc->wmi_diag_version);
    /* Always returns zero */
}
qdf_export_symbol(diag_fw_handler);

static QDF_STATUS
tlv_config_debug_module_cmd(
        wmi_unified_t wmi_handle, A_UINT32 param,
        A_UINT32 val, A_UINT32 *module_id_bitmap,
        A_UINT32 bitmap_len)
{
    struct dbglog_params dbg_param;

    dbg_param.param = param;
    dbg_param.val = val;
    dbg_param.module_id_bitmap = module_id_bitmap;
    dbg_param.bitmap_len = bitmap_len;

    return wmi_unified_dbglog_cmd_send(wmi_handle, &dbg_param);
}

void diag_set_log_lvl_tlv(ol_scn_t scn_handle, uint32_t log_lvl)
{
    wmi_unified_t wmi_handle;
    struct common_wmi_handle *pdev_wmi_handle;
    A_UINT32 val = 0;
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *) scn_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    wmi_handle = lmac_get_wmi_unified_hdl(scn->soc->psoc_obj);

    if (log_lvl > DBGLOG_LVL_MAX) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                ("dbglog_set_log_lvl:Invalid log level %d\n",
                 log_lvl));
        return;
    }

    WMI_DBGLOG_SET_MODULE_ID(val, WMI_DEBUG_LOG_MODULE_ALL);
    WMI_DBGLOG_SET_LOG_LEVEL(val, log_lvl);
    tlv_config_debug_module_cmd(wmi_handle,
            WMI_DEBUG_LOG_PARAM_LOG_LEVEL,
            val, NULL, 0);
}
qdf_export_symbol(diag_set_log_lvl_tlv);

void diag_init(void *soc)
{
}

struct dbglog_ops diag_ops = {
	.dbglog_set_log_lvl = diag_set_log_lvl_tlv,
	.dbglog_fw_handler = diag_fw_handler,
	.dbglog_init = diag_init,
};

void *diag_event_log_attach(void)
{
	struct dbglog_info *dbglog_handle;

	dbglog_handle =
		(struct dbglog_info *) qdf_mem_malloc(
			sizeof(struct dbglog_info));
	if (dbglog_handle == NULL) {
		qdf_print("allocation of dbglog handle failed %zu",
			sizeof(struct dbglog_info));
		return NULL;
	}

	dbglog_handle->ops = &diag_ops;

	return dbglog_handle;

}

void diag_event_log_detach(void *dbglog_handle)
{
	struct dbglog_info *dbglog = (struct dbglog_info *)dbglog_handle;

	if (dbglog && dbglog->ops)
		dbglog->ops = NULL;

	if(dbglog)
		qdf_mem_free(dbglog);
}
