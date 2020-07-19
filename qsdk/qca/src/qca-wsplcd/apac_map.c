/*
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * @@-COPYRIGHT-END-@@
 */
#include <sys/socket.h>
#include <sys/file.h>
#include <fcntl.h>
#include <netinet/ether.h>
#include <string.h>
#include "apac_map.h"
#include "wlanif_cmn.h"

extern struct wlanif_config *wlanIfWd;

ieee1905TLV_t *ieee1905MapAddRadioIdTLV(ieee1905TLV_t *TLV,
        u_int32_t *Len,
        struct apac_wps_session *sess);

/**
 * @brief Convert a string to an unsigned integer, performing proper error
 *        checking.
 *
 * @param [in] buf  the string to convert
 * @param [in] base  the base to use for the conversion
 * @param [in] paramName  the name of the parameter to use in any error logs
 * @param [in] line  the line number to use in any error logs
 * @param [out] val  the converted value on success
 *
 * @return APAC_TRUE on success; otherwise APAC_FALSE
 */
static apacBool_e apacHyfiMapParseInt(const char *buf, int base, const char *paramName,
                                      int line, unsigned long *val) {
    char *endptr;

    errno = 0;
    *val = strtoul(buf, &endptr, base);
    if (errno != 0 || *endptr != '\0') {
        dprintf(MSG_ERROR, "%s: Failed to parse %s '%s' on line %d\n",
                __func__, paramName, buf, line);
        return APAC_FALSE;
    }

    return APAC_TRUE;
}

/**
 * @brief Parse and store the settings for a single SSID.
 *
 * This captures the security settings for the SSID, along with whether it is
 * acting as backhaul, fronthaul, or both.
 *
 * @param [in] buf  the portion of the line to parse
 * @param [in] line  the line number (for use in error messages)
 * @param [out] eProfile  the entry to populate
 *
 * @return APAC_TRUE on success; otherwise APAC_FALSE
 */
static apacBool_e apacHyfiMapParseAndStoreEProfile(const char *buf, int line,
                                                   apacMapEProfile_t *eProfile) {
    char tag[MAX_SSID_LEN];
    int len = 0;
    const char *pos = buf;
    unsigned long val;

    len = apac_atf_config_line_getparam(pos, ',', tag, sizeof(tag));
    if (len <= 0) {
        dprintf(MSG_ERROR, "%s: Could not extract SSID on line %d\n",
                __func__, line);
        return APAC_FALSE;
    }

    eProfile->ssid = strdup(tag);
    pos += len + 1;

    len = apac_atf_config_line_getparam(pos, ',', tag, sizeof(tag));
    if (len <= 0) {
        dprintf(MSG_ERROR, "%s: Could not extract auth mode on line %d\n",
                __func__, line);
        return APAC_FALSE;
    }

    if (!apacHyfiMapParseInt(tag, 16, "auth mode", line, &val)) {
        return APAC_FALSE;
    }

    eProfile->auth = val;
    pos += len + 1;

    len = apac_atf_config_line_getparam(pos, ',', tag, sizeof(tag));
    if (len <= 0) {
        dprintf(MSG_ERROR, "%s: Could not extract encryption mode on line %d\n",
                __func__, line);
        return APAC_FALSE;
    }

    if (!apacHyfiMapParseInt(tag, 16, "encryption mode", line, &val)) {
        return APAC_FALSE;
    }

    eProfile->encr = val;
    pos += len + 1;

    len = apac_atf_config_line_getparam(pos, ',', tag, sizeof(tag));
    if (len <= 0) {
        dprintf(MSG_ERROR, "%s: Could not extract PSK on line %d\n",
                __func__, line);
        return APAC_FALSE;
    }

    eProfile->nw_key = strdup(tag);
    pos += len + 1;

    len = apac_atf_config_line_getparam(pos, ',', tag, sizeof(tag));
    if (len <= 0) {
        dprintf(MSG_ERROR, "%s: Could not extract backhaul flag on line %d\n",
                __func__, line);
        return APAC_FALSE;
    }

    if (!apacHyfiMapParseInt(tag, 0, "backhaul flag", line, &val)) {
        return APAC_FALSE;
    }

    eProfile->isBackhaul = val;
    pos += len + 1;

    if (strlen(pos) == 0) {
        dprintf(MSG_ERROR, "%s: Could not extract fronthaul flag on line %d\n",
                __func__, line);
        return APAC_FALSE;
    }

    if (!apacHyfiMapParseInt(pos, 0, "fronthaul flag", line, &val)) {
        return APAC_FALSE;
    }

    eProfile->isFronthaul = val;

    return APAC_TRUE;
}

/**
 * @brief Handle a line for the AL-specific encryption profile file format.
 *
 * @param [in] buf  the line read from the config file
 * @param [in] line  the line number (for use in error messages)
 * @param [out] eProfileMatcher  the entry to populate
 *
 * @return APAC_TRUE on success; otherwise APAC_FALSE
 */
static apacBool_e apacHyfiMapParseAndStoreALSpecificEProfile(
    const char *buf, int line, apacMapEProfileMatcher_t *eProfileMatcher) {
    apacMapEProfile_t *eProfile = NULL;
    char tag[MAX_SSID_LEN];
    int len = 0;
    const char *pos = buf;

    eProfileMatcher->matcherType = APAC_E_MAP_EPROFILE_MATCHER_TYPE_AL_SPECIFIC;
    eProfileMatcher->terminateMatching = APAC_FALSE;

    eProfile = &eProfileMatcher->typeParams.alParams.eprofile;

    len = apac_atf_config_line_getparam(pos, ',', tag, sizeof(tag));
    if (len <= 0) {
        dprintf(MSG_ERROR, "%s: Could not extract AL ID on line %d\n",
                __func__, line);
        return APAC_FALSE;
    } else {
        eProfileMatcher->typeParams.alParams.alId = strdup(tag);
        pos += len + 1;
    }

    len = apac_atf_config_line_getparam(pos,',',tag, sizeof(tag));
    if (len <= 0) {
        dprintf(MSG_ERROR, "%s: Could not extract operating class on line %d\n",
                __func__, line);
        return APAC_FALSE;
    } else {
        eProfileMatcher->typeParams.alParams.opclass = strdup(tag);
        pos += len + 1;
    }

    return apacHyfiMapParseAndStoreEProfile(pos, line, eProfile);
}

/**
 * @brief Parse a line that specifies the settings for a single SSID.
 *
 * @param [inout] map  the structure into which to place the associated data
 * @param [in] buf  the line read from the config file
 * @param [in] line  the line number (for use in error messages)
 *
 * @return APAC_TRUE on success; otherwise APAC_FALSE
 */
