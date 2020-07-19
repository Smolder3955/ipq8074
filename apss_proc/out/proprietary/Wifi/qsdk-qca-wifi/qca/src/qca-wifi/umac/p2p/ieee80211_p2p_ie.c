/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#include "ieee80211_var.h"
#include "ieee80211_p2p_proto.h"
#include "ieee80211P2P_api.h"
#include "ieee80211_ie_utils.h"
#include "ieee80211_p2p_ie.h"

#if UMAC_SUPPORT_P2P

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

u_int8_t *ieee80211_p2p_add_p2p_ie(u_int8_t *frm)
{
    u_int8_t  wfa_oui[3] = IEEE80211_P2P_WFA_OUI;
    struct ieee80211_p2p_ie *p2pie=(struct ieee80211_p2p_ie *) frm;

    p2pie->p2p_id = IEEE80211_P2P_IE_ID;
    p2pie->p2p_oui[0] = wfa_oui[0];
    p2pie->p2p_oui[1] = wfa_oui[1];
    p2pie->p2p_oui[2] = wfa_oui[2];
    p2pie->p2p_oui_type = IEEE80211_P2P_WFA_VER;
    p2pie->p2p_len=4;
    return (frm + sizeof(struct ieee80211_p2p_ie));
}

/*
 * Add the NOA IE to the frame
 * The tsf_offset parameter is used to skew the start times.
 */
u_int8_t *ieee80211_p2p_add_noa(
    u_int8_t *frm, 
    struct ieee80211_p2p_sub_element_noa *noa, 
    u_int32_t tsf_offset)
{
    u_int8_t    tmp_octet;
    int         i;

    *frm++ = IEEE80211_P2P_SUB_ELEMENT_NOA;     /* sub-element id */
    ASSERT(noa->num_descriptors <= IEEE80211_MAX_NOA_DESCRIPTORS);

    /*
     * Length = (2 octets for Index and CTWin/Opp PS) and
     * (13 octets for each NOA Descriptors)
     */
#if MAV_P2P_SUBTYPE
    *frm=0x1; /* Maverick subtype Version */
    frm++;
    *frm=IEEE80211_NOA_IE_SIZE(noa->num_descriptors);
#else  /* MAV_P2P_SUBTYPE */
    P2PIE_PUT_LE16(frm, IEEE80211_NOA_IE_SIZE(noa->num_descriptors));
    frm++;
#endif  /* MAV_P2P_SUBTYPE */
    frm++;

    *frm++ = noa->index;        /* Instance Index */

    tmp_octet = noa->ctwindow & IEEE80211_P2P_NOE_IE_CTWIN_MASK;
    if (noa->oppPS) {
        tmp_octet |= IEEE80211_P2P_NOE_IE_OPP_PS_SET;
    }
    *frm++ = tmp_octet;         /* Opp Ps and CTWin capabilities */

    for (i = 0; i < noa->num_descriptors; i++) {
        ASSERT(noa->noa_descriptors[i].type_count != 0);

        *frm++ = noa->noa_descriptors[i].type_count;
        
        P2PIE_PUT_LE32(frm, noa->noa_descriptors[i].duration);
        frm += 4;
        P2PIE_PUT_LE32(frm, noa->noa_descriptors[i].interval);
        frm += 4;
        P2PIE_PUT_LE32(frm, noa->noa_descriptors[i].start_time + tsf_offset);
        frm += 4;          
    }

    return frm;
}

int ieee80211_p2p_parse_noa(const u_int8_t *frm, u_int8_t num_noa_descriptors, struct ieee80211_p2p_sub_element_noa *noa)
{
    u_int8_t i;

    noa->index = *frm;
    ++frm;

    noa->oppPS = ((*frm) & (u_int8_t)IEEE80211_P2P_NOE_IE_OPP_PS_SET) ? 1 : 0;
    noa->ctwindow = ((*frm) & IEEE80211_P2P_NOE_IE_CTWIN_MASK);
    ++frm;

    noa->num_descriptors = num_noa_descriptors;

    for (i=0;i<num_noa_descriptors;++i) {
        noa->noa_descriptors[i].type_count = *frm;
        ++frm;
        noa->noa_descriptors[i].duration = le32toh(*((const u_int32_t*)frm));
        frm += 4;
        noa->noa_descriptors[i].interval = le32toh(*((const u_int32_t*)frm));
        frm += 4;
        noa->noa_descriptors[i].start_time = le32toh(*((const u_int32_t*)frm));
        frm += 4;
    }
    return EOK;
}

int ieee80211_p2p_parse_capability(const u_int8_t *frm, u_int8_t *dev_capab, u_int8_t *group_capab)
{
    *dev_capab = *frm;          /* Device Capabilities */
    ++frm;
    *group_capab = *frm;        /* Group Capabilities */
    return EOK;
}

int ieee80211_p2p_parse_group_info(const u8 *gi, size_t gi_len,
             struct p2p_group_info *info)
{
    const u8 *g, *gend;

    OS_MEMSET(info, 0, sizeof(*info));
    if (gi == NULL)
        return 0;

    g = gi;
    gend = gi + gi_len;
    while (g < gend) {
        struct p2p_client_info *cli;
        const u8 *t, *cend;
        int count;

        cli = &info->client[info->num_clients];
        cend = g + 1 + g[0];
        if (cend > gend)
            return -1; /* invalid data */
        /* g at start of P2P Client Info Descriptor */
        /* t at Device Capability Bitmap */
        t = g + 1 + 2 * ETH_ALEN;
        if (t > cend)
            return -1; /* invalid data */
        cli->p2p_device_addr = g + 1;
        cli->p2p_interface_addr = g + 1 + ETH_ALEN;
        cli->dev_capab = t[0];

        if (t + 1 + 2 + 8 + 1 > cend)
            return -1; /* invalid data */

        cli->config_methods = P2PIE_GET_BE16(&t[1]);
        cli->pri_dev_type = &t[3];

        t += 1 + 2 + 8;
        /* t at Number of Secondary Device Types */
        cli->num_sec_dev_types = *t++;
        if (t + 8 * cli->num_sec_dev_types > cend)
            return -1; /* invalid data */
        cli->sec_dev_types = t;
        t += 8 * cli->num_sec_dev_types;

        /* t at Device Name in WPS TLV format */
        if (t + 2 + 2 > cend)
            return -1; /* invalid data */
        if (P2PIE_GET_BE16(t) != ATTR_DEV_NAME)
            return -1; /* invalid Device Name TLV */
        t += 2;
        count = P2PIE_GET_BE16(t);
        t += 2;
        if (count > cend - t)
            return -1; /* invalid Device Name TLV */
        if (count >= 32)
            count = 32;
        cli->dev_name = (const char *) t;
        cli->dev_name_len = count;

        g = cend;

        info->num_clients++;
        if (info->num_clients == P2P_MAX_GROUP_ENTRIES)
            return -1;
    }

