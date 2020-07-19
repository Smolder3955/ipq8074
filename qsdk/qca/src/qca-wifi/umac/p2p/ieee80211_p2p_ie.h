/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 * P2P Protocol ie operation.
 */

#ifndef IEEE80211_P2P_IE_H
#define IEEE80211_P2P_IE_H

u_int8_t *ieee80211_p2p_add_p2p_ie(u_int8_t *frm);

/*
 * Parse P2P Capability attribute.
 */
int ieee80211_p2p_parse_capability(const u_int8_t *frm, u_int8_t *dev_capab, u_int8_t *group_capab);

/*
* parse NOA sub element.
*/
int ieee80211_p2p_parse_noa(const u_int8_t *frm, u_int8_t len, struct ieee80211_p2p_sub_element_noa *noa);

/*
* Add NOA sub element.
*/
u_int8_t *ieee80211_p2p_add_noa(u_int8_t *frm, struct ieee80211_p2p_sub_element_noa *noa, u_int32_t tsf_offset);

/**
 * struct p2p_parsed_ie - Parsed P2P message (or P2P IE)
 */
struct p2p_parsed_ie {
    struct iebuf *p2p_attributes;

    struct iebuf *wps_attributes;

    struct wps_parsed_attr wps_attributes_parsed;

    u8 dialog_token;

    const u8 *ssid;
    int      is_p2p_wildcard_ssid:1;
    struct ieee80211_rateset rateset;

    const u8 *capability;
    const u8 *go_intent;
    const u8 *status;
    const u8 *listen_channel;
    const u8 *operating_channel;
    //const u8 *channel;
    const u8 *channel_list;
    u8 channel_list_len;
    const u8 *config_timeout;
    const u8 *intended_addr;

    const u8 *group_info;
    size_t group_info_len;

    const u8 *group_id;
    size_t group_id_len;

    const u8 *invitation_flags;

    const u8 *noa;
    u8 noa_num_descriptors;

    const u8 *device_id;

    /* P2P Device Info */
    const u8 *p2p_device_info;
    size_t p2p_device_info_len;
    const u8 *p2p_device_addr;
    const u8 *pri_dev_type;
    u8 num_sec_dev_types;
    char device_name[33];
    u16 config_methods;

    /* From the WPS IE */
    u16 wps_config_methods;
    bool wps_selected_registrar_present;    /* whether the Selected Registrar attribute is present */
    u8 wps_selected_registrar;
    /* Not required for now: char wps_device_name[33]; */
    u8 wps_requested_dev_type[IEEE80211_P2P_DEVICE_TYPE_LEN];
};

#define P2P_MAX_GROUP_ENTRIES 50

struct p2p_group_info {
    unsigned int num_clients;
    struct p2p_client_info {
        const u8 *p2p_device_addr;
        const u8 *p2p_interface_addr;
        u8 dev_capab;
        u16 config_methods;
        const u8 *pri_dev_type;
        u8 num_sec_dev_types;
        const u8 *sec_dev_types;
        const char *dev_name;
        size_t dev_name_len;
    } client[P2P_MAX_GROUP_ENTRIES];
};

int ieee80211_p2p_parse_group_info(const u8 *gi, size_t gi_len, struct p2p_group_info *info);

/*
* Free P2P IE buffer created in ieee80211_p2p_parse_ies
*/
void ieee80211_p2p_parse_free(struct p2p_parsed_ie *msg);

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
int ieee80211_p2p_parse(osdev_t oshandle, wlan_if_t vap, const u8 *data, size_t len, struct p2p_parsed_ie *msg);

/*
* Parse P2P IE elements into p2p_parsed_ie struct
*/
int ieee80211_p2p_parse_ies(osdev_t oshandle, wlan_if_t vap, const u8 *data, size_t len, struct p2p_parsed_ie *msg);