static apacBool_e apacHyfiMapParseAndStoreGenericEProfileSSID(apacMapData_t *map,
                                                              const char *buf, int line) {
    char tag[MAX_SSID_LEN];
    size_t len;
    const char *pos = buf;
    const char *ssidKey = NULL;

    apacMapEProfile_t *eProfile = NULL;

    while (isspace(*pos)) {
        pos++;
    }

    len = apac_atf_config_line_getparam(pos, ',', tag, sizeof(tag));
    if (len <= 0) {
        dprintf(MSG_ERROR, "%s: Could not extract SSID key on line %d\n", __func__, line);
        return APAC_FALSE;
    }

    ssidKey = strdup(tag);
    if (!ssidKey) {
        dprintf(MSG_ERROR, "%s: Failed to copy SSID key\n", __func__);
        return APAC_FALSE;
    }

    pos += len + 1;

    do {
        // Place this into the SSID table. We assume the file contains no
        // duplicates.
        if (map->ssidCnt == sizeof(map->eProfileSSID) / sizeof(map->eProfileSSID[0])) {
            dprintf(MSG_ERROR, "%s: Too many SSIDs specified (max=%u)\n", __func__,
                    sizeof(map->eProfileSSID) / sizeof(map->eProfileSSID[0]));
            break;
        }

        eProfile = &map->eProfileSSID[map->ssidCnt].eprofile;
        if (apacHyfiMapParseAndStoreEProfile(pos, line, eProfile)) {
            dprintf(MSG_DEBUG, "%s: Storing SSID key '%s' at index %u\n", __func__,
                    ssidKey, map->ssidCnt);

            map->eProfileSSID[map->ssidCnt].ssidKey = ssidKey;
            map->ssidCnt++;
            return APAC_TRUE;
        }

    } while (0);

    // If we reach here, there was an error. Perform cleanup.
    free((char *)ssidKey);
    return APAC_FALSE;
}

/**
 * @brief Search for the SSID key in the table, returning its index if found.
 *
 * @param [in] map  the structure storing the SSIDs
 * @param [in] key  the SSID key to lookup
 *
 * @return the index at which the SSID key is stored, or -1 if not found
 */
static ssize_t apacHyfiMapLookupSSIDKey(apacMapData_t *map, const char *key) {
    u8 i;
    for (i = 0; i < map->ssidCnt; ++i) {
        if (strcmp(map->eProfileSSID[i].ssidKey, key) == 0) {
            return i;
        }
    }

    // not found
    return -1;
}

/**
 * @brief Create a single generic encryption profile policy from the template.
 *
 * @param [in] map  the structure storing the encryption profiles
 * @param [in] ssidKey  the key of the SSID to use for the profile to be stored
 * @param [in] matcherTemplate  the template matching parameters to use for
 *                              this profile matcher
 * @param [in] terminateMatching  whether to mark this entry as terminating the
 *                                matches
 * @param [inout] index  the next index to store
 *
 * @return APAC_TRUE on success; otherwise APAC_FALSE
 */
static apacBool_e apacHyfiMapStoreGenericEProfilePolicy(
    apacMapData_t *map, const char *ssidKey,
    struct apacMapEProfileMatcherGenericParams_t *matcherTemplate,
    apacBool_e terminateMatching, int *index) {
    // Find the matching SSID entry
    int ssidIndex = apacHyfiMapLookupSSIDKey(map, ssidKey);
    if (ssidIndex < 0) {
        dprintf(MSG_ERROR, "%s: Unknown SSID key '%s' specified\n", __func__, ssidKey);
        return APAC_FALSE;
    }

    if (*index == APAC_MAXNUM_NTWK_NODES) {
        dprintf(MSG_ERROR,
                "%s: Cannot store all encryption profiles; only %d supported\n", __func__,
                APAC_MAXNUM_NTWK_NODES);
        return APAC_FALSE;
    }

    // Finalize the matcher entry and store it in the complete array
    matcherTemplate->ssidIndex = ssidIndex;
    map->eProfileMatcher[*index].matcherType = APAC_E_MAP_EPROFILE_MATCHER_TYPE_GENERIC;
    map->eProfileMatcher[*index].terminateMatching = terminateMatching;
    map->eProfileMatcher[*index].typeParams.genericParams = *matcherTemplate;
    (*index)++;

    return APAC_TRUE;
}

/**
 * @brief Parse a line that specifies the matching parameters for SSIDs
 *        to instantiate.
 * @param [inout] map  the structure into which to place the associated data
 * @param [in] buf  the line read from the config file
 * @param [in] line  the line number (for use in error messages)
 * @param [inout] index  the next available encryption profile matcher index;
 *                       this will be updated for each profile stored
 *
 * @return APAC_TRUE on success; otherwise APAC_FALSE
 */
static apacBool_e apacHyfiMapParseAndStoreGenericEProfilePolicy(apacMapData_t *map,
                                                                const char *buf, int line,
                                                                int *index) {
    char tag[MAX_SSID_LEN];
    int len;
    const char *pos = buf;
    unsigned long val;
    u8 i;

    struct apacMapEProfileMatcherGenericParams_t matcherTemplate;

    while (isspace(*pos)) {
        pos++;
    }

    len = apac_atf_config_line_getparam(pos, ',', tag, sizeof(tag));
    if (len <= 0) {
        dprintf(MSG_ERROR,
                "%s: Could not extract number of operating class ranges on line %d\n",
                __func__, line);
        return APAC_FALSE;
    }

    if (!apacHyfiMapParseInt(tag, 0, "number of operating class ranges", line, &val)) {
        return APAC_FALSE;
    }

    matcherTemplate.numOpClassRanges = val;
    pos += len + 1;

    for (i = 0; i < matcherTemplate.numOpClassRanges; ++i) {
        // Min operating class parameter
        len = apac_atf_config_line_getparam(pos, ',', tag, sizeof(tag));
        if (len <= 0) {
            dprintf(MSG_ERROR,
                    "%s: Could not extract min operating class on line %d\n",
                    __func__, line);
            return APAC_FALSE;
        }

        if (!apacHyfiMapParseInt(tag, 0, "min operating class", line, &val)) {
            return APAC_FALSE;
        }

        matcherTemplate.opClassRanges[i].minOpClass = val;
        pos += len + 1;

        // Max operating class parameter
        len = apac_atf_config_line_getparam(pos, ',', tag, sizeof(tag));
        if (len <= 0) {
            dprintf(MSG_ERROR,
                    "%s: Could not extract max operating class on line %d\n",
                    __func__, line);
            return APAC_FALSE;
        }

        if (!apacHyfiMapParseInt(tag, 0, "max operating class", line, &val)) {
            return APAC_FALSE;
        }

        matcherTemplate.opClassRanges[i].maxOpClass = val;
        pos += len + 1;
    }

    // Now keep extracting SSIDs until we run out of space or find a
    // non-matching one
    len = apac_atf_config_line_getparam(pos, ',', tag, sizeof(tag));
    while (len > 0) {  // more to follow
        if (!apacHyfiMapStoreGenericEProfilePolicy(
                map, tag, &matcherTemplate, APAC_FALSE /* terminateMatching */, index)) {
            return APAC_FALSE;
        }

        pos += len + 1;
        len = apac_atf_config_line_getparam(pos, ',', tag, sizeof(tag));
    }

    // Last entry
    return apacHyfiMapStoreGenericEProfilePolicy(
        map, pos, &matcherTemplate, APAC_TRUE /* terminateMatching */, index);
}

/**
 * @brief Handle a line for the generic encryption profile file format.
 *
 * @param [inout] map  the structure into which to place the associated data
 * @param [in] buf  the line read from the config file
 * @param [in] line  the line number (for use in error messages)
 *
 * @return APAC_TRUE on success; otherwise APAC_FALSE
 */
