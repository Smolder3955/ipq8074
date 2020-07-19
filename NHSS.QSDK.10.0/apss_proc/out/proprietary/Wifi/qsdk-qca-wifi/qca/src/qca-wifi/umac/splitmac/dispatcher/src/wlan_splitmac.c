/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */
#if WLAN_SUPPORT_SPLITMAC
#include <ieee80211_var.h>
#include <wlan_splitmac.h>
#include <wlan_mlme_dispatcher.h>

int splitmac_is_enabled(struct wlan_objmgr_vdev *vdev)
{
    return splitmac_get_enabled_flag(vdev);
}

void splitmac_set_enabled_flag(struct wlan_objmgr_vdev *vdev, int enabled)
{
    struct splitmac_vdev_priv_obj *splitmac_vdev = NULL;

    if (vdev == NULL) {
        splitmac_err("%s: vdev is NULL!\n",__func__);
        return;
    }

    splitmac_vdev = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_SPLITMAC);
    if (splitmac_vdev == NULL) {
        splitmac_err("%s: splitmac_vdev is NULL!\n",__func__);
        return;
    }

    splitmac_vdev->splitmac_enabled = enabled;
}

int splitmac_get_enabled_flag(struct wlan_objmgr_vdev *vdev)
{
    struct splitmac_vdev_priv_obj *splitmac_vdev = NULL;

    if (vdev == NULL) {
        splitmac_err("%s: vdev is NULL!\n",__func__);
        return -EFAULT;
    }

    splitmac_vdev = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_SPLITMAC);
    if (splitmac_vdev == NULL) {
        splitmac_err("%s: splitmac_vdev is NULL!\n",__func__);
        return -EFAULT;
    }

    return splitmac_vdev->splitmac_enabled;
}

void splitmac_api_set_state(struct wlan_objmgr_peer *peer, int state)
{
    struct splitmac_peer_priv_obj *splitmac_peer = NULL;

    if (peer == NULL) {
        splitmac_err("%s: peer is NULL!\n",__func__);
        return;
    }

    splitmac_peer = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_SPLITMAC);
    splitmac_peer->splitmac_state = state;
}

int splitmac_api_get_state(struct wlan_objmgr_peer *peer)
{
    struct splitmac_peer_priv_obj *splitmac_peer = NULL;

    if (peer == NULL) {
        splitmac_err("%s: peer is NULL!\n",__func__);
        return 0;
    }

    splitmac_peer = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_SPLITMAC);

    return (splitmac_peer->splitmac_state);
}


int splitmac_api_add_client(struct wlan_objmgr_vdev *vdev, u_int8_t *stamac,
                u_int16_t associd, u_int8_t qos, void *lrates,
                void *htrates, u_int16_t vhtrates, u_int16_t herates)
{
    struct wlan_objmgr_psoc *psoc = NULL;
    struct wlan_objmgr_peer *peer = NULL;
    struct splitmac_peer_priv_obj *splitmac_peer = NULL;
    int status = 0;
    uint8_t pdev_id;

    if (splitmac_is_enabled(vdev) != 1) {
        splitmac_err("%s: splitmac not enabled!\n",__func__);
        return -EFAULT;
    }

    psoc = wlan_vdev_get_psoc(vdev);
    if (psoc == NULL) {
        splitmac_err("%s: psoc is NULL!\n",__func__);
        return -EFAULT;
    }
    pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
    peer = wlan_objmgr_get_peer(psoc, pdev_id, stamac, WLAN_SPLITMAC_ID);
    if (peer == NULL) {
        splitmac_err("%s: peer %s not found!\n",__func__, ether_sprintf(stamac));
        return -EFAULT;
    }
    splitmac_peer = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_SPLITMAC);
    if (splitmac_peer == NULL) {
        splitmac_err("%s: splitmac_peer not found!\n",__func__);
        wlan_objmgr_peer_release_ref(peer, WLAN_SPLITMAC_ID);
        return -EFAULT;
    }
    if (splitmac_api_get_state(peer) != SPLITMAC_IN_ASSOC_REQ) {
        wlan_objmgr_peer_release_ref(peer, WLAN_SPLITMAC_ID);
        splitmac_err("%s: peer %s in wrong state 0x%x!\n",__func__, ether_sprintf(stamac),
                             splitmac_api_get_state(peer));
        return -EINVAL;
    }

    if ( wlan_peer_is_flag_set(peer, IEEE80211_NODE_DELAYED_CLEANUP) ||
            wlan_peer_is_flag_set(peer, IEEE80211_NODE_LEAVE_ONGOING) ) {
        wlan_objmgr_peer_release_ref(peer, WLAN_SPLITMAC_ID);
        splitmac_err("%s: peer %s under deletion!\n",__func__, ether_sprintf(stamac));
        return -EINVAL;
    }

    splitmac_api_set_state(peer, SPLITMAC_ADDCLIENT_START);
    status = wlan_add_client(vdev, peer, associd, qos, lrates, htrates, vhtrates, herates);
    if (status == 0) {
           splitmac_api_set_state(peer, SPLITMAC_ADDCLIENT_END);
    }
    wlan_objmgr_peer_release_ref(peer, WLAN_SPLITMAC_ID);

    return status;
}


