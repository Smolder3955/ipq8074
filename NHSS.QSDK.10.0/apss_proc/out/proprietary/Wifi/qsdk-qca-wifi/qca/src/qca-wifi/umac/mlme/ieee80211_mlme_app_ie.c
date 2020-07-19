/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 *  Copyright (c) 2010 Atheros Communications Inc.  All rights reserved.
 */

#include "ieee80211_mlme_priv.h"    /* Private to MLME module */


struct wlan_mlme_app_ie {
    wlan_if_t                   vaphandle;

    /* app ie entry for each frame type */
    struct app_ie_entry         entry[IEEE80211_APPIE_MAX_FRAMES];
};

void wlan_mlme_remove_ie_list(wlan_mlme_app_ie_t app_ie_handle)
{
    struct ieee80211vap *vap;
    struct app_ie_entry *ie_entry, *next_entry;
    ieee80211_frame_type ftype;
    if (app_ie_handle == NULL) {
        printk("%s: handle is NULL. Do nothing.\n", __func__);
        return;
    }
    vap = app_ie_handle->vaphandle;
    ASSERT(vap);
    for (ftype = 0; ftype < IEEE80211_APPIE_MAX_FRAMES; ftype++)
    {
        STAILQ_FOREACH_SAFE(ie_entry, &vap->iv_app_ie_list[ftype], link_entry, next_entry)
        {
            STAILQ_REMOVE(&vap->iv_app_ie_list[ftype], ie_entry, app_ie_entry, link_entry);
            OS_FREE(ie_entry->app_ie.ie);
            OS_FREE(ie_entry);
        }
        vap->iv_app_ie_list[ftype].total_ie_len = 0;
    }
    OS_FREE(app_ie_handle);
}

/**
 * check wpa/rsn ie is present in the ie buffer passed in.
 * Returns false if the WPA IE is found.
 * NOTE: Assumed that the vap's lock to protect the vap->iv_app_ie_list[] structures are held.
 */
bool ieee80211_mlme_app_ie_check_wpaie(
    struct ieee80211vap     *vap,
    ieee80211_frame_type ftype)
{
    struct app_ie_entry         *ie_entry;
    bool                        add_wpa_ie = true;

    ASSERT(ftype < IEEE80211_APPIE_MAX_FRAMES);
    if (vap->iv_app_ie_list[ftype].total_ie_len == 0) {
        /* Not found since empty list */
        return true;
    }
    ASSERT(!STAILQ_EMPTY(&vap->iv_app_ie_list[ftype]));

    STAILQ_FOREACH(ie_entry, &vap->iv_app_ie_list[ftype], link_entry) {
        ASSERT(ie_entry->app_ie.ie != NULL);
        ASSERT(ie_entry->app_ie.length > 0);
        add_wpa_ie = ieee80211_check_wpaie(vap, ie_entry->app_ie.ie, ie_entry->app_ie.length);
        if (!add_wpa_ie) {
            /* Found WPA IE */
            break;
        }
    }
    return add_wpa_ie;
}

/*
 * Function to append the Application IE for this frame type.
 * Returns the new buffer pointer after the newly added IE and the
 * length of the added IE.
 * NOTE: Assumed that the vap's lock to protect the vap->iv_app_ie_list[] structures are held.
 */
u_int8_t *ieee80211_mlme_app_ie_append(
    struct ieee80211vap     *vap,
    ieee80211_frame_type    ftype,
    u_int8_t                *frm
    )
{
    struct app_ie_entry     *ie_entry;
    u_int16_t               ie_len;

    ASSERT(ftype < IEEE80211_APPIE_MAX_FRAMES);

    ie_len = 0;

    if (STAILQ_EMPTY(&vap->iv_app_ie_list[ftype])) {
        /* No change */
        vap->iv_app_ie_list[ftype].changed = false; /* indicates that we have used this ie. */
        return frm;
    }

    STAILQ_FOREACH(ie_entry, &vap->iv_app_ie_list[ftype], link_entry) {
        ASSERT(ie_entry->app_ie.ie != NULL);
        ASSERT(ie_entry->app_ie.length > 0);

        if (IEEE80211_VAP_IS_HIDESSID_ENABLED(vap) && (ftype == IEEE80211_FRAME_TYPE_BEACON) && iswpsoui(ie_entry->app_ie.ie) ) {
            continue;
        } else {
            OS_MEMCPY(frm, ie_entry->app_ie.ie, ie_entry->app_ie.length);

            frm += ie_entry->app_ie.length;
            ie_len += ie_entry->app_ie.length;
        }
    }
    ASSERT(ie_len == vap->iv_app_ie_list[ftype].total_ie_len);

    vap->iv_app_ie_list[ftype].changed = false; /* indicates that we have used this ie. */

    return frm;
}