static apacBool_e apacHyfiMapParseAndStoreGenericEProfile(apacMapData_t *map,
                                                          const char *buf, int line,
                                                          int *index) {
    // First determine the type of the line
    const char *ssidTag = "SSID:";
    const char *policyTag = "Generic-Policy:";
    if (strncmp(buf, ssidTag, strlen(ssidTag)) == 0) {
        return apacHyfiMapParseAndStoreGenericEProfileSSID(map, buf + strlen(ssidTag),
                                                           line);
    } else if (strncmp(buf, policyTag, strlen(policyTag)) == 0) {
        return apacHyfiMapParseAndStoreGenericEProfilePolicy(map, buf + strlen(policyTag),
                                                             line, index);
    } else {
        dprintf(MSG_ERROR, "%s: Unexpected line: '%s'\n", __func__, buf);
        return APAC_FALSE;
    }
}

/**
 * @brief Resolve the encryption profiles from the config file.
 *
 * @param [inout] map  the structure into which to place the encryption profiles
 * @param [in] matcherType  the format of the profiles within the file
 * @param [in] fname  the name of the file to read
 *
 * @return APAC_TRUE on success; otherwise APAC_FALSE
 */
static apacBool_e apacHyfiMapParseAndStoreConfig(apacMapData_t *map,
                                                 apacMapEProfileMatcherType_e matcherType,
                                                 const char *fname) {
    FILE *f = NULL;
    char buf[256] = {0};
    int index = 0;
    int line = 1;
    int errors = 0;
    apacMapEProfileMatcher_t *eProfileMatcher = NULL;

    apacHyfi20TRACE();

    int lock_fd = open(APAC_LOCK_FILE_PATH, O_RDONLY);
    if (lock_fd < 0) {
        dprintf(MSG_ERROR, "Failed to open lock file %s\n", APAC_LOCK_FILE_PATH);
        return APAC_FALSE;
    }

    if (flock(lock_fd, LOCK_EX) == -1) {
        dprintf(MSG_ERROR, "Failed to flock lock file %s\n", APAC_LOCK_FILE_PATH);
        close(lock_fd);
        return APAC_FALSE;
    }

    dprintf(MSG_DEBUG, "Reading Map 1.0 configuration file %s ...\n", fname);

    f = fopen(fname, "r");

    if (f == NULL) {
        dprintf(MSG_ERROR,
                "Could not open configuration file '%s' for reading.\n",
                fname);
        return APAC_FALSE;
    }

    while (fgets(buf, sizeof(buf), f) != NULL) {
        // remove the trailing carriage return and/or newline
        buf[strcspn(buf, "\r\n")] = '\0';

        if (strlen(buf) == 0) {
            continue;
        }
        eProfileMatcher = &map->eProfileMatcher[index];

        if (APAC_E_MAP_EPROFILE_MATCHER_TYPE_AL_SPECIFIC == g_map_cfg_file_format) {
            if (!apacHyfiMapParseAndStoreALSpecificEProfile(buf, line, eProfileMatcher)) {
                errors++;
                break;
            }

            index++;
        } else if (APAC_E_MAP_EPROFILE_MATCHER_TYPE_GENERIC == g_map_cfg_file_format) {
            if (!apacHyfiMapParseAndStoreGenericEProfile(map, buf, line, &index)) {
                errors++;
                break;
            }
        } else {
            dprintf(MSG_ERROR, "%s: Unexpected encryption profile matcher type: %d\n",
                    __func__, g_map_cfg_file_format);
            return APAC_FALSE;
        }

        if (index >= APAC_MAXNUM_NTWK_NODES) {
            dprintf(MSG_ERROR,
                    "%s: Cannot store all encryption profiles; only %d supported\n",
                    __func__, APAC_MAXNUM_NTWK_NODES);
            break;
        }

        line++;
        memset(buf, 0x00, sizeof(buf));
    }

    if (flock(lock_fd, LOCK_UN) == 1) {
        dprintf(MSG_ERROR, "Failed to unlock file %s\n", APAC_LOCK_FILE_PATH);
        errors++;
    }

    map->eProfileCnt = index;
    dprintf(MSG_INFO, "%s: Read %u MAP encryption profiles\n", __func__,
            map->eProfileCnt);
    close(lock_fd);
    fclose(f);

    if (APAC_E_MAP_EPROFILE_MATCHER_TYPE_AL_SPECIFIC == g_map_cfg_file_format) {
        if (map->eProfileCnt == 0 && errors == 0) {
            dprintf(MSG_INFO, "%s: No Profiles found in configuration file '%s', Tear Down \n ",
                    __func__, fname);
            return APAC_TRUE;
        }
    }

    if (errors) {
        dprintf(MSG_ERROR,
                "%s: %d errors found in configuration file '%s'\n",
                __func__, errors, fname);
    }

    return (errors == 0);
}

apacBool_e apacHyfiMapDInit(apacMapData_t *map) {
    u8 i = 0;
    apacMapEProfileMatcher_t *eProfileMatcher = NULL;
    apacMapEProfile_t *eProfile = NULL;

    apacHyfi20TRACE();

    for (i = 0; i < map->eProfileCnt; i++) {
        eProfileMatcher = &map->eProfileMatcher[i];
        if (APAC_E_MAP_EPROFILE_MATCHER_TYPE_AL_SPECIFIC ==
            eProfileMatcher->matcherType) {
            eProfile = &eProfileMatcher->typeParams.alParams.eprofile;

            free((char *)eProfileMatcher->typeParams.alParams.alId);
            free((char *)eProfileMatcher->typeParams.alParams.opclass);

            free((char *)eProfile->ssid);
            free((char *)eProfile->nw_key);
        }
    }
    map->eProfileCnt = 0;

    for (i = 0; i < map->ssidCnt; i++) {
        free((char *) map->eProfileSSID[i].ssidKey);
        free((char *) map->eProfileSSID[i].eprofile.ssid);
        free((char *) map->eProfileSSID[i].eprofile.nw_key);
    }
    map->ssidCnt = 0;

    return APAC_TRUE;
}

