/*
 * Copyright (c) 2011, Atheros Communications
 *
 * This file contains the routine for P2P Protocol Module for Group Owner.
 * It is used in the new higher API for win8 Wifi-Direct.
 *
 */
/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "ieee80211_var.h"
#include "ieee80211P2P_api.h"
#include "ieee80211_p2p_proto.h"
#include "ieee80211_p2p_ie.h"
#include "ieee80211_p2p_go.h"
#include "ieee80211_p2p_go_power.h"
#include "ieee80211_p2p_go_priv.h"
#include "ieee80211_p2p_prot_go.h"
#include "ieee80211_p2p_prot_api.h"

/* Macros to help to print the mac address */
#define MAC_ADDR_FMT        "%x:%x:%x:%x:%x:%x"
#define SPLIT_MAC_ADDR(addr)                    \
    ((addr)[0])&0x0FF,((addr)[1])&0x0FF,        \
    ((addr)[2])&0x0FF,((addr)[3])&0x0FF,        \
    ((addr)[4])&0x0FF,((addr)[5])&0x0FF         \

struct prot_go_member {
	struct prot_go_member   *next;
	u_int8_t                addr[IEEE80211_ADDR_LEN];
    size_t                  client_info_len;
    u_int8_t                *client_info;
};

struct prot_go_group {
    struct prot_go_member  *members;
};

/* Instance Information for P2P Protocol Module for GO */
struct ieee80211_p2p_prot_go {

    spinlock_t             lock;
    ieee80211_vap_t        vap;
    osdev_t                os_handle;
    wlan_p2p_go_t          p2p_go_handle;

    struct prot_go_group   group;

    /* For now, do not send the listen timing on GO port. The difficulty is
     * that this information is at the p2p device VAP. */
    u_int16_t              ext_listen_period;
    u_int16_t              ext_listen_interval;

    char                   p2p_dev_name[IEEE80211_MAX_P2P_DEV_NAME_LEN+1]; /* add 1 for NULL termination */
    u_int16_t              p2p_dev_name_len;
    u_int8_t               dev_cap;                    /* device capability bitmap */
    u_int8_t               grp_cap;                    /* group capability bitmap */
    u_int8_t               wpsVersionEnabled;          /* WPS versions that are currently enabled */
    u_int8_t               maxGroupLimit;              /* Maximum no. of P2P clients that GO should allow */
    u_int16_t              configMethods;              /* WSC Methods supported - P2P Device Info Attribute */
    u_int8_t               primaryDeviceType[IEEE80211_P2P_DEVICE_TYPE_LEN];
    u_int8_t               numSecDeviceTypes;
    u_int8_t               *secondaryDeviceTypes;
    u_int8_t               p2pDeviceAddr[IEEE80211_ADDR_LEN];     
};

static int
wlan_p2p_prot_GO_update_probe_resp_ie(ieee80211_p2p_prot_go_t h_prot_go);

void static
prot_go_assoc_ind(os_handle_t ctx, u_int8_t *macaddr, u_int16_t result, wbuf_t wbuf, wbuf_t resp_wbuf);

void static
prot_go_reassoc_ind(os_handle_t ctx, u_int8_t *macaddr, u_int16_t result, wbuf_t wbuf, wbuf_t resp_wbuf);

static void
wlan_p2p_prot_go_update_beacon_ie(ieee80211_p2p_prot_go_t h_prot_go);

/* MLME Events handlers */
static wlan_mlme_event_handler_table prot_go_mlme_evt_handler_table = {
    /* MLME confirmation handler */
    NULL,                       /* mlme_join_complete_set_country */
    NULL,                       /* mlme_join_complete_infra */
    NULL,                       /* mlme_join_complete_adhoc */
    NULL,                       /* mlme_auth_complete */
    NULL,                       /* mlme_assoc_req */
    NULL,                       /* mlme_assoc_complete */
    NULL,                       /* mlme_reassoc_complete */
    NULL,                       /* mlme_deauth_complete */
    NULL,                       /* mlme_disassoc_complete */
    NULL,                       /* mlme_txchanswitch_complete */
    NULL,                       /* mlme_repeater_cac_complete */

    /* MLME indication handler */
    NULL,                       /* mlme_auth_indication */
    NULL,                       /* mlme_deauth_indication */
    prot_go_assoc_ind,          /* mlme_assoc_indication */
    prot_go_reassoc_ind,        /* mlme_reassoc_indication */
    NULL,                       /* mlme_disassoc_indication */
    NULL,                       /* mlme_ibss_merge_start_indication */
    NULL,                       /* mlme_ibss_merge_completion_indication */
    NULL,                       /* wlan_radar_detected */
};

