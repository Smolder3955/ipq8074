/*
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.

 * Copyright (c) 2011-2016,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#ifndef _ATF_DEFS_I_H_
#define _ATF_DEFS_I_H_

#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <qdf_list.h>
#include <qdf_timer.h>
#include <qdf_util.h>
#include <wlan_atf_utils_defs.h>

#define atf_log(level, args...) \
QDF_TRACE(QDF_MODULE_ID_ATF, level, ## args)

#define atf_logfl(level, format, args...) atf_log(level, FL(format), ## args)

#define atf_fatal(format, args...) \
	atf_logfl(QDF_TRACE_LEVEL_FATAL, format, ## args)
#define atf_err(format, args...) \
	atf_logfl(QDF_TRACE_LEVEL_ERROR, format, ## args)
#define atf_warn(format, args...) \
	atf_logfl(QDF_TRACE_LEVEL_WARN, format, ## args)
#define atf_info(format, args...) \
	atf_logfl(QDF_TRACE_LEVEL_INFO, format, ## args)
#define atf_debug(format, args...) \
	atf_logfl(QDF_TRACE_LEVEL_DEBUG, format, ## args)

#define CFG_NUM_VDEV                           16
#define PER_UNIT_1000                          1000
#define PER_UNIT_100                           100
#define ATF_TPUT_MAX_STA                       20
#define ATF_MIN_BUFS                           64
#define ATF_MAX_BUFS                           1024
#define ATF_MAX_SUBGROUPS                      20
#define ATF_TPUT_RESV_AIRTIME                  10
/* Num of Access Catergories */
#define WME_NUM_AC                             4
/* build up allocate table (secs)*/
#define ATF_CFG_WAIT                           1
/* ATF Scheduling Policies */
#define ATF_GROUP_SCHED_FAIR                    0    /* Can Borrow & contribute */
#define ATF_GROUP_SCHED_STRICT                  1    /* Cannot borrow & contribute */
#define ATF_GROUP_SCHED_FAIR_UB                 2    /* Can contribute, cannot borrow */
#define ATF_GROUP_NUM_SCHED_POLICIES            3    /* Max number of supported sched policies */
/* With ATF, limit max clients */
#define ATF_MAX_AID_DEF                        (ATF_ACTIVED_MAX_CLIENTS + 1)
#define ATF_MAX_GROUPS                         (ATF_ACTIVED_MAX_ATFGROUPS + 1)
/* 200 msec */
#define ATF_TOKEN_INTVL_MS                     200
#define ATF_DENOMINATION                       1000
#define ATF_GROUP_NAME_LEN                     32
#define ATF_TRUNCATE_GRP_NAME                  4
/* SSID group name created & maintained in the driver by default */
#define DEFAULT_GROUPNAME                      "QC-DeFaUlT-AtFgRoUp\0"
/* Sub-group name created & maintained in the driver by default */
#define DEFAULT_SUBGROUPNAME                   "QC-DeFaUlT-AtFsUbGrOuP"
/* Subgroup types */
#define ATF_SUBGROUP_TYPE_SSID                 0x0
#define ATF_SUBGROUP_TYPE_PEER                 0x1
#define ATF_SUBGROUP_TYPE_AC                   0x2
#define ATF_SG_ACTIVE                          0x1 /* Indicates subgroup is active */
#define ATF_SET_SG_ACTIVE(_v)                 ((_v)->sg_flag |= ATF_SG_ACTIVE)
#define ATF_RESET_SG_ACTIVE(_v)               ((_v)->sg_flag &= ~ATF_SG_ACTIVE)
#define ATF_IS_SG_ACTIVE(_v)                  ((_v)->sg_flag & ATF_SG_ACTIVE)
#define ATF_SET_PEER_CFG(_v)                  (_v |= 1)
#define ATF_RESET_PEER_CFG(_v)                (_v &= ~(1))

