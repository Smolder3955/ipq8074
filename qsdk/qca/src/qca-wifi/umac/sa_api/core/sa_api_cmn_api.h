/*
 * Copyright (c) 2013,2017 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 * 2013 Qualcomm Atheros, Inc.
 */

#if UNIFIED_SMARTANTENNA
#include "sa_api_defs.h"
#include "wlan_sa_api_tgt_api.h"
#include <wlan_mlme_dispatcher.h>

extern struct smartantenna_ops *g_sa_ops;
extern qdf_atomic_t g_sa_init;

extern uint32_t rate_table_24[MAX_OFDM_CCK_RATES];
extern uint32_t rate_table_5[MAX_OFDM_CCK_RATES];

static inline
void sa_api_peer_connect(struct wlan_objmgr_pdev *pdev,
			struct wlan_objmgr_peer *peer,
			struct sa_rate_cap *rate_cap);

static inline
void sa_api_peer_connect_iterate(struct wlan_objmgr_pdev *pdev,
		void *object, void *arg)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)arg;
	struct wlan_objmgr_peer *bsspeer;
	uint8_t authorized = 0;

	bsspeer = wlan_vdev_get_bsspeer(vdev);

	authorized = wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_AUTH);

	if ((peer != bsspeer) && authorized)  {
		sa_api_peer_connect(pdev, peer, NULL);
	}
}

static inline
void sa_api_cwm_iterate(struct wlan_objmgr_pdev *pdev,
		void *object, void *arg)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)arg;
	struct wlan_objmgr_peer *bsspeer;
	struct peer_sa_api *pe;
	uint8_t ch_width = 0; /* wlan_vdev_get_ch_width(vdev); */

	bsspeer = wlan_vdev_get_bsspeer(vdev);

	pe = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_SA_API);

	if ((peer != bsspeer) && pe->ch_width != ch_width)  {
		pe->ch_width = ch_width;
		sa_api_peer_connect(pdev, peer, NULL);
	}
}

static inline
int sa_api_get_rxantenna(struct pdev_sa_api *pa, uint32_t *rx_ant)
{
	if (g_sa_ops->sa_get_rxantenna) {
		return g_sa_ops->sa_get_rxantenna(pa->interface_id, rx_ant);
	} else {
		ASSERT(FALSE); /* This condition should not happen */
	}
	return SMART_ANT_STATUS_FAILURE;
}

static inline
int sa_api_init(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_vdev *vdev,
				struct pdev_sa_api *pa, int new_init)
{
	struct sa_config sa_init_config;
	uint32_t rx_antenna;
	uint32_t antenna_array[SMART_ANTENNA_MAX_RATE_SERIES];
	int i = 0;
	uint8_t tx_chainmask = 0, rx_chainmask = 0;
	int ret = 0;
	uint32_t enable;
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_psoc *psoc;
	enum QDF_OPMODE opmode;

	if (g_sa_ops == NULL) {
		qdf_nofl_err("Smart Antenna functions are not registered !!! ");
		return QDF_STATUS_E_FAILURE;
	}