    return 0;
}

static inline struct iebuf *
ieee802_11_vendor_ie_concat(osdev_t oshandle, const u8 *ies, size_t ies_len,
                            const u8 *oui_vendor, u8 oui_type)
{
    struct iebuf *buf;
    const u8 *end, *pos, *ie;

    if ((ies == NULL) || (ies_len < 2))
        return NULL;
    pos = ies;
    end = ies + ies_len;
    ie = NULL;

    while (pos + 1 < end) {
        if (pos + 2 + pos[1] > end)
            return NULL;
        if (pos[0] == IEEE80211_P2P_SUB_ELEMENT_VENDOR &&
            pos[1] >= 4 &&
            pos[2] == oui_vendor[0] &&
            pos[3] == oui_vendor[1] &&
            pos[4] == oui_vendor[2] &&
            pos[5] == oui_type)
        {
            ie = pos;
            break;
        }
        pos += 2 + pos[1];
    }

    if (ie == NULL)
        return NULL; /* No specified vendor IE found */

    buf = iebuf_alloc(oshandle, ies_len);
    if (buf == NULL)
        return NULL;

    /*
     * There may be multiple vendor IEs in the message, so need to
     * concatenate their data fields.
     */
    while (pos + 1 < end) {
        if (pos + 2 + pos[1] > end)
            break;
        if (pos[0] == IEEE80211_P2P_SUB_ELEMENT_VENDOR &&
            pos[1] >= 4 &&
            pos[2] == oui_vendor[0] &&
            pos[3] == oui_vendor[1] &&
            pos[4] == oui_vendor[2] &&
            pos[5] == oui_type)
        {
            iebuf_put_data(buf, pos + 6, pos[1] - 4);
        }
        pos += 2 + pos[1];
    }

    return buf;
}

static int
ieee80211_p2p_parse_subelement(wlan_if_t vap, u8 id, const u8 *data, u16 len,
                struct p2p_parsed_ie *msg)
{
    const u8 *pos;
    size_t i, nlen;