/* Macros for handling unaligned memory accesses */
#define P2PIE_GET_BE16(a) ((u16) (((a)[0] << 8) | (a)[1]))
#define P2PIE_PUT_BE16(a, val)          \
    do {                    \
        (a)[0] = ((u16) (val)) >> 8;    \
        (a)[1] = ((u16) (val)) & 0xff;  \
    } while (0)

#define P2PIE_GET_LE16(a) ((u16) (((a)[1] << 8) | (a)[0]))
#define P2PIE_PUT_LE16(a, val)          \
    do {                    \
        (a)[1] = ((u16) (val)) >> 8;    \
        (a)[0] = ((u16) (val)) & 0xff;  \
    } while (0)

#define P2PIE_GET_BE24(a) ((((u32) (a)[0]) << 16) | (((u32) (a)[1]) << 8) | \
             ((u32) (a)[2]))
#define P2PIE_PUT_BE24(a, val)                  \
    do {                            \
        (a)[0] = (u8) ((((u32) (val)) >> 16) & 0xff);   \
        (a)[1] = (u8) ((((u32) (val)) >> 8) & 0xff);    \
        (a)[2] = (u8) (((u32) (val)) & 0xff);       \
    } while (0)

#define P2PIE_GET_BE32(a) ((((u32) (a)[0]) << 24) | (((u32) (a)[1]) << 16) | \
             (((u32) (a)[2]) << 8) | ((u32) (a)[3]))
#define P2PIE_PUT_BE32(a, val)                  \
    do {                            \
        (a)[0] = (u8) ((((u32) (val)) >> 24) & 0xff);   \
        (a)[1] = (u8) ((((u32) (val)) >> 16) & 0xff);   \
        (a)[2] = (u8) ((((u32) (val)) >> 8) & 0xff);    \
        (a)[3] = (u8) (((u32) (val)) & 0xff);       \
    } while (0)

#define P2PIE_GET_LE32(a) ((((u32) (a)[3]) << 24) | (((u32) (a)[2]) << 16) | \
             (((u32) (a)[1]) << 8) | ((u32) (a)[0]))
#define P2PIE_PUT_LE32(a, val)                  \
    do {                            \
        (a)[3] = (u8) ((((u32) (val)) >> 24) & 0xff);   \
        (a)[2] = (u8) ((((u32) (val)) >> 16) & 0xff);   \
        (a)[1] = (u8) ((((u32) (val)) >> 8) & 0xff);    \
        (a)[0] = (u8) (((u32) (val)) & 0xff);       \
    } while (0)

#define P2PIE_GET_BE64(a) ((((u64) (a)[0]) << 56) | (((u64) (a)[1]) << 48) | \
             (((u64) (a)[2]) << 40) | (((u64) (a)[3]) << 32) | \
             (((u64) (a)[4]) << 24) | (((u64) (a)[5]) << 16) | \
             (((u64) (a)[6]) << 8) | ((u64) (a)[7]))
#define P2PIE_PUT_BE64(a, val)              \
    do {                        \
        (a)[0] = (u8) (((u64) (val)) >> 56);    \
        (a)[1] = (u8) (((u64) (val)) >> 48);    \
        (a)[2] = (u8) (((u64) (val)) >> 40);    \
        (a)[3] = (u8) (((u64) (val)) >> 32);    \
        (a)[4] = (u8) (((u64) (val)) >> 24);    \
        (a)[5] = (u8) (((u64) (val)) >> 16);    \
        (a)[6] = (u8) (((u64) (val)) >> 8); \
        (a)[7] = (u8) (((u64) (val)) & 0xff);   \
    } while (0)

#define P2PIE_GET_LE64(a) ((((u64) (a)[7]) << 56) | (((u64) (a)[6]) << 48) | \
             (((u64) (a)[5]) << 40) | (((u64) (a)[4]) << 32) | \
             (((u64) (a)[3]) << 24) | (((u64) (a)[2]) << 16) | \
             (((u64) (a)[1]) << 8) | ((u64) (a)[0]))

/**
 * P2P_MAX_REG_CLASSES - Maximum number of regulatory classes
 */
#define P2P_MAX_REG_CLASSES         10