/* Subgroup config types */
#define ATF_CFGTYPE_SSID                       0x0
#define ATF_CFGTYPE_PEER                       0x1
#define ATF_CFGTYPE_AC                         0x2
#define ATF_CFGTYPE_GROUP                      0x3
/* Minimum Data log (history) required for ATF Fairqueuing algo */
#define ATF_DATA_LOG_SIZE                      3
#define ATF_DATA_LOG_ZERO                      0
/* ATF scheduling policy */
#define ATF_GROUP_SCHED_FAIR                   0
/* Any node with ununsed token percent (avg) greater than this value
 * can contribute
 */
#define ATF_UNUSEDTOKENS_CONTRIBUTE_THRESHOLD  45
/* Minimum token (percent) reserved for a node even when it is idle */
#define ATF_RESERVERD_TOKEN_PERCENT            1
/* Minimum unalloted token (percent) reserved */
#define ATF_RESERVED_UNALLOTED_TOKEN_PERCENT   5
/* Max Contribution Level */
#define ATF_MAX_CONTRIBUTE_LEVEL               4
/* No. of Iterations after which the contribute level increments.
This should be a power of 2 */
#define ATF_NUM_ITER_CONTRIBUTE_LEVEL_INCR     4
/* Block traffic on all TID's */
#define ATF_TX_BLOCK_AC_ALL                    0xF
/**
 * enum ATF_AIRTIME_DISTRIBUTION - Airtime distribution phases
 * @ATF_AIRTIME_ESTIMATE:    Initial Estination
 * @ATF_AIRTIME_DECONFIG:    Deconfiguration if something goes wrong
 * @ATF_AIRTIME_SUBSEQUENT:  Subsequent airtime allocation
 * @ATF_AIRTIME_EXTRA:       Extra airtime allocation
 * @ATF_AIRTIME_NOTIFY:      Notify allocated airtime
 * @ATF_AIRTIME_MAX:         Max of enum
 *
 * This is being used internally to manage the distribution phases.
 */
typedef enum {
	ATF_AIRTIME_ESTIMATE   = 1,
	ATF_AIRTIME_DECONFIG   = 2,
	ATF_AIRTIME_SUBSEQUENT = 3,
	ATF_AIRTIME_EXTRA      = 4,
	ATF_AIRTIME_NOTIFY     = 5,
	ATF_AIRTIME_MAX
} ATF_AIRTIME_DISTRIBUTION;

/**
 * struct event_data_atf_config - Structure for event config
 * @macaddr: MAC address
 * @config:  Config value
 */
struct event_data_atf_config {
	uint8_t    macaddr[QDF_MAC_ADDR_SIZE];
	int32_t    config;
};

/**
 * struct atf_subtype - To get private command
 * @id_type: Sub command of IOCTL
 */
struct atf_subtype {
	uint16_t   id_type;
};

/**
 * struct ssid_val - SSID configuration object
 * @id_type:    Sub command
 * @ssid:       SSID
 * @value:      Value to configure
 * @ssid_exist: Flag to check SSID exist
 */
struct ssid_val {
	uint16_t   id_type;
	uint8_t    ssid[WLAN_SSID_MAX_LEN + 1];
	uint32_t   value;
	uint8_t    ssid_exist;
};

/**
 * struct atf_ac - AC configuration object
 * @id_type:     Sub command
 * @ac_id:       Access category number
 * @ac_val:      Value to configure
 * @ac_ssid:     Associated SSID
 */
struct atf_ac {
	uint16_t  id_type;
	int8_t ac_id;
	int32_t ac_val;
	uint8_t ac_ssid[WLAN_SSID_MAX_LEN+1];
};

/**
 * struct sta_val - STA configuration object
 * @id_type: Sub command
 * @sta_mac: STA MAC address
 * @ssid:    SSID to which sta is connected
 * @value:   Value to configure
 */
struct sta_val {
	uint16_t   id_type;
	uint8_t    sta_mac[QDF_MAC_ADDR_SIZE];
	uint8_t    ssid[WLAN_SSID_MAX_LEN + 1];
	uint32_t   value;
};

/**
 * struct atf_group - ATF group
 * @id_type: Sub command
 * @name:    Name of group
 * @ssid:    SSID
 * @value:   value to configure
 */
