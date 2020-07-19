#ifndef _ACFG_WSUPP_H
#define _ACFG_WSUPP_H

#include "acfg_types.h"
#include "acfg_wsupp_event.h"
#include "acfg_wsupp_if.h"
#include "acfg_wsupp_nw.h"

/**
 * This file defines module level APIs for supplicant
 * configuration.
 */

/**
 * @brief Initialize the wireless supplicant interface sockets.
 *        This routine opens sockets for communications with wpa_supplicant.
 *        This routine can be called multiple times to open either duplicate
 *        communication lines, or for multiple interfaces, which like
 *        p2p groups may be created dynamically. A deleted interface will
 *        leave broken sockets - and so must be uninit(ed) by the user app. \n
 *        Each init(ed) interface also allows one event socket to be
 *        opened where events may be selected for and read when available.
 * @param [in] ifname NULL=default, char string for control interface name,
 *        for example "/var/run/wpa_supplicant/wlan1"
 * 
 * @return an opaque handle to the module, NULL on failure
 */
acfg_wsupp_hdl_t* acfg_wsupp_init(const char *ifname);

/**
 * @brief Cleanup any module-level resources. \n
 *        Should be called once for each acfg_wsupp_init call.
 * @param [in] mctx opaque handle to the module
 */
void acfg_wsupp_uninit(acfg_wsupp_hdl_t *mctx);

/**
 * @brief Request a acfg_p2p_find - with optional timeout.
 * @param [in] mctx opaque handle to the module
 * @param [in] timeout 0=use default, otherwise optional timeout in seconds.
 * @param [in] type  social or progressive
 * 
 * The default behavior is to run a single full scan in the beginning and
 * then scan only social channels. 
 * type=social will scan only social channels, i.e., 
 * it skips the initial full scan. 
 * 
 * type=progressive is like the default behavior, 
 * but it will scan through all the channels progressively one channel at
 * the time in the Search state rounds. This will help in finding new
 * groups or groups missed during the initial full scan.
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_find(acfg_wsupp_hdl_t *mctx, int timeout, 
        enum acfg_find_type type);

/**
 * @brief Request a acfg_p2p_connect \n
 * connect to an existing p2p network
 *
 * typical wpa_cli command:
 * 
 * acfg_p2p_connect \<peeraddr> \<pbc|pin|PIN#> [label|display|keypad] [persistent] [join|auth] [go_intent=<0..15>] [freq=<in MHz>] 
 * 
 * Start P2P group formation with a discovered P2P peer. 
 * This includes optional group owner negotiation, group interface setup,
 * provisioning, and establishing data connection.
 * 
 * The \<pbc|pin|PIN#> parameter specifies the WPS provisioning method. \n
 * "pbc" string starts pushbutton method, \n
 * "pin" string start PIN method using an automatically generated PIN 
 * (which will be returned as the command return code), \n
 * PIN# is actually replaced with a string of digits that
 * means that a pre-selected PIN can be used (e.g., 12345670). 
 *  [label|display|keypad] is used with PIN method to specify which PIN is used 
 *      label=PIN from local label, 
 *      display=dynamically generated random PIN from local display, 
 *      keypad=PIN entered from peer device label or display). 
 * 
 * "persistent" parameter can be used to request a persistent group to be formed.
 * 
 * "join" indicates that this is a command to join an existing group as a client.
 * It skips the GO Negotiation part. 
 * This will send a Provision Discovery Request message to the target GO before
 * associating for WPS provisioning.
 * 
 * "auth" is test only and will not be implemented.
 * 
 * "go_intent" can be used to override the default GO Intent for this 
 * GO Negotiation.
 * "freq" can be used to set a forced operating channel 
 * (e.g., freq=2412 to select 2.4 GHz channel 1). 
 * 
 * 
 * @param [in] mctx opaque handle to the module
 * @param [in] mac_addr ptr to array of 6 bytes
 * @param [in] PIN "pbc"|"pin", otherwise ptr to PIN string.
 * @param [in] pintype source of above optional pin
 * @param [in] persistent  true if a persistent connect is required
 * @param [in] join  true if wish to joint an existing group as a client
 * @param [in] go_intent  optional, \<0 not used, else 0 to 15 is legal
 * @param [in] freq  optional, 0 if not forced, else freq in MHZ
 *
 * @return  status of operation
 */
acfg_status_t acfg_p2p_connect(acfg_wsupp_hdl_t *mctx, mac_addr_t mac_addr,
        char *PIN, enum acfg_pin_type pintype, int persistent, int join,
        int go_intent, int freq);