/*
 * Create an instance to Application IE module to append new IE's for various frames.
 * Returns a handle for this instance to use for subsequent calls.
 */
wlan_mlme_app_ie_t wlan_mlme_app_ie_create(wlan_if_t vaphandle)
{
    struct ieee80211vap     *vap = vaphandle;
    struct ieee80211com     *ic = vap->iv_ic;
    wlan_mlme_app_ie_t      app_ie_handle = NULL;
    int                     error = 0;

    do {

        app_ie_handle = (wlan_mlme_app_ie_t)OS_MALLOC(ic->ic_osdev, sizeof(struct wlan_mlme_app_ie), 0);
        if (app_ie_handle == NULL) {
            error = -ENOMEM;
            break;
        }

        OS_MEMZERO(app_ie_handle, sizeof(struct wlan_mlme_app_ie));

        app_ie_handle->vaphandle = vaphandle;

    } while ( false );

    if (error != 0) {
        /* Some error. */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: Failed and error=%d\n", __func__, error);

        if (app_ie_handle != NULL) {
            OS_FREE(app_ie_handle);
            app_ie_handle = NULL;
        }
    }

    return app_ie_handle;
}

int wlan_mlme_app_ie_delete_id(wlan_mlme_app_ie_t app_ie_handle, ieee80211_frame_type ftype, u_int8_t identifier)
{
    struct ieee80211vap *vap;
    struct app_ie_entry *ie_entry, *next_entry;

    if (app_ie_handle == NULL) {
        printk("%s: handle is NULL. Do nothing.\n", __func__);
        return -EINVAL;
    }
    vap = app_ie_handle->vaphandle;
    ASSERT(vap);

    IEEE80211_VAP_LOCK(vap);

    /*
     * Temp: reduce window of race with beacon update in Linux AP.
     * In Linux AP, ieee80211_beacon_update is called in ISR, so
     * iv_lock is not acquired.
     */
    IEEE80211_VAP_APPIE_UPDATE_DISABLE(vap);

    STAILQ_FOREACH_SAFE(ie_entry, &vap->iv_app_ie_list[ftype], link_entry,next_entry)
    {
        if(ie_entry->identifier == identifier)
        {
            STAILQ_REMOVE(&vap->iv_app_ie_list[ftype], ie_entry, app_ie_entry, link_entry);
            vap->iv_app_ie_list[ftype].total_ie_len -= ie_entry->app_ie.length;
            OS_FREE(ie_entry->app_ie.ie);
            OS_FREE(ie_entry);
        }
    }
    /* Set appropriate flag so that the IE gets updated in the next beacon */
    IEEE80211_VAP_APPIE_UPDATE_ENABLE(vap);

    IEEE80211_VAP_UNLOCK(vap);

    return 0;

}

/*
 * Detach from Application IE module. Any remaining IE's will be removed and freed.
 */