int apacGetPIFMapCap(apacHyfi20Data_t *pData) {
    apacPifMap_t *pIFMapData = NULL;
    int32_t Sock;
    struct iwreq iwr;
    struct ifreq ifr;
    struct ieee80211req_athdbg req = {0};
    int j, i = 0, k = 0;
    apacHyfi20AP_t *pAP = &pData->ap[0];
    struct ether_addr radioAddr;

    apacHyfi20TRACE();

    for (i = 0; i < APAC_NUM_WIFI_FREQ; i++) {

        if (!pAP[i].valid) {
            continue;
        }

        if (!pAP[i].ifName) {
            dprintf(MSG_ERROR, "%s - Invalid arguments: ifName is NULL", __func__);
            goto out;
        }

        dprintf(MSG_ERROR, "%s  %s \n", __func__, pAP[i].ifName);

        if ((Sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            dprintf(MSG_ERROR, "%s: Create ioctl socket failed!", __func__);
            goto out;
        }

        if (fcntl(Sock, F_SETFL, fcntl(Sock, F_GETFL) | O_NONBLOCK)) {
            dprintf(MSG_ERROR, "%s: fcntl() failed", __func__);
            goto err;
        }

        pIFMapData = &pAP[i].pIFMapData;
        strlcpy(iwr.ifr_name, pAP[i].ifName, IFNAMSIZ);
        strlcpy(ifr.ifr_name, pAP[i].radioName, IFNAMSIZ);
        iwr.u.data.pointer = (void *)&req;
        iwr.u.data.length = (sizeof(struct ieee80211req_athdbg));
        req.cmd = IEEE80211_DBGREQ_MAP_RADIO_HWCAP;
        req.needs_reply = DBGREQ_REPLY_IS_REQUIRED;

        if (!wlanIfWd) {
            dprintf(MSG_ERROR, "%s - wlanif nl init failed\n", __func__);
            goto err;
        }

        if (wlanIfWd->ops->getDbgreq(wlanIfWd->ctx, pAP[i].ifName, (void *)&req, (sizeof(struct ieee80211req_athdbg))) < 0)  {
            dprintf(MSG_ERROR, "%s: ioctl() failed, ifName: %s\n", __func__, pAP[i].ifName);
            goto err;
        }

        if (ioctl(Sock, SIOCGIFHWADDR, &ifr) < 0) {
            goto err;
        }

        os_memcpy(radioAddr.ether_addr_octet, ifr.ifr_hwaddr.sa_data, 6);
        os_memcpy(pAP[i].radio_mac, radioAddr.ether_addr_octet, 6);
        pIFMapData->apcap.hwcap.max_supported_bss = req.data.mapapcap.hwcap.max_supported_bss;
        pIFMapData->apcap.hwcap.num_supported_op_classes =
            req.data.mapapcap.hwcap.num_supported_op_classes;

        for (k = 0; k < req.data.mapapcap.hwcap.num_supported_op_classes; k++) {
            pIFMapData->apcap.hwcap.opclasses[k].opclass =
                req.data.mapapcap.hwcap.opclasses[k].opclass;
            pIFMapData->apcap.hwcap.opclasses[k].max_tx_pwr_dbm =
                req.data.mapapcap.hwcap.opclasses[k].max_tx_pwr_dbm;
            pIFMapData->apcap.hwcap.opclasses[k].num_non_oper_chan =
                req.data.mapapcap.hwcap.opclasses[k].num_non_oper_chan;

            dprintf(MSG_INFO, "Operating class  %d Txpwer %x Number of unoperable channel %d\n",
                    pIFMapData->apcap.hwcap.opclasses[k].opclass,
                    pIFMapData->apcap.hwcap.opclasses[k].max_tx_pwr_dbm,
                    pIFMapData->apcap.hwcap.opclasses[k].num_non_oper_chan);

            for (j = 0; j < pIFMapData->apcap.hwcap.opclasses[k].num_non_oper_chan; j++) {
                pIFMapData->apcap.hwcap.opclasses[k].non_oper_chan_num[j] =
                    req.data.mapapcap.hwcap.opclasses[k].non_oper_chan_num[j];
                dprintf(MSG_INFO, "Unoperable channel list %d \n",
                        pIFMapData->apcap.hwcap.opclasses[k].non_oper_chan_num[j]);
            }
        }
        close(Sock);
    }

    return 0;
err:
    close(Sock);
out:
    return -1;
}

apacBool_e apacHyfiMapInit(apacMapData_t *map) {
    apacHyfi20Data_t *hyfi20;
    apacHyfi20TRACE();

    hyfi20 = MAPToHYFI20(map);

    if (hyfi20->config.role != APAC_REGISTRAR) {
        dprintf(MSG_DEBUG, "%s: not registrar! nothing to do\n", __func__);
        return APAC_TRUE;
    }

    if (!apacHyfiMapIsEnabled(map))
        return APAC_TRUE;

    return apacHyfiMapParseAndStoreConfig(map, g_map_cfg_file_format, g_map_cfg_file);
}

/**
 * @brief Dump the parameters for an encryption profile.
 *
 * @param [in] prefix  the string prefix to use prior to the encryption
 *                     profile parameters
 * @param [in] eProfile  the encryption profile to dump
 */
static void apacHyfiMapConfigDumpEProfile(const char *prefix,
                                          const apacMapEProfile_t *eProfile) {
    dprintf(MSG_MSGDUMP, "%s%s,0x%04x,0x%04x,%s,%d,%d\n", prefix, eProfile->ssid,
            eProfile->auth, eProfile->encr, eProfile->nw_key, eProfile->isBackhaul,
            eProfile->isFronthaul);
}

void apacHyfiMapConfigDump(const apacMapData_t *map) {
    u8 i = 0, j = 0;
    const apacMapEProfileMatcher_t *eProfileMatcher = NULL;
    const apacMapEProfile_t *eProfile = NULL;
    const struct apacMapEProfileMatcherGenericParams_t *genericParams = NULL;
    apacHyfi20Data_t *hyfi20;
    apacHyfi20TRACE();

    hyfi20 = MAPToHYFI20(map);

    if (hyfi20->config.role != APAC_REGISTRAR) {
        dprintf(MSG_ERROR, "%s, not registrar!\n", __func__);
        return;
    }

    for (i = 0; i < map->eProfileCnt; i++) {
        eProfileMatcher = &map->eProfileMatcher[i];
        dprintf(MSG_MSGDUMP, "Profile #%u type = %u terminate = %u\n", i,
                eProfileMatcher->matcherType, eProfileMatcher->terminateMatching);

        if (APAC_E_MAP_EPROFILE_MATCHER_TYPE_AL_SPECIFIC ==
            eProfileMatcher->matcherType) {
            eProfile = &eProfileMatcher->typeParams.alParams.eprofile;

            dprintf(MSG_MSGDUMP, "AL-specific EProfile: %s,%s\n",
                    eProfileMatcher->typeParams.alParams.alId,
                    eProfileMatcher->typeParams.alParams.opclass);
            apacHyfiMapConfigDumpEProfile("  ", eProfile);
        } else if (APAC_E_MAP_EPROFILE_MATCHER_TYPE_GENERIC ==
                   eProfileMatcher->matcherType) {
            genericParams = &eProfileMatcher->typeParams.genericParams;
            dprintf(MSG_MSGDUMP,
                    "Generic Profile #%u: numOpClassRanges=%u terminateMatching=%u\n", i,
                    genericParams->numOpClassRanges, eProfileMatcher->terminateMatching);

            for (j = 0; j < genericParams->numOpClassRanges; ++j) {
                dprintf(
                    MSG_MSGDUMP, "  OpClassRange #%u: MinOpClass=%u MaxOpClass=%u\n", j,
                    genericParams->opClassRanges[j].minOpClass,
                    genericParams->opClassRanges[j].maxOpClass);
            }

            dprintf(MSG_MSGDUMP, "  SSID Index=%u\n", genericParams->ssidIndex);
            eProfile = &map->eProfileSSID[genericParams->ssidIndex].eprofile;
            apacHyfiMapConfigDumpEProfile("  ", eProfile);
        }
    }
}

apacBool_e apacHyfiMapIsEnabled(apacMapData_t *map)
{
    return map->enable;
}

apacBool_e apacHyfiMapPfComplianceEnabled(apacMapData_t *map)
{
    return map->mapPfCompliant;
}

/* Registrar/Enrollee sends WPS msg */
int apacHyfiMapSendMultipleM2(struct apac_wps_session *sess)
{
    u8 *frame;
    ieee1905MessageType_e msgType = IEEE1905_MSG_TYPE_AP_AUTOCONFIGURATION_WPS;
    u16 mid = apacHyfi20GetMid();
    u8 flags = 0;
    u8 *src;
    u8 *dest;
    APAC_WPS_DATA    *pWpsData = sess->pWpsData;
    struct wps_tlv *tlvList = &pWpsData->sndMsgM2[0];
    u8 fragNum = 0;
    size_t frameLen;
    ieee1905TLV_t *TLV;
    size_t sentLen = 0;
    u8  more = 0;
    u8 index = 0;
    apacBool_e radioTlvSent = APAC_FALSE;

    apacHyfi20TRACE();
    src = sess->own_addr;
    dest = sess->dest_addr;

    if (!src || !dest) {
        dprintf(MSG_ERROR, "src or dest address is null!\n");
        dprintf(MSG_ERROR, " src: "); printMac(MSG_DEBUG, src);
        dprintf(MSG_ERROR, " dest: "); printMac(MSG_DEBUG, dest);

        return -1;
    }

nextIndex:
    frame = apacHyfi20GetXmitBuf();
    frameLen = IEEE1905_FRAME_MIN_LEN;
    TLV = (ieee1905TLV_t *)((ieee1905Message_t *)frame)->content;

    if(radioTlvSent == APAC_FALSE) {
        TLV = ieee1905MapAddRadioIdTLV(TLV, &sentLen, sess);
        frameLen += sentLen;
        radioTlvSent = APAC_TRUE;
    }

    if (tlvList[index].length >= IEEE1905_CONTENT_MAXLEN) {
        return -1;
    }

    while(index < pWpsData->M2TlvCnt)
    {
        // To make sure TLV boundry fragmentation;
        // 2 * min 1 for EOM , one for TLV itself
        if (sentLen + tlvList[index].length < IEEE1905_CONTENT_MAXLEN - 2 * IEEE1905_TLV_MIN_LEN) {
            ieee1905TLVSet(TLV, tlvList[index].type, tlvList[index].length, tlvList[index].value.ptr_, frameLen);
            sentLen += tlvList[index].length;
            dprintf(MSG_ERROR, "%s sent len %d Type %x \n",__func__, sentLen,tlvList[index].type);
            index++;
        } else {
            more = 1;
            break;
        }
        TLV = ieee1905TLVGetNext(TLV);
    }

    if (!more)
        flags |= IEEE1905_HEADER_FLAG_LAST_FRAGMENT;

    ieee1905EndOfTLVSet(TLV);
    /* set up packet header */
    apacHyfi20SetPktHeader(frame, msgType, mid, fragNum, flags, src, dest);
    /* send packet */

    dprintf(MSG_INFO," %s Sending frame Length %d flags %x Mid %d fid %d Index %d \n",__func__, frameLen,
            flags, mid,fragNum, index);

    if (sess->pData->config.sendOnAllIFs == APAC_FALSE) {
        if (send(sess->pData->bridge.sock, frame, frameLen, 0) < 0) {
            perror("apacHyfi20SendWps");
            return -1;
        }
    } else {  /* send unicast packet on all interfaces. Debug Only! */
        int i;

        for (i = 0; i < APAC_MAXNUM_HYIF; i++) {
            if (sess->pData->hyif[i].ifIndex == -1)
                continue;

            if (apacHyfi20SendL2Packet(&sess->pData->hyif[i], frame, frameLen) < 0) {
                perror("apacHyfi20SendWps-onAllIFs");
            }
        }
    }

    fragNum++;

    if (more) {
        more = 0; //for next iteration
        sentLen = 0;
        frameLen = 0;
        flags = 0;
        goto nextIndex;
    }

    return 0;
}

apacBool_e apacHyfiMapParse1905TLV(struct apac_wps_session *sess,
        u8 *content, u32 contentLen,
        struct wps_tlv *container,
        s32 maxContainerSize, u8 *ieParsed)
{
    ieee1905TLV_t *TLV = (ieee1905TLV_t *)content;
    u_int32_t accumulatedLen = 0;
    ieee1905TlvType_e tlvType;
    apacBool_e retv = APAC_TRUE;
    u32 index = 0;
    struct wps_tlv *wps = NULL;
    u8 *ptr = NULL;

    accumulatedLen = ieee1905TLVLenGet(TLV) + IEEE1905_TLV_MIN_LEN;

    while( accumulatedLen <= contentLen ) {
        tlvType = ieee1905TLVTypeGet(TLV);
        wps = &container[index];
        if (tlvType ==  IEEE1905_TLV_TYPE_END_OF_MESSAGE ) {
            /* End-of-message */
            retv = APAC_TRUE;
            break;
        } else if( tlvType == IEEE1905_TLV_TYPE_WPS) {
            wps->type = IEEE1905_TLV_TYPE_WPS;
            wps->length = ieee1905TLVLenGet(TLV);
            if (wps->length)
                wps->value.ptr_ = os_malloc(wps->length);
            else {
                retv = APAC_FALSE;
                break;
            }

            if (!wps->value.ptr_) {
                retv = APAC_FALSE;
                break;
            }
            ptr = ieee1905TLVValGet(TLV);
            os_memcpy(wps->value.ptr_, ptr, wps->length);
            index++;
        } else if (tlvType == IEEE1905_TLV_TYPE_RADIO_IDENTIFIER) {
            sess->radioCap = malloc(ieee1905TLVLenGet(TLV));
            if (sess->radioCap) {
                sess->radioCapLen = ieee1905TLVLenGet(TLV);
                ptr = ieee1905TLVValGet(TLV);
                memcpy(sess->radioCap, ptr, sess->radioCapLen);
            }
        } else if (tlvType == IEEE1905_TLV_TYPE_AP_RADIO_BASIC_CAP) {
            sess->basicRadioCap = malloc(ieee1905TLVLenGet(TLV));
            if (sess->basicRadioCap) {
                ptr = ieee1905TLVValGet(TLV);
                sess->basicRadioCapLen = ieee1905TLVLenGet(TLV);
                memcpy(sess->basicRadioCap, ptr, sess->basicRadioCapLen);
            }
        }

        TLV = ieee1905TLVGetNext(TLV);
        accumulatedLen += ieee1905TLVLenGet(TLV) + IEEE1905_TLV_MIN_LEN;
    }

    *ieParsed = index;

    dprintf(MSG_MSGDUMP, "%s ToTal Frame received  %d \n",__func__, *ieParsed);

    return retv;
}

ieee1905TLV_t *ieee1905MapAddBasicRadioTLV(ieee1905TLV_t *TLV, u_int32_t *Len, u8 band,
                                           apacHyfi20Data_t *pData) {
    u_int16_t tlvLen = 0;
    u_int8_t *ptr = NULL;
    u_int16_t i = 0, j = 0;
    apacPifMap_t *pIFMapData = NULL;
    apacMapData_t *map = NULL;

    apacHyfi20TRACE();

    map = HYFI20ToMAP(pData);

    TLV = ieee1905TLVGetNext(TLV);
    ieee1905TLVTypeSet(TLV, IEEE1905_TLV_TYPE_AP_RADIO_BASIC_CAP);
    ptr = ieee1905TLVValGet(TLV);

    pIFMapData = &pData->ap[band].pIFMapData;

    if (!pIFMapData) return TLV;

    os_memcpy(ptr, pData->ap[band].radio_mac, ETH_ALEN);
    tlvLen += ETH_ALEN; /* MAC addr len */
    ptr += ETH_ALEN;

    *ptr++ = map->MapConfMaxBss;  // configured value
    tlvLen++;
    *ptr++ = pIFMapData->apcap.hwcap.num_supported_op_classes;
    tlvLen++;

    for (i = 0; i < pIFMapData->apcap.hwcap.num_supported_op_classes; i++) {
        *ptr++ = pIFMapData->apcap.hwcap.opclasses[i].opclass;
        tlvLen++;
        *ptr++ = pIFMapData->apcap.hwcap.opclasses[i].max_tx_pwr_dbm;
        tlvLen++;
        *ptr++ = pIFMapData->apcap.hwcap.opclasses[i].num_non_oper_chan;
        tlvLen++;

        for (j = 0; j < pIFMapData->apcap.hwcap.opclasses[i].num_non_oper_chan; j++) {
            *ptr++ = pIFMapData->apcap.hwcap.opclasses[i].non_oper_chan_num[j];
            tlvLen++;
        }
    }

    ieee1905TLVLenSet(TLV, tlvLen, *Len);
    dprintf(MSG_INFO, "Added basic radio TLV(Enrollee) framelen %d tlvlen %d \n", *Len, tlvLen);

    printMsg((u8 *)TLV, tlvLen, MSG_DEBUG);
    return TLV;
}

ieee1905TLV_t *ieee1905MapAddRadioIdTLV(ieee1905TLV_t *TLV,
        u_int32_t *Len,
        struct apac_wps_session *sess)
{

    u_int8_t *ptr = NULL;
    u8 *Data = NULL;

    apacHyfi20TRACE();

    if(sess->basicRadioCapLen)
        Data = sess->basicRadioCap;
    else
        return TLV;

    ieee1905TLVTypeSet( TLV, IEEE1905_TLV_TYPE_RADIO_IDENTIFIER);
    ptr = ieee1905TLVValGet(TLV);
    os_memcpy(ptr, Data, ETH_ALEN);

    ieee1905TLVLenSet(TLV, ETH_ALEN, *Len);
    printMsg((u8 *)TLV, ETH_ALEN,MSG_MSGDUMP);

    return ieee1905TLVGetNext(TLV);
}

u8 ieee1905MapParseBasicRadioTLV(u8 *val, u_int32_t Len, u8 *maxBss, u8 minop, u8 maxop,
                                 apacBool_e checkAllOpClasses) {
    u8 *ptr = NULL;
    u_int16_t i = 0, j = 0, unoperable = 0;
    u8 opclassCnt = 0, opclass = 0;
    apacBool_e retv = APAC_FALSE;

    apacHyfi20TRACE();

    ptr = val;
    ptr += ETH_ALEN;  // skipping radio mac address.

    *maxBss = *ptr++;
    opclassCnt = *ptr++;

    dprintf(MSG_INFO, "Received M1 maxbss %d opclasscnt %d  \n", *maxBss, opclassCnt);
    dprintf(MSG_INFO, "Checking match against minop %u maxop %u checkAllOpClasses %u\n", minop,
            maxop, checkAllOpClasses);

    for (i = 0; i < opclassCnt; i++) {
        opclass = *ptr++;
        ptr++;  // skipping tx power
        unoperable = *ptr++;

        for (j = 0; j < unoperable; j++) {
            ptr++;
        }

        // Does this operating class fall within the range?
        if (opclass >= minop && opclass <= maxop) {
            retv = APAC_TRUE;
        } else {
            retv = APAC_FALSE;
        }

        if (retv || !checkAllOpClasses) {
            break;
        }
    }

    return retv;
}

u8 apac_map_get_configured_maxbss(struct apac_wps_session *sess)
{
    apacHyfi20Data_t *hyfi20 = NULL;
    apacMapData_t *map = NULL;

    apacHyfi20TRACE();

    hyfi20 = sess->pData;

    map = HYFI20ToMAP(hyfi20);

    return map->MapConfMaxBss;
}

/**
 * @brief Populate the parameters for instantiating a BSS from an encryption
 *        profile.
 *
 * @param [in] eProfile  the encryption profile to copy from
 * @param [out] ap  the structure into which to copy the data
 */
static void apac_map_copy_apinfo_from_eprofile(const apacMapEProfile_t *eProfile,
                                               apacHyfi20AP_t *ap) {
    ap->ssid_len = os_strlen((const char *)eProfile->ssid);
    memcpy(ap->ssid, eProfile->ssid, ap->ssid_len);
    ap->ssid[ap->ssid_len] = '\0';  // only for logging

    ap->auth = eProfile->auth;
    ap->encr = eProfile->encr;

    ap->mapbh = eProfile->isBackhaul;
    ap->mapfh = eProfile->isFronthaul;

    ap->nw_key_len = os_strlen(eProfile->nw_key);
    os_memcpy(ap->nw_key, eProfile->nw_key, ap->nw_key_len);
    ap->nw_key[ap->nw_key_len] = '\0';  // only for logging

    dprintf(MSG_MSGDUMP , "MAP SSID %s ssid len %d  \n",ap->ssid, ap->ssid_len);
    dprintf(MSG_MSGDUMP , "MAP AUTH %x  \n", ap->auth);
    dprintf(MSG_MSGDUMP , "MAP ENCR  %x \n",ap->encr);
    dprintf(MSG_MSGDUMP , "MAP nw_key %s \n", ap->nw_key);
    dprintf(MSG_MSGDUMP , "MAP nw_key len %d \n",ap->nw_key_len);
    dprintf(MSG_MSGDUMP , "MAP Fronthaul  %x  \n",ap->mapfh);
    dprintf(MSG_MSGDUMP , "MAP Backhaul  %x  \n",ap->mapbh);
}

/**
 * @brief Populate the parameters for instantiating a BSS from the AL-specific
 *        encryption profile.
 *
 * @param [in] eProfileMatcher  the encryption profile to copy from
 * @param [out] ap  the structure into which to copy the data
 */
static void apac_map_copy_apinfo_al_specific(
    const apacMapEProfileMatcher_t *eProfileMatcher, apacHyfi20AP_t *ap) {
    const apacMapEProfile_t *eProfile = &eProfileMatcher->typeParams.alParams.eprofile;

    apac_map_copy_apinfo_from_eprofile(eProfile, ap);
}

/**
 * @brief Populate the parameters for instantiating a BSS from a generic
 *        encryption profile.
 *
 * @param [in] map  the overall structure for Multi-AP state
 * @param [in] eProfileMatcher  the generic encryption profile to copy from
 * @param [out] ap  the structure into which to copy the data
 */
static void apac_map_copy_apinfo_generic(
    const apacMapData_t *map, const apacMapEProfileMatcher_t *eProfileMatcher,
    apacHyfi20AP_t *ap) {
    const apacMapEProfile_t *eProfile =
        &map->eProfileSSID[eProfileMatcher->typeParams.genericParams.ssidIndex].eprofile;

    apac_map_copy_apinfo_from_eprofile(eProfile, ap);
}

apacBool_e apac_map_copy_apinfo(apacMapData_t *map, u8 index, apacHyfi20AP_t *ap) {
    apacMapEProfileMatcher_t *eProfileMatcher = NULL;

    apacHyfi20TRACE();

    if (index >= APAC_MAXNUM_NTWK_NODES && (index != 0xff)) return APAC_FALSE;

    /// Default Profile for Teardown
    if (index == 0xff) {
        ap->ssid_len = os_strlen("teardown");
        memcpy(ap->ssid, "teardown", ap->ssid_len);
        ap->ssid[ap->ssid_len] = '\0';

        ap->auth = WPS_AUTHTYPE_WPA2PSK;
        ap->encr = WPS_ENCRTYPE_AES;

        ap->mapbh = 0;
        ap->mapfh = 0;

        ap->nw_key_len = os_strlen("teardown");
        os_memcpy(ap->nw_key, "teardown", ap->nw_key_len);
        ap->nw_key[ap->nw_key_len] = '\0';
        return APAC_TRUE;
    }

    eProfileMatcher = &map->eProfileMatcher[index];
    if (APAC_E_MAP_EPROFILE_MATCHER_TYPE_AL_SPECIFIC == eProfileMatcher->matcherType) {
        apac_map_copy_apinfo_al_specific(eProfileMatcher, ap);
        return APAC_TRUE;
    } else if (APAC_E_MAP_EPROFILE_MATCHER_TYPE_GENERIC == eProfileMatcher->matcherType) {
        apac_map_copy_apinfo_generic(map, eProfileMatcher, ap);
        return APAC_TRUE;
    } else {
        dprintf(MSG_ERROR, "%s: Invalid profile matcher type %u for index %u\n",
                __func__, eProfileMatcher->matcherType, index);
        return APAC_FALSE;
    }
}

static apacBool_e apac_map_match_eprofile_al_specific(
    struct apac_wps_session *sess, const apacMapEProfileMatcher_t *eProfileMatcher,
    u8 *maxBSS) {
    char buf[1024] = { 0 };
    u8 minop = 0 , maxop = 0;
    const apacMapEProfile_t *eProfile = NULL;

    snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x ", sess->dest_addr[0], sess->dest_addr[1],
            sess->dest_addr[2], sess->dest_addr[3], sess->dest_addr[4], sess->dest_addr[5]);

    if (!strncasecmp(buf, eProfileMatcher->typeParams.alParams.alId,
                     IEEE80211_ADDR_LEN * 2)) {
        eProfile = &eProfileMatcher->typeParams.alParams.eprofile;

        dprintf(MSG_MSGDUMP,
                "Checking AL-specific EProfile for %s: "
                "%s,%s,%s,0x%04x,0x%04x,%s,%d,%d \n",
                buf, eProfileMatcher->typeParams.alParams.alId,
                eProfileMatcher->typeParams.alParams.opclass, eProfile->ssid,
                eProfile->auth, eProfile->encr, eProfile->nw_key,
                eProfile->isBackhaul, eProfile->isFronthaul);

        if (!strncasecmp(eProfileMatcher->typeParams.alParams.opclass, "11x",
                         3)) {
            minop = 110;
            maxop = 120;
        } else if (!strncasecmp(eProfileMatcher->typeParams.alParams.opclass,
                                "12x", 3)) {
            minop = 121;
            maxop = 130;//to compliant with WFA , ideally it should be 129
        } else if (!strncasecmp(eProfileMatcher->typeParams.alParams.opclass,
                                "8x", 2)) {
            minop = 80;
            maxop = 89;
        } else { //opclass not there
            dprintf(MSG_ERROR, "Unexpected op class %s\n",
                    eProfileMatcher->typeParams.alParams.opclass);
            return APAC_FALSE;
        }

        dprintf(MSG_DEBUG, "%s Profile based MinOp %d Maxop %d \n",__func__,
                minop, maxop);

        if (ieee1905MapParseBasicRadioTLV(sess->basicRadioCap, sess->basicRadioCapLen,
                                          maxBSS, minop, maxop,
                                          APAC_FALSE /* checkAllOperatingClasses */)) {
            return APAC_TRUE;
        }
    }

    return APAC_FALSE;
}