/*
 * Routine to find a member of this group.
 * Return NULL if such member is not found.
 */
static struct prot_go_member *
prot_go_find_member(ieee80211_p2p_prot_go_t h_prot_go, u_int8_t *mac_addr)
{
    struct prot_go_member *m;

    m = h_prot_go->group.members;
    while (m) {
        if (IEEE80211_ADDR_EQ(m->addr, mac_addr))
            break;
        m = m->next;
    }

    return m;
}

/*
 * Routine to create a new member of this group.
 * Return the member handle.
 */
static struct prot_go_member *
prot_go_create_member(ieee80211_p2p_prot_go_t h_prot_go, u_int8_t *mac_addr)
{
    struct prot_go_member   *m;

    m = (struct prot_go_member *) OS_MALLOC(h_prot_go->os_handle, sizeof(struct prot_go_member), 0);
    if (m == NULL) {
        IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: OS_MALLOC failed, size=%d\n",__func__, sizeof(struct prot_go_member));
        return NULL;
    }
    OS_MEMZERO(m, sizeof(struct prot_go_member));

    OS_MEMCPY(m->addr, mac_addr, IEEE80211_ADDR_LEN);

    return m;
}

static void
prot_go_free_member(struct prot_go_member *m)
{
    if (m->client_info) {
        OS_FREE(m->client_info);
    }
    OS_FREE(m);
}

/*
 * Routine to remove a new member of this group.
 * Return the error code.
 */
static int
prot_go_remove_member(ieee80211_p2p_prot_go_t h_prot_go, u_int8_t *mac_addr)
{
    struct prot_go_member *m, *prev;

    m = h_prot_go->group.members;
    prev = NULL;
    while (m) {
        if (IEEE80211_ADDR_EQ(m->addr, mac_addr))
            break;
        prev = m;
        m = m->next;
    }

    if (m) {
        if (prev)
            prev->next = m->next;
        else
            h_prot_go->group.members = m->next;
        prot_go_free_member(m);
    }
    else {
        return -EINVAL;
    }

    return EOK;
}

static void
prot_go_mlme_event_callback(ieee80211_vap_t vap, ieee80211_mlme_event *event, void *arg)
{
    ieee80211_p2p_prot_go_t h_prot_go = (ieee80211_p2p_prot_go_t) arg;
    wlan_node_t             ni; /* sta causing event */
    u_int8_t                *mac_addr;
    struct prot_go_member   *member = NULL;

    if ((event->type != IEEE80211_MLME_EVENT_STA_JOIN) &&
        (event->type != IEEE80211_MLME_EVENT_STA_LEAVE))
    {
        //We are not interested in the other events.
        return;
    }

    ni = event->u.event_sta.ni;
    mac_addr = wlan_node_getmacaddr(ni);
    ASSERT(mac_addr != NULL);

    /* Ignore the event for my own BSS node */
    if (IEEE80211_ADDR_EQ(mac_addr, vap->iv_myaddr)) {
        /* Skip */
        return;
    }

    if (event->type == IEEE80211_MLME_EVENT_STA_JOIN) {
        member = prot_go_find_member(h_prot_go, mac_addr);
        if (member == NULL) {
            /* If not found, then create this new member */
            member = prot_go_create_member(h_prot_go, mac_addr);

            IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                              "%s: Create new member "MAC_ADDR_FMT". member=%pK\n",
                              __func__, SPLIT_MAC_ADDR(mac_addr), member);

            wlan_p2p_prot_GO_update_probe_resp_ie(h_prot_go);
        }
        else {
            IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: Warning: member exists! "MAC_ADDR_FMT". member=%pK\n",
                                 __func__, SPLIT_MAC_ADDR(mac_addr), member);
        }
    }
    else if (event->type == IEEE80211_MLME_EVENT_STA_LEAVE) {
        int retval;
        retval = prot_go_remove_member(h_prot_go, mac_addr);
        if (retval == EOK) {
            IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                                    "%s: Remove member "MAC_ADDR_FMT".\n",
                                    __func__, SPLIT_MAC_ADDR(mac_addr));

            wlan_p2p_prot_GO_update_probe_resp_ie(h_prot_go);
        }
        else {
            IEEE80211_DPRINTF_VB(vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                    "%s: Error=0x%x in Removing member "MAC_ADDR_FMT".\n",
                                    __func__, retval, SPLIT_MAC_ADDR(mac_addr));
        }
    }
    //Else we are not interested in the other events.
}

