/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
#include <da_dp.h>
struct dadp_pdev *wlan_get_dp_pdev(struct wlan_objmgr_pdev *pdev)
{
	if (NULL == pdev) {
		qdf_print("%s: PDEV is NULL!", __func__);
		return NULL;
	}

	return (struct dadp_pdev *)pdev->dp_handle;
}

struct dadp_vdev *wlan_get_dp_vdev(struct wlan_objmgr_vdev *vdev)
{
	if (NULL == vdev) {
		qdf_print("%s: VDEV is NULL!", __func__);
		return NULL;
	}

	return (struct dadp_vdev *)vdev->dp_handle;
}

struct dadp_peer *wlan_get_dp_peer(struct wlan_objmgr_peer *peer)
{
	if (NULL == peer) {
		qdf_print("%s: PEER is NULL!", __func__);
		return NULL;
	}

	return (struct dadp_peer *)peer->dp_handle;
}

/*
 * TBD_CRYPTO:
 * Introduce below APIs in crypto module (cmn_dev)
 */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
uint8_t wlan_crypto_peer_get_header(struct wlan_crypto_key *key)
{
	return 0;
}

uint8_t wlan_crypto_peer_get_trailer(struct wlan_crypto_key * key)
{
	return 0;
}

uint8_t wlan_crypto_peer_get_miclen(struct wlan_crypto_key * key)
{
	return 0;
}

uint8_t wlan_crypto_peer_get_cipher_type(struct wlan_crypto_key * key)
{
	return 0;
}

EXPORT_SYMBOL(wlan_crypto_peer_get_cipher_type);
#endif