    switch (id) {
    case IEEE80211_P2P_SUB_ELEMENT_STATUS:
        if (len < 1) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Status "
                   "subelement (length %d)\n", __func__, len);
            return -1;
        }
        msg->status = data;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Status: %d\n", __func__, data[0]);
        break;
    /* case IEEE80211_P2P_SUB_ELEMENT_MINOR_REASON: */
    case IEEE80211_P2P_SUB_ELEMENT_CAPABILITY:
        if (len < 2) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Capability "
                   "subelement (length %d)\n", __func__, len);
            return -1;
        }
        msg->capability = data;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Device Capability %02x "
               "Group Capability %02x\n", __func__, data[0], data[1]);
        break;
    case IEEE80211_P2P_SUB_ELEMENT_DEVICE_ID:
        if (len < IEEE80211_ADDR_LEN) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Device ID length (!=6). "
                   "subelement length %d\n", __func__, len);
            return -1;
        }
        msg->device_id = data;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Device ID " MACSTR,
               __func__, MAC2STR(msg->device_id));
        break;
    case IEEE80211_P2P_SUB_ELEMENT_GO_INTENT:
        if (len < 1) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short GO Intent "
                   "subelement length %d\n", __func__, len);
            return -1;
        }
        msg->go_intent = data;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * GO Intent: Intent %u "
               "Tie breaker %u\n", __func__, data[0] >> 1, data[0] & 0x01);
        break;
    case IEEE80211_P2P_SUB_ELEMENT_CONFIGURATION_TIMEOUT:
        if (len < 2) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Configuration "
                   "Timeout subelement (length %d)\n", __func__, len);
            return -1;
        }
        msg->config_timeout = data;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Configuration Timeout\n",
                          __func__);
        break;
    case IEEE80211_P2P_SUB_ELEMENT_LISTEN_CHANNEL:
        if (len == 0) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * listen Channel: Ignore null "
                              "channel (len %d)\n", __func__, len);
            break;
        }
        if (len < 2) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Listen Channel "
                   "subelement (length %d)\n", __func__, len);
            return -1;
        }
        msg->listen_channel = data;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Listen Channel: Country=%x %x %x, Regulatory Class %d "
               "Channel Number %d\n", __func__, data[0], data[1], data[2], data[3], data[4]);
        break;
    case IEEE80211_P2P_SUB_ELEMENT_OP_CHANNEL:
        if (len == 0) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Operational Channel: Ignore null "
                              "channel (len %d)\n", __func__, len);
            break;
        }
        if (len < 2) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Op Channel "
                   "subelement (length %d)\n", __func__, len);
            return -1;
        }
        msg->operating_channel = data;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Op Channel: Country=%x %x %x, Regulatory Class %d "
               "Channel Number %d\n", __func__, data[0], data[1], data[2], data[3], data[4]);
        break;
    /* case IEEE80211_P2P_SUB_ELEMENT_GROUP_BSSID: */
    /* case IEEE80211_P2P_SUB_ELEMENT_EXTENDED_LISTEN_TIMING: */
    case IEEE80211_P2P_SUB_ELEMENT_INTENDED_INTERFACE_ADDR:
        if (len < IEEE80211_ADDR_LEN) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Intended P2P "
                   "Interface Address subelement (length %d)\n", __func__,
                   len);
            return -1;
        }
        msg->intended_addr = data;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Intended P2P Interface Address: "
                          MACSTR, __func__,  MAC2STR(msg->intended_addr));
        break;
    /* case IEEE80211_P2P_SUB_ELEMENT_MANAGEABILITY: */
    case IEEE80211_P2P_SUB_ELEMENT_CHANNEL_LIST:
        if (len < 3) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Channel List "
                   "subelement (length %d)\n", __func__, len);
            return -1;
        }
        msg->channel_list = data;
        msg->channel_list_len = len;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Channel List: Country String "
               "'%c%c%c'\n", __func__, data[0], data[1], data[2]);
        break;
    case IEEE80211_P2P_SUB_ELEMENT_NOA:
        if ((len < 2) || ((len - 2) % IEEE80211_P2P_NOA_DESCRIPTOR_LEN) != 0 )  {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid NOA "
                   "subelement size (length %d)\n", __func__, len);
            return -1;
        }
        len = (len - 2)/IEEE80211_P2P_NOA_DESCRIPTOR_LEN;
        if (len > IEEE80211_MAX_NOA_DESCRIPTORS) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid NOA "
                   "subelement too large (length %d)\n", __func__, len);
            return -1;
        }
        msg->noa = data;
        msg->noa_num_descriptors = len;
        break;
    case IEEE80211_P2P_SUB_ELEMENT_DEVICE_INFO:
        if (len < IEEE80211_ADDR_LEN + 2 + 8 + 1) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Too short Device Info "
                   "subelement (length %d)\n", __func__, len);
            return -1;
        }
        msg->p2p_device_info = data;
        msg->p2p_device_info_len = len;
        pos = data;
        msg->p2p_device_addr = pos;
        pos += IEEE80211_ADDR_LEN;
        msg->config_methods = P2PIE_GET_BE16(pos);
        pos += 2;
        msg->pri_dev_type = pos;
        pos += 8;
        msg->num_sec_dev_types = *pos++;
        if (msg->num_sec_dev_types * 8 > data + len - pos) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Device Info underflow "
                              "length %d\n", __func__, len);
            return -1;
        }
        pos += msg->num_sec_dev_types * 8;
        if (data + len - pos < 4) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Device Name "
                   "length %d\n", __func__, (int)(data + len - pos));
            return -1;
        }
        if (P2PIE_GET_BE16(pos) != ATTR_DEV_NAME) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Unexpected Device Name "
                              "val %x != ATTR_DEV_NAME %x\n",
                              __func__, P2PIE_GET_BE16(pos), ATTR_DEV_NAME);
            return -1;
        }
        pos += 2;
        nlen = P2PIE_GET_BE16(pos);
        pos += 2;
        if (data + len - pos < (int) nlen || nlen > 32) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Device Name "
                   "length %d (buf len %d)\n", __func__,  (int) nlen,
                   (int) (data + len - pos));
            return -1;
        }
        OS_MEMCPY(msg->device_name, pos, nlen);
        for (i = 0; i < nlen; i++) {
            if (msg->device_name[i] == '\0')
                break;
            if (msg->device_name[i] < 32)
                msg->device_name[i] = '_';
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Device Info: addr " MACSTR
               " primary device type %u-%08X-%u device name '%s' "
               "config methods 0x%x\n", __func__,
               MAC2STR(msg->p2p_device_addr),
               P2PIE_GET_BE16(msg->pri_dev_type),
               P2PIE_GET_BE32(&msg->pri_dev_type[2]),
               P2PIE_GET_BE16(&msg->pri_dev_type[6]),
               msg->device_name, msg->config_methods);
        break;
    case IEEE80211_P2P_SUB_ELEMENT_GROUP_INFO:
        msg->group_info = data;
        msg->group_info_len = len;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Group Info "
                          "length %d\n", __func__, len);
        break;
    case IEEE80211_P2P_SUB_ELEMENT_GROUP_ID:
        if ((len < IEEE80211_ADDR_LEN) || (len > IEEE80211_ADDR_LEN + 32)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: P2P: Invalid P2P Group ID, len=%d\n",
                              __func__, len);
            return -1;
        }
        msg->group_id = data;
        msg->group_id_len = len;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * P2P Group ID: Device Address " MACSTR "\n",
               __func__, MAC2STR(msg->group_id));
        break;
    case IEEE80211_P2P_SUB_ELEMENT_INVITATION_FLAGS:
        if (len < 1) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT,
                              "%s: * P2P Too short Invitation Flag attribute (length %d)\n",
                              __func__, len);
            return -1;
        }
        msg->invitation_flags = data;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT,
                          "%s: P2P: * Invitation Flags: bitmap 0x%x\n",
                          __func__, data[0]);
        break;
    /* case IEEE80211_P2P_SUB_ELEMENT_INTERFACE: */
    /* case IEEE80211_P2P_SUB_ELEMENT_VENDOR: */
    default:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Skipped unknown subelement %d "
               "(length %d)\n", __func__, id, len);
        break;
    }

    return 0;
}


/**
 * ieee80211_p2p_parse_p2p_ie - Parse P2P IE
 * @vap: handle for IEEE80211_DPRINTF
 * @buf: Concatenated P2P IE(s) payload
 * @msg: Buffer for returning parsed attributes
 * Returns: 0 on success, -1 on failure
 *
 * Note: Caller is responsible for clearing the msg data structure before
 * calling this function.
 */
static int
ieee80211_p2p_parse_p2p_ie(wlan_if_t vap, const struct iebuf *buf,
                           struct p2p_parsed_ie *msg)
{
    const u8 *pos = iebuf_head_u8(buf);
    const u8 *end = pos + iebuf_len(buf);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Parsing P2P IE pos %pK end %pK\n",
                      __func__, pos, end);

    while (pos < end) {
        u16 attr_len;
        if (pos + 2 >= end) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid P2P subelement "
                              "pos + 2 %pK >= end %pK\n", __func__, pos+2, end);
            return -1;
        }
