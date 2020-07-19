/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*/

#include "wlan_son_pub.h"
#include "wlan_son_internal.h"
#include "wlan_son_ucfg_api.h"

#if QCA_SUPPORT_SON

/**
 * @brief Add the Whole Home Coverage repeater AP Info information
 * element to a frame.
 * @param [in] frm  the place in the frame to which to start writing the IE
 * @param [in] whc_caps  the bitmask that describes the AP's capabilities
 * @param [in] vap handle
 * @param [in] Pointer to length which gets total IE length
 *
 * @return pointer to where the remainder of the frame should be written
 */

u_int8_t *son_add_rept_info_ie(u_int8_t *frm)
{
	static const u_int8_t oui[6] = {
		(QCA_OUI & 0xff), ((QCA_OUI >> 8) & 0xff), ((QCA_OUI >> 16) & 0xff),
		QCA_OUI_WHC_REPT_TYPE, QCA_OUI_WHC_REPT_INFO_SUBTYPE,
		QCA_OUI_WHC_REPT_INFO_VERSION
	};

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = sizeof(oui);
	OS_MEMCPY(frm, oui, sizeof(oui));
	frm += sizeof(oui);

	return frm;
}

/**
 * @brief Check whether the configured uplink rate can be applied for mixed backchaul
 *
 * configured uplink rate will be applied only in Repeater mode and
 * ethernet backhaul found.
 *
 * @param [in] son pdev
 *
 * @return false if Wireless backhaul found and or CAP mode, Otherwise
 * true.
 */
static bool
son_whc_is_wired_backhaul(const struct son_pdev_priv *pdev)
{
	/* To handle the initialization state where there is
	 * no topology responses received
	 */
	if ( !pdev->son_backhaul_type ) {
		return false;
	}

	return ((pdev->son_backhaul_type == SON_BACKHAUL_TYPE_WIFI) ? false : true);
}
/**
 * @brief Add the Whole Home Coverage AP Info information element to a frame.
 * @param [in] frm  the place in the frame to which to start writing the IE
 * @param [in] whc_caps  the bitmask that describes the AP's capabilities
 * @param [in] vap handle
 * @param [in] Pointer to length which gets total IE length
 *
 * @return pointer to where the remainder of the frame should be written
 */
u_int8_t *son_add_ap_info_ie(u_int8_t *frm, u_int16_t whc_caps,
			     struct wlan_objmgr_vdev *vdev,
			     u_int16_t* ie_len)
{
	static const u_int8_t oui[6] = {
		(QCA_OUI & 0xff), ((QCA_OUI >> 8) & 0xff), ((QCA_OUI >> 16) & 0xff),
		QCA_OUI_WHC_TYPE, QCA_OUI_WHC_AP_INFO_SUBTYPE,
		QCA_OUI_WHC_AP_INFO_VERSION
	};
#define MIN_LENGTH 9
#define UPLINK_BSSID_IE_LENGTH 10
	u_int8_t *plen = NULL,dist = 0;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;
	u_int16_t uplink_rate;
	wlan_chan_t chan = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if(!pdev)
		return frm;

	pd_priv = wlan_son_get_pdev_priv(pdev);

	*frm++ = IEEE80211_ELEMID_VENDOR;
	plen = frm++;
	*plen = MIN_LENGTH;
	OS_MEMCPY(frm, oui, sizeof(oui));
	frm += sizeof(oui);

	/* Populate the WHC capabilities bit mask in little endian byte order */
	*frm++ = (whc_caps & 0xff);
	*frm++ = ((whc_caps >> 8) & 0xff);

	dist = son_core_get_root_dist(vdev);
	*frm++ = dist;
	*plen += 22;

	if((dist == 0)) {
		/* ROOTAP indicator */
		*frm++ = 1;
		uplink_rate = 0xFFFF;
	} else {
		/* ROOTAP indicator */
		*frm++ = 0;
		/* true means self rate */
		uplink_rate = (u_int16_t) son_ucfg_rep_datarate_estimator(
                        son_get_backhaul_rate(vdev, true),
                        son_get_backhaul_rate(vdev, false),
                        (ucfg_son_get_root_dist(vdev) - 1),
                        ucfg_son_get_scaling_factor(vdev));
	}
	/* set uplink rate for repeater note and ethernet backhaul type*/
	if(dist && son_whc_is_wired_backhaul(pd_priv)) {
		uplink_rate = pd_priv->recv_ul_rate_mixedbh;
	}
	pd_priv->curr_ul_rate_mixedbh = uplink_rate;
	/* # of connected REs */
	*frm++ = son_repeater_cnt_get(vdev);
	/* uplink BSSID of this RE */
	qdf_mem_copy(frm, pd_priv->uplink_bssid, IEEE80211_ADDR_LEN);
	frm += IEEE80211_ADDR_LEN;
	/* uplink(STA) rate of this RE (Little Endian) */
	*frm++ = uplink_rate & 0xff;
	*frm++ = uplink_rate >> 8 & 0xff;

	chan = wlan_vdev_get_channel(vdev);
	if (chan == NULL) {
		qdf_mem_zero(frm,IEEE80211_ADDR_LEN*2);
		frm += IEEE80211_ADDR_LEN*2;
	} else if (IEEE80211_IS_CHAN_5GHZ(chan)) {
		qdf_mem_copy(frm, wlan_vdev_mlme_get_macaddr(vdev), IEEE80211_ADDR_LEN);
		frm += IEEE80211_ADDR_LEN;
		qdf_mem_copy(frm, pd_priv->son_otherband_bssid, IEEE80211_ADDR_LEN);
		frm += IEEE80211_ADDR_LEN;
	} else {
		qdf_mem_copy(frm, pd_priv->son_otherband_bssid, IEEE80211_ADDR_LEN);
		frm += IEEE80211_ADDR_LEN;
		qdf_mem_copy(frm, wlan_vdev_mlme_get_macaddr(vdev), IEEE80211_ADDR_LEN);
		frm += IEEE80211_ADDR_LEN;
	}
	/* return the length of this ie, including element ID and length field */
	*ie_len = (u_int16_t)(*plen + 2);
#undef MIN_LENGTH
#undef UPLINK_BSSID_IE_LENGTH
	return frm;
}