struct atf_group {
	uint16_t    id_type;
	uint8_t     name[ATF_GROUP_NAME_LEN];
	uint8_t     ssid[WLAN_SSID_MAX_LEN + 1];
	uint32_t    value;
};

/**
 * struct atf_subgroup_list - ATF Subgroup list element
 * @sg_next:                List of ATF subgroup instances
 * @sg_name:                Subgroup name
 * @sg_id:                  Subgroup id
 * @sg_type:                Type of subgroup 0 - SSID, 1 - Peer, 2 - AC
 * @sg_peermac:             Peer MAC for peer config entry
 * @ac_id:                  Access category Id, for AC config entry
 * @sg_del:                 Mark subgroup for deletion
 * @usr_cfg_val:            Airtime value configured by user
 * @sg_atf_units:           Allotted airtime for this subgroup
 * @tx_tokens:              Final tx tokens based on user config +
 *                          channel avail + borrow
 * @shadow_tx_tokens:       Copy of tx_tokens
 * @tokens_excess_consumed: Transmit took more time than estimated
 *                          Actual airtime - Estimated airtime
 * @tokens_less_consumed:   Transmit completion happened earlier than estimated
 *                          Estimated airtime - Actual airtime
 * @sg_shadow_tx_tokens:    Copy of tx_tokens. Used a temp variable for calc
 * @sg_raw_tx_tokens:       Tx tokens, user config + channel availability
 * @sg_act_tx_tokens:       Tokens available, after adjustment of excess
 *                          consumed in prev cycle
 * @sg_atfdata_logged:      Data log available
 * @sg_atfindex:            Index to the data log
 * @sg_borrowedtokens:      Tokens borrowed in the previous iteration
 * @sg_contributedtokens:   Tokens contributed in the previous iteration
 * @sg_unusedtokenpercent:  Unused tokens for the last 3 iterations
 * @sg_flag:                Flag to indicate the subgroup status
 * @sg_atf_debug_mask:      Size of the history of statistics
 * @sg_atf_debug_id:        Write index into the history of statistics
 * @sg_allowed_bufs:        Buffers that this subgroup can hold at max
 * @sg_num_buf_held:        Buffers held by a subgroup currently
 * @sg_num_bufs_sent:       Buffers transmitted (or failed)
 * @sg_num_bytes_sent:      Bytes transmitted (or failed)
 * @sg_max_num_buf_held:    Max buffers held by a subgroup
 * @sg_num_min_buf_held:    Min buffers held by a subgroup
 * @sg_pkt_drop_nobuf:      Packets dropped as subgroup is holding max
 *                          allowed buffers
 * @sg_allowed_buf_updated: Set after allowed_bufs has been computed once
 * @sg_atfborrow:           Flag. Node is looking to borrow tokens
 * @sg_contribute_id:       Index used for contribution level
 * @sg_contribute_level:    level of contribution
 * @sg_atftidstate:         TID state. applicable only when 1:1 mapping between
 *                          node and subgroup
 * @atf_sg_lock:            Lock to protect ATF subgroup
 * @sg_atf_stats:           Statistics
 * @sg_atf_debug:           History of statistics
 */