/**
 * P2P_MAX_REG_CLASS_CHANNELS - Maximum number of channels per regulatory class
 */
#define P2P_MAX_REG_CLASS_CHANNELS  20

/**
 * struct p2p_channels - List of supported channels
 */
struct p2p_channels {
    /**
    * reg_classes - Number of reg_class entries in use
    */
    size_t reg_classes;

    /**
    * struct p2p_reg_class - Supported regulatory class
    */
    struct p2p_reg_class {
        /**
        * reg_class - Regulatory class (IEEE 802.11-2007, Annex J)
        */
        u_int8_t    reg_class;
        
        /**
        * channel - Supported channels
        */
        u_int8_t    channel[P2P_MAX_REG_CLASS_CHANNELS];
        
        /**
        * channels - Number of channel entries in use
        */
        u_int8_t    channels;
    } reg_class[P2P_MAX_REG_CLASSES];
};

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

/* P2P Capability - Device Capability bitmap */
#define P2P_DEV_CAPAB_SERVICE_DISCOVERY BIT(0)
#define P2P_DEV_CAPAB_CLIENT_DISCOVERABILITY BIT(1)
#define P2P_DEV_CAPAB_CONCURRENT_OPER BIT(2)
#define P2P_DEV_CAPAB_INFRA_MANAGED BIT(3)
#define P2P_DEV_CAPAB_DEVICE_LIMIT BIT(4)
#define P2P_DEV_CAPAB_INVITATION_PROCEDURE BIT(5)

/* P2P Capability - Group Capability bitmap */
#define P2P_GROUP_CAPAB_GROUP_OWNER BIT(0)
#define P2P_GROUP_CAPAB_PERSISTENT_GROUP BIT(1)
#define P2P_GROUP_CAPAB_GROUP_LIMIT BIT(2)
#define P2P_GROUP_CAPAB_INTRA_BSS_DIST BIT(3)
#define P2P_GROUP_CAPAB_CROSS_CONN BIT(4)
#define P2P_GROUP_CAPAB_PERSISTENT_RECONN BIT(5)
#define P2P_GROUP_CAPAB_GROUP_FORMATION BIT(6)

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

/* P2P Capability - Device Capability bitmap */
#define P2P_DEV_CAPAB_SERVICE_DISCOVERY         BIT(0)
#define P2P_DEV_CAPAB_CLIENT_DISCOVERABILITY    BIT(1)
#define P2P_DEV_CAPAB_CONCURRENT_OPER           BIT(2)
#define P2P_DEV_CAPAB_INFRA_MANAGED             BIT(3)
#define P2P_DEV_CAPAB_DEVICE_LIMIT              BIT(4)
#define P2P_DEV_CAPAB_INVITATION_PROCEDURE      BIT(5)

/* P2P Capability - Group Capability bitmap */
#define P2P_GROUP_CAPAB_GROUP_OWNER             BIT(0)
#define P2P_GROUP_CAPAB_PERSISTENT_GROUP        BIT(1)
#define P2P_GROUP_CAPAB_GROUP_LIMIT             BIT(2)
#define P2P_GROUP_CAPAB_INTRA_BSS_DIST          BIT(3)
#define P2P_GROUP_CAPAB_CROSS_CONN              BIT(4)
#define P2P_GROUP_CAPAB_PERSISTENT_RECONN       BIT(5)
#define P2P_GROUP_CAPAB_GROUP_FORMATION         BIT(6)

/* Action frame categories (IEEE 802.11-2007, 7.3.1.11, Table 7-24) */
#define WLAN_ACTION_SPECTRUM_MGMT 0
#define WLAN_ACTION_QOS 1
#define WLAN_ACTION_DLS 2
#define WLAN_ACTION_BLOCK_ACK 3
#define WLAN_ACTION_PUBLIC 4
#define WLAN_ACTION_RADIO_MEASUREMENT 5
#define WLAN_ACTION_FT 6
#define WLAN_ACTION_HT 7
#define WLAN_ACTION_SA_QUERY 8
#define WLAN_ACTION_WMM 17 /* WMM Specification 1.1 */
#define WLAN_ACTION_VENDOR_SPECIFIC 127