	if (!pa->enable) {
		qdf_nofl_err("SA disabled");
		return QDF_STATUS_E_FAILURE;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		sa_api_err("psoc null !!! \n");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 *  handling Multile VAP cases
	 */
	if (new_init & SMART_ANT_NEW_CONFIGURATION) {
		if (!(pa->state & SMART_ANT_STATE_INIT_DONE)) {
			qdf_atomic_inc(&g_sa_init);
		} else {
			return SMART_ANT_STATUS_SUCCESS;
		}
		pa->interface_id = tgt_get_if_id(psoc, pdev);
	}

	opmode = wlan_vdev_mlme_get_opmode(vdev);

	sa_api_info("opmode %d newinit %x", opmode, new_init);
	if ((QDF_SAP_MODE == opmode) || (QDF_STA_MODE == opmode)) {
		/* TODO: What abt repeater case, need to check calling place for repeater*/
		if (g_sa_ops->sa_init) {
			sa_init_config.radio_id = (pa->interface_id << SMART_ANT_INTERFACE_ID_SHIFT) | pa->radio_id;
			sa_init_config.max_fallback_rates = pa->max_fallback_rates;
			tx_chainmask = wlan_vdev_mlme_get_txchainmask(vdev);
			rx_chainmask = wlan_vdev_mlme_get_rxchainmask(vdev);
			sa_init_config.nss =  wlan_vdev_mlme_get_nss(vdev);
			sa_init_config.txrx_chainmask = (tx_chainmask | (rx_chainmask << 4));

			if (QDF_SAP_MODE == opmode) {
				sa_init_config.bss_mode = SMART_ANT_BSS_MODE_AP;
			} else {
				sa_init_config.bss_mode = SMART_ANT_BSS_MODE_STA;
			}

			if (new_init & SMART_ANT_STA_NOT_CONNECTED) {
				new_init &= ~SMART_ANT_STA_NOT_CONNECTED;
				/* Set Channel number to 0 ("zero") to request default params from SA module
				 * to help scanning while station is not connected.
				 * Helpful only in IEEE80211_M_STA mode.
				 */
				sa_init_config.channel_num = 0;
			} else {
				sa_init_config.channel_num = wlan_vdev_get_chan_num(vdev);
			}
			/* Assume smart antenna module requires both Tx and Rx feedback for now.
			 * smart antenna module can set these values to zero (0) if he doesn't
			 * need any of Tx feedback or Rx feedback or both.
			 */
			sa_init_config.txrx_feedback = SMART_ANT_TX_FEEDBACK_MASK | SMART_ANT_RX_FEEDBACK_MASK;
			ret = g_sa_ops->sa_init(&sa_init_config, new_init);
			ASSERT(ret < 0);  /* -ve value: init error */
			/* Bit 0 in ret is mode. all other bits are discarded. */
			pa->mode = ret & SMART_ANT_MODE;

			sa_api_get_rxantenna(pa, &rx_antenna);
			/*
			 * Create bitmap of smart antenna enabled and Tx/Rx feedback subscription
			 * state. bit 0 represents smart antenna enabled/disabled, bit 4 represents
			 * Tx subscription state and bit 5 represents Rx subscription state.
			 */
			enable = SMART_ANT_ENABLE_MASK | (sa_init_config.txrx_feedback &
					(SMART_ANT_TX_FEEDBACK_MASK | SMART_ANT_RX_FEEDBACK_MASK));
			pa->enable = enable; /*save smart antenna enable bitmap */
			/* Enable smart antenna for First new init */
			if (new_init & SMART_ANT_NEW_CONFIGURATION) {
				/* Enable smart antenna , params@ ic, enable, mode, RX antenna */
				tgt_sa_api_start_sa(psoc, pa->pdev_obj, enable, pa->mode, rx_antenna);
				pa->state |= SMART_ANT_STATE_INIT_DONE;
				pa->state &= ~(SMART_ANT_STATE_DEINIT_DONE); /* clear de init */
			} else {
				tgt_sa_api_set_rx_antenna(psoc, pdev, rx_antenna);
			}

			for (i = 0; i <= pa->max_fallback_rates; i++) {
				antenna_array[i] = rx_antenna;
			}

			/* set TX antenna to default antennas to BSS node */
			peer = wlan_vdev_get_bsspeer(vdev);
			if (!peer) {
				sa_api_err("Invalid BSS peer\n");
                                return SMART_ANT_STATUS_FAILURE;
			}
			tgt_sa_api_set_tx_antenna(psoc, peer, &antenna_array[0]);
		}
	}
	return SMART_ANT_STATUS_SUCCESS;
}

static inline
int sa_api_deinit(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_vdev *vdev, struct pdev_sa_api *pa, int notify)
{
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		sa_api_err("psoc null !!! \n");
		return QDF_STATUS_E_FAILURE;
	}

	if (SMART_ANTENNA_ENABLED(pa)) {
		if (pa->state & SMART_ANT_STATE_DEINIT_DONE) {
			sa_api_err("Deinit is already done !!! \n");
			return 0;
		}
                if (notify) {
			if (g_sa_ops && g_sa_ops->sa_deinit) {
				qdf_atomic_dec(&g_sa_init);
				g_sa_ops->sa_deinit(pa->interface_id);
			}
			tgt_sa_api_start_sa(psoc, pa->pdev_obj, 0, pa->mode, 0);
			pa->enable = 0;
			pa->state |= SMART_ANT_STATE_DEINIT_DONE;
			pa->state &= ~(SMART_ANT_STATE_INIT_DONE); /* clear init */
		}
	}
	return SMART_ANT_STATUS_FAILURE;
}