/*
 * Routine to attach the P2P Protocol module from the Group Owner module.
 * Returns EOK if success. Else some non-zero error code.
 */
int
wlan_p2p_prot_go_attach(wlan_p2p_go_t p2p_go_handle)
{
    ieee80211_p2p_prot_go_t     h_prot_go = NULL;
    osdev_t                     oshandle;
    int                         retval = EOK;
    bool                        reg_mlme_evt_handler = false;
    bool                        reg_vap_mlme_evt_handler = false;

    if (p2p_go_handle->prot_info) {
        /* Already attached. */
        IEEE80211_DPRINTF_VB(p2p_go_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, already attached. handle=%pK\n",
                          __func__, p2p_go_handle->prot_info);
        return -EINVAL;
    }

    oshandle = ieee80211com_get_oshandle(p2p_go_handle->devhandle);
    h_prot_go = (struct ieee80211_p2p_prot_go *)OS_MALLOC(oshandle, sizeof(struct ieee80211_p2p_prot_go), 0);
    if (h_prot_go == NULL) {
        IEEE80211_DPRINTF_VB(p2p_go_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: unable to alloc memory. size=%d\n",
                          __func__, sizeof(struct ieee80211_p2p_prot_go));
        retval = -ENOMEM;
        goto failed_prot_go_attach;
    }
    OS_MEMZERO(h_prot_go, sizeof(struct ieee80211_p2p_prot_go));

    h_prot_go->os_handle = oshandle;
    h_prot_go->vap = p2p_go_handle->vap;
    spin_lock_init(&h_prot_go->lock);

    /* Register the MLME event callbacks */
    retval = wlan_vap_register_mlme_event_handlers(h_prot_go->vap, 
                                                   (os_if_t)h_prot_go,
                                                   &prot_go_mlme_evt_handler_table);
    if (retval != 0) {
        IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: wlan_vap_register_mlme_event_handlers ret error=0x%x\n",
                          __func__, retval);
        retval = -EINVAL;
        goto failed_prot_go_attach;
    }
    reg_vap_mlme_evt_handler = true;

    /* register with mlme module */
    if (ieee80211_mlme_register_event_handler(h_prot_go->vap, prot_go_mlme_event_callback, h_prot_go) != EOK) {
            IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_MLME, 
                "%s: ERROR, ieee80211_mlme_register_evet_handler failed \n",
                   __func__);
            retval = -EINVAL;
            goto failed_prot_go_attach;
    }
    reg_mlme_evt_handler = true;

    p2p_go_handle->prot_info = h_prot_go;
    h_prot_go->p2p_go_handle = p2p_go_handle;

    return EOK;

failed_prot_go_attach:

    /* Some failure in attaching this module */

    if (reg_vap_mlme_evt_handler) {
        wlan_vap_unregister_mlme_event_handlers(p2p_go_handle->vap, 
                                                (os_if_t)h_prot_go, 
                                                &prot_go_mlme_evt_handler_table);
    }

    if (reg_mlme_evt_handler) {
        ieee80211_mlme_unregister_event_handler(p2p_go_handle->vap, 
                                                prot_go_mlme_event_callback,
                                                (os_if_t)h_prot_go);
    }

    if (h_prot_go) {
        OS_FREE(h_prot_go);
    }

    return retval;
}

/*
 * Routine to detach the P2P Protocol module from the Group Owner module.
 */