/* Public action codes */
#define WLAN_PA_VENDOR_SPECIFIC 9
#define WLAN_PA_GAS_INITIAL_REQ 10
#define WLAN_PA_GAS_INITIAL_RESP 11
#define WLAN_PA_GAS_COMEBACK_REQ 12
#define WLAN_PA_GAS_COMEBACK_RESP 13

#define OUI_WFA 0x506f9a
#define P2P_OUI_TYPE 9

#define P2P_MAX_GO_INTENT 15

typedef struct {
    u_int8_t    *start;     /* from of IE */
    u_int8_t    *curr;      /* pointer to write next byte */
    int         remainder;  /* reminding empty spaces */
} p2pbuf;

/* Move the buffer pointer forward by size len and return the pointer prior to adding */
static inline u_int8_t *p2pbuf_put(p2pbuf *ies, size_t len)
{
    u_int8_t *start = ies->curr;
    ies->curr += len;
    ies->remainder -= (int)len;
    ASSERT(ies->remainder >= 0);

    return start;
}

static inline void p2pbuf_put_u8(p2pbuf *ies, u_int8_t data8)
{
    *(ies->curr) = data8;
    ies->curr++;
    ies->remainder--;
    ASSERT(ies->remainder >= 0);
}

static inline void p2pbuf_put_le16(p2pbuf *ies, u_int16_t data16)
{
    P2PIE_PUT_LE16(ies->curr, data16);
    ies->curr += 2;
    ies->remainder -= 2;
    ASSERT(ies->remainder >= 0);
}

static inline void p2pbuf_put_be16(p2pbuf *ies,  u_int16_t data16)
{
    P2PIE_PUT_BE16(ies->curr, data16);
    ies->curr += 2;
    ies->remainder -= 2;
    ASSERT(ies->remainder >= 0);
}

static inline void p2pbuf_put_be24(p2pbuf *ies,  u_int32_t data24)
{
    P2PIE_PUT_BE24(ies->curr, data24);
    ies->curr += 3;
    ies->remainder -= 3;
    ASSERT(ies->remainder >= 0);
}

static inline void p2pbuf_put_data(p2pbuf *ies, const void *data, size_t len)
{
    if (len == 0) {
        return;
    }
    ASSERT(data);
    OS_MEMCPY(ies->curr, data, len);
    ies->curr += len;
    ies->remainder -= (int)len;
    ASSERT(ies->remainder >= 0);
}

static int inline p2pbuf_tailroom(p2pbuf *ies)
{
    return ies->remainder;
}

static inline u_int8_t *
p2pbuf_get_bufptr(p2pbuf *ies)
{
    return ies->curr;
}

void p2pbuf_init(p2pbuf *ies, u_int8_t *ie_buf, int max_len);

u_int8_t *p2pbuf_add_ie_hdr(p2pbuf *ies);

void p2pbuf_update_ie_hdr(p2pbuf *ies, u_int8_t *ielen_ptr, int *ie_len);

void p2pbuf_add_device_id(p2pbuf *ies, const u_int8_t *dev_addr);

void p2pbuf_add_capability(p2pbuf *buf, u_int8_t dev_capab, u_int8_t group_capab);

void p2pbuf_add_status(p2pbuf *buf, u8 status);

void p2pbuf_add_group_id(p2pbuf *buf, const u8 *dev_addr,
			  const u8 *ssid, u_int16_t ssid_len);

void p2pbuf_add_listen_channel(
    p2pbuf *buf, const u_int8_t *country, u_int8_t reg_class, u_int8_t channel);

void p2pbuf_add_ext_listen_timing(p2pbuf *buf, u_int16_t period, u_int16_t interval);

void p2pbuf_add_operating_channel(p2pbuf *buf, const u_int8_t *country,
                                   u_int8_t reg_class, u_int8_t channel);