static inline
uint32_t sa_api_convert_rate_5g(uint32_t rate_code)
{
	uint32_t rate_code_converted = 0;
	uint8_t mcs = 0, nss = 0, i = 0;

	if (IS_HT_RATE(rate_code)) {
		rate_code &= HT_RATE_MASK;
		nss = (rate_code & HT_NSS_MASK) >> HT_NSS_SHIFT;
		mcs = (rate_code & HT_MCS_MASK);
		rate_code_converted = AR600P_ASSEMBLE_HW_RATECODE(mcs, nss, AR600P_PREAMBLE_HT);
	} else {
		for (i = 0; i < MAX_OFDM_CCK_RATES; i++) {
			if (rate_table_24[i] == rate_code) {
				break;
			}
		}
		if (i < MAX_OFDM_CCK_RATES) {
			rate_code_converted = rate_table_5[i];
		}
	}
	return rate_code_converted;
}


static inline
uint32_t sa_api_convert_rate_2g(uint32_t rate_code)
{
	uint32_t rate_code_converted = 0;
	uint8_t preamble = (rate_code & VHT_PREAMBLE_MASK) >> VHT_MCS_SHIFT;
	uint8_t nss = 0, mcs = 0;
	int i = 0;

	if (preamble == AR600P_PREAMBLE_HT) {
		nss = (rate_code & VHT_NSS_MASK) >> VHT_NSS_SHIFT;
		mcs = (rate_code & VHT_MCS_MASK);

		rate_code_converted =  (HT_RATE_FLAG | nss << HT_NSS_SHIFT | mcs);
	} else {
		for (i = 0; i < MAX_OFDM_CCK_RATES; i++) {
			if (rate_table_5[i] == rate_code)
				break;
		}
		if (i < MAX_OFDM_CCK_RATES)
			rate_code_converted = rate_table_24[i];
	}
	return rate_code_converted;
}