int wlan_mlme_app_ie_delete(wlan_mlme_app_ie_t app_ie_handle, ieee80211_frame_type ftype, u_int8_t *app_ie)
{
    struct ieee80211vap *vap;
    struct app_ie_entry *ie_entry, *next_entry;

    if (app_ie_handle == NULL) {
        printk("%s: handle is NULL. Do nothing.\n", __func__);
        return -EINVAL;
    }
    if(app_ie == NULL)
    {
        printk("%s: appie is NULL. Do nothing.\n", __func__);
        return -EINVAL;
    }

    vap = app_ie_handle->vaphandle;
    ASSERT(vap);

    IEEE80211_VAP_LOCK(vap);

    /*
     * Temp: reduce window of race with beacon update in Linux AP.
     * In Linux AP, ieee80211_beacon_update is called in ISR, so
     * iv_lock is not acquired.
     */
    IEEE80211_VAP_APPIE_UPDATE_DISABLE(vap);

    STAILQ_FOREACH_SAFE(ie_entry, &vap->iv_app_ie_list[ftype], link_entry,next_entry)
    {
        if(ie_entry->app_ie.ie[0] == app_ie[0])
        {
            if(app_ie[0] != IEEE80211_ELEMID_VENDOR || !(OS_MEMCMP(&ie_entry->app_ie.ie[2],&app_ie[2],3)))
            {
                if(app_ie[5] == 0xff || app_ie[5] == ie_entry->app_ie.ie[5])
                {
                    STAILQ_REMOVE(&vap->iv_app_ie_list[ftype], ie_entry, app_ie_entry, link_entry);
                    vap->iv_app_ie_list[ftype].total_ie_len -= ie_entry->app_ie.length;
                    OS_FREE(ie_entry->app_ie.ie);
                    OS_FREE(ie_entry);
                    if(app_ie[5] != 0xff)
                    {
                        break;
                    }
                }
            }
        }
    }
    /* Set appropriate flag so that the IE gets updated in the next beacon */
    IEEE80211_VAP_APPIE_UPDATE_ENABLE(vap);

    IEEE80211_VAP_UNLOCK(vap);

    /* Update beacon template for VAP everytime we have an update in the IE */
    if (ftype == IEEE80211_FRAME_TYPE_BEACON) {
        wlan_vdev_beacon_update(vap);
    }

    return 0;

}

/*
This function checks if any duplicate ie is set. If any duplicate vendor ie is found, it is replaced
*/

static void ieee80211_check_duplicate_ie_delete(struct ieee80211vap *vap, ieee80211_frame_type ftype,const u_int8_t *new_app_ie, u_int32_t new_app_ie_length)
{
    struct app_ie_entry  *ie_entry,*next_entry;
    STAILQ_FOREACH_SAFE(ie_entry, &vap->iv_app_ie_list[ftype], link_entry,next_entry)
    {
        if(ie_entry->app_ie.ie == NULL || ie_entry->app_ie.length <=0)
        {
            return;
        }
        if(ie_entry->app_ie.ie[0] == new_app_ie[0])
        {
            if(new_app_ie[0] != IEEE80211_ELEMID_VENDOR || !(OS_MEMCMP(&ie_entry->app_ie.ie[2],&new_app_ie[2],4)))
            {
                STAILQ_REMOVE(&vap->iv_app_ie_list[ftype], ie_entry, app_ie_entry, link_entry);
                vap->iv_app_ie_list[ftype].total_ie_len -= ie_entry->app_ie.length;
                OS_FREE(ie_entry->app_ie.ie);
                OS_FREE(ie_entry);
                return;
            }
        }
    }
}
/*
Creates a new node to add ie and attaches it to the head
of the existing ie list
*/
int wlan_mlme_app_ie_set(
    wlan_mlme_app_ie_t      app_ie_handle,
    ieee80211_frame_type    ftype,
    const u_int8_t                *buf,
    u_int16_t               buflen,
    u_int8_t                identifier)
{
    struct ieee80211vap     *vap = app_ie_handle->vaphandle;
    struct ieee80211com     *ic = vap->iv_ic;
    int                     error = 0;
    u_int8_t                *iebuf = NULL;
    struct app_ie_entry     *ie_entry;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: Adding App IE for fr-type=%d\n",
                      __func__, ftype);
    if(buf == NULL || buflen == 0)
    {
        printk("NULL IE or zero length received");
        error = -EINVAL;
        return error;
    }
    if (ftype >= IEEE80211_FRAME_TYPE_MAX)
    {
        printk("Invalid frame type");
        error = -EINVAL;
        return error;
    }
    IEEE80211_VAP_LOCK(vap);
    IEEE80211_VAP_APPIE_UPDATE_DISABLE(vap);
    ie_entry = (struct app_ie_entry*)OS_MALLOC(ic->ic_osdev,sizeof(struct app_ie_entry),0);
    if(ie_entry == NULL)
    {
        printk("Not able to allocate memory");
        error = -ENOMEM;
        goto exit;
    }
    iebuf = (u_int8_t*)OS_MALLOC(ic->ic_osdev,buflen*sizeof(u_int8_t),0);
    if(iebuf == NULL)
    {
        printk("Not able to allocate memory");
        OS_FREE(ie_entry);
        error = -ENOMEM;
        goto exit;
    }
    if (IEEE80211_VAP_IS_HIDESSID_ENABLED(vap) && (ftype == IEEE80211_FRAME_TYPE_BEACON) && iswpsoui(buf) ) {
        goto exit;
    }

    if ( iswpsoui(buf)) {
        wlan_set_param(vap,IEEE80211_WPS_MODE,1);
    }
    OS_MEMCPY(iebuf,buf,buflen);
    ie_entry->app_ie.ie = iebuf;
    ie_entry->app_ie.length = buflen;
    ie_entry->identifier = identifier;
    STAILQ_INSERT_TAIL(&vap->iv_app_ie_list[ftype], ie_entry, link_entry);
    vap->iv_app_ie_list[ftype].total_ie_len += ie_entry->app_ie.length;
    vap->iv_app_ie_list[ftype].changed = true;