void
wlan_p2p_prot_go_detach(wlan_p2p_go_t p2p_go_handle)
{
    ieee80211_p2p_prot_go_t     h_prot_go;
    int                         retval;

    if (p2p_go_handle->prot_info == NULL) {
        /* Already detached. */
        IEEE80211_DPRINTF_VB(p2p_go_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, not attached. Nothing to do\n", __func__);
        return;
    }
    h_prot_go = p2p_go_handle->prot_info;

    if (h_prot_go->secondaryDeviceTypes) {
        OS_FREE(h_prot_go->secondaryDeviceTypes);
    }

    retval = wlan_vap_unregister_mlme_event_handlers(p2p_go_handle->vap, 
                                                     (os_if_t)h_prot_go, 
                                                     &prot_go_mlme_evt_handler_table);
    if (retval != 0) {
        IEEE80211_DPRINTF_VB(p2p_go_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: Error, wlan_vap_unregister_mlme_event_handlers ret error=0x%x\n",
                             __func__, retval);
    }

    retval = ieee80211_mlme_unregister_event_handler(p2p_go_handle->vap, 
                                                     prot_go_mlme_event_callback,
                                                     (os_if_t)h_prot_go);
    if (retval != 0) {
        IEEE80211_DPRINTF_VB(p2p_go_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: Error, wlan_mlme_unregister_event_handlers ret error=0x%x\n",
                             __func__, retval);
    }

    spin_lock_destroy(&h_prot_go->lock);

    p2p_go_handle->prot_info = NULL;

    OS_FREE(h_prot_go);
}

/**
 * To set a parameter in the P2P Protocol GO module.
 * @param p2p_go_handle         : handle to the p2p GO object.
 * @param param_type            : type of parameter to set.
 * @param param                 : new parameter.
 * @return Error Code. Equal 0 if success.
 */
int wlan_p2p_prot_go_set_param(wlan_p2p_go_t p2p_go_handle, 
                               wlan_p2p_go_param_type param_type, u_int32_t param)
{
    int                     ret = 0;
    ieee80211_p2p_prot_go_t prot_info;

    if (p2p_go_handle->prot_info == NULL) {
        IEEE80211_DPRINTF_VB(p2p_go_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, protocol module not attached.\n",
                          __func__);
        return EINVAL;
    }
    prot_info = p2p_go_handle->prot_info;

    switch(param_type) {
    case WLAN_P2PGO_DEVICE_CAPABILITY:
        prot_info->dev_cap = param;
        wlan_p2p_prot_GO_update_probe_resp_ie(prot_info);
        wlan_p2p_prot_go_update_beacon_ie(prot_info);
        break;

    case WLAN_P2PGO_WPS_VERSION_ENABLED:
        prot_info->wpsVersionEnabled = (u_int8_t)param;
        break;

    case WLAN_P2PGO_GROUP_CAPABILITY:
        prot_info->grp_cap = param;

        /* Add the Group Owner bit. Always set to 1 for GO vap */
        prot_info->grp_cap |= WLAN_P2P_PROT_GROUP_CAP_GROUP_OWNER;

        wlan_p2p_prot_GO_update_probe_resp_ie(prot_info);
        wlan_p2p_prot_go_update_beacon_ie(prot_info);
        break;

    case WLAN_P2PGO_MAX_GROUP_LIMIT:
        /* TODO: use this group size limit in the GO configuration */
        prot_info->maxGroupLimit = (u_int8_t)param;
        break;

    case WLAN_P2PGO_DEVICE_CONFIG_METHODS:
        prot_info->configMethods = (u_int16_t)param;
        wlan_p2p_prot_GO_update_probe_resp_ie(prot_info);
        break;
    default:
        ret = EINVAL;
    }

    return ret;
}

/**
 * To set a parameter array in the P2P GO module.
 *  @param p2p_go_handle : handle to the p2p GO object.
 *  @param param         : config paramaeter.
 *  @param byte_arr      : byte array .
 *  @param len           : length of byte array.
 *  @return 0  on success and -ve on failure.
 */
int
wlan_p2p_prot_go_set_param_array(wlan_p2p_go_t p2p_go_handle, wlan_p2p_go_param_type param,
                                 u_int8_t *byte_arr, u_int32_t len)
{
    ieee80211_p2p_prot_go_t prot_info;

    if (p2p_go_handle->prot_info == NULL) {
        IEEE80211_DPRINTF_VB(p2p_go_handle->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, protocol module not attached.\n",
                          __func__);
        return EINVAL;
    }
    prot_info = p2p_go_handle->prot_info;

    switch (param) {
    case WLAN_P2PGO_DEVICE_NAME:
        if (byte_arr) {
            OS_MEMCPY(prot_info->p2p_dev_name, (const char *)byte_arr, len);
            /* ensure that p2p_dev_name is null-terminated */
            prot_info->p2p_dev_name[len] = '\0';
            prot_info->p2p_dev_name_len = len;
        }
        else {
            prot_info->p2p_dev_name_len = 0;
        }
        wlan_p2p_prot_GO_update_probe_resp_ie(prot_info);
        break;

    case WLAN_P2PGO_PRIMARY_DEVICE_TYPE:
        OS_MEMCPY(&prot_info->primaryDeviceType, byte_arr, len);
        wlan_p2p_prot_GO_update_probe_resp_ie(prot_info);
        break;

    case WLAN_P2PGO_SECONDARY_DEVICE_TYPES:
        if (prot_info->secondaryDeviceTypes) {
            OS_FREE(prot_info->secondaryDeviceTypes);
        }
        prot_info->secondaryDeviceTypes = byte_arr;
        prot_info->numSecDeviceTypes = len/IEEE80211_P2P_DEVICE_TYPE_LEN;
        wlan_p2p_prot_GO_update_probe_resp_ie(prot_info);
        break;

    case WLAN_P2PGO_P2P_DEVICE_ADDR:
        ASSERT(len == sizeof(prot_info->p2pDeviceAddr));
        OS_MEMCPY(prot_info->p2pDeviceAddr, byte_arr, len);

        wlan_p2p_prot_go_update_beacon_ie(prot_info);
        wlan_p2p_prot_GO_update_probe_resp_ie(prot_info);
        break;

    default:
        break;
    }

    return 0;
}

/* Fill in the P2P Client Info Descriptor */
static void
p2pbuf_client_info(p2pbuf *buf, struct prot_go_member *m)
{
    if (m->client_info == NULL)
        return;

    if (p2pbuf_tailroom(buf) < m->client_info_len) {
        return;
    }

    p2pbuf_put_data(buf, m->client_info, m->client_info_len);
}

/* Create the P2P Group Info attribute */
static void p2pbuf_add_group_info(p2pbuf *buf, struct prot_go_group *group)
{
    struct prot_go_member   *m;
	u8                      *group_info;

    /* P2P Group Info */
    group_info = p2pbuf_put(buf, 0);        /* pointer to start of buffer. Used for length calc. */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_GROUP_INFO);
    p2pbuf_put_le16(buf, 0);                 /* Length to be filled later */
    for (m = group->members; m; m = m->next) {
        /* Fill in each member's client info descriptor */
        p2pbuf_client_info(buf, m);
    }

    /* Update attribute length */
    P2PIE_PUT_LE16(group_info + 1, (u8 *) p2pbuf_get_bufptr(buf) - group_info - 3);
}