/**
 * @brief Request a acfg_p2p_listen - with optional timeout.
 * @param [in] mctx opaque handle to the module
 * @param [in] timeout 0=use default, otherwise optional timeout in seconds.
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_listen(acfg_wsupp_hdl_t *mctx, int timeout);

/**
 * @brief acfg_p2p_ext_listen [\<period> \<interval>]
 * 
 * Configure Extended Listen Timing. \n If the parameters are zero, this
 * feature is disabled. \n If the parameters are non-zero, Listen State will
 * be entered every interval msec for at least period msec. Both values
 * have acceptable range of 1-65535 (with interval obviously having to be
 * larger than or equal to duration). \n If the P2P module is not idle at the
 * time the Extended Listen Timing timeout occurs, the Listen State
 * operation will be skipped.
 * 
 * The configured values will also be advertised to other P2P Devices. The
 * received values are available in the p2p_peer command output:
 * 
 * ext_listen_period=100 ext_listen_interval=5000
 * 
 * @param [in] mctx opaque handle to the module
 * @param [in] ext_listen_period
 * @param [in] ext_listen_interval
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_ext_listen(acfg_wsupp_hdl_t *mctx,
        int ext_listen_period, int ext_listen_interval);

/**
 * @brief Invite a peer to join a group (e.g., group=p2p1), \n or to reinvoke a
 * persistent group (e.g., persistent=4). \n
 *
 * If the peer device is the GO of the persistent group, the peer parameter
 * is not needed. Otherwise it is used to specify which device to invite. \n
 * go_dev_addr parameter can be used to override the GO device address for
 * Invitation Request should it be not known for some reason (this should
 * not be needed in most cases).
 * 
 * @param [in] mctx opaque handle to the module
 * @param [in] persistent  optional, persistent id or \<0 if not used.
 * @param [in] group name  optional, NULL if not used.
 * @param [in] peer_mac_addr  optional, ptr to ptr of peer mac, NULL if not used.
 * @param [in] go_dev_addr  optional, ptr to ptr of GO mac, NULL if not used.
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_invite(acfg_wsupp_hdl_t *mctx, int persistent,
        const char *group, 
        mac_addr_t peer_mac_addr, mac_addr_t go_dev_addr);


/**
 * @brief Lists the configured networks, including stored information for
 * persistent groups. 
 * 
 * The identifier in this list is used with acfg_p2p_group_add and
 * acfg_p2p_invite to indicate which persistent group is to be reinvoked.
 * 
 * @param [in] mctx opaque handle to the module
 * @param [out] reply - optional returned string for command result
 * @param [in,out] reply_len - in max size reply, out actual reply size
 * 
 * @return  status of operation
 */
acfg_status_t acfg_list_networks(acfg_wsupp_hdl_t *mctx, 
        char *reply, size_t *reply_len);