struct atf_subgroup_list {
	qdf_list_node_t  sg_next;
	uint8_t     sg_name[WLAN_SSID_MAX_LEN + 1];
	uint32_t    sg_id;
	uint8_t     sg_type;
	int8_t      sg_peermac[6];
	int8_t      ac_id;
	int8_t      sg_del;
	uint32_t    usr_cfg_val;
	uint32_t    sg_atf_units;
	uint32_t    tx_tokens;
	uint32_t    shadow_tx_tokens;
	uint32_t    tokens_excess_consumed;
	uint32_t    tokens_less_consumed;
	uint32_t    sg_shadow_tx_tokens;
	uint32_t    sg_raw_tx_tokens;
	uint32_t    sg_act_tx_tokens;
	uint32_t    sg_atfdata_logged;
	uint32_t    sg_atfindex;
	uint32_t    sg_borrowedtokens;
	uint32_t    sg_contributedtokens;
	uint32_t    sg_unusedtokenpercent[5];
	uint32_t    sg_flag;
	uint16_t    sg_atf_debug_mask;
	uint16_t    sg_atf_debug_id;
	uint16_t    sg_allowed_bufs;
	uint16_t    sg_num_buf_held;
	uint16_t    sg_num_bufs_sent;
	uint16_t    sg_num_bytes_sent;
	uint16_t    sg_max_num_buf_held;
	uint16_t    sg_min_num_buf_held;
	uint16_t    sg_pkt_drop_nobuf;
	uint8_t     sg_allowed_buf_updated;
	uint8_t     sg_atfborrow;
	uint32_t    sg_contribute_id;
	uint32_t    sg_contribute_level;
	uint32_t    sg_atftidstate;
	qdf_spinlock_t  atf_sg_lock;
	struct  atf_stats  sg_atf_stats;
	struct  atf_stats  *sg_atf_debug;
};

/**
 * struct atf_ac_config - ATF AC config parameters
 * @ac:             Airtime corresponding to each AC ID
 * @ac_bitmap:      Bitmap to indicate configured ACs
 * @ac_cfg_flag:    Flag to indicate AC configuration
 */
struct atf_ac_config {
	uint32_t    ac[WME_NUM_AC][1]; /* Airtime corresponding to each AC id*/
	uint8_t     ac_bitmap;      /* Bitmap to indicate configured ACs */
	uint8_t     ac_cfg_flag;    /* Flag to indicate AC configuration */
};

/**
 * struct atf_group_list - ATF group list element
 * @group_next:                      List of ATF group instances
 * @group_name:                      Group name
 * @atf_num_sg_borrow:               Number of subgroups looking to borrow tokens
 * @atf_num_clients:                 Number of clients in the group
 * @atf_num_active_sg:               Number of subgroups active in the group
 * @atf_contributabletokens:         Tokens available for contribution
 * @shadow_atf_contributabletokens:  Tokens available for contribution
 * @group_del:                       Indicates this group is to be deleted
 * @group_airtime:                   User configured airtime for the group
 * @group_unused_airtime:            Unused airtime within an SSID
 * @subgroup_count:                  No. of subgroups in this group
 * @num_subgroup:                    No. of subgroups in this group
 * @tot_airtime_peer:                Total airtime assigned to all peer subgroups within a group
 * @tot_airtime_ac:                  Total airtime assigned to all ac subgroups withing a group
 * @group_sched:                     Group scheduling policy
 * @atf_subgroups:                   Subgroup list
 */
struct atf_group_list {
	qdf_list_node_t  group_next;
	uint8_t          group_name[WLAN_SSID_MAX_LEN + 1];
	uint32_t         atf_num_sg_borrow;
	uint32_t         atf_num_clients;
	uint32_t         atf_num_active_sg;
	uint32_t         atf_contributabletokens;
	uint32_t         shadow_atf_contributabletokens;
	uint8_t          group_del;
	uint32_t         group_airtime;
	uint32_t         group_unused_airtime;
	uint32_t         subgroup_count;
	uint32_t         num_subgroup;
	uint32_t         tot_airtime_peer;
	uint32_t         tot_airtime_ac;
	uint32_t         group_sched;
	qdf_list_t       atf_subgroups;
};