#if MAV_P2P_SUBTYPE
        if(pos[1] != 0x01){
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Maverick P2P version"
                                "version[%d] != 0x01\n", __func__, pos[1]);
            return -1;
        }
        
        attr_len = pos[2];
#else  /* MAV_P2P_SUBTYPE */
        attr_len = P2PIE_GET_LE16(pos + 1);
#endif  /* MAV_P2P_SUBTYPE */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Subelement %d length %u\n",
               __func__, pos[0], attr_len);
        if (pos + 3 + attr_len > end) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Subelement underflow "
                   "(len=%u left=%d)\n", __func__,
                   attr_len, (int) (end - pos - 3));
            return -1;
        }
        if (ieee80211_p2p_parse_subelement(vap, pos[0], pos + 3, attr_len, msg))
            return -1;
        pos += 3 + attr_len;
    }

    return 0;
}



static int
wps_set_attr(wlan_if_t vap, struct wps_parsed_attr *attr, u16 type,
            const u8 *pos, u16 len)
{
    switch (type) {
    case ATTR_CONFIG_METHODS:
        if (len != 2) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Config Methods "
                   "length %u\n", __func__, len);
            return -1;
        }
        attr->config_methods = pos;
        break;
    case ATTR_DEV_NAME:
        attr->dev_name = pos;
        attr->dev_name_len = len;
        break;
	case ATTR_VERSION:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Version "
				   "length %u", __func__, len);
			return -1;
		}
		attr->version = pos;
		break;
	case ATTR_MSG_TYPE:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Message Type "
				   "length %u", __func__, len);
			return -1;
		}
		attr->msg_type = pos;
		break;
	case ATTR_ENROLLEE_NONCE:
		if (len != WPS_NONCE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Enrollee Nonce "
				   "length %u", __func__, len);
			return -1;
		}
		attr->enrollee_nonce = pos;
		break;
	case ATTR_REGISTRAR_NONCE:
		if (len != WPS_NONCE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Registrar Nonce "
				   "length %u", __func__, len);
			return -1;
		}
		attr->registrar_nonce = pos;
		break;
	case ATTR_UUID_E:
		if (len != WPS_UUID_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid UUID-E "
				   "length %u", __func__, len);
			return -1;
		}
		attr->uuid_e = pos;
		break;
	case ATTR_UUID_R:
		if (len != WPS_UUID_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid UUID-R "
				   "length %u", __func__, len);
			return -1;
		}
		attr->uuid_r = pos;
		break;
	case ATTR_AUTH_TYPE_FLAGS:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Authentication "
				   "Type Flags length %u", __func__, len);
			return -1;
		}
		attr->auth_type_flags = pos;
		break;
	case ATTR_ENCR_TYPE_FLAGS:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Encryption Type "
				   "Flags length %u", __func__, len);
			return -1;
		}
		attr->encr_type_flags = pos;
		break;
	case ATTR_CONN_TYPE_FLAGS:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Connection Type "
				   "Flags length %u", __func__, len);
			return -1;
		}
		attr->conn_type_flags = pos;
		break;
	case ATTR_SELECTED_REGISTRAR_CONFIG_METHODS:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Selected "
				   "Registrar Config Methods length %u", __func__, len);
			return -1;
		}
		attr->sel_reg_config_methods = pos;
		break;
	case ATTR_PRIMARY_DEV_TYPE:
		if (len != WPS_DEV_TYPE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Primary Device "
				   "Type length %u", __func__, len);
			return -1;
		}
		attr->primary_dev_type = pos;
		break;
	case ATTR_RF_BANDS:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid RF Bands length "
				   "%u", __func__, len);
			return -1;
		}
		attr->rf_bands = pos;
		break;
	case ATTR_ASSOC_STATE:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Association State "
				   "length %u", __func__, len);
			return -1;
		}
		attr->assoc_state = pos;
		break;
	case ATTR_CONFIG_ERROR:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Configuration "
				   "Error length %u", __func__, len);
			return -1;
		}
		attr->config_error = pos;
		break;
	case ATTR_DEV_PASSWORD_ID:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Device Password "
				   "ID length %u", __func__, len);
			return -1;
		}
		attr->dev_password_id = pos;
		break;
	case ATTR_OOB_DEVICE_PASSWORD:
		if (len != WPS_OOB_DEVICE_PASSWORD_ATTR_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid OOB Device "
				   "Password length %u", __func__, len);
			return -1;
		}
		attr->oob_dev_password = pos;
		break;
	case ATTR_OS_VERSION:
		if (len != 4) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid OS Version length "
				   "%u", __func__, len);
			return -1;
		}
		attr->os_version = pos;
		break;
	case ATTR_WPS_STATE:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Wi-Fi Protected "
				   "Setup State length %u", __func__, len);
			return -1;
		}
		attr->wps_state = pos;
		break;
	case ATTR_AUTHENTICATOR:
		if (len != WPS_AUTHENTICATOR_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Authenticator "
				   "length %u", __func__, len);
			return -1;
		}
		attr->authenticator = pos;
		break;
	case ATTR_R_HASH1:
		if (len != WPS_HASH_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid R-Hash1 length %u",
				   __func__, len);
			return -1;
		}
		attr->r_hash1 = pos;
		break;
	case ATTR_R_HASH2:
		if (len != WPS_HASH_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid R-Hash2 length %u",
				   __func__, len);
			return -1;
		}
		attr->r_hash2 = pos;
		break;
	case ATTR_E_HASH1:
		if (len != WPS_HASH_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid E-Hash1 length %u",
				   __func__, len);
			return -1;
		}
		attr->e_hash1 = pos;
		break;
	case ATTR_E_HASH2:
		if (len != WPS_HASH_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid E-Hash2 length %u",
				   __func__, len);
			return -1;
		}
		attr->e_hash2 = pos;
		break;
	case ATTR_R_SNONCE1:
		if (len != WPS_SECRET_NONCE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid R-SNonce1 length "
				   "%u", __func__, len);
			return -1;
		}
		attr->r_snonce1 = pos;
		break;
	case ATTR_R_SNONCE2:
		if (len != WPS_SECRET_NONCE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid R-SNonce2 length "
				   "%u", __func__, len);
			return -1;
		}
		attr->r_snonce2 = pos;
		break;
	case ATTR_E_SNONCE1:
		if (len != WPS_SECRET_NONCE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid E-SNonce1 length "
				   "%u", __func__, len);
			return -1;
		}
		attr->e_snonce1 = pos;
		break;
	case ATTR_E_SNONCE2:
		if (len != WPS_SECRET_NONCE_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid E-SNonce2 length "
				   "%u", __func__, len);
			return -1;
		}
		attr->e_snonce2 = pos;
		break;
	case ATTR_KEY_WRAP_AUTH:
		if (len != WPS_KWA_LEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Key Wrap "
				   "Authenticator length %u", __func__, len);
			return -1;
		}
		attr->key_wrap_auth = pos;
		break;
	case ATTR_AUTH_TYPE:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Authentication "
				   "Type length %u", __func__, len);
			return -1;
		}
		attr->auth_type = pos;
		break;
	case ATTR_ENCR_TYPE:
		if (len != 2) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Encryption "
				   "Type length %u", __func__, len);
			return -1;
		}
		attr->encr_type = pos;
		break;
	case ATTR_NETWORK_INDEX:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Network Index "
				   "length %u", __func__, len);
			return -1;
		}
		attr->network_idx = pos;
		break;
	case ATTR_NETWORK_KEY_INDEX:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Network Key Index "
				   "length %u", __func__, len);
			return -1;
		}
		attr->network_key_idx = pos;
		break;
	case ATTR_MAC_ADDR:
		if (len != ETH_ALEN) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid MAC Address "
				   "length %u", len);
			return -1;
		}
		attr->mac_addr = pos;
		break;
	case ATTR_KEY_PROVIDED_AUTO:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Key Provided "
				   "Automatically length %u", __func__, len);
			return -1;
		}
		attr->key_prov_auto = pos;
		break;
	case ATTR_802_1X_ENABLED:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid 802.1X Enabled "
				   "length %u", __func__, len);
			return -1;
		}
		attr->dot1x_enabled = pos;
		break;
    case ATTR_SELECTED_REGISTRAR:
        if (len != 1) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Selected Registrar"
                    " length %u\n", __func__, len);
            return -1;
        }
        attr->selected_registrar = pos;
        break;
	case ATTR_REQUEST_TYPE:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Request Type "
				   "length %u", __func__, len);
			return -1;
		}
		attr->request_type = pos;
		break;
	case ATTR_RESPONSE_TYPE:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Response Type "
				   "length %u", __func__, len);
			return -1;
		}
		attr->response_type = pos;
		break;
	case ATTR_MANUFACTURER:
		attr->manufacturer = pos;
		attr->manufacturer_len = len;
		break;
	case ATTR_MODEL_NAME:
		attr->model_name = pos;
		attr->model_name_len = len;
		break;
	case ATTR_MODEL_NUMBER:
		attr->model_number = pos;
		attr->model_number_len = len;
		break;
	case ATTR_SERIAL_NUMBER:
		attr->serial_number = pos;
		attr->serial_number_len = len;
		break;      
	case ATTR_PUBLIC_KEY:
		attr->public_key = pos;
		attr->public_key_len = len;
		break;
	case ATTR_ENCR_SETTINGS:
		attr->encr_settings = pos;
		attr->encr_settings_len = len;
		break;
	case ATTR_CRED:
		if (attr->num_cred >= MAX_CRED_COUNT) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Skipped Credential "
				   "attribute (max %d credentials)", __func__, 
				   MAX_CRED_COUNT);
			break;
		}
		attr->cred[attr->num_cred] = pos;
		attr->cred_len[attr->num_cred] = len;
		attr->num_cred++;
		break;
	case ATTR_SSID:
		attr->ssid = pos;
		attr->ssid_len = len;
		break;
	case ATTR_NETWORK_KEY:
		attr->network_key = pos;
		attr->network_key_len = len;
		break;
	case ATTR_EAP_TYPE:
		attr->eap_type = pos;
		attr->eap_type_len = len;
		break;
	case ATTR_EAP_IDENTITY:
		attr->eap_identity = pos;
		attr->eap_identity_len = len;
		break;
	case ATTR_AP_SETUP_LOCKED:
		if (len != 1) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid AP Setup Locked "
				   "length %u", __func__, len);
			return -1;
		}
		attr->ap_setup_locked = pos;
		break;
	case ATTR_REQUESTED_DEV_TYPE:
        if (len != IEEE80211_P2P_DEVICE_TYPE_LEN) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid Requested Device Type "
                   "length %u\n", __func__, len);
            return -1;
        }
		if (attr->num_req_dev_type >= MAX_REQ_DEV_TYPE_COUNT) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Skipped Requested Device "
				   "Type attribute (max %u types)", __func__, 
				   MAX_REQ_DEV_TYPE_COUNT);
			break;
		}
		attr->req_dev_type[attr->num_req_dev_type] = pos;
		attr->num_req_dev_type++;
		break;
	case ATTR_VENDOR_EXT:
		break;        

	default:
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT,
               "%s: Unsupported attribute type 0x%x "
               "len=%u\n", __func__, __func__, type, len);
        break;
    }

    return 0;
}


