/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */
#include "ol_if_athvar.h"
#include "ol_defines.h"
#include <init_deinit_lmac.h>
#include "wmi_unified_api.h"
#include <ol_txrx_types.h>
#include <target_type.h>

static int
wds_peer_event_handler_wifi3_0(ol_scn_t sc, u_int8_t *datap, u_int32_t len)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    wds_addr_event_t wds_ev, *wds_addr_event;
    uint8_t *dest_mac;
    struct common_wmi_handle *wmi_handle;

    if (lmac_get_tgt_type(soc->psoc_obj) != TARGET_TYPE_QCA8074)
            return 0;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (!wmi_handle) {
        qdf_err("Failed to get wmi handle");
        return -1;
    }

    if (wmi_extract_wds_addr_event(wmi_handle, datap, len, &wds_ev)) {
        qdf_err("Unable to extract wds addr event\n");
    } else {
        struct wlan_objmgr_vdev *vdev;
        struct ieee80211vap *vap;
        void *auth_cookie, *ast_entry;
        ol_txrx_soc_handle soc_txrx_handle;

        wds_addr_event = &wds_ev;
        dest_mac = wds_ev.dest_mac;
        soc_txrx_handle = wlan_psoc_get_dp_handle(soc->psoc_obj);
        ast_entry = cdp_peer_ast_hash_find_soc((struct cdp_soc_t *)soc_txrx_handle,
                                            dest_mac);

        vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc->psoc_obj,
                wds_ev.vdev_id, WLAN_MLME_SB_ID);
        if (vdev == NULL) {
            /*With HKv1 WAR, AST entries of type WDS/MEC are not deleted
             * untill response from WiFi FW is received.
             * During vap down per radio we may receive wds delete response from FW when vap is NULL.
             * Handle response to free the AST entries.
             */
            if (ast_entry && wds_addr_event->event_type[0] == 2) {
                cdp_peer_ast_free_entry((struct cdp_soc_t *)soc_txrx_handle, ast_entry);
            }
            return -1;
        }

        vap = wlan_vdev_get_mlme_ext_obj(vdev);
        if (vap == NULL) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
            return -1;
        }

        qdf_debug("WDS ast event type = %d for mac %pM",
                                    wds_addr_event->event_type[0], dest_mac);

        if (ast_entry) {
            auth_cookie = cdp_peer_ast_get_cp_ctx(soc_txrx_handle, ast_entry);
            if (wds_addr_event->event_type[0] == 2) {
                cdp_peer_ast_free_entry((struct cdp_soc_t *)soc_txrx_handle, ast_entry);
                wlan_wds_delete_response_handler(vap, auth_cookie);
            }
        } else {
            qdf_print("ast entry not found for %pM", dest_mac);
        }
        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
    }

    return 0;
}

void
wds_addr_init_wifi3_0(wmi_unified_t wmi_handle)
{
    wmi_unified_register_event_handler(wmi_handle, wmi_wds_peer_event_id,
                                       wds_peer_event_handler_wifi3_0, WMI_RX_UMAC_CTX);
}
qdf_export_symbol(wds_addr_init_wifi3_0);

void
wds_addr_detach_wifi3_0(wmi_unified_t wmi_handle)
{
    wmi_unified_unregister_event_handler(wmi_handle, wmi_wds_peer_event_id);
}
qdf_export_symbol(wds_addr_detach_wifi3_0);