static inline
int sa_api_get_default_txantenna(struct wlan_objmgr_pdev *pdev,  uint32_t *default_txant)
{
	struct pdev_sa_api *pa;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (!pa) {
		sa_api_err("smart antenna private object null\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (g_sa_ops->sa_get_txdefaultantenna) {
		return g_sa_ops->sa_get_txdefaultantenna(pa->interface_id, default_txant);
	} else {
		QDF_ASSERT(FALSE); /* This condition should not happen */
	}
	return QDF_STATUS_E_FAILURE;
}

static inline
int sa_api_get_bcn_txantenna(struct wlan_objmgr_pdev *pdev, uint32_t *ant)
{
	struct pdev_sa_api *pa;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (!pa) {
		sa_api_err("smart antenna private object null\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (!SMART_ANTENNA_ENABLED(pa)) {
		return SMART_ANT_STATUS_FAILURE;
	}

	if (g_sa_ops->sa_get_bcn_txantenna) {
		return g_sa_ops->sa_get_bcn_txantenna(pa->interface_id, ant);
	} else {
		QDF_ASSERT(FALSE); /* This condition should not happen */
	}
	return SMART_ANT_STATUS_FAILURE;
}

static inline
int sa_api_get_txantenna(struct peer_sa_api *pe, uint32_t *tx_ant_array)
{
	if (g_sa_ops->sa_get_txantenna) {
		return g_sa_ops->sa_get_txantenna(pe->ccp, tx_ant_array);
	} else {
		ASSERT(FALSE); /* This condition should not happen */
	}
	return SMART_ANT_STATUS_FAILURE;
}

static inline
int sa_api_get_node_config(struct peer_sa_api *pe, uint32_t id, uint32_t *config)
{
	if (g_sa_ops->sa_get_node_config) {
		return g_sa_ops->sa_get_node_config(pe->ccp, id, config);
	} else {
		ASSERT(FALSE); /* This condition should not happen */
	}
	return SMART_ANT_STATUS_FAILURE;
}

static inline
int sa_api_get_traininfo(struct peer_sa_api *peer, uint32_t *train_rate_array, uint32_t *train_ant_array, uint32_t *num_train_pkts)
{
	if (g_sa_ops->sa_get_traininginfo) {
		return g_sa_ops->sa_get_traininginfo(peer->ccp, train_rate_array, train_ant_array, num_train_pkts);
	} else {
		ASSERT(FALSE); /* This condition should not happen */
	}
	return SMART_ANT_STATUS_FAILURE;
}

static inline
int sa_api_update_tx_feedback(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, void *tx_feedback)
{
	uint32_t rx_antenna, tx_antenna_array[SMART_ANT_MAX_SA_CHAINS];
	uint8_t status = 0;
	uint32_t train_rate_array[SMART_ANTENNA_MAX_RATE_SERIES];
	uint32_t train_antenna_array[SMART_ANT_MAX_SA_CHAINS], num_train_pkts;
	uint32_t tx_feedback_config = 0;
	struct wlan_objmgr_psoc *psoc;
	struct pdev_sa_api *pa;
	struct peer_sa_api *pe;

	/* intentionally avoiding locks in data path code */
	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		sa_api_err("psoc null !!! \n");
		return QDF_STATUS_E_FAILURE;
	}

	/* intentionally avoiding locks in data path code */
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (!pa) {
		sa_api_err("pa null !!! \n");
		return QDF_STATUS_E_FAILURE;
	}

	/* intentionally avoiding locks in data path code */
	pe = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_SA_API);
	if (!pe) {
		sa_api_err("pe null !!! \n");
		return QDF_STATUS_E_FAILURE;
	}

	if (pe->ccp == NULL) {
		return SMART_ANT_STATUS_FAILURE;
	}

	if (!SMART_ANTENNA_TX_FEEDBACK_ENABLED(pa)) {
		return SMART_ANT_STATUS_FAILURE;
	}

	if (g_sa_ops->sa_update_txfeedback) {
		if (SMART_ANT_STATUS_SUCCESS == g_sa_ops->sa_update_txfeedback(pe->ccp, (struct  sa_tx_feedback *)tx_feedback, &status)) {
			if (status) {
				if (status & SMART_ANT_TX_FEEDBACK_CONFIG_REQUIRED) {
					if (SMART_ANT_STATUS_SUCCESS == sa_api_get_node_config(pe, SA_TX_FEEDBACK_CONFIG, &tx_feedback_config)) {
						/* configure target to send 1 tx feedback to host for N ppdus sent */
						tgt_sa_api_set_node_config_ops(psoc, peer, SA_TX_FEEDBACK_CONFIG, 1, &tx_feedback_config);
					}
				}
				if (status & SMART_ANT_RX_CONFIG_REQUIRED) {
					if (SMART_ANT_STATUS_SUCCESS == sa_api_get_rxantenna(pa, &rx_antenna)) {
						/* set RX antenna */
						tgt_sa_api_set_rx_antenna(psoc, pdev, rx_antenna);
					}
				}
				if (status & SMART_ANT_TX_CONFIG_REQUIRED) {
					if (SMART_ANT_STATUS_SUCCESS == sa_api_get_txantenna(pe, &tx_antenna_array[0])) {
						/* set TX antenna array */
						tgt_sa_api_set_tx_antenna(psoc, peer, &tx_antenna_array[0]);
					}
				}
				if (status & SMART_ANT_TRAINING_REQUIRED) {
					/* TODO: Instead of passing 3 arguments we can pass structure with required fields */
					/* Get train parameters and program train info to lower modules */
					if (SMART_ANT_STATUS_SUCCESS == g_sa_ops->sa_get_traininginfo(pe->ccp, &train_rate_array[0], &train_antenna_array[0], &num_train_pkts)) {
						tgt_sa_api_set_training_info(psoc, peer, &train_rate_array[0], &train_antenna_array[0], num_train_pkts);
					}
				}
			}
		}
		return SMART_ANT_STATUS_SUCCESS;
	}
	return SMART_ANT_STATUS_FAILURE;
}