/**
 * struct atf_config - ATF configuration structure
 * @struct peer_id:  Peer details
 * @    cfg_flag:         Peer config
 * @    sta_cfg_value:    User config % for sta
 * @    sta_cfg_mark:     Label sta cfg from user input until re-update table
 *                        to decide if remove this mark or remove this entry
 * @    index_vdev:       Peer associated vdev index [1-16],
 *                        not vdev configurated 0xff, empty entry 0
 * @    sta_mac:          Station mac address
 * @    sta_cal_value:    Result of calulate
 *                        (sta_cfg_value[atfcfg_set.peer_id[j].index_vdev]
 *                        *vap_cfg_value)/100
 * @    sta_assoc_status: Sta association status,
 *                        1: associated, 0:No-assoc if sta is configed
 * @    index_group:      Index of group to which peer is linked.
 *                        '0XFF' if peer is not part of any atfgroup
 * @    sta_cal_ac_value: Airtime calculated for ac configured STA
 * @struct vdev:     Total Vdevs
 * @    vdev_cfg_value:   User config % for ssid
 * @    essid:            SSID name
 * @    cfg_flag:         Vdev Config
 * @    atf_cfg_ac:       AC configuration for ssid
 * @struct atfgroup: Group details
 * @    grpname:          group name
 * @    grp_num_ssid:     Number of ssids added in this group
 * @    grp_sched_policy: Group schedule policy
 * @    grp_cfg_value:    Airtime for this group
 * @    grp_ssid:         List of SSIDs in the group
 * @    atf_cfg_ac:       AC configuration for group
 * @    grplist_entry:    Group list entry
 * @grp_num_cfg:     Total number of groups configured
 * @peer_num_cfg:    User config how many stas
 * @peer_num_cal:    Total stas need to be pass down fm
 * @peer_cal_bitmap: Bitmap for those stas of average val
 * @vdev_num_cfg:    User config how many vdev
 * @percentage_unit: default 1000, rang 100 or 1000
 */
struct atf_config {
	struct {
		uint8_t    cfg_flag;
		uint32_t   sta_cfg_value[ATF_CFG_NUM_VDEV + 1];
		uint8_t    sta_cfg_mark;
		uint8_t    index_vdev;
		uint8_t    sta_mac[QDF_MAC_ADDR_SIZE];
		uint32_t   sta_cal_value;
		uint8_t    sta_assoc_status;
		uint8_t    index_group;
		uint32_t   sta_cal_ac_value[WME_NUM_AC];
	} peer_id[ATF_ACTIVED_MAX_CLIENTS];
	struct {
		uint32_t   vdev_cfg_value;
		uint8_t    essid[WLAN_SSID_MAX_LEN+1];
		uint8_t    cfg_flag;
		struct     atf_ac_config atf_cfg_ac;
	} vdev[ATF_CFG_NUM_VDEV];
	struct {
		uint8_t    grpname[WLAN_SSID_MAX_LEN + 1];
		uint32_t   grp_num_ssid;
		uint32_t   grp_sched_policy;
		uint32_t   grp_cfg_value;
		uint8_t    grp_ssid[ATF_CFG_NUM_VDEV]
					[WLAN_SSID_MAX_LEN + 1];
		struct     atf_ac_config atf_cfg_ac;
		struct     atf_group_list *grplist_entry;
	} atfgroup[ATF_ACTIVED_MAX_ATFGROUPS];
	uint8_t    grp_num_cfg;
	uint8_t    peer_num_cfg;
	uint8_t    peer_num_cal;
	uint64_t   peer_cal_bitmap;
	uint8_t    vdev_num_cfg;
	uint32_t   percentage_unit;
};

/**
 * struct atf_tput_table - ATF throughput table
 * @mac_addr: MAC address
 * @airtime:  Airtime allocated to the peer
 * @order:    Order of the entry
 * @tput:     Allocated throughput
 */
struct atf_tput_table {
	uint8_t    mac_addr[QDF_MAC_ADDR_SIZE];
	uint8_t    airtime;
	uint8_t    order;
	uint32_t   tput;
};

/**
 * struct peer_atf - peer specific atf object
 * @atf_peer_stats:            Statistics
 * @atfcapable:                atf capable client
 * @ac_list_ptr:               Pointer to atf subgroup per AC
 * @atf_group:                 List of atf group
 * @block_tx_traffic:          Block data traffic tx for this node
 * @atf_airtime_cap:           Configured max airtime for the node
 * @atf_airtime_configured:    Dynamic airtime configured for this node
 * @atf_tput:                  Configured throughput for the node
 * @atf_airtime:               Airtime that will be alloted to a node
 * @atf_airtime_new:           Temp value to hold the airtime estimated
 * @atf_airtime_more:          Temp value to hold the difference of the
 *                             above two
 * @atf_airtime_subseq:        Sum of airtimes of all nodes which have
 *                             lower priority
 * @throughput:                Possible UDP DL BE throughput for this node,
 *                             from ratectrl
 * @atf_token_allocated:       ATF token allocated
 * @atf_token_utilized:        ATF token utilized
 * @block_tx_bitmap:           Bit map indicating the AC blocked
 * @peer_obj:                  Reference to peer global object
 */