int splitmac_api_del_client(struct wlan_objmgr_vdev *vdev, u_int8_t *stamac)
{
    struct wlan_objmgr_psoc *psoc = NULL;
    struct wlan_objmgr_peer *peer = NULL;
    uint8_t pdev_id;

    if (splitmac_is_enabled(vdev) != 1) {
        splitmac_err("%s: splitmac not enabled!\n",__func__);
        return -EFAULT;
    }

    psoc = wlan_vdev_get_psoc(vdev);
    if (psoc == NULL) {
        splitmac_err("%s: psoc is NULL!\n",__func__);
        return -EFAULT;
    }
    pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
    peer = wlan_objmgr_get_peer(psoc, pdev_id, stamac, WLAN_SPLITMAC_ID);
    if (peer == NULL) {
        splitmac_err("%s: peer %s not found!\n",__func__, ether_sprintf(stamac));
        return -EFAULT;
    }

    splitmac_info("Delete STA: %s\n", ether_sprintf(stamac));

    wlan_deliver_mlme_evt_disassoc(vdev, stamac, peer);

    wlan_objmgr_peer_release_ref(peer, WLAN_SPLITMAC_ID);

    return 0;
}

int splitmac_api_authorize_client(struct wlan_objmgr_vdev *vdev, u_int8_t *stamac, u_int32_t authorize)
{

    if (splitmac_is_enabled(vdev) != 1) {
        splitmac_err("%s: splitmac not enabled!\n",__func__);
        return -EFAULT;
    }

    splitmac_info("Authorize STA: %s, authorize=%d\n", ether_sprintf(stamac), authorize);

    authorize = !!authorize;
    wlan_peer_auth(vdev, stamac, authorize);
    return 0;
}

int splitmac_api_set_key(struct wlan_objmgr_vdev *vdev, u_int8_t *macaddr, u_int8_t cipher,
                      u_int16_t keyix, u_int32_t keylen, u_int8_t *keydata)
{
    if (splitmac_is_enabled(vdev) != 1) {
        splitmac_err("%s: splitmac not enabled!\n",__func__);
        return -EFAULT;
    }

    return wlan_vdev_set_key(vdev, macaddr, cipher,
                         keyix, keylen, keydata);
}

int splitmac_api_del_key(struct wlan_objmgr_vdev *vdev, u_int8_t *macaddr, u_int16_t keyix)
{
    if (splitmac_is_enabled(vdev) != 1) {
        splitmac_err("%s: splitmac not enabled!\n",__func__);
        return -EFAULT;
    }

    splitmac_info("del key for STA %s\n", ether_sprintf(macaddr));

    return wlan_vdev_del_key(vdev, keyix, macaddr);
}

static QDF_STATUS wlan_splitmac_vdev_obj_create_handler(struct wlan_objmgr_vdev *vdev,
                         void *arg)
{
    QDF_STATUS status;
    struct splitmac_vdev_priv_obj *splitmac_vdev_obj;

    splitmac_vdev_obj = qdf_mem_malloc(sizeof(struct splitmac_vdev_priv_obj));
    if (splitmac_vdev_obj == NULL) {
        return QDF_STATUS_E_NOMEM;
    }
    qdf_mem_zero(splitmac_vdev_obj, sizeof(struct splitmac_vdev_priv_obj));

    splitmac_vdev_obj->vdev = vdev;
    status = wlan_objmgr_vdev_component_obj_attach(vdev,
                               WLAN_UMAC_COMP_SPLITMAC,
                               (void *)splitmac_vdev_obj,
                               QDF_STATUS_SUCCESS);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_mem_free(splitmac_vdev_obj);
    }

    return status;
}