static inline
int sa_api_update_rx_feedback(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, void *rx_feedback)
{
	uint32_t rx_antenna;
	uint8_t status = 0;
	uint32_t train_rate_array[SMART_ANTENNA_MAX_RATE_SERIES];
	uint32_t train_antenna_array[SMART_ANT_MAX_SA_CHAINS], num_train_pkts;
	struct wlan_objmgr_psoc *psoc;
	struct pdev_sa_api *pa;
	struct peer_sa_api *pe;

	/* intentionally avoiding locks in data path code */
	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		sa_api_err("psoc null !!! \n");
		return QDF_STATUS_E_FAILURE;
	}

	/* intentionally avoiding locks in data path code */
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (!pa) {
		sa_api_err("pa null !!! \n");
		return QDF_STATUS_E_FAILURE;
	}

	if (!SMART_ANTENNA_RX_FEEDBACK_ENABLED(pa)) {
		return SMART_ANT_STATUS_FAILURE;
	}

	/* intentionally avoiding locks in data path code */
	pe = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_SA_API);
	if (!pe) {
		sa_api_err("pe null !!! \n");
		return QDF_STATUS_E_FAILURE;
	}

	if (pe->ccp == NULL) {
		return SMART_ANT_STATUS_FAILURE;
	}

	if (g_sa_ops->sa_update_rxfeedback) {
		if (SMART_ANT_STATUS_SUCCESS == g_sa_ops->sa_update_rxfeedback(pe->ccp, (struct sa_rx_feedback *)rx_feedback, &status)) {
			if (status) {
				if (status & SMART_ANT_RX_CONFIG_REQUIRED) {
					if (SMART_ANT_STATUS_SUCCESS == sa_api_get_rxantenna(pa, &rx_antenna)) {
						/* set RX antenna */
						tgt_sa_api_set_rx_antenna(psoc, pdev, rx_antenna);
					}
				}
				if (status & SMART_ANT_TRAINING_REQUIRED) {
					/* TODO: Instead of passing 3 arguments we can pass structure with required fields */
					/* Get train parameters and program train info to lower modules */
					if (SMART_ANT_STATUS_SUCCESS == g_sa_ops->sa_get_traininginfo(pe->ccp, &train_rate_array[0], &train_antenna_array[0], &num_train_pkts)) {
						tgt_sa_api_set_training_info(psoc, peer, &train_rate_array[0], &train_antenna_array[0], num_train_pkts);
					}
				}
			}
			return SMART_ANT_STATUS_SUCCESS;
		}
	}

	return SMART_ANT_STATUS_FAILURE;
}

static inline
void sa_api_peer_connect(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, struct sa_rate_cap *rate_cap)
{
	struct sa_node_info node_info;
	struct sa_rate_info rate_info;
	uint8_t i = 0;
	uint32_t htindex = 0, legacyindex = 0;
	struct peer_phy_info {
		uint8_t rxstreams;
		uint8_t streams;
		uint8_t cap;
		uint32_t mode;
	} phy_info;
	struct peer_sa_api *pe;
	struct pdev_sa_api *pa;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;

	psoc = wlan_pdev_get_psoc(pdev);
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (!psoc || !pa) {
		sa_api_err("psoc %pK pa %pK !!! \n", psoc, pa);
		return;
	}
	sa_api_debug("+");

	if (!SMART_ANTENNA_ENABLED(pa)) {
		return;
	}

	pe = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_SA_API);
	if (!pe) {
		sa_api_err("pe null !!! \n");
		return;
	}

	qdf_mem_zero(&node_info, sizeof(struct sa_node_info));

	wlan_node_get_peer_phy_info(peer, &phy_info.rxstreams, &phy_info.streams, &phy_info.cap, &phy_info.mode);
	pe->ch_width = wlan_node_get_peer_chwidth(peer);

	if (tgt_sa_api_prepare_rateset(psoc, pdev, peer, &rate_info) == QDF_STATUS_SUCCESS) {
		/* form rate set to sa module */
		for (i = 0; i < rate_info.num_of_rates; i++) {
			if (IS_HT_RATE(rate_info.rates[i].ratecode)) { /* ht rate */
				if (pe->ch_width == IEEE80211_CWM_WIDTH20) {
					node_info.rate_cap.ratecode_20[htindex] = sa_api_convert_rate_5g(rate_info.rates[i].ratecode);
					htindex++;
					node_info.rate_cap.ratecount[RATE_INDEX_BW20]++;
				} else {
					node_info.rate_cap.ratecode_40[htindex] = sa_api_convert_rate_5g(rate_info.rates[i].ratecode);
					htindex++;
					node_info.rate_cap.ratecount[RATE_INDEX_BW40]++;
				}
			} else { /* legacy rate */
				node_info.rate_cap.ratecode_legacy[legacyindex] = sa_api_convert_rate_5g(rate_info.rates[i].ratecode);
				legacyindex++;
				node_info.rate_cap.ratecount[RATE_INDEX_CCK_OFDM]++;
			}
		}
	} else {
		/* rates are already set */
		if (rate_cap != NULL) {
			qdf_mem_copy(&node_info.rate_cap, rate_cap, sizeof(struct sa_rate_cap));
		}
	}

	qdf_mem_copy(node_info.mac_addr, wlan_peer_get_macaddr(peer), 6);
	node_info.radio_id = pa->interface_id;

	node_info.max_ch_bw = pe->ch_width;
	node_info.chainmask = ((phy_info.streams) | (phy_info.rxstreams << 4));
	/*11AX TODO (Phase II) - Is HE OPmode needed here ? */
	node_info.opmode = ((phy_info.mode & IEEE80211_NODE_VHT) ? OPMODE_VHT : (phy_info.mode & IEEE80211_NODE_VHT & IEEE80211_NODE_HT ? OPMODE_HT : OPMODE_CCK_OFDM));;

	node_info.ni_cap = phy_info.cap; /* TODO: refined cap need to be filled */

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, 0, WLAN_SA_API_ID);
	node_info.channel_num =  wlan_vdev_get_chan_num(vdev);
	if (g_sa_ops->sa_peer_connect) {
		g_sa_ops->sa_peer_connect(&pe->ccp, &node_info);
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_SA_API_ID);
}