/*
 * This routine is called update the P2P IE's for Probe Response frame for the GO.
 * The new IE is stored in p2p_go_handle->p2p_ie.
 */
static int
wlan_p2p_prot_GO_update_probe_resp_ie(ieee80211_p2p_prot_go_t h_prot_go)
{
    wlan_p2p_go_t           p2p_go_handle;
    p2pbuf                  p2p_buf, *ies;
    u_int8_t                *ielen_ptr;
    int                     ie_len = 0;
    u8                      *p2p_ie_ptr;
    u_int16_t               p2p_ie_len = 0;

    if (h_prot_go == NULL) {
        /* Not attached. */
        IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, Protocol GO module not attached. Cannot proceed.\n", __func__);
        return -EIO;
    }
    p2p_go_handle = h_prot_go->p2p_go_handle;

    /* Allocate buffer for P2P IE */
    p2p_ie_ptr = (u_int8_t *)OS_MALLOC(h_prot_go->os_handle, IEEE80211_MAX_P2P_IE , 0);
    if (!p2p_ie_ptr) {
        IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: Error, mem alloc failed. len=%d.\n",
                             __func__, IEEE80211_MAX_P2P_IE);
        return -ENOMEM;
    }

    ies = &p2p_buf;

    p2pbuf_init(ies, p2p_ie_ptr, IEEE80211_MAX_P2P_IE);
    ielen_ptr = p2pbuf_add_ie_hdr(ies);     /* Add the P2P IE */

    spin_lock(&(h_prot_go->lock));  /* mutual exclusion with OS updating the parameters */

    /* Add P2P Capability */
    p2pbuf_add_capability(ies, h_prot_go->dev_cap, h_prot_go->grp_cap);

    /* Add Extended Listen Timing */
    if (h_prot_go->ext_listen_interval) {
        p2pbuf_add_ext_listen_timing(ies, h_prot_go->ext_listen_period, h_prot_go->ext_listen_interval);
    }

    /* Note: the NOA IE is added separately in p2p_go_add_p2p_ie() */

    /* Add P2P Device Info */
    p2pbuf_add_device_info(ies, h_prot_go->p2pDeviceAddr, 
                           (u_int8_t *)h_prot_go->p2p_dev_name, h_prot_go->p2p_dev_name_len, 
                           h_prot_go->configMethods, h_prot_go->primaryDeviceType,
                           h_prot_go->numSecDeviceTypes, h_prot_go->secondaryDeviceTypes);

    /* P2P Group Info */
    p2pbuf_add_group_info(ies, &h_prot_go->group);

    spin_unlock(&(h_prot_go->lock));  /* mutual exclusion with OS updating the parameters */

    p2pbuf_update_ie_hdr(ies, ielen_ptr, &ie_len);  /* last thing to do is update the IE length */

    p2p_ie_len = ie_len + 2;            /* plus 2 bytes for IE header */

    ieee80211_p2p_go_store_proberesp_p2p_ie(p2p_go_handle, p2p_ie_ptr, p2p_ie_len);

    return EOK;
}