/**
 * @brief Attempt to match the radio capabilities against this generic
 *        encryption profile matcher.
 *
 * @param [in] sess  the information for the WPS session (where the radio
 *                   capabilities can be found)
 * @param [in] eProfileMatcher  the matcher to evaluate against the radio
 *                              capabilities
 * @param [out] maxBSS  the number of BSSes the agent indicates it can support
 *                      on the radio
 *
 * @return APAC_TRUE if there was a match; otherwise APAC_FALSE
 */
static apacBool_e apac_map_match_eprofile_generic(
    struct apac_wps_session *sess, const apacMapEProfileMatcher_t *eProfileMatcher,
    u8 *maxBSS) {
    apacBool_e match = APAC_TRUE;

    // Only is a match if all of the operating class ranges specified match
    u8 i;
    for (i = 0; i < eProfileMatcher->typeParams.genericParams.numOpClassRanges; ++i) {
        match &= ieee1905MapParseBasicRadioTLV(
            sess->basicRadioCap, sess->basicRadioCapLen, maxBSS,
            eProfileMatcher->typeParams.genericParams.opClassRanges[i].minOpClass,
            eProfileMatcher->typeParams.genericParams.opClassRanges[i].maxOpClass,
            APAC_TRUE /* checkAllOperatingClasses */);
    }

    return match;
}