/**
 * @brief Add Multi AP Information Element to a frame
 * @param [in] frm  the place in the frame to which to start writing the IE
 * @param [in] vdev handle
 * @param [in] ie_len Pointer to length which gets total IE length
 *
 * @return pointer to where the remainder of the frame should be written
 */
u_int8_t *son_add_multi_ap_ie(u_int8_t *frm, struct wlan_objmgr_vdev *vdev,
			      u_int16_t* ie_len)
{
	u_int8_t attribute = 0;
	u_int8_t attributelen = 0;
	u_int8_t attributevalue = 0;

	static const u_int8_t oui[4] = {
		(WFA_OUI_IE & 0xff),
		((WFA_OUI_IE >> 8) & 0xff),
		((WFA_OUI_IE >> 16) & 0xff),
		WFA_OUI_MAP_TYPE_IE
	};

	u_int8_t *plen = NULL;

	if(!vdev)
		return frm;

	*frm++ = IEEE80211_ELEMID_VENDOR;
	plen = frm++;

	*plen = 4;
	qdf_mem_copy(frm, oui, sizeof(oui));
	frm += sizeof(oui);

	*plen += 3;
	attribute = 6;
	attributelen = 1;

	attributevalue = son_vdev_map_capability_get(vdev, SON_MAP_CAPABILITY_VAP_TYPE);

	/* Add attribute and Length */
	*frm++ = attribute;
	*frm++ = attributelen;
	*frm++ = attributevalue;

	/* return the length of this ie, including element ID and length field */
	*ie_len = (u_int16_t)(*plen + 2);

	return frm;
}

/**
 * @brief Parse a Whole Home Coverage AP Info IE, storing the capabilities
 *        of the AP.
 *
 * @param [in] ni  the BSS node for the AP that advertised these capabilities
 * @param [in] ie  the beginning of the information element
 */

void son_process_whc_apinfo_ie(struct wlan_objmgr_peer *peer, const u_int8_t *ie)
{
	const struct ieee80211_ie_whc_apinfo *whcAPInfoIE =
		(const struct ieee80211_ie_whc_apinfo *) ie;
	struct wlan_objmgr_vdev *vdev = NULL;
	u_int16_t whcCaps = 0;

	/* Unfortunately we cannot derive the expected length from the IE without
	 * having to also account for padding. Hence, the hard coded value here.
	 */
	if (whcAPInfoIE->whc_apinfo_len < 9) {
		SON_LOGI( "WHC AP Info too short, len %u", whcAPInfoIE->whc_apinfo_len);
		return;
	}

	vdev = wlan_peer_get_vdev(peer);
	whcCaps = LE_READ_2(&whcAPInfoIE->whc_apinfo_capabilities);
	if (whcAPInfoIE->whc_apinfo_version != QCA_OUI_WHC_AP_INFO_VERSION) {
		SON_LOGW("WHC AP Info unexpected version %u", whcAPInfoIE->whc_apinfo_version);
		/* WHC IE Version mismatch found, just parse only the minimal required data
		 * and go to WDS mode, if possible, so that REPACD will enable only one backhaul
		 * interface for software upgrade */
		goto end;
	}

	if (whcCaps & QCA_OUI_WHC_AP_INFO_CAP_SON) {
		son_set_whc_apinfo_flag(peer, IEEE80211_NODE_WHC_APINFO_SON);
	}
end:
	if (whcCaps & QCA_OUI_WHC_AP_INFO_CAP_WDS) {
		son_set_whc_apinfo_flag(peer, IEEE80211_NODE_WHC_APINFO_WDS);
	}

	/* The + 1 sets up this node so that it advertises the right hop count
	 * for anybody that tries to associate to it.
	 */
	if (whcAPInfoIE->whc_apinfo_root_ap_dist == SON_INVALID_ROOT_AP_DISTANCE)
		son_core_set_root_dist(vdev, SON_INVALID_ROOT_AP_DISTANCE);
	else
		son_core_set_root_dist(vdev, whcAPInfoIE->whc_apinfo_root_ap_dist + 1);

	return;
}
/**
 * @brief Parse a Whole Home Coverage repeater AP Info IE, storing the capabilities
 *        of the AP.
 *
 * @param [in] ni  the BSS node for the AP that advertised these capabilities
 * @param [in] ie  the beginning of the information element
 */