static QDF_STATUS wlan_splitmac_vdev_obj_destroy_handler(struct wlan_objmgr_vdev *vdev,
                          void *arg)
{
    QDF_STATUS status;
    void *splitmac_vdev_obj;

    splitmac_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(vdev,
                            WLAN_UMAC_COMP_SPLITMAC);
    if (splitmac_vdev_obj == NULL) {
        return QDF_STATUS_E_FAILURE;
    }

    status = wlan_objmgr_vdev_component_obj_detach(vdev,
                               WLAN_UMAC_COMP_SPLITMAC,
                               splitmac_vdev_obj);
    qdf_mem_free(splitmac_vdev_obj);

    return status;
}

QDF_STATUS
wlan_splitmac_peer_obj_create_handler(struct wlan_objmgr_peer *peer, void *arg)
{
    struct splitmac_peer_priv_obj *sp = NULL;

    if (NULL == peer) {
        return QDF_STATUS_E_FAILURE;
    }

    sp = (struct splitmac_peer_priv_obj *)qdf_mem_malloc(sizeof(struct splitmac_peer_priv_obj));
    if (NULL == sp) {
        return QDF_STATUS_E_FAILURE;
    }
    qdf_mem_zero(sp, sizeof(struct splitmac_peer_priv_obj));
    sp->peer = peer;
    wlan_objmgr_peer_component_obj_attach(peer, WLAN_UMAC_COMP_SPLITMAC,
                          (void *)sp,
                          QDF_STATUS_SUCCESS);

    return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_splitmac_peer_obj_destroy_handler(struct wlan_objmgr_peer *peer, void *arg)
{
    struct splitmac_peer_priv_obj *sp = NULL;

    if (NULL == peer) {
        return QDF_STATUS_E_FAILURE;
    }
    sp = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_SPLITMAC);
    if (sp != NULL) {
        wlan_objmgr_peer_component_obj_detach(peer, WLAN_UMAC_COMP_SPLITMAC,
                              (void *)sp);
        qdf_mem_free(sp);
    }

    return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_splitmac_init(void)
{
    if (wlan_objmgr_register_vdev_create_handler(WLAN_UMAC_COMP_SPLITMAC,
        wlan_splitmac_vdev_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
        splitmac_err("%s: wlan_objmgr_register_vdev_create_handler failed\n",__func__);
        return QDF_STATUS_E_FAILURE;
    }
    if (wlan_objmgr_register_vdev_destroy_handler(WLAN_UMAC_COMP_SPLITMAC,
        wlan_splitmac_vdev_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
        splitmac_err("%s: wlan_objmgr_register_vdev_destroy_handler failed\n",__func__);
        return QDF_STATUS_E_FAILURE;
    }
    if (wlan_objmgr_register_peer_create_handler(WLAN_UMAC_COMP_SPLITMAC,
        wlan_splitmac_peer_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
        splitmac_err("%s: wlan_objmgr_register_peer_create_handler failed\n",__func__);
        return QDF_STATUS_E_FAILURE;
    }
    if (wlan_objmgr_register_peer_destroy_handler(WLAN_UMAC_COMP_SPLITMAC,
        wlan_splitmac_peer_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
        splitmac_err("%s: wlan_objmgr_register_peer_destroy_handler failed\n",__func__);
        return QDF_STATUS_E_FAILURE;
    }
    return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_splitmac_deinit(void)
{
    if (wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_SPLITMAC,
        wlan_splitmac_vdev_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
        splitmac_err("%s: wlan_objmgr_unregister_vdev_create_handler failed\n",__func__);
        return QDF_STATUS_E_FAILURE;
    }
    if (wlan_objmgr_unregister_vdev_destroy_handler(WLAN_UMAC_COMP_SPLITMAC,
        wlan_splitmac_vdev_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
        splitmac_err("%s: wlan_objmgr_unregister_vdev_destroy_handler failed\n",__func__);
        return QDF_STATUS_E_FAILURE;
    }
    if (wlan_objmgr_unregister_peer_create_handler(WLAN_UMAC_COMP_SPLITMAC,
        wlan_splitmac_peer_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
        splitmac_err("%s: wlan_objmgr_unregister_peer_create_handler failed\n",__func__);
        return QDF_STATUS_E_FAILURE;
    }
    if (wlan_objmgr_unregister_peer_destroy_handler(WLAN_UMAC_COMP_SPLITMAC,
        wlan_splitmac_peer_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
        splitmac_err("%s: wlan_objmgr_unregister_peer_destroy_handler failed\n",__func__);
        return QDF_STATUS_E_FAILURE;
    }
    return QDF_STATUS_SUCCESS;
}

#endif /* WLAN_SUPPORT_SPLITMAC */