/**
 * @brief Reject connection attempt from a peer (specified with a device
 * address). This is a mechanism to reject a pending GO Negotiation with a
 * peer and request to automatically block any further connection or
 * discovery of the peer.
 *
 * @param [in] mctx  opaque handle to the module
 * @param [in] peer_mac_addr  peer macaddr to block.
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_reject(acfg_wsupp_hdl_t *mctx, mac_addr_t peer_mac_addr);

/**
 * @brief Stop ongoing P2P device discovery or other operation (connect,
 * listen mode). Like a soft reset.
 * @param [in] mctx opaque handle to the module
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_stop_find(acfg_wsupp_hdl_t *mctx);



/**
 * @brief Flush P2P peer table and state. Like a hard reset.
 * @param [in] mctx opaque handle to the module
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_flush(acfg_wsupp_hdl_t *mctx);

/**
 * @brief acfg_p2p_get_passphrase  Get the passphrase for a group (only
 * available when acting as a GO).  
 * @param [in] mctx opaque handle to the module
 * @param [out] reply - optional returned string for command result
 * @param [in,out] reply_len - in max size reply, out actual reply size
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_get_passphrase(acfg_wsupp_hdl_t *mctx, char *reply, 
        size_t *reply_len);
/**
 * @brief p2p_presence_req [\<duration> \<interval>] [\<duration> \<interval>] \n
 * 
 * Send a P2P Presence Request to the GO (this is only available when
 * acting as a P2P client).  \n
 * 
 * If no duration/interval pairs are given, (set values to zero), the
 * request indicates that this client has no special needs for GO presence. \n
 * 
 * The first parameter pair gives the preferred duration and interval
 * values in microseconds. \n 
 * 
 * If the second pair is included, that indicates
 * which value would be acceptable. \n 
 * 0 for a duration/interval pair means it is not requested.
 * 
 * @param [in] mctx opaque handle to the module
 * @param [in] preferred_duration
 * @param [in] preferred_interval
 * @param [in] acceptable_duration
 * @param [in] acceptable_interval
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_presence_req(acfg_wsupp_hdl_t *mctx, 
        uint32_t preferred_duration, uint32_t preferred_interval,
        uint32_t acceptable_duration, uint32_t acceptable_interval);

/* @brief acfg_p2p_prov_disc \<peer device address> \<display|keypad|pbc> Send
 * P2P provision discovery request to the specified peer. The parameters
 * for this command are the P2P device address of the peer and the desired
 * configuration method. For example, "p2p_prov_disc 02:01:02:03:04:05
 * display" would request the peer to display a PIN for us and
 * "p2p_prov_disc 02:01:02:03:04:05 keypad" would request the peer to enter
 * a PIN that we display..  
 * @param [in] mctx opaque handle to the module
 * @param [in] mac_addr  peer mac.
 * @param [in] type  acfg_prov_disc_type of call.
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_prov_disc(acfg_wsupp_hdl_t *mctx, mac_addr_t mac_addr,
        enum acfg_prov_disc_type type);


/**
 * @brief Schedule a P2P service discovery request. 
 * 
 * The parameters for this command are the device address of the peer
 * device (or 00:00:00:00:00:00 for wildcard query that is sent to every
 * discovered P2P peer that supports service discovery) and P2P Service
 * Query TLV(s) as hexdump.
 * 
 * For example, \n
 * "p2p_serv_disc_req 00:00:00:00:00:00 02000001" \n
 * schedules a request for listing all supported service discovery
 * protocols and requests this to be sent to all discovered peers.
 * 
 * The pending requests are sent during device discovery (see
 * acfg_p2p_find). Only a single pending wildcard query is supported, but
 * there can be multiple pending peer device specific queries (each will be
 * sent in sequence whenever the peer is found). 
 * 
 * This command returns an identifier for the pending query (e.g.,
 * "1f77628") that can be used to cancel the request.
 * 
 * For UPnP, an alternative command format can be used to specify a single
 * query TLV (i.e., a service discovery for a specific UPnP service) The
 * upnp string should just be included in the user supplied blob string.: \n
 * 
 * p2p_serv_disc_req 00:00:00:00:00:00 upnp \<version hex> \<ST: from M-SEARCH> \n
 * 
 * For example: \n
 * p2p_serv_disc_req 00:00:00:00:00:00 upnp 10 urn:schemas-upnp-org:device:InternetGatewayDevice:1
 * 
 * Directed requests will be automatically removed when the specified peer
 * has replied to it.
 * 
 * @param [in] mctx opaque handle to the module
 * @param [in] mac_addr  optional, ptr to peer mac, NULL if wildcard.
 * @param [in] blob my TLV(s)
 * @param [out] query_handle  discovery handle
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_serv_disc_req(acfg_wsupp_hdl_t *mctx, 
        mac_addr_t mac_addr,
        char *blob, int *query_handle);
/**
 * @brief acfg_p2p_serv_disc_cancel_req \<query identifier>  
 * 
 * Cancel a pending P2P service discovery request. 
 * 
 * This command takes a single parameter: identifier for the pending query
 * (the value returned by 
 * 
 * p2p_serv_disc_req, e.g., "p2p_serv_disc_cancel_req 1f77628".
 * 
 * @param [in] mctx opaque handle to the module
 * @param [in] query_handle  service request id
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_serv_disc_cancel_req(acfg_wsupp_hdl_t *mctx, 
        int query_handle);

/**
 * @brief acfg_p2p_serv_disc_resp 
 * 
 * Reply to a service discovery query. This command takes following
 * parameters:
 * 
 * frequency in MHz, 
 * destination address, 
 * dialog token, 
 * response TLV(s). 
 * 
 * The first three parameters are copied from the request event.
 * 
 * For example, 
 * "p2p_serv_disc_resp 2437 02:40:61:c2:f3:b7 1 0300000101".
 * 
 * @param [in] mctx opaque handle to the module
 * @param [in] freqMHz frequency in MHz
 * @param [in] dest_addr destination mac address
 * @param [in] dialog_token
 * @param [in] blob response TLV(s)
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_serv_disc_resp(acfg_wsupp_hdl_t *mctx, int freqMHz,
        mac_addr_t dest_addr, int dialog_token, char *blob);

/**
 * @brief acfg_p2p_service_update Indicate that local services have changed.
 * This is used to increment the P2P service indicator value so that peers
 * know when previously cached information may have changed.

 * @param [in] mctx opaque handle to the module
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_service_update(acfg_wsupp_hdl_t *mctx);

/**
 * @brief acfg_p2p_service_add \n
 * 
 * Add a local UPnP service for internal SD query processing.
 * 
 * upnp Examples:
 * 
 * p2p_service_add upnp 10 uuid:6859dede-8574-59ab-9332-123456789012::upnp:rootdevice \n
 * p2p_service_add upnp 10 uuid:5566d33e-9774-09ab-4822-333456785632::upnp:rootdevice \n
 * p2p_service_add upnp 10 uuid:1122de4e-8574-59ab-9322-333456789044::urn:schemas-upnp-org:service:ContentDirectory:2 \n
 * p2p_service_add upnp 10 uuid:5566d33e-9774-09ab-4822-333456785632::urn:schemas-upnp-org:service:ContentDirectory:2 \n
 * p2p_service_add upnp 10 uuid:6859dede-8574-59ab-9332-123456789012::urn:schemas-upnp-org:device:InternetGatewayDevice:1 \n
 * 
 * Bonjour examples:
 * 
 * # AFP Over TCP (PTR) \n
 * p2p_service_add bonjour 0b5f6166706f766572746370c00c000c01 074578616d706c65c027
 * 
 * # AFP Over TCP (TXT) (RDATA=null) \n
 * p2p_service_add bonjour 076578616d706c650b5f6166706f766572746370c00c001001 00
 * 
 * # IP Printing over TCP (PTR) (RDATA=MyPrinter._ipp._tcp.local.) \n
 * p2p_service_add bonjour 045f697070c00c000c01 094d795072696e746572c027
 * # IP Printing over TCP (TXT) (RDATA=txtvers=1,pdl=application/postscript) \n
 * p2p_service_add bonjour 096d797072696e746572045f697070c00c001001 09747874766572733d311a70646c3d6170706c69636174696f6e2f706f7374736372797074
 * 
 * # Supported Service Type Hash (SSTH) \n
 * p2p_service_add bonjour 000000 <32-byte bitfield as hexdump>
 * (note: see P2P spec Annex E.4 for information on how to construct the bitfield)
 * 
 * @param [in] mctx opaque handle to the module
 * @param [in] type code for whatever semi-proprietary "standard" being supported
 * @param [in] blob string required by named standard.
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_service_add(acfg_wsupp_hdl_t *mctx,
        enum acfg_service_type type, char *blob);

/**
 * @brief acfg_p2p_service_del \n
 * 
 * Remove a local Bonjour/UpNp service from internal SD query processing
 * 
 * @param [in] mctx opaque handle to the module
 * @param [in] type code for whatever semi-proprietary "standard" being supported
 * @param [in] blob string required by named standard.
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_service_del(acfg_wsupp_hdl_t *mctx, 
        enum acfg_service_type type, char *blob);
/**
 * @brief acfg_p2p_service_flush \n
 * 
 * Remove all local services from internal SD query processing. 
 *
 * @param [in] mctx opaque handle to the module
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_service_flush(acfg_wsupp_hdl_t *mctx);

/**
 * @brief Request a acfg_p2p_group_add - create a group with a name
 * @param [in] mctx opaque handle to the module
 * @param [in] freq optional, freq to force, in MHZ or ==0 if not used.
 * @param [in] persistent  optional, != 0 if want to start a new persistent group.
 * @param [in] persist_id  optional, restart an existing group with an id, or \<0 if not used.\n
 *              Note, only one of persistent or persist_id can be used on a call\n
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_group_add(acfg_wsupp_hdl_t *mctx, int freq, 
        int persistent, int persist_id);


/**
 * @brief Request a acfg_p2p_group_remove - remove a group with a iface name
 * @param [in] mctx opaque handle to the module
 * @param [in] name - interface name
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_group_remove(acfg_wsupp_hdl_t *mctx, const char *name);

/**
 * @brief Request a wps call to handle various security requests
 * @param [in] mctx opaque handle to the module
 * @param [in] wps which form of wps request
 * @param [in] params - optional string parameters
 * @param [out] reply - optional returned string for command result
 * @param [in,out] reply_len - in max size reply, out actual reply size
 * 
 * @return  status of operation
 */