static inline
int sa_api_set_param(struct wlan_objmgr_pdev *pdev, char *params)
{
	struct sa_config_params sa_params;
	uint32_t tx_antenna_array[SMART_ANT_MAX_SA_CHAINS];
	uint8_t  mac_addr[6] = {'\0'};
	uint32_t *config = (uint32_t *) params;
	uint32_t radio_mac1 = config[0];
	uint32_t mac2 = config[1];
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_psoc *psoc;
	int retval = 0;
	int config_required = 0;
	uint32_t rx_antenna = 0;
	uint32_t tx_feedback_config = 0;
	uint32_t train_rate_array[SMART_ANTENNA_MAX_RATE_SERIES];
	uint32_t train_antenna_array[SMART_ANT_MAX_SA_CHAINS], num_train_pkts;
	struct pdev_sa_api *pa;
	struct peer_sa_api *pe;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	psoc = wlan_pdev_get_psoc(pdev);
	if (!pa || !psoc) {
		sa_api_err("null pa %pK, psoc %pK !!! \n", pa, psoc);
		return QDF_STATUS_E_FAILURE;
	}

	sa_params.radio_id = pa->interface_id;
	sa_params.param_type = (config[0] & SA_PARM_TYPE_MASK) >> SA_PARM_TYPE_SHIFT;
	sa_params.param_id  = config[2];
	sa_params.param_value  = config[3];
	sa_params.ccp = NULL;

	if (sa_params.param_type) {
		/* look for node */
		mac_addr[0] = (uint8_t)((radio_mac1 & 0x0000ff00) >> 8);
		mac_addr[1] = (uint8_t)((radio_mac1 & 0x000000ff) >> 0);
		mac_addr[2] = (uint8_t)((mac2 & 0xff000000) >> 24);
		mac_addr[3] = (uint8_t)((mac2 & 0x00ff0000) >> 16);
		mac_addr[4] = (uint8_t)((mac2 & 0x0000ff00) >> 8);
		mac_addr[5] = (uint8_t)((mac2 & 0x000000ff) >> 0);

		peer = wlan_objmgr_get_peer(psoc,
					    wlan_objmgr_pdev_get_pdev_id(pdev),
					    &mac_addr[0], WLAN_SA_API_ID);
		if (peer == NULL) {
			sa_api_err("peer null !!! \n");
			return SMART_ANT_STATUS_FAILURE;
		}

		pe = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_SA_API);
		if (!pe) {
			sa_api_err("pe null !!! \n");
			return QDF_STATUS_E_FAILURE;
		}

		if ((pe == NULL) || (pe->ccp == NULL)) {
			return SMART_ANT_STATUS_FAILURE;
		}
		sa_params.ccp  = pe->ccp;
		retval = g_sa_ops->sa_set_param(&sa_params);
	} else {
		retval = g_sa_ops->sa_set_param(&sa_params);
	}

	config_required = (retval == SMART_ANT_STATUS_FAILURE) ? 0 : retval;
	if (config_required) {
		if (config_required & SMART_ANT_RX_CONFIG_REQUIRED) {
			if (SMART_ANT_STATUS_SUCCESS == sa_api_get_rxantenna(pa, &rx_antenna)) {
				/* set RX antenna */
				tgt_sa_api_set_rx_antenna(psoc, pdev, rx_antenna);
			}
		} else if ((config_required & SMART_ANT_TX_FEEDBACK_CONFIG_REQUIRED) && (peer != NULL)) {
			if (SMART_ANT_STATUS_SUCCESS == sa_api_get_node_config(pe, SA_TX_FEEDBACK_CONFIG, &tx_feedback_config)) {
				/* configure target to send 1 tx feedback to host for N ppdus sent */
				tgt_sa_api_set_node_config_ops(psoc, peer, SA_TX_FEEDBACK_CONFIG, 1, &tx_feedback_config);
			}
		} else if ((config_required & SMART_ANT_TX_CONFIG_REQUIRED) && (peer != NULL)) {
			if (SMART_ANT_STATUS_SUCCESS == sa_api_get_txantenna(pe, &tx_antenna_array[0])) {
				/* set TX antenna array */
				tgt_sa_api_set_tx_antenna(psoc, peer, &tx_antenna_array[0]);
			}
		} else if ((config_required & SMART_ANT_TRAINING_REQUIRED) && (peer != NULL)) {
			if (SMART_ANT_STATUS_SUCCESS == g_sa_ops->sa_get_traininginfo(pe->ccp, &train_rate_array[0], &train_antenna_array[0], &num_train_pkts)) {
				tgt_sa_api_set_training_info(psoc, peer, &train_rate_array[0], &train_antenna_array[0], num_train_pkts);
			}
		}
	}

	if (peer) {
		wlan_objmgr_peer_release_ref(peer, WLAN_SA_API_ID);
	}

	return retval;
}