static int
wps_parse_msg(wlan_if_t vap, const struct iebuf *msg, struct wps_parsed_attr *attr)
{
    const u8 *pos, *end;
    u16 type, len;

    OS_MEMZERO(attr, sizeof(*attr));
    pos = iebuf_head(msg);
    end = pos + iebuf_len(msg);

    while (pos < end) {
        if (end - pos < 4) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Invalid message - "
                   "%lu bytes remaining\n", __func__, (unsigned long) (end - pos));
            return -1;
        }

        type = P2PIE_GET_BE16(pos);
        pos += 2;
        len = P2PIE_GET_BE16(pos);
        pos += 2;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: attr type=0x%x len=%u\n",
               __func__, type, len);

        if (len > end - pos) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Attribute overflow\n", __func__);
            return -1;
        }

        if (wps_set_attr(vap, attr, type, pos, len) < 0)
            return -1;

        pos += len;
    }

    return 0;
}

static int
ieee80211_p2p_parse_wps_ie(wlan_if_t vap, const struct iebuf *buf, struct p2p_parsed_ie *msg)
{
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Parsing WPS IE\n", __func__);

	if (wps_parse_msg(vap, buf, &msg->wps_attributes_parsed))
		return -1;

	if (msg->wps_attributes_parsed.dev_name && msg->wps_attributes_parsed.dev_name_len < sizeof(msg->device_name) &&
	    !msg->device_name[0])
		OS_MEMCPY(msg->device_name, msg->wps_attributes_parsed.dev_name, msg->wps_attributes_parsed.dev_name_len);
	else if (msg->wps_attributes_parsed.config_methods)
		msg->config_methods = P2PIE_GET_BE16(msg->wps_attributes_parsed.config_methods);

    if (msg->wps_attributes_parsed.selected_registrar != NULL) {
        msg->wps_selected_registrar = *(msg->wps_attributes_parsed.selected_registrar);
        msg->wps_selected_registrar_present = true;
    }
    else {
        msg->wps_selected_registrar_present = false;
    }

	return 0;

}