/**
 * @brief Find the matching encryption profiles for the given radio based on
 *        its capabilities.
 *
 * @param [in] sess  the information for the WPS session (where the radio
 *                   capabilities can be found)
 * @param [out] list  array into which the matching encryption profiles will
 *                    be placed; must be at least APAC_MAXNUM_NTRK_NODES in
 *                    length
 * @param [out] requested_m2  the number of M2 messages (and thus the number of
 *                            BSSes) that the agent can support on the radio
 *
 * @return the number of profile matches (equivalently, the number of elements
 *         in list that are valid)
 */
u8 apac_map_get_eprofile(struct apac_wps_session *sess, u8 *list, u8 *requested_m2) {
    u8  maxBSS = 0, *listptr = NULL;
    u8 i = 0, profilecnt = 0;
    apacMapEProfileMatcher_t *eProfileMatcher = NULL;
    apacHyfi20Data_t *hyfi20 = NULL;
    apacMapData_t *map = NULL;
    apacBool_e match;

    apacHyfi20TRACE();

    hyfi20 = sess->pData;

    map = HYFI20ToMAP(hyfi20);

    listptr = list;
    *listptr = 0xff; //default value used in teardown
    *requested_m2 = 1; // max bss to send one teardown

    ieee1905MapParseBasicRadioTLV(sess->basicRadioCap, sess->basicRadioCapLen, &maxBSS, 0, 0,
                                  APAC_FALSE /* checkAllOpClasses */);

    if (hyfi20->config.role != APAC_REGISTRAR || map->eProfileCnt == 0 ) {
        dprintf(MSG_ERROR, "%s, not registrar or map file not found !\n", __func__);
        if (*listptr == 0xff) {
            for (i = 0; i < maxBSS; i++) {
                *listptr++ = 0xff;
            }
        }
        *requested_m2 = maxBSS;
        return profilecnt;
    }

    for(i = 0 ; i < map->eProfileCnt; i++) {
        match = APAC_FALSE;
        eProfileMatcher = &map->eProfileMatcher[i];

        if (APAC_E_MAP_EPROFILE_MATCHER_TYPE_AL_SPECIFIC == eProfileMatcher->matcherType) {
            match = apac_map_match_eprofile_al_specific(sess, eProfileMatcher, &maxBSS);
        } else if (APAC_E_MAP_EPROFILE_MATCHER_TYPE_GENERIC == eProfileMatcher->matcherType) {
            match = apac_map_match_eprofile_generic(sess, eProfileMatcher, &maxBSS);
        } else {
            dprintf(MSG_ERROR, "%s: Unexpected matcher type at index %u\n", __func__, i);
        }

        if (match) {
            dprintf(MSG_MSGDUMP, "%s: Matched eprofile #%u\n", __func__, i);

            *listptr++ = i;
            profilecnt++;

            if (eProfileMatcher->terminateMatching) {
                // No further matching should be done for this radio
                break;
            }
        }
    }

    if (profilecnt < maxBSS) {
        dprintf(MSG_DEBUG, "%s: No profile match found for remaining BSS = %u \n", __func__,
                maxBSS - profilecnt);
        *listptr += profilecnt;
        for (i = 0; i < maxBSS - profilecnt; i++) {
            *listptr++ = 0xff;
        }
    }
    *requested_m2 = maxBSS;

    return profilecnt;
}