void son_process_whc_rept_info_ie(struct wlan_objmgr_peer *peer,
				  const u_int8_t *ie)
{
	const struct ieee80211_ie_whc_rept_info *whcReptInfoIE =
		(const struct ieee80211_ie_whc_rept_info *)ie;

	if (whcReptInfoIE->whc_rept_info_len < 6) {
		SON_LOGW("WHC Rept Info too short, len %u", whcReptInfoIE->whc_rept_info_len);
		return;
	}

	if (whcReptInfoIE->whc_rept_info_version != QCA_OUI_WHC_REPT_INFO_VERSION) {
		SON_LOGW("WHC Rept Info unexpected version %u", whcReptInfoIE->whc_rept_info_version);
		return;
	}

	son_set_whc_rept_info(peer);

	return;
}
int isqca_son_rept_oui(u_int8_t *frm, u_int8_t whc_subtype)
{
	return ((frm[1] > 4) && (LE_READ_4(frm+2) == ((QCA_OUI_WHC_REPT_TYPE<<24)|QCA_OUI)) &&
		(*(frm+6) == whc_subtype));
}

EXPORT_SYMBOL(isqca_son_rept_oui);

#else

/**
 * @brief Check if received frame has son repeater ie or not.
 *
 * @param [in] pointer to  ie location in frame.
 * @param [in] whc_caps  the bitmask that describes the AP's capabilities
 * @return True is ie are present otherwise false.
 */

int
isqca_son_rept_oui(u_int8_t *frm, u_int8_t whc_subtype)
{
	return false;
}

EXPORT_SYMBOL(isqca_son_rept_oui);

/**
 * @brief Add the Whole Home Coverage AP Info information element to a frame.
 *
 * @param [in] frm  the place in the frame to which to start writing the IE
 * @param [in] whc_caps  the bitmask that describes the AP's capabilities
 * @param [in] vap handle
 * @param [in] Pointer to length which gets total IE length
 *
 * @return pointer to where the remainder of the frame should be written
 */
u_int8_t *son_add_ap_info_ie(u_int8_t *frm, u_int16_t whc_caps,
			     struct wlan_objmgr_vdev *vdev, u_int16_t* ie_len)
{
	return frm;

}
/**
 * @brief Add the Whole Home Coverage repater Info information element to a frame.
 *
 * @param [in] frm  the place in the frame to which to start writing the IE
 * @param [in] whc_caps  the bitmask that describes the AP's capabilities
 * @param [in] vap handle
 * @param [in] Pointer to length which gets total IE length
 *
 * @return pointer to where the remainder of the frame should be written
 */

u_int8_t *son_add_rept_info_ie(u_int8_t *frm)
{
	return frm;
}
/**
 * @brief Parse a Whole Home Coverage AP Info IE, storing the capabilities
 *        of the AP.
 *
 * @param [in] ni  the BSS node for the AP that advertised these capabilities
 * @param [in] ie  the beginning of the information element
 */

/**
 * @brief Add Multi AP Information Element to a frame
 * @param [in] frm  the place in the frame to which to start writing the IE
 * @param [in] vdev handle
 * @param [in] ie_len Pointer to length which gets total IE length
 *
 * @return pointer to where the remainder of the frame should be written
 */
u_int8_t *son_add_multi_ap_ie(u_int8_t *frm, struct wlan_objmgr_vdev *vdev,
			      u_int16_t* ie_len)
{
	return frm;
}

void son_process_whc_apinfo_ie(struct wlan_objmgr_peer *peer,
			       const u_int8_t *ie)
{
	return;
}
/**
 * @brief Parse a Whole Home Coverage repeater AP Info IE, storing the capabilities
 *        of the AP.
 *
 * @param [in] ni  the BSS node for the AP that advertised these capabilities
 * @param [in] ie  the beginning of the information element
 */
void son_process_whc_rept_info_ie(struct wlan_objmgr_peer *peer,
				  const u_int8_t *ie)
{
	return;
}

#endif