struct peer_atf {
	uint32_t                atf_units;
	uint32_t                tx_tokens;
	uint32_t                shadow_tx_tokens;
	uint32_t                raw_tx_tokens;
	uint32_t                atfdata_logged;
	uint32_t                atfindex;
	uint32_t                borrowedtokens;
	uint32_t                contributedtokens;
	uint32_t                unusedtokenpercent[5];
	uint32_t                atfborrow;
	struct atf_stats        atf_peer_stats;
	struct atf_stats        *atf_debug;
	uint16_t                atf_debug_mask;
	uint16_t                atf_debug_id;
	uint8_t                 atfcapable;
	struct atf_subgroup_list *ac_list_ptr[WME_NUM_AC];
	struct atf_group_list   *atf_group;
	uint8_t                 block_tx_traffic;
	uint8_t                 atf_airtime_cap;
	uint8_t                 atf_airtime_configured:1;
	uint32_t                atf_tput;
	uint16_t                atf_airtime;
	uint16_t                atf_airtime_new;
	uint16_t                atf_airtime_more;
	uint16_t                atf_airtime_subseq;
	uint32_t                throughput;
	uint16_t                atf_token_allocated;
	uint16_t                atf_token_utilized;
	uint8_t                 block_tx_bitmap;
	uint8_t                 ac_blk_cnt;
	struct wlan_objmgr_peer *peer_obj;
};

/**
 * struct vdev_atf - vdev specific atf object
 * @vdev_atf_sched:    Per ssid atf scheduling policy
 * @block_tx_traffic:  Block data traffic tx for this vap
 * @tx_blk_cnt:        Number of peers under this vap blocked to transmit
 * @ac_blk_cnt;        Number of ACs under this vap blocked to transmit
 * @vdev_obj:          Reference to vdev global object
 */
struct vdev_atf {
	uint8_t                 vdev_atf_sched:2,
	                        block_tx_traffic:1;
	uint8_t                 tx_blk_cnt;
	uint8_t                 ac_blk_cnt;
	struct wlan_objmgr_vdev *vdev_obj;
};