u8 apac_map_parse_vendor_ext(struct apac_wps_session *sess,
        u8 *pos,
        u8 len, u8 *mapBssType)
{
    u32 vendor_id;
#define WPS_VENDOR_ID_WFA 14122 //37 2a
#define WFA_ELEM_MAP_BSS_CONFIGURATION 0x06
    apacHyfi20TRACE();

    if (len < 3) {
        dprintf(MSG_DEBUG, "WPS: Skip invalid Vendor Extension");
        return 0;
    }

    vendor_id = WPA_GET_BE24(pos);
    switch (vendor_id) {
        case WPS_VENDOR_ID_WFA:
            len -=3;
            pos +=3;
            const u8 *end = pos + len;
            u8 id, elen;

            while (end - pos >= 2) {
                id = *pos++;
                elen = *pos++;
                if (elen > end - pos)
                    break;

                switch(id) {
                    case WFA_ELEM_MAP_BSS_CONFIGURATION:
                        dprintf(MSG_MSGDUMP,"Received Map Bss Type %x \n", *pos);
                        *mapBssType = *pos;
                        break;
                }
                pos += elen;
            }
    }
    return 0;
}

apacBool_e map_wps_build_cred_network_idx(struct credbuf *credentials)
{
    dprintf(MSG_MSGDUMP,"Network Index");
    WPA_PUT_BE16(credentials->buf + credentials->len, ATTR_NETWORK_INDEX);
    credentials->len += 2;
    WPA_PUT_BE16(credentials->buf + credentials->len, 1);
    credentials->len += 2;
    credentials->buf[credentials->len++] = 1;
    return APAC_TRUE;
}