/**
 * ieee80211_p2p_parse_free - Free temporary data from P2P parsing
 * @msg: Parsed attributes
 */
void
ieee80211_p2p_parse_free(struct p2p_parsed_ie *msg)
{
    if (msg) {
        iebuf_free(msg->p2p_attributes);
        msg->p2p_attributes = NULL;

        iebuf_free(msg->wps_attributes);

        msg->wps_attributes = NULL;
    }
}

/**
 * ieee80211_p2p_parse_ies - Parse P2P message IEs (both WPS and P2P IE)
 * @osdev: handle for malloc
 * @vap: handle for IEEE80211_DPRINTF
 * @data: IEs from the message
 * @len: Length of data buffer in octets
 * @msg: Buffer for returning parsed attributes
 * Returns: 0 on success, -1 on failure
 *
 * Note: Caller is responsible for clearing the msg data structure before
 * calling this function.
 *
 * Note: Caller must free temporary memory allocations by calling
 * ieee80211_p2p_parse_free() when the parsed data is not needed anymore.
 */
int
ieee80211_p2p_parse_ies(osdev_t oshandle, wlan_if_t vap,
                        const u8 *data, size_t len, struct p2p_parsed_ie *msg)
{

    struct iebuf                *buf;
    const u8                    *ssid;
    const u8                    *frm, *efrm;
    struct ieee80211_rateset    *rs = &(msg->rateset);
    int                         i;
    const u_int8_t              wfa_oui[3] = IEEE80211_P2P_WFA_OUI;
    const u_int8_t              wsc_oui[3] = IEEE80211_WSC_OUI;
    if (msg == NULL)
        return -1;

    /* Check the SSID and collect the rates */
    ssid = NULL;
    rs->rs_nrates = 0;
    frm = data;
    efrm = frm + len;
    while (((frm+1) < efrm) && (frm + frm[1] + 1 < efrm)) {

        switch (*frm) {
        case IEEE80211_ELEMID_RATES:
        case IEEE80211_ELEMID_XRATES:
            for (i = 0; i < frm[1]; i++) {
                if (rs->rs_nrates == IEEE80211_RATE_MAXSIZE)
                    break;
                rs->rs_rates[rs->rs_nrates] = frm[1 + i] & IEEE80211_RATE_VAL;
                rs->rs_nrates++;
            }
            break;
        case IEEE80211_ELEMID_SSID:
            ssid = frm;
            break;
        }

        frm += frm[1] + 2;
    }

    if (frm > efrm) {
        printk("%s: Err: IE's buf misaligned\n", __func__);
    }

    if (ssid) {
        /* Check if SSID is a wildcard P2P IE */
        msg->ssid = ssid;

        if ((ssid[1] == IEEE80211_P2P_WILDCARD_SSID_LEN) &&
            (OS_MEMCMP(ssid + 2, IEEE80211_P2P_WILDCARD_SSID, IEEE80211_P2P_WILDCARD_SSID_LEN) == 0))
        {
            msg->is_p2p_wildcard_ssid = 1;
        }
    }

    buf = ieee802_11_vendor_ie_concat(oshandle, data, len, wsc_oui, IEEE80211_P2P_WPS_VER);
    if (buf) {
        if (ieee80211_p2p_parse_wps_ie(vap, buf, msg)) {
            iebuf_free(buf);
            ieee80211_p2p_parse_free(msg);
            return -1;
	}
        iebuf_free(buf);
    }

    msg->p2p_attributes = ieee802_11_vendor_ie_concat(oshandle, data, len, wfa_oui,
                              IEEE80211_P2P_WFA_VER);
	if (msg->p2p_attributes &&
        ieee80211_p2p_parse_p2p_ie(vap, msg->p2p_attributes, msg) != 0){
		ieee80211_p2p_parse_free(msg);
		return -1;
	}

    return 0;
}

/**
 * ieee80211_p2p_parse - Parse a P2P Action frame contents
 * @data: Action frame payload after Category and Code fields
 * @len: Length of data buffer in octets
 * @msg: Buffer for returning parsed attributes
 * Returns: 0 on success, -1 on failure
 *
 * Note: Caller must free temporary memory allocations by calling
 * ieee80211_p2p_parse_free() when the parsed data is not needed anymore.
 */
int ieee80211_p2p_parse(osdev_t oshandle, wlan_if_t vap, const u8 *data, size_t len, struct p2p_parsed_ie *msg)
{
    OS_MEMZERO(msg, sizeof(*msg));
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: Parsing the received message\n", __func__);
    if (len < 1) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: No Dialog Token in the message\n", __func__);
        return -1;
    }
    msg->dialog_token = data[0];
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT, "%s: * Dialog Token: %d\n", __func__,  msg->dialog_token);

    return ieee80211_p2p_parse_ies(oshandle, vap, data + 1, len - 1, msg);
}