acfg_status_t acfg_wps_req(acfg_wsupp_hdl_t *mctx, enum acfg_wps_type wps,
        const char *params,
        char *reply, size_t *reply_len);

/**
 * @brief List P2P Device Addresses of all the P2P peers we know. The
 * optional "discovered parameter" filters out the peers that we have not
 * fully discovered, i.e. which we have only seen in a received Probe
 * Request frame. Must be called multiple times to collect all addrs, one
 * macaddr is returned per call, macaddr == 00:00:00:00:00:00 if peer is
 * not fully discovered.
 * will return "fail" status when no more peers are available.
 * 
 * @param [in] mctx  opaque handle to the module
 * @param [in] discovered  !=0 if want to filter partial peers
 * @param [in] peer_mac_addr  peer macaddr.
 * @param [in] first  1 asking for first entry, 0=asking for next entry
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_peers(acfg_wsupp_hdl_t *mctx, int discovered, int first,
        mac_addr_t peer_mac_addr);

/**
 * @brief Fetch information about a known P2P peer.
 * @param [in] mctx opaque handle to the module
 * @param [in] peer_mac_addr  peer macaddr.
 * @param [out] reply - optional returned string for command result
 * @param [in,out] reply_len - in max size reply, out actual reply size
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_peer(acfg_wsupp_hdl_t *mctx, mac_addr_t peer_mac_addr, 
        char *reply, size_t *reply_len);

/**
 * @brief Request a wifi scan
 * @param [in] mctx opaque handle to the module\n
 *      results are returned with the acfg_bss call \n
 *      You must wait for ACFG_WPA_EVENT_SCAN_RESULTS event before fetching
 *      results with bss id calls.\n
 * 
 * @return  status of operation
 */