/**
 * struct pdev_atf - radio specific atf object
 * @atf_commit:                      1- atf enable, 0- atf disable
 * @atf_tput_based:                  Throughput based airtime allocation
 * @atf_tput_tbl_num:                Number of entries in throughput table
 * @atf_tput_order_max:              Track the maximum order for tput table
 * @atf_txbuf_share:                 Indicate Tx Buffer share enable/disable
 * @atf_resv_airtime:                Reserved airtime for a Radio
 * @atf_logging:                     ATF logging enable/disable
 * @atf_txbuf_max:                   Maximum number of Tx buffers configured
 * @atf_txbuf_min:                   Minimum number of Tx buffers configured
 * @atf_airtime_override:            Airtime to be overridden
 * @atf_tput_tbl:                    ATF throughput table
 * @atfcfg_set:                      ATF configurations
 * @atf_req:                         ATF request reference
 * @atf_peer_req:                    ATF peer request reference
 * @atf_group_req:                   ATF group request reference
 * @bwf_req:                         Bandwith fairness request reference
 * @atfcfg_timer:                    Timer to update atf table on STA join/leave
 * @atf_tasksched:                   Flag to indicate update task scheduled
 * @atf_lock:                        Lock to synchronise atf table updates and
 *                                   tasksched flag updates
 * @atf_tokenalloc_timer:            Timer to periodically updates node txtokens
 * @alloted_tx_tokens:               Sum of txtoken's distributed in a cycle
 * @shadow_alloted_tx_tokens:        Shadow of sum of txtoken's distributed
 *                                   in a cycle
 * @atf_sched:                       Enable/Disable strict and OBSS scheduling
 * @atf_chbusy:                      Channel busy stats collected every atf
 *                                   timer interval
 * @atf_avail_tokens:                Actual available tokens based on OBSS
 *                                   interference
 * @txtokens_common:                 Common tokens given for clients > 32
 * @atf_maxclient:                   Enable/disable max client support
 * @atf_ssidgroup:                   Enable/disable ATF ssid grouping support
 * @atf_obss_scale:                  Scaling % to add to avail tokens
 * @atf_groups_borrow:               Flag to indicate if there are groups
 *                                   looking to borrow tokens
 * @atf_tokens_unassigned:           Unassigned tx tokens
 * @atf_total_num_clients_borrow:    Total clients looking to borrow,
 *                                   across all groups
 * @atf_num_clients_borrow_strict:   Total clients looking to borrow,
 *                                   across all groups strict sched
 * @atf_num_clients_borrow_fair_ub:  Total clients looking to borrow,
 *                                   across all groups fair ub
 * @atf_num_active_sg:               Total no. of active subgroups
 * @atf_num_active_sg_strict:        Total no. of active subgroups strict sched
 * @atf_num_active_sg_fair_ub:       Total no. of active subgroups fair ub
 * @atf_total_contributable_tokens:  Sum of contributable tokens of all groups
 * @atf_contrlevel_incr:             No. of consecutive iterations
 *                                   after which the contribution level increments.
 * @atf_groups:                      List of ATF group instances
 * @atf_num_clients:                 Maximum number of clients allowed to
 *                                   associate in ATF enabled case
 * @pdev_obj:                        Reference to pdev global object
 *
 * Internal use only
 * @stop_looping:                    While peer travarsal default itrator
 *                                   function doesn't support partial traversal.
 *                                   This flag used to stop processing further.
 */
struct pdev_atf {
	uint8_t                          atf_commit;
	uint16_t                         atf_tput_based:1,
					 atf_tput_tbl_num:7,
					 atf_tput_order_max:7;
	uint8_t                          atf_txbuf_share;
	uint8_t                          atf_resv_airtime;
	uint8_t                          atf_logging;
	uint16_t                         atf_txbuf_max;
	uint16_t                         atf_txbuf_min;
	uint16_t                         atf_airtime_override;
	struct atf_tput_table            atf_tput_tbl[ATF_TPUT_MAX_STA];
	struct atf_config                atfcfg_set;
	struct pdev_atf_req              atf_req;
	struct pdev_atf_peer_ext_request atf_peer_req;
	struct pdev_atf_ssid_group_req   atf_group_req;
	struct pdev_atf_group_wmm_ac_req atf_group_ac_req;
	struct pdev_bwf_req              bwf_req;
	qdf_timer_t                      atfcfg_timer;
	uint8_t                          atf_tasksched;
	qdf_spinlock_t                   atf_lock;
	qdf_timer_t                      atf_tokenalloc_timer;
	uint32_t                         alloted_tx_tokens;
	uint32_t                         shadow_alloted_tx_tokens;
	uint32_t                         atf_sched;
	uint32_t                         atf_chbusy;
	uint32_t                         atf_avail_tokens;
	uint32_t                         txtokens_common;
	uint8_t                          atf_maxclient;
	uint8_t                          atf_ssidgroup;
	uint32_t                         atf_obss_scale;
	uint32_t                         atf_groups_borrow;
	uint32_t                         atf_tokens_unassigned;
	uint32_t                         atf_total_num_clients_borrow;
	uint32_t                         atf_num_active_sg;
	uint32_t                         atf_num_active_sg_strict;
	uint32_t                         atf_num_active_sg_fair_ub;
	uint32_t                         atf_num_sg_borrow_strict;
	uint32_t                         atf_num_sg_borrow_fair_ub;
	uint32_t                         atf_total_contributable_tokens;
	uint32_t                         atf_contr_level_incr;
	qdf_list_t                       atf_groups;
	uint32_t                         atf_num_clients;
	struct wlan_objmgr_pdev          *pdev_obj;