/* Init. the P2P Buffer structure */
void p2pbuf_init(p2pbuf *ies, u_int8_t *ie_buf, int max_len)
{
    ies->start = ie_buf;        /* from of IE */
    ASSERT(max_len >= 2);
    ies->remainder = max_len;   /* reminding empty space */
    ies->curr = ie_buf;         /* pointer to write next byte */
}

/* Add the P2P IE. Returns the pointer to len field so that it can updated later. */
u_int8_t *p2pbuf_add_ie_hdr(p2pbuf *ies)
{
    const u_int8_t  wfa_oui[3] = IEEE80211_P2P_WFA_OUI;
    u_int8_t        *len;

    p2pbuf_put_u8(ies, IEEE80211_P2P_IE_ID);
    len = p2pbuf_put(ies, 1); /* IE length to be filled */
    p2pbuf_put_u8(ies, wfa_oui[0]);
    p2pbuf_put_u8(ies, wfa_oui[1]);
    p2pbuf_put_u8(ies, wfa_oui[2]);
    p2pbuf_put_u8(ies, IEEE80211_P2P_WFA_VER);
    return len;
}

/* Update the length field of this IE */
void p2pbuf_update_ie_hdr(p2pbuf *ies, u_int8_t *ielen_ptr, int *ie_len)
{
    int len;

    //TODO: support multiple P2P IE's so that its length can be > 256
    ASSERT(ielen_ptr != NULL);
    ASSERT(ielen_ptr <= ies->curr);
    len =  ies->curr - ielen_ptr - 1;
    ASSERT(len <= 0x000FF);
    *ielen_ptr = len;
    if (ie_len) {
        *ie_len = len;
    }
}

/* Add the P2P Device ID attribute */
void p2pbuf_add_device_id(p2pbuf *ies, const u_int8_t *dev_addr)
{
    /* P2P Device ID */
    p2pbuf_put_u8(ies, IEEE80211_P2P_SUB_ELEMENT_DEVICE_ID);
    p2pbuf_put_le16(ies, IEEE80211_ADDR_LEN);
    p2pbuf_put_data(ies, dev_addr, IEEE80211_ADDR_LEN);
}

void p2pbuf_add_capability(p2pbuf *buf, u_int8_t dev_capab, u_int8_t group_capab)
{
    /* P2P Capability */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_CAPABILITY);
    p2pbuf_put_le16(buf, 2);            /* attribute length*/
    p2pbuf_put_u8(buf, dev_capab);      /* Device Capabilities */
    p2pbuf_put_u8(buf, group_capab);    /* Group Capabilities */
}

void p2pbuf_add_status(p2pbuf *buf, u8 status)
{
    /* Status */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_STATUS);
    p2pbuf_put_le16(buf, 1);
    p2pbuf_put_u8(buf, status);
}

void p2pbuf_add_group_id(p2pbuf *buf, const u8 *dev_addr,
              const u8 *ssid, u_int16_t ssid_len)
{
    /* P2P Group ID */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_GROUP_ID);
    p2pbuf_put_le16(buf, IEEE80211_ADDR_LEN + ssid_len);
    p2pbuf_put_data(buf, dev_addr, IEEE80211_ADDR_LEN);
    p2pbuf_put_data(buf, ssid, ssid_len);
}

void p2pbuf_add_listen_channel(
    p2pbuf *buf, const u_int8_t *country, u_int8_t reg_class, u_int8_t channel)
{
    /* Listen Channel */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_LISTEN_CHANNEL);
    p2pbuf_put_le16(buf, 5);
    p2pbuf_put_data(buf, country, 3);
    p2pbuf_put_u8(buf, reg_class);      /* Regulatory Class */
    p2pbuf_put_u8(buf, channel);        /* Channel Number */
}

void p2pbuf_add_ext_listen_timing(p2pbuf *buf, u_int16_t period, u_int16_t interval)
{
    /* Extended Listen Timing */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_EXTENDED_LISTEN_TIMING);
    p2pbuf_put_le16(buf, 4);
    p2pbuf_put_le16(buf, period);
    p2pbuf_put_le16(buf, interval);
}

void p2pbuf_add_operating_channel(p2pbuf *buf, const u_int8_t *country,
                                   u_int8_t reg_class, u_int8_t channel)
{
    /* Operating Channel */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_OP_CHANNEL);
    p2pbuf_put_le16(buf, 5);
    p2pbuf_put_data(buf, country, 3);
    p2pbuf_put_u8(buf, reg_class); /* Regulatory Class */
    p2pbuf_put_u8(buf, channel); /* Channel Number */
}

/* Create the P2P Device Info attribute */
void p2pbuf_add_device_info(
    p2pbuf      *buf, 
    u_int8_t    *dev_addr,          /* device MAC address (6 bytes) */
    u_int8_t    *p2p_dev_name,      /* device name string */
    u_int16_t   p2p_dev_name_len,   /* device name length */
    u_int16_t   config_methods,     /* WPS config methods */
    u_int8_t    *pri_dev_type,      /* primary device type (8 bytes) */
    u_int8_t    num_sec_dev_types,  /* number of secondary device type */
    u_int8_t    *sec_dev_types      /* secondary device types */
    )
{
    u_int8_t    *len;
    u_int16_t   attrib_len;

    /* P2P Device Info */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_DEVICE_INFO);
    len = p2pbuf_get_bufptr(buf);   /* remember the location of the attribute length */
    p2pbuf_put_le16(buf, 2);        /* IE length to be filled */

    /* P2P Device address */
    p2pbuf_put_data(buf, dev_addr, IEEE80211_ADDR_LEN);

    /* Config Methods */
    p2pbuf_put_be16(buf, config_methods);

    /* Primary Device Type */
    p2pbuf_put_data(buf, pri_dev_type, IEEE80211_P2P_DEVICE_TYPE_LEN);

    /* Number of Secondary Device Types */
    p2pbuf_put_u8(buf, num_sec_dev_types);

    /* Secondary Device Type List */
    if (num_sec_dev_types > 0) {
        ASSERT(sec_dev_types != NULL);
        p2pbuf_put_data(buf, sec_dev_types, 
                        IEEE80211_P2P_DEVICE_TYPE_LEN * num_sec_dev_types);
    }

    /* Device Name */
    p2pbuf_put_be16(buf, ATTR_DEV_NAME);
    p2pbuf_put_be16(buf, p2p_dev_name_len);
    p2pbuf_put_data(buf, p2p_dev_name, p2p_dev_name_len);

    /* Update attribute length */
    attrib_len = p2pbuf_get_bufptr(buf) - len - 2;
    P2PIE_PUT_LE16(len, attrib_len);
}