/*
 * This routine is called update the P2P IE's for Beacon frame for the GO.
 * The wlan_p2p_go_update_p2p_ie() is called to update the created P2P IE's sub-attributes.
 */
static void
wlan_p2p_prot_go_update_beacon_ie(ieee80211_p2p_prot_go_t h_prot_go)
{
    p2pbuf                  p2p_buf, *ies;
    u_int8_t                *frm;
    int                     sub_ie_len;

    if (h_prot_go == NULL) {
        /* Not attached. */
        IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error, Protocol GO module not attached. Cannot proceed.\n", __func__);
        return;
    }

    ies = &p2p_buf;

    /* Allocate a buffer big enough for the P2P Capability (5 bytes) and P2P Device ID (9 bytes) */
#define BUF_SIZE_BEACON_P2P_SUBATTR     (5 + 9)
    frm = (u_int8_t *)OS_MALLOC(h_prot_go->os_handle, BUF_SIZE_BEACON_P2P_SUBATTR, 0);
    if (frm == NULL) {
        IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: unable to alloc memory. size=%d\n",
                          __func__, BUF_SIZE_BEACON_P2P_SUBATTR);
        return;
    }

    p2pbuf_init(ies, frm, BUF_SIZE_BEACON_P2P_SUBATTR);
    /* NOTE: this buffer is only for sub-attributes and does not include the P2P IE header */

    spin_lock(&(h_prot_go->lock));  /* mutual exclusion with OS updating the parameters */

    /* Add P2P Capability */
    p2pbuf_add_capability(ies, h_prot_go->dev_cap, h_prot_go->grp_cap);

    if ((h_prot_go->p2pDeviceAddr[0] == 0) && (h_prot_go->p2pDeviceAddr[1] == 0) &&
        (h_prot_go->p2pDeviceAddr[2] == 0) && (h_prot_go->p2pDeviceAddr[3] == 0) &&
        (h_prot_go->p2pDeviceAddr[4] == 0) && (h_prot_go->p2pDeviceAddr[5] == 0))
    {
        IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: P2P Device Address is all zero and not initialized.\n",
                          __func__);
    }
    else {
        p2pbuf_add_device_id(ies, h_prot_go->p2pDeviceAddr);
    }

    spin_unlock(&(h_prot_go->lock));  /* mutual exclusion with OS updating the parameters */

    sub_ie_len = p2pbuf_get_bufptr(ies) - frm;
    wlan_p2p_go_update_p2p_ie(h_prot_go->p2p_go_handle, frm, sub_ie_len);

    IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                      "%s: GO Protocol module: Beacon P2P sub-attributes update.\n", __func__);

    return;

#undef BUF_SIZE_BEACON_P2P_SUBATTR
}

/*
 * Function is a callback function for MLME Event callback for Reassociation or association.
 * This function will collect the ASSOC_REQ or REASSOC_REQ's Device Info attribute from the P2P IE's.
 */