exit:
    IEEE80211_VAP_APPIE_UPDATE_ENABLE(vap);
    IEEE80211_VAP_UNLOCK(vap);
    return error;
}


/* The IE passed either can be a individual IE or set of IEs in a single
block of memory. When this is single IE it is assumed like {T, L, V},
while app_ie_length is  1 + 1 + V bytes. if that is block of memory, the
IE should be like {{T, L, V} {T, L, V}... }. This function would walk
through each of them by looking at T's and L's, and comparing them with
the one already there in iv_ie_list[frame_type]. All duplicates are
eliminated and replaced with the updated one, new ones are added. A
set of IEs in a single block of memory are split and put in separate
ie_entry elements.
*/

int wlan_mlme_app_ie_set_check(struct ieee80211vap *vap, ieee80211_frame_type ftype,const u_int8_t *app_ie, u_int32_t app_ie_length, u_int8_t identifier)
{
    int i = 0;
    int j=0;
    int block_length = 0;
    int error = 0;
    struct wlan_mlme_app_ie *vie_handle = NULL;
    struct app_ie_entry *ie_entry, *next_entry;

    if(ftype >= IEEE80211_APPIE_MAX_FRAMES)
    {
        error = -EINVAL;
        return error;
    }
    if (vap->vie_handle == NULL)
    {
        vap->vie_handle = wlan_mlme_app_ie_create(vap);
        vie_handle = vap->vie_handle;
        if (vie_handle == NULL)
        {
            printk("not able to get vie_handle");
            error = -ENOMEM;
            return error;
        }
    }
    else
    {
        vie_handle = vap->vie_handle;
    }
#if ATH_SUPPORT_HS20
    /*
     * Parse the appie and add the extended capabilities to driver and
     * remove extended capabilities from appie..Disabling this macro
     * ATH_SUPPORT_HS20 can result in association failure for some stations.
     */
    app_ie_length = wlan_mlme_parse_appie(vap, ftype, app_ie, app_ie_length);
#endif
    /*
    First delete the existing IEs based on ID
    */
#if UMAC_SUPPORT_CFG80211
    if((identifier != 0) && !(vap->iv_cfg80211_create))
#else
    if (identifier != 0)
#endif
    {
        error = wlan_mlme_app_ie_delete_id(vie_handle, ftype, identifier);
        if(error != 0 || app_ie_length == 0 || app_ie == NULL)
        {
            goto out;
        }
    }
    /*
     * add or update IE
     */
    while((i+1 < app_ie_length) && (app_ie[i+1]+1 < app_ie_length))
    {
        block_length = app_ie[i+1] + 2;
        if (IEEE80211_VAP_IS_HIDESSID_ENABLED(vap) && (ftype == IEEE80211_FRAME_TYPE_BEACON) && iswpsoui(&(app_ie[i])))
        {
            i += block_length;
            continue;
        }
        if(vap->iv_app_ie_list[ftype].total_ie_len != 0)
        {
            ieee80211_check_duplicate_ie_delete(vap, ftype, &app_ie[i], block_length);
        }
        error = wlan_mlme_app_ie_set(vie_handle,ftype,&app_ie[i],block_length,identifier);
        i += block_length;
        if(error != 0)
        {
            while(j<i)
            {
                block_length = app_ie[j+1] + 2;
                STAILQ_FOREACH_SAFE(ie_entry, &vap->iv_app_ie_list[ftype], link_entry,next_entry)
                {
                    if(block_length == ie_entry->app_ie.length && !(OS_MEMCMP(&app_ie[j], &ie_entry->app_ie.ie, block_length)))
                    {
                        STAILQ_REMOVE(&vap->iv_app_ie_list[ftype], ie_entry, app_ie_entry, link_entry);
                        vap->iv_app_ie_list[ftype].total_ie_len -= block_length;
                        OS_FREE(ie_entry->app_ie.ie);
                        OS_FREE(ie_entry);
                        break;
                    }
                }
                j+=block_length;
            }
            return error;
        }
    }

    /* Update beacon template for VAP everytime we have an update in the IE */
    if (ftype == IEEE80211_FRAME_TYPE_BEACON) {
        wlan_vdev_beacon_update(vap);
    }

out:
    return error;
}