void p2pbuf_add_public_action_hdr(p2pbuf *buf, u_int8_t subtype, u_int8_t dialog_token)
{
    p2pbuf_put_u8(buf, WLAN_ACTION_PUBLIC);
    p2pbuf_put_u8(buf, WLAN_PA_VENDOR_SPECIFIC);
    p2pbuf_put_be24(buf, OUI_WFA);
    p2pbuf_put_u8(buf, P2P_OUI_TYPE);
    
    p2pbuf_put_u8(buf, subtype); /* OUI Subtype */
    p2pbuf_put_u8(buf, dialog_token);
}

void p2pbuf_add_go_intent(p2pbuf *buf, u_int8_t go_intent)
{
    /* Group Owner Intent */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_GO_INTENT);
    p2pbuf_put_le16(buf, 1);
    p2pbuf_put_u8(buf, go_intent);
}

void p2pbuf_add_config_timeout(p2pbuf *buf, u_int8_t go_timeout, u_int8_t client_timeout)
{
    /* Configuration Timeout */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_CONFIGURATION_TIMEOUT);
    p2pbuf_put_le16(buf, 2);
    p2pbuf_put_u8(buf, go_timeout);
    p2pbuf_put_u8(buf, client_timeout);
}

void p2pbuf_add_invitation_flags(p2pbuf *buf, u8 flags)
{
    /* Invitation Flags */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_INVITATION_FLAGS);
    p2pbuf_put_le16(buf, 1);
    p2pbuf_put_u8(buf, flags);
}

void p2pbuf_add_group_bssid(p2pbuf *buf, const u8 *bssid)
{
    /* P2P Group BSSID */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_GROUP_BSSID);
    p2pbuf_put_le16(buf, IEEE80211_ADDR_LEN);
    p2pbuf_put_data(buf, bssid, IEEE80211_ADDR_LEN);
}

void p2pbuf_add_intended_addr(p2pbuf *buf, const u_int8_t *interface_addr)
{
    /* Intended P2P Interface Address */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_INTENDED_INTERFACE_ADDR);
    p2pbuf_put_le16(buf, IEEE80211_ADDR_LEN);
    p2pbuf_put_data(buf, interface_addr, IEEE80211_ADDR_LEN);
}

void p2pbuf_add_channel_list(p2pbuf *buf, const u_int8_t *country,
                             struct p2p_channels *chan)
{
    u_int8_t    *len;
    size_t      i;
    
    /* Channel List */
    p2pbuf_put_u8(buf, IEEE80211_P2P_SUB_ELEMENT_CHANNEL_LIST);
    len = p2pbuf_put(buf, 2); /* IE length to be filled */
    p2pbuf_put_data(buf, country, 3); /* Country String */
    
    for (i = 0; i < chan->reg_classes; i++) {
        struct p2p_reg_class *c = &chan->reg_class[i];
        p2pbuf_put_u8(buf, c->reg_class);
        p2pbuf_put_u8(buf, c->channels);
        p2pbuf_put_data(buf, c->channel, c->channels);
    }
    
    /* Update attribute length */
    P2PIE_PUT_LE16(len, (u_int8_t *)p2pbuf_put(buf, 0) - len - 2);
}

/*
 * Parse the Channel List attribute to extract the country code and channel list.
 */
int
wlan_p2p_parse_channel_list(wlan_if_t vap, const u8 *channel_list, size_t channel_list_len,
                            struct p2p_channels *rx_ch_list, u_int8_t *rx_country)
{
    const u8    *pos, *end;
	size_t      channels;

    OS_MEMZERO(rx_ch_list, sizeof(struct p2p_channels));
    pos = channel_list;
    end = channel_list + channel_list_len;

    if (end - pos < 3) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT,
                          "%s: P2P: Error: no country code; too short\n", __func__);
        return -1;
    }

    OS_MEMCPY(rx_country, pos, 3);

    pos += 3;
    
    while (pos + 2 < end) {
        struct p2p_reg_class *cl = &rx_ch_list->reg_class[rx_ch_list->reg_classes];
        cl->reg_class = *pos++;
        if (pos + 1 + pos[0] > end) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT,
                              "%s: P2P: Invalid peer Channel List\n", __func__);
            return -1;
        }
        channels = *pos++;
        cl->channels = channels > P2P_MAX_REG_CLASS_CHANNELS ?
                        P2P_MAX_REG_CLASS_CHANNELS : (u_int8_t)channels;
        OS_MEMCPY(cl->channel, pos, cl->channels);
        pos += channels;
        rx_ch_list->reg_classes++;
        if (rx_ch_list->reg_classes == P2P_MAX_REG_CLASSES)
            break;
    }

    return 0;
}

/*
 * Parse the Group ID attribute to extract the Device ID and SSID.
 */
int
wlan_p2p_parse_group_id(wlan_if_t vap, const u8 *group_id, size_t group_id_len,
                        const u8 **dev_addr, const u8 **ssid, u_int16_t *ssid_len)
{
    if (group_id_len < IEEE80211_ADDR_LEN) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_INPUT,
                          "%s: P2P: Error: group ID len=%d too short\n", __func__, group_id_len);
        return -1;
    }
    *dev_addr = group_id;
    *ssid_len = (u_int16_t)group_id_len - IEEE80211_ADDR_LEN;
    if (*ssid_len) {
        *ssid = &group_id[IEEE80211_ADDR_LEN];
    }
    else {
        *ssid = NULL;
    }
    return 0;
}


#endif