acfg_status_t acfg_scan(acfg_wsupp_hdl_t *mctx);

/**
 * @brief Request One complete scan entry, by entry number 1..n
 * @param [in] mctx opaque handle to the module
 * @param [in] entry_id 0 to N for entry number desired, can iterate for all.
 * @param [out] bssid  bssid for this entry.
 * @param [out] freq - operating frequency in MHZ
 * @param [out] beacon_int  Beacon interval in ms.
 * @param [out] capabilities
 * @param [out] qual - signal quality
 * @param [out] noise
 * @param [out] level signal level
 * @param [out] misc string containing everything including 
 *              tsf, ie, flags and ssid \n
 *              returned for this bss\n
 * @param [in,out] misc_len - in max size misc, out actual misc size
 * 
 * @return  status of operation
 * 
 * wpa_cli output sample: \n
 * id=0 \n
 * bssid=00:0b:85:5b:a6:ec \n
 * freq=2462 \n
 * beacon_int=100 \n
 * capabilities=0x0431 \n
 * qual=0 \n
 * noise=0 \n
 * level=55 \n
 * tsf=0000000000000000 \n
 * ie=000a4154482d474c4f42414c010882848b960c12182403010b0504000100000706555320010b142a010232043048606c301a0100000fac020200000fac02000fac040100000fac0128000000851e01008f000f00ff03400061703a35623a61363a653000000000000900002d9606004096001100dd1c0050f20101000050f20202000050f2020050f20401000050f2010000dd050040960305dd050040960b01dd050040961401dd06004096010104 \n
 * flags=[WPA-EAP-TKIP+CCMP][WPA2-EAP-TKIP+CCMP][ESS] \n
 * ssid=ATH-GLOBAL \n
 * 
 */
acfg_status_t acfg_bss(acfg_wsupp_hdl_t *mctx, int entry_id, mac_addr_t bssid,
        int *freq, int *beacon_int, int *capabilities, int *qual, int *noise, 
        int *level, char *misc, size_t *misc_len);

/**
 * @brief set some p2p parameter at run time.
 * @param [in] mctx opaque handle to the module
 * @param [in] type to be set in supplicant
 * @param [in] val - optional integer value if type demands one
 * @param [in] str - optional string, if type demands one
 * 
 * @return  status of operation
 */
acfg_status_t acfg_p2p_set(acfg_wsupp_hdl_t *mctx, enum acfg_p2pset_type type,
        int val, char *str);

/**
 * @brief set some supplicant parameter at run time.
 * @param [in] mctx opaque handle to the module
 * @param [in] type to be set in supplicant
 * @param [in] val - optional integer value if type demands one
 * @param [in] str - optional string, if type demands one
 * 
 * @return  status of operation
 */
acfg_status_t acfg_set(acfg_wsupp_hdl_t *mctx, enum acfg_set_type type,
        int val, char *str);

#endif