	uint8_t                          stop_looping;
};

/**
 * struct atf_context : ATF global context
 * Members used in Distribution phases
 * @numclients:                      Number of clients
 * @configured_airtime:              Configured airtime
 * @airtime:                         Airtime
 * @associated_sta:                  Number of associated STAs
 * @airtime_limit:                   Airtime limit
 * @airtime_resv:                    Airtime reserved
 * @configured_sta:                  Configured STA count
 * @atf_dist_state:                  Enum value to track Distribution phases
 *
 * @atf_mode:                        Indicate atf_mode module parameter value
 * @atf_fmcap:                       Indicate firmware capabile of ATF
 * @atf_msdu_desc:                   Number of descriptors configured
 * @atf_peers:                       Number of peers configured
 * @atf_max_vdevs:                   Number of vdevs configured
 * @psoc_obj:                        Reference to psoc global object
 *
 * Call back funtions to invoke independent of OL/DA
 * @atf_reset_vdev:                  Reset all vedevs in pdev
 * @atf_pdev_atf_init:               Initialize pdev_atf object
 * @atf_pdev_atf_deinit:             De-initialize pdev_atf object
 * @atf_set:                         Set atf enable
 * @atf_clear:                       Set atf disable
 * @atf_set_maxclient:               Set maxclient feature enable/disable
 * @atf_get_maxclient:               Get maxclient feature enable/disable
 * @atf_set_group_unused_airtime:    Set unused airtime to group
 * @atf_update_group_tbl:            Update group table
 * @atf_update_tbl_to_fm:            Update table to firmware
 * @atf_build_bwf_for_fm:            Build bandwidth fairness for firmware
 * @atf_get_dynamic_enable_disable:  Get dynamic Enable/Disable supported or not
 * @atf_open:                        ATF component open function
 * @atf_enable:                      ATF component enable function
 * @atf_disable:                     ATF component disable function
 */
struct atf_context {
	uint32_t numclients;
	uint32_t configured_airtime;
	uint32_t airtime;
	uint32_t associated_sta;
	uint32_t airtime_limit;
	uint32_t airtime_resv;
	uint32_t configured_sta;
	ATF_AIRTIME_DISTRIBUTION atf_dist_state;

	uint8_t  atf_mode;
	uint32_t atf_fmcap;
	uint32_t atf_msdu_desc;
	uint32_t atf_peers;
	uint32_t atf_max_vdevs;
	struct wlan_objmgr_psoc *psoc_obj;

	void (*atf_reset_vdev)(struct wlan_objmgr_pdev *pdev);
	void (*atf_pdev_atf_init)(struct wlan_objmgr_pdev *pdev,
				  struct pdev_atf *pa);
	void (*atf_pdev_atf_deinit)(struct wlan_objmgr_pdev *pdev,
				    struct pdev_atf *pa);
	int32_t (*atf_set)(struct wlan_objmgr_vdev *vdev);
	int32_t (*atf_clear)(struct wlan_objmgr_vdev *vdev);
	int32_t (*atf_set_maxclient)(struct wlan_objmgr_pdev *pdev,
				     uint8_t enable);
	int32_t (*atf_get_maxclient)(struct wlan_objmgr_pdev *pdev);
	void (*atf_set_group_unused_airtime)(struct pdev_atf *pa);
	void (*atf_update_group_tbl)(struct pdev_atf *pa, uint8_t stacnt,
				     uint8_t index, uint32_t vdev_per_unit);
	int32_t (*atf_update_tbl_to_fm)(struct wlan_objmgr_pdev *pdev);
	int32_t (*atf_build_bwf_for_fm)(struct wlan_objmgr_pdev *pdev);
	int32_t (*atf_get_dynamic_enable_disable)(struct pdev_atf *pa);
	void (*atf_open)(struct wlan_objmgr_psoc *psoc);
	void (*atf_enable)(struct wlan_objmgr_psoc *psoc);
	void (*atf_disable)(struct wlan_objmgr_psoc *psoc);
};

#endif /* _ATF_DEFS_I_H_*/