/*
 * To get the Application IE for this frame type and instance.
 */
int wlan_mlme_app_ie_get(
    wlan_mlme_app_ie_t      app_ie_handle,
    ieee80211_frame_type    ftype,
    u_int8_t                *buf,
    u_int32_t               *ielen,
    u_int32_t               buflen,
    u_int8_t                *temp_buf)
{
    int                     error = 0;
    struct ieee80211vap     *vap;
    bool list_all = false;
    struct app_ie_entry *ie_entry, *next_entry;
    u_int8_t len;
    if(app_ie_handle == NULL)
    {
        return error;
    }
    vap = app_ie_handle->vaphandle;

    if(vap == NULL)
    {
        return error;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: get IE for ftype=%d\n",
                      __func__, ftype);

    if (ftype >= IEEE80211_FRAME_TYPE_MAX) {
        error = -EINVAL;
        return error;
    }
    len = temp_buf[1];
    if(len == 0)
    {
        list_all = true;
    }

    IEEE80211_VAP_LOCK(vap);

    STAILQ_FOREACH_SAFE(ie_entry, &vap->iv_app_ie_list[ftype], link_entry,next_entry)
    {
        if(list_all || ie_entry->app_ie.ie[0] == temp_buf[0])
        {
            if(list_all || temp_buf[0] != IEEE80211_ELEMID_VENDOR || !(OS_MEMCMP(&ie_entry->app_ie.ie[2],&temp_buf[2],3)))
            {
                if(buflen < ((*ielen) + ie_entry->app_ie.length))
                {
                    printk("buffer length exceeded");
                    error = -EINVAL;
                    goto out;
                }
                buf[(*ielen)] = ftype;
                (*ielen)++;
                OS_MEMCPY(&buf[(*ielen)], ie_entry->app_ie.ie, ie_entry->app_ie.length);
                (*ielen) += ie_entry->app_ie.length;
            }
        }
    }
out:
    IEEE80211_VAP_UNLOCK(vap);

    return error;
}

/*
 * To get the IE Element for this element id.
 */
int wlan_mlme_app_ie_get_elemid( wlan_mlme_app_ie_t app_ie_handle,
                                 ieee80211_frame_type ftype, u_int8_t *iebuf,
                                 u_int32_t *ielen, u_int32_t elem_id)
{
    int error = -EINVAL;
    struct ieee80211vap     *vap;
    struct app_ie_entry *ie_entry, *next_entry;

    if (!app_ie_handle) {
        return error;
    }

    vap = app_ie_handle->vaphandle;

    if (!vap) {
        return error;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: get IE with elemid : %d for ftype=%d\n",
                      __func__, elem_id, ftype);

    if (ftype >= IEEE80211_FRAME_TYPE_MAX) {
        return error;
    }

    IEEE80211_VAP_LOCK(vap);

    *ielen = 0;
    STAILQ_FOREACH_SAFE(ie_entry, &vap->iv_app_ie_list[ftype], link_entry,next_entry) {
        if (ie_entry->app_ie.ie[0] == elem_id) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: elem_id %d found. Len : %d \n",
                      __func__, elem_id, ie_entry->app_ie.length);
            OS_MEMCPY(iebuf, ie_entry->app_ie.ie, ie_entry->app_ie.length);
            *ielen = ie_entry->app_ie.length;
            error = 0;
            goto out;
        }
    }
out:
    IEEE80211_VAP_UNLOCK(vap);

    return error;
}