static inline
int sa_api_get_param(struct wlan_objmgr_pdev *pdev, char *params)
{
	struct sa_config_params sa_params;
	uint8_t  mac_addr[6] = {'\0'};
	uint32_t *config = (uint32_t *) params;
	uint32_t radio_mac1 = config[0];
	uint32_t mac2 = config[1];
	int retval = 0;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_peer *peer = NULL;
	struct pdev_sa_api *pa;
	struct peer_sa_api *pe;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	psoc = wlan_pdev_get_psoc(pdev);
	if (!pa) {
		sa_api_err("null pa %pK, psoc %pK !!! \n", pa, psoc);
		return QDF_STATUS_E_FAILURE;
	}

	if (!SMART_ANTENNA_ENABLED(pa)) {
		return SMART_ANT_STATUS_FAILURE;
	}

	sa_params.radio_id = pa->interface_id;
	sa_params.param_type = (config[0] & SA_PARM_TYPE_MASK) >> SA_PARM_TYPE_SHIFT;
	sa_params.param_id  = config[2];
	if (sa_params.param_type) {
		/* look for node */
		mac_addr[0] = (uint8_t)((radio_mac1 & 0x0000ff00) >> 8);
		mac_addr[1] = (uint8_t)((radio_mac1 & 0x000000ff) >> 0);
		mac_addr[2] = (uint8_t)((mac2 & 0xff000000) >> 24);
		mac_addr[3] = (uint8_t)((mac2 & 0x00ff0000) >> 16);
		mac_addr[4] = (uint8_t)((mac2 & 0x0000ff00) >> 8);
		mac_addr[5] = (uint8_t)((mac2 & 0x000000ff) >> 0);

		peer = wlan_objmgr_get_peer(psoc,
					    wlan_objmgr_pdev_get_pdev_id(pdev),
					    mac_addr,
					    WLAN_SA_API_ID);

		pe = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_SA_API);
		if (!pe) {
			sa_api_err("pe null !!! \n");
			return QDF_STATUS_E_FAILURE;
		}

		if ((peer == NULL) || (pe) || (pe->ccp == NULL)) {
			return SMART_ANT_STATUS_FAILURE;
		}
		sa_params.ccp  = pe->ccp;
		retval = g_sa_ops->sa_get_param(&sa_params);
	} else {
		retval = g_sa_ops->sa_get_param(&sa_params);
	}

	if (peer) {
		wlan_objmgr_peer_release_ref(peer, WLAN_SA_API_ID);
	}
	config[0] = sa_params.param_value;
	return retval;
}