void static
reassoc_indication(os_handle_t ctx, u_int8_t *macaddr, u_int16_t result, wbuf_t wbuf, wbuf_t resp_wbuf, bool reassoc)
{
    ieee80211_p2p_prot_go_t h_prot_go = (ieee80211_p2p_prot_go_t)ctx;
    struct prot_go_member   *member = NULL;
    struct p2p_parsed_ie    msg;
    u_int8_t                *frm;
    u_int16_t               frame_len, ie_len;
    struct ieee80211_frame  *wh;
    int                     retval;

    if (result != 0) {
        /* A failure. Do nothing */
        return;
    }

    /* Extract and Add the client information */
    OS_MEMZERO(&msg, sizeof(struct p2p_parsed_ie));
    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    frame_len =  wbuf_get_pktlen(wbuf);
    frm = (u_int8_t *)&wh[1];   /* skip header to get to the payload */

    /*
     * asreq frame format
     *    [2] capability information
     *    [2] listen interval
     *    [6*] current AP address (reassoc only)
     *    [tlv] ssid
     *    [tlv] supported rates
     *    [tlv] extended supported rates
     *    [tlv] WPA or RSN
     *    [tlv] WME
     *    [tlv] HT Capabilities
     *    [tlv] Atheros capabilities
     */
    if (reassoc) {
        /* For reassoc, skip cap (2), listen interval (2), and AP addr (6) */
        if (frame_len < (sizeof(struct ieee80211_frame) + 10)) {
            /* Error */
            IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: invalid Reassoc frame. len=%d too short.\n",
                                 __func__, frame_len);
            return;
        }
        ie_len = frame_len - sizeof(struct ieee80211_frame) - 10;
        frm += 10;
    }
    else {
        /* For assoc, skip cap (2), and listen interval (2) */
        if (frame_len < (sizeof(struct ieee80211_frame) + 4)) {
            /* Error */
            IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                                 "%s: invalid Assoc frame. len=%d too short.\n",
                                 __func__, frame_len);
            return;
        }
        ie_len = frame_len - sizeof(struct ieee80211_frame) - 4;
        frm += 4;
    }

    retval = ieee80211_p2p_parse_ies(h_prot_go->os_handle, h_prot_go->vap, frm, ie_len, &msg);
    if (retval != 0) {
        IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: invalid Assoc/Reassoc frame. Ignored.\n",__func__);
        return;
    }

    if ((msg.p2p_device_info == NULL) || (msg.p2p_device_info_len == 0)) {
        /* Error: No device info field. */
        IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                          "%s: Error: no p2p device info in Assoc/Reassoc frame. Ignored.\n",__func__);
        return;
    }

    spin_lock(&(h_prot_go->lock));

    member = prot_go_find_member(h_prot_go, macaddr);
    if (member == NULL) {
        /* If not found, then create this new member */
        IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                             "%s: Error: member don't exists! "MAC_ADDR_FMT".\n",
                             __func__, SPLIT_MAC_ADDR(macaddr));
        goto fail_prot_go_reassoc_ind;
    }

    if (member->client_info_len < msg.p2p_device_info_len) {
        /* Need to alloc a new buffer for client info. */
        if (member->client_info != NULL) {
            /* Free the old one */
            OS_FREE(member->client_info);
            member->client_info = NULL;
            member->client_info_len = 0;
        }

        member->client_info = (u8 *)OS_MALLOC(h_prot_go->os_handle, msg.p2p_device_info_len, 0);
        if (member->client_info == NULL) {
            IEEE80211_DPRINTF_VB(h_prot_go->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                              "%s: unable to alloc memory for client info. size=%d\n",
                              __func__, msg.p2p_device_info_len);
            goto fail_prot_go_reassoc_ind;
        }
        member->client_info_len = msg.p2p_device_info_len;
    }

    OS_MEMCPY(member->client_info, msg.p2p_device_info, msg.p2p_device_info_len);

fail_prot_go_reassoc_ind:

    spin_unlock(&(h_prot_go->lock));
    ieee80211_p2p_parse_free(&msg);

    wlan_p2p_prot_GO_update_probe_resp_ie(h_prot_go);

    return;
}

/* Function is a callback function for MLME Event callback for Association */
void static
prot_go_assoc_ind(os_handle_t ctx, u_int8_t *macaddr, u_int16_t result, wbuf_t wbuf, wbuf_t resp_wbuf)
{
    reassoc_indication(ctx, macaddr, result, wbuf, resp_wbuf, false);
}

/*
 * Function is a callback function for MLME Event callback for Reassociation.
 */
void static
prot_go_reassoc_ind(os_handle_t ctx, u_int8_t *macaddr, u_int16_t result, wbuf_t wbuf, wbuf_t resp_wbuf)
{
    reassoc_indication(ctx, macaddr, result, wbuf, resp_wbuf, true);
}