apacBool_e map_wps_build_cred_ssid(struct credbuf *credentials, apacMapAP_t *map)
{
    dprintf(MSG_MSGDUMP,"SSID");
    WPA_PUT_BE16(credentials->buf + credentials->len, ATTR_SSID);
    credentials->len += 2;
    WPA_PUT_BE16(credentials->buf + credentials->len, map->ssid_len);
    credentials->len += 2;
    os_memcpy(credentials->buf + credentials->len, map->ssid, map->ssid_len);
    credentials->len += map->ssid_len;
    return APAC_TRUE;
}

apacBool_e map_wps_build_cred_auth_type(struct credbuf *credentials, apacMapAP_t *map)
{
    u16 auth_type = map->auth;
    dprintf(MSG_MSGDUMP,"Authentication Type");

    if (auth_type & WPS_AUTH_WPA2PSK)
        auth_type = WPS_AUTH_WPA2PSK;
    else if (auth_type & WPS_AUTH_WPAPSK)
        auth_type = WPS_AUTH_WPAPSK;
    else if (auth_type & WPS_AUTH_OPEN)
        auth_type = WPS_AUTH_OPEN;

    WPA_PUT_BE16(credentials->buf + credentials->len, ATTR_AUTH_TYPE);
    credentials->len += 2;
    WPA_PUT_BE16(credentials->buf + credentials->len, 2);
    credentials->len += 2;
    WPA_PUT_BE16(credentials->buf + credentials->len, auth_type);
    credentials->len += 2;
    return APAC_TRUE;
}

apacBool_e map_wps_build_cred_encr_type(struct credbuf *credentials, apacMapAP_t *map)
{
    u16 encr_type = map->encr;
    dprintf(MSG_MSGDUMP,"Encryption Type");

    if (map->auth & (WPS_AUTH_WPA2PSK | WPS_AUTH_WPAPSK)) {
        if (encr_type & WPS_ENCR_AES)
            encr_type = WPS_ENCR_AES;
        else if (encr_type & WPS_ENCR_TKIP)
            encr_type = WPS_ENCR_TKIP;
    }

    WPA_PUT_BE16(credentials->buf + credentials->len, ATTR_ENCR_TYPE);
    credentials->len += 2;
    WPA_PUT_BE16(credentials->buf + credentials->len, 2);
    credentials->len += 2;
    WPA_PUT_BE16(credentials->buf + credentials->len, encr_type);
    credentials->len += 2;
    return APAC_TRUE;
}

apacBool_e map_wps_build_cred_network_key(struct credbuf *credentials, apacMapAP_t *map)
{
    dprintf(MSG_MSGDUMP,"Network Key Index");

    WPA_PUT_BE16(credentials->buf + credentials->len, ATTR_NETWORK_KEY_INDEX);
    credentials->len += 2;
    WPA_PUT_BE16(credentials->buf + credentials->len, 1);
    credentials->len += 2;
    credentials->buf[credentials->len++] = 1;

    dprintf(MSG_MSGDUMP,"Network Key");
    WPA_PUT_BE16(credentials->buf + credentials->len, ATTR_NETWORK_KEY);
    credentials->len += 2;
    WPA_PUT_BE16(credentials->buf + credentials->len, map->nw_key_len);
    credentials->len += 2;
    os_memcpy(credentials->buf + credentials->len, map->nw_key, map->nw_key_len);
    credentials->len += map->nw_key_len;
    return APAC_TRUE;
}

apacBool_e map_wps_build_cred_mac_addr(struct credbuf *credentials, apacMapAP_t *map)
{
    u_int8_t macZero[6] = {0};
    dprintf(MSG_MSGDUMP,"MAC ADDR");
    WPA_PUT_BE16(credentials->buf + credentials->len, ATTR_MAC_ADDR);
    credentials->len += 2;
    WPA_PUT_BE16(credentials->buf + credentials->len, ETH_ALEN);
    credentials->len += 2;
    os_memcpy(credentials->buf + credentials->len, macZero , ETH_ALEN);
    credentials->len += ETH_ALEN;
    return APAC_TRUE;
}

apacBool_e apacHyfiMapBuildBackHaulCred(apacMapAP_t *map, u8 radioIdx) {
  struct credbuf credentials;
  credentials.len = 0;
  u8 *totalLen = NULL;
  u8 *bufptr = credentials.buf;
  char filename[50];
  FILE *fp = NULL;

  snprintf(filename, sizeof(filename), "/var/run/hostapd_cred_wifi%d.bin", radioIdx - 1);
  fp = fopen(filename, "wb");

  if(!fp)
      return APAC_FALSE;

  /* Fill Auth Type */
  WPA_PUT_BE16(credentials.buf + credentials.len, ATTR_CRED);
  credentials.len += 2;
  totalLen = credentials.buf + credentials.len;
  credentials.len += 2;

  /* Fill Network */
  map_wps_build_cred_network_idx(&credentials);

  /* Fill SSID */
  map_wps_build_cred_ssid(&credentials, map);

  /* Fill Auth Type */
  map_wps_build_cred_auth_type(&credentials, map);

  /* Fill Encryption Type */
  map_wps_build_cred_encr_type(&credentials, map);

  /* Fill Network Key */
  map_wps_build_cred_network_key(&credentials, map);

  /* Fill Mac Addr */
  map_wps_build_cred_mac_addr(&credentials, map);

  *(u_int16_t *)totalLen = htons(credentials.len - 4);

  if (fp) {
    fwrite(bufptr, 1, credentials.len, fp);
    fclose(fp);
  }

  return APAC_TRUE;
}