static inline
void sa_api_peer_disconnect(struct wlan_objmgr_peer *peer)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	struct peer_sa_api *pe;
	struct pdev_sa_api *pa;
	enum QDF_OPMODE opmode;

	if (!peer) {
		sa_api_err("peer is null\n");
		return;
	}

	vdev = wlan_peer_get_vdev(peer);
	pe = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_SA_API);
	if (!vdev || !pe) {
		sa_api_err("vdev %pK pe %pK\n", vdev, pe);
		return;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	opmode = wlan_vdev_mlme_get_opmode(vdev);
	if (!pdev) {
		sa_api_err("pdev is NULL\n");
		return;
	}

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (!pa) {
		sa_api_err("pa null !!! \n");
		return;
	}

	if (SMART_ANTENNA_ENABLED(pa)) {
		/*  Do we need to check removing own vap/node ??? */
		if ((opmode == QDF_SAP_MODE) && (peer != wlan_vdev_get_bsspeer(vdev))) {
			if (g_sa_ops->sa_peer_disconnect) {
				if (pe->ccp != NULL) {
					g_sa_ops->sa_peer_disconnect(pe->ccp);
					pe->ccp = NULL;
				}
			}
		} else if ((opmode == QDF_STA_MODE) &&
				(peer != wlan_vdev_get_bsspeer(vdev))) {
			if (g_sa_ops->sa_peer_disconnect) {
				if (pe->ccp != NULL) {
					g_sa_ops->sa_peer_disconnect(pe->ccp);
					pe->ccp = NULL;
				}
			}
		}
	}
}

static inline
int sa_api_channel_change(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_sa_api *pa;
	struct wlan_objmgr_vdev *vdev;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (!pa) {
		sa_api_err("pa null !!! \n");
		return QDF_STATUS_E_FAILURE;
	}

	if (!SMART_ANTENNA_ENABLED(pa)) {
		return SMART_ANT_STATUS_FAILURE;
	}

	if (wlan_pdev_scan_in_progress(pdev)) {
		return 0;
	}

	if ((pa->state != SMART_ANT_STATE_INIT_DONE)) {
		return SMART_ANT_STATUS_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, 0, WLAN_SA_API_ID);
	if (NULL == vdev) {
		sa_api_err("vdev null !!! \n");
		return SMART_ANT_STATUS_FAILURE;
	}

	/* call smart antenna deint and init with re configuration */
	sa_api_deinit(pdev, vdev, pa, SMART_ANT_RECONFIGURE);
	sa_api_init(pdev, vdev, pa, SMART_ANT_RECONFIGURE);

	/* call peer_connect to each node in the list */
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
			sa_api_peer_connect_iterate,
			vdev, 0, WLAN_SA_API_ID);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_SA_API_ID);

	return SMART_ANT_STATUS_SUCCESS;
}

static inline
int sa_api_cwm_action(struct wlan_objmgr_pdev  *pdev)
{
	struct pdev_sa_api *pa;
	struct wlan_objmgr_vdev *vdev;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (!pa) {
		sa_api_err("pa null !!! \n");
		return QDF_STATUS_E_FAILURE;
	}
	/* get current cwm mode */
	if (!SMART_ANTENNA_ENABLED(pa)) {
		return SMART_ANT_STATUS_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, 0, WLAN_SA_API_ID);
	if (NULL == vdev) {
		sa_api_err("vdev null !!! \n");
		return SMART_ANT_STATUS_FAILURE;
	}

	/* call peer_connect to each node in the list */
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
			sa_api_cwm_iterate, vdev, 0, WLAN_SA_API_ID);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_SA_API_ID);
	return SMART_ANT_STATUS_SUCCESS;
}

#else

static inline
int sa_api_get_bcn_txantenna(struct wlan_objmgr_pdev *pdev, uint32_t *ant)
{
	*bcn_txant = 0xffffffff;
	return 0;
}

static inline
int sa_api_cwm_action(struct wlan_objmgr_pdev  *pdev)
{
	return 0;
}
#endif