void p2pbuf_add_group_bssid(p2pbuf *buf, const u8 *bssid);

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
    );

void p2pbuf_add_public_action_hdr(p2pbuf *buf, u_int8_t subtype, u_int8_t dialog_token);
void p2pbuf_add_go_intent(p2pbuf *buf, u_int8_t go_intent);
void p2pbuf_add_config_timeout(p2pbuf *buf, u_int8_t go_timeout, u_int8_t client_timeout);
void p2pbuf_add_intended_addr(p2pbuf *buf, const u_int8_t *interface_addr);
void p2pbuf_add_channel_list(p2pbuf *buf, const u_int8_t *country,
                             struct p2p_channels *chan);
void p2pbuf_add_invitation_flags(p2pbuf *buf, u8 flags);
/*
 * Parse the Channel List attribute to extract the country code and channel list.
 */
int
wlan_p2p_parse_channel_list(wlan_if_t vap, const u8 *channel_list, size_t channel_list_len,
                            struct p2p_channels *rx_ch_list, u_int8_t *rx_country);

/*
 * Parse the Group ID attribute to extract the Device ID and SSID.
 */
int
wlan_p2p_parse_group_id(wlan_if_t vap, const u8 *group_id, size_t group_id_len,
                        const u8 **dev_addr, const u8 **ssid, u_int16_t *ssid_len);

/*
 * Internal data structure for iebuf. Please do not touch this directly from
 * elsewhere. This is only defined in header file to allow inline functions
 * from this file to access data.
 */
struct iebuf {
    size_t size; /* total size of the allocated buffer */
    size_t used; /* length of data in the buffer */
    /* optionally followed by the allocated buffer */
};

/**
 * iebuf_len - Get the current length of a iebuf buffer data
 * @buf: iebuf buffer
 * Returns: Currently used length of the buffer
 */
static inline size_t
iebuf_len(const struct iebuf *buf)
{
    return buf->used;
}

/**
 * iebuf_head - Get pointer to the head of the buffer data
 * @buf: iebuf buffer
 * Returns: Pointer to the head of the buffer data
 */
static inline const void *
iebuf_head(const struct iebuf *buf)
{
    return buf + 1;
}

static inline const u8 *
iebuf_head_u8(const struct iebuf *buf)
{
    return iebuf_head(buf);
}

/**
 * iebuf_mhead - Get modifiable pointer to the head of the buffer data
 * @buf: iebuf buffer
 * Returns: Pointer to the head of the buffer data
 */
static inline void *
iebuf_mhead(struct iebuf *buf)
{
    return buf + 1;
}

static inline u8 *
iebuf_mhead_u8(struct iebuf *buf)
{
    return iebuf_mhead(buf);
}

static void
iebuf_overflow(const struct iebuf *buf, size_t len)
{
    KASSERT(0, ("iebuf %pK (size=%lu used=%lu) overflow len=%lu",
           buf, (unsigned long)buf->size, (unsigned long)buf->used,
           (unsigned long)len));
}

static inline void *
iebuf_put(struct iebuf *buf, size_t len)
{
    void *tmp = iebuf_mhead_u8(buf) + iebuf_len(buf);
    buf->used += len;
    if (buf->used > buf->size) {
        iebuf_overflow(buf, len);
    }
    return tmp;
}

static inline void
iebuf_put_data(struct iebuf *buf, const void *data, size_t len)
{
    if (data)
        OS_MEMCPY(iebuf_put(buf, len), data, len);
}

static inline struct iebuf *
iebuf_alloc(osdev_t pNicDev, size_t len)
{
    struct iebuf *buf = (struct iebuf *)OS_MALLOC(pNicDev, sizeof(struct iebuf) + len, 0);
    if (buf == NULL)
        return NULL;
    OS_MEMZERO(buf, sizeof(struct iebuf) + len);
    buf->size = len;
    return buf;
}

static inline void
iebuf_free(struct iebuf *buf)
{
    if (buf == NULL)
        return;
    OS_FREE(buf);
}

#endif
