/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2006 Sam Leffler, Errno Consulting
 * Copyright (c) 2005-2006 Atheros Communications, Inc.
 * Copyright (c) 2010, Atheros Communications Inc. 
 * 
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 * 
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */

#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"
#include "ah_eeprom.h"
#include "ah_devid.h"

#include "ah_regdomain.h"

/* used throughout this file... */
#define    N(a)    (sizeof (a) / sizeof (a[0]))

/* 10MHz is half the 11A bandwidth used to determine upper edge freq
   of the outdoor channel */
#define HALF_MAXCHANBW        10

/* Mask to check whether a domain is a multidomain or a single
   domain */

#define MULTI_DOMAIN_MASK 0xFF00

#define    WORLD_SKU_MASK        0x00F0
#define    WORLD_SKU_PREFIX    0x0060

#if !NO_HAL
static int
chansort(const void *a, const void *b)
{
#define CHAN_FLAGS    (CHANNEL_ALL|CHANNEL_HALF|CHANNEL_QUARTER)
    const HAL_CHANNEL_INTERNAL *ca = a;
    const HAL_CHANNEL_INTERNAL *cb = b;

    return (ca->channel == cb->channel) ?
        (ca->channel_flags & CHAN_FLAGS) -
            (cb->channel_flags & CHAN_FLAGS) :
        ca->channel - cb->channel;
#undef CHAN_FLAGS
}
typedef int ath_hal_cmp_t(const void *, const void *);
static    void ath_hal_sort(void *a, u_int32_t n, u_int32_t es, ath_hal_cmp_t *cmp);
static const COUNTRY_CODE_TO_ENUM_RD* findCountry(HAL_CTRY_CODE countryCode, HAL_REG_DOMAIN rd);
static bool getWmRD(struct ath_hal *ah, int regdmn, u_int16_t channelFlag, REG_DOMAIN *rd);
#endif

#define isWwrSKU(_ah) (((getEepromRD((_ah)) & WORLD_SKU_MASK) == WORLD_SKU_PREFIX) || \
               (getEepromRD(_ah) == WORLD))

#define isWwrSKU_NoMidband(_ah) ((getEepromRD((_ah)) == WOR3_WORLD) || \
                (getEepromRD(_ah) == WOR4_WORLD) || \
                (getEepromRD(_ah) == WOR5_ETSIC))
#define isUNII1OddChan(ch) ((ch == 5170) || (ch == 5190) || (ch == 5210) || (ch == 5230))

/*
 * By default, the regdomain tables reference the common tables
 * from ah_regdomain_common.h.  These default tables can be replaced
 * by calls to populate_regdomain_tables functions.
 */
HAL_REG_DMN_TABLES Rdt = {
    ahCmnRegDomainPairs,    /* regDomainPairs */
    AH_NULL,                /* regDmnVendorPairs */
    ahCmnAllCountries,      /* allCountries */
    AH_NULL,                /* customMappings */
    ahCmnRegDomains,        /* regDomains */

    N(ahCmnRegDomainPairs),    /* regDomainPairsCt */
    0,                         /* regDmnVendorPairsCt */
    N(ahCmnAllCountries),      /* allCountriesCt */
    0,                         /* customMappingsCt */
    N(ahCmnRegDomains),        /* regDomainsCt */
};

#if !NO_HAL
u_int16_t
getEepromRD(struct ath_hal *ah)
{
    return AH_PRIVATE(ah)->ah_current_rd &~ WORLDWIDE_ROAMING_FLAG;
}

/*
 * Test to see if the bitmask array is all zeros
 */
static bool
isChanBitMaskZero(u_int64_t *bitmask)
{
    int i;

    for (i=0; i<BMLEN; i++) {
        if (bitmask[i] != 0)
            return false;
    }
    return true;
}

/*
 * Return whether or not the regulatory domain/country in EEPROM
 * is acceptable.
 */
static bool
isEepromValid(struct ath_hal *ah)
{
    u_int16_t rd = getEepromRD(ah);
    int i;

    if (rd & COUNTRY_ERD_FLAG) {
        u_int16_t cc = rd &~ COUNTRY_ERD_FLAG;
        for (i = 0; i < Rdt.allCountriesCt; i++)
            if (Rdt.allCountries[i].countryCode == cc)
                return true;
    } else {
        for (i = 0; i < Rdt.regDomainPairsCt; i++)
            if (Rdt.regDomainPairs[i].regDmnEnum == rd)
                return true;
    }
    HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: invalid regulatory domain/country code 0x%x\n",
        __func__, rd);
    return false;
}

/*
 * Return whether or not the FCC Mid-band flag is set in EEPROM
 */
static bool
isFCCMidbandSupported(struct ath_hal *ah)
{
    u_int32_t regcap;

    ath_hal_getcapability(ah, HAL_CAP_REG_FLAG, 0, &regcap);

    if (regcap & AR_EEPROM_EEREGCAP_EN_FCC_MIDBAND) {
        return true;
    }
    else {
        return false;
    }
}

static const REG_DMN_PAIR_MAPPING *
getRegDmnPair(HAL_REG_DOMAIN reg_dmn)
{
    int i;
    for (i = 0; i < Rdt.regDomainPairsCt; i++) {
        if (Rdt.regDomainPairs[i].regDmnEnum == reg_dmn) {
            return &Rdt.regDomainPairs[i];
        }
    }
    return AH_NULL;
}

/*
 * Returns whether or not the specified country code
 * is allowed by the EEPROM setting
 */
static bool
isCountryCodeValid(struct ath_hal *ah, HAL_CTRY_CODE cc)
{
    u_int16_t  rd;
    int i, support5G, support2G;
    u_int modesAvail;
    const COUNTRY_CODE_TO_ENUM_RD *country = AH_NULL;
    const REG_DMN_PAIR_MAPPING *regDmn_eeprom, *regDmn_input;

    /* Default setting requires no checks */
    if (cc == CTRY_DEFAULT || cc & COUNTRY_ERD_FLAG)
        return true;
#ifdef AH_DEBUG_COUNTRY
    if (cc == CTRY_DEBUG)
        return true;
#endif
    rd = getEepromRD(ah);
    HDPRINTF(ah, HAL_DBG_REGULATORY,
        "%s: EEPROM regdomain 0x%x\n", __func__, rd);

    if (rd & COUNTRY_ERD_FLAG) {
        /* EEP setting is a country - config shall match */
        HDPRINTF(ah, HAL_DBG_REGULATORY,
            "%s: EEPROM setting is country code %u\n",
            __func__, rd &~ COUNTRY_ERD_FLAG);
        return (cc == (rd & ~COUNTRY_ERD_FLAG));
    }


    for (i = 0; i < Rdt.allCountriesCt; i++) {
        if (cc == Rdt.allCountries[i].countryCode) {
#ifdef AH_SUPPORT_11D
            if ((rd & WORLD_SKU_MASK) == WORLD_SKU_PREFIX) {
                return true;
            }
#endif
            country = &Rdt.allCountries[i];
            if (country->regDmnEnum == rd ||
                rd == DEBUG_REG_DMN || rd == NO_ENUMRD) {
                return true;
            }
        }
    }
    if (country == AH_NULL) return false;

    /* consider device capability and look into the regdmn for 2G&5G */
    modesAvail = ath_hal_getWirelessModes(ah);
    support5G = modesAvail & (HAL_MODE_11A | HAL_MODE_TURBO |
                HAL_MODE_108A | HAL_MODE_11A_HALF_RATE |
                HAL_MODE_11A_QUARTER_RATE |
                HAL_MODE_11NA_HT40PLUS |
                HAL_MODE_11NA_HT40MINUS);
    support2G = modesAvail & (HAL_MODE_11G | HAL_MODE_11B | HAL_MODE_PUREG |
                HAL_MODE_108G |
                HAL_MODE_11NG_HT40PLUS |
                HAL_MODE_11NG_HT40MINUS);

    regDmn_eeprom = getRegDmnPair(rd);
    regDmn_input = getRegDmnPair(country->regDmnEnum);
    if (!regDmn_eeprom || !regDmn_input)
        return false;

    if (support5G && support2G) {
        /* force a strict binding between regdmn and countrycode */
        return false;
    } else if (support5G) {
        if (regDmn_eeprom->regDmn5GHz == regDmn_input->regDmn5GHz)
            return true;
    } else if (support2G) {
        if (regDmn_eeprom->regDmn2GHz == regDmn_input->regDmn2GHz)
            return true;
    } else {
        HDPRINTF(ah, HAL_DBG_REGULATORY,
            "%s: neither 2G nor 5G is supported\n", __func__);
    }
    return false;
}

/*
 * Return the mask of available modes based on the hardware
 * capabilities and the specified country code and reg domain.
 */
static u_int
ath_hal_getwmodesnreg(struct ath_hal *ah, const COUNTRY_CODE_TO_ENUM_RD *country,
             REG_DOMAIN *rd5GHz)
{
    u_int modesAvail;

    /* Get modes that HW is capable of */
    modesAvail = ath_hal_getWirelessModes(ah);

    /* Check country regulations for allowed modes */
#ifndef ATH_NO_5G_SUPPORT
    if ((modesAvail & (HAL_MODE_11A_TURBO|HAL_MODE_TURBO)) &&
        (!country->allow11aTurbo))
        modesAvail &= ~(HAL_MODE_11A_TURBO | HAL_MODE_TURBO);
#endif
    if ((modesAvail & HAL_MODE_11G_TURBO) &&
        (!country->allow11gTurbo))
        modesAvail &= ~HAL_MODE_11G_TURBO;
    if ((modesAvail & HAL_MODE_11G) &&
        (!country->allow11g))
        modesAvail &= ~HAL_MODE_11G;
#ifndef ATH_NO_5G_SUPPORT
    if ((modesAvail & HAL_MODE_11A) &&
        (isChanBitMaskZero(rd5GHz->chan11a)))
        modesAvail &= ~HAL_MODE_11A;
#endif

    if ((modesAvail & HAL_MODE_11NG_HT20) &&
       (!country->allow11ng20))
        modesAvail &= ~HAL_MODE_11NG_HT20;

    if ((modesAvail & HAL_MODE_11NA_HT20) &&
       (!country->allow11na20))
        modesAvail &= ~HAL_MODE_11NA_HT20;

    if ((modesAvail & HAL_MODE_11NG_HT40PLUS) &&
       (!country->allow11ng40))
        modesAvail &= ~HAL_MODE_11NG_HT40PLUS;

    if ((modesAvail & HAL_MODE_11NG_HT40MINUS) &&
       (!country->allow11ng40))
        modesAvail &= ~HAL_MODE_11NG_HT40MINUS;

    if ((modesAvail & HAL_MODE_11NA_HT40PLUS) &&
       (!country->allow11na40))
        modesAvail &= ~HAL_MODE_11NA_HT40PLUS;

    if ((modesAvail & HAL_MODE_11NA_HT40MINUS) &&
       (!country->allow11na40))
        modesAvail &= ~HAL_MODE_11NA_HT40MINUS;

    return modesAvail;
}

#endif
/*
 * Return the mask of available modes based on the hardware
 * capabilities and the specified country code.
 */

#if !NO_HAL
u_int __ahdecl
ath_hal_getwirelessmodes(struct ath_hal *ah, HAL_CTRY_CODE cc)
{
    const COUNTRY_CODE_TO_ENUM_RD *country=AH_NULL;
    u_int mode=0;
    REG_DOMAIN rd;

    country = findCountry(cc, getEepromRD(ah));
    if (country != AH_NULL) {
        if (getWmRD(ah, country->regDmnEnum, ~CHANNEL_2GHZ, &rd))
            mode = ath_hal_getwmodesnreg(ah, country, &rd);
    }
    return(mode);
}

#endif
/*
 * Return if device is public safety.
 */
bool __ahdecl
ath_hal_ispublicsafetysku(struct ath_hal *ah)
{
    u_int16_t rd;

    rd = getEepromRD(ah);

    switch (rd) {
        case FCC4_FCCA:
        case (CTRY_UNITED_STATES_FCC49 | COUNTRY_ERD_FLAG):
            return true;

        case DEBUG_REG_DMN:
        case NO_ENUMRD:
            if (AH_PRIVATE(ah)->ah_countryCode == 
                        CTRY_UNITED_STATES_FCC49) {
                return true;
            }
            break;
    }

    return false;
}

/*
 * Find the country code.
 */
#if !NO_HAL
HAL_CTRY_CODE __ahdecl
findCountryCode(u_int8_t *countryString)
{
    int i;

    for (i = 0; i < Rdt.allCountriesCt; i++) {
        if ((Rdt.allCountries[i].isoName[0] == countryString[0]) &&
            (Rdt.allCountries[i].isoName[1] == countryString[1]))
            return (Rdt.allCountries[i].countryCode);
    }
    return (0);        /* Not found */
}

/*
 * Find the conformance_test_limit by country code.
 */
u_int8_t __ahdecl
findCTLByCountryCode(HAL_CTRY_CODE countrycode, bool is2G)
{
    int i = 0, j = 0, k = 0;
    
    if  (countrycode == 0) {
        return NO_CTL;
    }
    
     for (i = 0; i < Rdt.allCountriesCt; i++) {
        if (Rdt.allCountries[i].countryCode == countrycode) {
            for (j = 0; j < Rdt.regDomainPairsCt; j++) {
                if (Rdt.regDomainPairs[j].regDmnEnum == Rdt.allCountries[i].regDmnEnum) {
                    for (k = 0; k < Rdt.regDomainsCt; k++) {
                        if (is2G) {
                             if (Rdt.regDomains[k].regDmnEnum == Rdt.regDomainPairs[j].regDmn2GHz)
                                 return Rdt.regDomains[k].conformance_test_limit;                
                        } else {
                             if (Rdt.regDomains[k].regDmnEnum == Rdt.regDomainPairs[j].regDmn5GHz)
                                 return Rdt.regDomains[k].conformance_test_limit;                
                        }
                    }
                }
            }
        }
    }

    return NO_CTL;
}
#endif

/*
 * Find the pointer to the country element in the country table
 * corresponding to the country code
 */
static const COUNTRY_CODE_TO_ENUM_RD*
findCountry(HAL_CTRY_CODE countryCode, HAL_REG_DOMAIN rd)
{
    int i;


    for (i = 0; i < Rdt.allCountriesCt; i++) {
        if (Rdt.allCountries[i].countryCode == countryCode)
            return (&Rdt.allCountries[i]);
    }
    return (AH_NULL);        /* Not found */
}

/*
 * Calculate a default country based on the EEPROM setting.
 */
#if !NO_HAL
static HAL_CTRY_CODE
getDefaultCountry(struct ath_hal *ah)
{
    u_int16_t rd;
    int i;

    rd =getEepromRD(ah);
    if (rd & COUNTRY_ERD_FLAG) {
        const COUNTRY_CODE_TO_ENUM_RD *country=AH_NULL;
        u_int16_t cc= rd & ~COUNTRY_ERD_FLAG;

        country = findCountry(cc, rd);
        if (country != AH_NULL)
            return cc;
    }
    /*
     * Check reg domains that have only one country
     */
    for (i = 0; i < Rdt.regDomainPairsCt; i++) {
        if (Rdt.regDomainPairs[i].regDmnEnum == rd) {
            if (Rdt.regDomainPairs[i].singleCC != 0) {
                return Rdt.regDomainPairs[i].singleCC;
            } else {
                i = Rdt.regDomainPairsCt;
            }
        }
    }
    return CTRY_DEFAULT;
}

#endif
/*
 * Convert Country / RegionDomain to RegionDomain
 * rd could be region or country code.
*/
HAL_REG_DOMAIN
ath_hal_getDomain(struct ath_hal *ah, u_int16_t rd)
{
    if (rd & COUNTRY_ERD_FLAG) {
        const COUNTRY_CODE_TO_ENUM_RD *country=AH_NULL;
        u_int16_t cc= rd & ~COUNTRY_ERD_FLAG;

        country = findCountry(cc, rd);
        if (country != AH_NULL)
            return country->regDmnEnum;
        return 0;
    }
    return rd;
}

#if !NO_HAL
static bool
isValidRegDmn(int reg_dmn, REG_DOMAIN *rd)
{
    int i;

    for (i = 0; i < Rdt.regDomainsCt; i++) {
        if (Rdt.regDomains[i].regDmnEnum == reg_dmn) {
            if (rd != AH_NULL) {
                OS_MEMCPY(rd, &Rdt.regDomains[i], sizeof(REG_DOMAIN));
            }
            return true;
        }
    }
    return false;
}

static bool
isValidRegDmnPair(int regDmnPair)
{
    int i;

    if (regDmnPair == NO_ENUMRD) return false;
    for (i = 0; i < Rdt.regDomainPairsCt; i++) {
        if (Rdt.regDomainPairs[i].regDmnEnum == regDmnPair) return true;
    }
    return false;
}

#endif
/*
 * Return the Wireless Mode Regulatory Domain based
 * on the country code and the wireless mode.
 */
#if !NO_HAL
static bool
getWmRD(struct ath_hal *ah, int reg_dmn, u_int16_t channelFlag, REG_DOMAIN *rd)
{
    int i, found;
    u_int64_t flags=NO_REQ;
    const REG_DMN_PAIR_MAPPING *regPair=AH_NULL;
    int regOrg;


    regOrg = reg_dmn;
    if (reg_dmn == CTRY_DEFAULT) {
        u_int16_t rdnum;
        rdnum =getEepromRD(ah);

        if (!(rdnum & COUNTRY_ERD_FLAG)) {
            if (isValidRegDmn(rdnum, AH_NULL) ||
                isValidRegDmnPair(rdnum)) {
                reg_dmn = rdnum;
            }
        }
    }

    if ((reg_dmn & MULTI_DOMAIN_MASK) == 0) {

        for (i = 0, found = 0; (i < Rdt.regDomainPairsCt) && (!found); i++) {
            if (Rdt.regDomainPairs[i].regDmnEnum == reg_dmn) {
                regPair = &Rdt.regDomainPairs[i];
                found = 1;
            }
        }
        if (!found) {
            HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: Failed to find reg domain pair %u\n",
                 __func__, reg_dmn);
            return false;
        }
        if (!(channelFlag & CHANNEL_2GHZ)) {
            reg_dmn = regPair->regDmn5GHz;
            flags = regPair->flags5GHz;
        }
        if (channelFlag & CHANNEL_2GHZ) {
            reg_dmn = regPair->regDmn2GHz;
            flags = regPair->flags2GHz;
        }
    }

    /*
     * We either started with a unitary reg domain or we've found the 
     * unitary reg domain of the pair
     */

    found = isValidRegDmn(reg_dmn, rd);
    if (!found) {
        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: Failed to find unitary reg domain %u\n",
             __func__, reg_dmn);
        return false;
    } else {
        if (regPair == AH_NULL) {
            HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: No set reg domain pair\n", __func__);
            return false;
        }
        rd->pscan &= regPair->pscanMask;
        if (((regOrg & MULTI_DOMAIN_MASK) == 0) &&
            (flags != NO_REQ)) {
            rd->flags = flags;
        }
        /*
         * Use only the domain flags that apply to the current mode.
         * In particular, do not apply 11A Adhoc flags to b/g modes.
         */
        rd->flags &= (channelFlag & CHANNEL_2GHZ) ? 
            REG_DOMAIN_2GHZ_MASK : REG_DOMAIN_5GHZ_MASK;
        return true;
    }
}

static bool
IS_BIT_SET(int bit, u_int64_t *bitmask)
{
    int byteOffset, bitnum;
    u_int64_t val;

    byteOffset = bit/64;
    bitnum = bit - byteOffset*64;
    val = ((u_int64_t) 1) << bitnum;
    if (bitmask[byteOffset] & val)
        return true;
    else
        return false;
}
    
/* Add given regclassid into regclassids array upto max of maxregids */
static void
ath_add_regclassid(u_int8_t *regclassids, u_int maxregids, u_int *nregids, u_int8_t regclassid)
{
    int i;

    /* Is regclassid valid? */
    if (regclassid == 0)
        return;

    for (i=0; i < maxregids; i++) {
        if (regclassids[i] == regclassid)
            return;
        if (regclassids[i] == 0)
            break;
    }

    if (i == maxregids)
        return;
    else {
        regclassids[i] = regclassid;
        *nregids += 1;
    }

    return;
}

static bool
getEepromRegExtBits(struct ath_hal *ah, REG_EXT_BITMAP bit)
{
    return ((AH_PRIVATE(ah)->ah_current_rd_ext & (1<<bit))? true : false);
}

/* Initialize NF cal history buffer */
static void 
ath_hal_init_NF_buffer(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichans,
                       int nchans)
{
    int i, j, next, nf_hist_len;

#ifdef ATH_NF_PER_CHAN
    nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
#else
    nf_hist_len = HAL_NF_CAL_HIST_LEN_SMALL;
#endif

    for (next = 0; next < nchans; next++) {
        int16_t noisefloor;
        noisefloor = IS_CHAN_2GHZ(&ichans[next]) ?
            AH_PRIVATE(ah)->nf_2GHz.nominal :
            AH_PRIVATE(ah)->nf_5GHz.nominal;
        ichans[next].nf_cal_hist.base.curr_index = 0;
        for (i = 0; i < NUM_NF_READINGS; i++) {
            ichans[next].nf_cal_hist.base.priv_nf[i] = noisefloor;
            for (j = 0; j < nf_hist_len; j++) {
                ichans[next].nf_cal_hist.nf_cal_buffer[j][i] = noisefloor;
            }
        }
    }
}

void __ahdecl ath_hal_init_NF_buffer_for_regdb_chans(struct ath_hal *ah, u_int cc)
{
    HAL_CHANNEL_INTERNAL *ichans = &AH_TABLES(ah)->ah_channels[0];
    int nchans = N(AH_TABLES(ah)->ah_channels);

    ath_hal_init_NF_buffer(ah, ichans, nchans);
}
#endif

#define IS_HT40_MODE(_mode)      \
                    (((_mode == HAL_MODE_11NA_HT40PLUS  || \
                     _mode == HAL_MODE_11NG_HT40PLUS    || \
                     _mode == HAL_MODE_11NA_HT40MINUS   || \
                     _mode == HAL_MODE_11NG_HT40MINUS) ? true : false))

/*
 * Setup the channel list based on the information in the EEPROM and
 * any supplied country code.  Note that we also do a bunch of EEPROM
 * verification here and setup certain regulatory-related access
 * control data used later on.
 */

#if !NO_HAL
bool __ahdecl
ath_hal_init_channels(struct ath_hal *ah,
              HAL_CHANNEL *chans, u_int maxchans, u_int *nchans,
              u_int8_t *regclassids, u_int maxregids, u_int *nregids,
              HAL_CTRY_CODE cc, u_int32_t modeSelect,
              bool enableOutdoor, bool enableExtendedChannels, 
              bool block_dfs_enable)
{
#define CHANNEL_HALF_BW        10
#define CHANNEL_QUARTER_BW    5
    u_int modesAvail;
    u_int16_t maxChan=7000;
    const COUNTRY_CODE_TO_ENUM_RD *country=AH_NULL;
    REG_DOMAIN rd5GHz, rd2GHz;
    const struct cmode *cm;
    HAL_CHANNEL_INTERNAL *ichans=&AH_TABLES(ah)->ah_channels[0];
    int next=0,b;
    u_int8_t ctl;
    int is_quarterchan_cap, is_halfchan_cap, is_49ghz_cap;
    HAL_DFS_DOMAIN dfsDomain=0;
    int regdmn;
    u_int16_t chanSep;
#ifndef ATH_NO_5G_SUPPORT
    u_int ch144;
#endif 

    HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: cc %u mode 0x%x%s%s\n", __func__,
         cc, modeSelect, enableOutdoor? " Enable outdoor" : " ",
         enableExtendedChannels ? " Enable ecm" : "");

    memset(&rd5GHz,0,sizeof(REG_DOMAIN));
    memset(&rd2GHz,0,sizeof(REG_DOMAIN));

    /*
     * We now have enough state to validate any country code
     * passed in by the caller.
     */
    if (!isCountryCodeValid(ah, cc)) {
        /* NB: Atheros silently ignores invalid country codes */
        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: invalid country code %d\n",
                __func__, cc);
        return false;
    }

    /*
     * Validate the EEPROM setting and setup defaults
     */
    if (!isEepromValid(ah)) {
        /*
         * Don't return any channels if the EEPROM has an
         * invalid regulatory domain/country code setting.
         */
        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: invalid EEPROM contents\n",__func__);
        return false;
    }

    AH_PRIVATE(ah)->ah_countryCode = getDefaultCountry(ah);

#if (AH_RADAR_CALIBRATE != 0)
    if ((AH_PRIVATE(ah)->ah_devid != AR5210_AP) ||
        (AH_PRIVATE(ah)->ah_devid != AR5210_PROD) ||
        (AH_PRIVATE(ah)->ah_devid != AR5210_DEFAULT) ||
        (AH_PRIVATE(ah)->ah_devid != AR5211_DEVID) ||
        (AH_PRIVATE(ah)->ah_devid != AR5311_DEVID) ||
        (AH_PRIVATE(ah)->ah_devid != AR5211_FPGA11B) ||
        (AH_PRIVATE(ah)->ah_devid != AR5211_DEFAULT)) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "Overriding country code and reg domain for DFS testing\n");
        AH_PRIVATE(ah)->ah_countryCode = CTRY_UZBEKISTAN;
        AH_PRIVATE(ah)->ah_current_rd = FCC3_FCCA;
    }
#endif

    if (AH_PRIVATE(ah)->ah_countryCode == CTRY_DEFAULT) {
        /*
         * XXX - TODO: Switch based on Japan country code
         * to new reg domain if valid japan card
         */
        AH_PRIVATE(ah)->ah_countryCode = cc & COUNTRY_CODE_MASK;

        if ((AH_PRIVATE(ah)->ah_countryCode == CTRY_DEFAULT) &&
                (getEepromRD(ah) == CTRY_DEFAULT)) {
                /* Set to DEBUG_REG_DMN for debug or set to CTRY_UNITED_STATES for testing */
                AH_PRIVATE(ah)->ah_countryCode = CTRY_UNITED_STATES;
        }
    }

#ifdef AH_SUPPORT_11D
    if(AH_PRIVATE(ah)->ah_countryCode == CTRY_DEFAULT)
    {
        regdmn =getEepromRD(ah) & ~COUNTRY_ERD_FLAG;
        country = AH_NULL;
    }
    else {
#endif
        /* Get pointers to the country element and the reg domain elements */
        country = findCountry(AH_PRIVATE(ah)->ah_countryCode, getEepromRD(ah));

        if (country == AH_NULL) {
            HDPRINTF(ah, HAL_DBG_REGULATORY, "Country is NULL!!!!, cc= %d\n",
                AH_PRIVATE(ah)->ah_countryCode);
            return false;
        } else {
            regdmn = country->regDmnEnum;
#ifdef AH_SUPPORT_11D
            if (((getEepromRD(ah) & WORLD_SKU_MASK) == WORLD_SKU_PREFIX) && (cc == CTRY_UNITED_STATES)) {
                if(!isWwrSKU_NoMidband(ah) && isFCCMidbandSupported(ah))
                    regdmn = FCC3_FCCA;
                else
                    regdmn = FCC1_FCCA;
            }
#endif
        }
#ifdef AH_SUPPORT_11D
    }
#endif

#ifndef ATH_NO_5G_SUPPORT
    if (!getWmRD(ah, regdmn, ~CHANNEL_2GHZ, &rd5GHz)) {
        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: couldn't find unitary 5GHz reg domain for country %u\n",
             __func__, AH_PRIVATE(ah)->ah_countryCode);
        return false;
    }
#endif
    if (!getWmRD(ah, regdmn, CHANNEL_2GHZ, &rd2GHz)) {
        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: couldn't find unitary 2GHz reg domain for country %u\n",
             __func__, AH_PRIVATE(ah)->ah_countryCode);
        return false;
    }

    if (rd5GHz.dfsMask & DFS_FCC3)
        dfsDomain = DFS_FCC_DOMAIN;
    if (rd5GHz.dfsMask & DFS_MKK4)
        dfsDomain = DFS_MKK4_DOMAIN;
    if (rd5GHz.dfsMask & DFS_ETSI)
        dfsDomain = DFS_ETSI_DOMAIN;
    if (dfsDomain != AH_PRIVATE(ah)->ah_dfsDomain) {
        AH_PRIVATE(ah)->ah_dfsDomain = dfsDomain;
    }
    if(country == AH_NULL) {
        modesAvail = ath_hal_getWirelessModes(ah);
    }
    else {
        modesAvail = ath_hal_getwmodesnreg(ah, country, &rd5GHz);

        if (cc == CTRY_DEBUG) {
            if (modesAvail & HAL_MODE_11N_MASK) {
                modeSelect &= HAL_MODE_11N_MASK;
            }
        }
		/* To skip 5GHZ channel when regulatory domain is NULL1_WORLD */
		if ( regdmn == NULL1_WORLD) {
            modeSelect &= ~(HAL_MODE_11A|HAL_MODE_TURBO|HAL_MODE_108A|HAL_MODE_11A_HALF_RATE|HAL_MODE_11A_QUARTER_RATE
							|HAL_MODE_11NA_HT20|HAL_MODE_11NA_HT40PLUS|HAL_MODE_11NA_HT40MINUS);
        }

        if (!enableOutdoor)
            maxChan = country->outdoorChanStart;
        }

    next = 0;

    if (maxchans > N(AH_TABLES(ah)->ah_channels))
        maxchans = N(AH_TABLES(ah)->ah_channels);

    is_halfchan_cap = AH_PRIVATE(ah)->ah_caps.hal_chan_half_rate;
    is_quarterchan_cap = AH_PRIVATE(ah)->ah_caps.hal_chan_quarter_rate;
    is_49ghz_cap = AH_PRIVATE(ah)->ah_caps.hal49Ghz;

#if ICHAN_WAR_SYNCH
    AH_PRIVATE(ah)->ah_ichan_set = true;
    spin_lock(&AH_PRIVATE(ah)->ah_ichan_lock);
#endif
    for (cm = modes; cm < &modes[N(modes)]; cm++) {
        u_int16_t c, c_hi, c_lo;
        u_int64_t *channelBM=AH_NULL;
        REG_DOMAIN *rd=AH_NULL;
        const REG_DMN_FREQ_BAND *fband=AH_NULL,*freqs;
        int8_t low_adj=0, hi_adj=0;

        if ((cm->mode & modeSelect) == 0) {
            HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: skip mode 0x%x flags 0x%x\n",
                 __func__, cm->mode, cm->flags);
            continue;
        }
        if ((cm->mode & modesAvail) == 0) {
            HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: !avail mode 0x%x (0x%x) flags 0x%x\n",
                 __func__, modesAvail, cm->mode, cm->flags);
            continue;
        }
        if (!ath_hal_getChannelEdges(ah, cm->flags, &c_lo, &c_hi)) {
            /* channel not supported by hardware, skip it */
            HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: channels 0x%x not supported by hardware\n",
                 __func__,cm->flags);
            continue;
        }
        switch (cm->mode) {
#ifndef ATH_NO_5G_SUPPORT
        case HAL_MODE_TURBO:
            rd = &rd5GHz;
            channelBM = rd->chan11a_turbo;
            freqs = &regDmn5GhzTurboFreq[0];
            ctl = rd->conformance_test_limit | CTL_TURBO;
            break;
        case HAL_MODE_11A:
        case HAL_MODE_11NA_HT20:
        case HAL_MODE_11NA_HT40PLUS:
        case HAL_MODE_11NA_HT40MINUS:
            rd = &rd5GHz;
            channelBM = rd->chan11a;
            freqs = &regDmn5GhzFreq[0];
            ctl = rd->conformance_test_limit;
            break;
#endif
        case HAL_MODE_11B:
            rd = &rd2GHz;
            channelBM = rd->chan11b;
            freqs = &regDmn2GhzFreq[0];
            ctl = rd->conformance_test_limit | CTL_11B;
            break;
        case HAL_MODE_11G:
        case HAL_MODE_11NG_HT20:
        case HAL_MODE_11NG_HT40PLUS:
        case HAL_MODE_11NG_HT40MINUS:
            rd = &rd2GHz;
            channelBM = rd->chan11g;
            freqs = &regDmn2Ghz11gFreq[0];
            ctl = rd->conformance_test_limit | CTL_11G;
            break;
        case HAL_MODE_11G_TURBO:
            rd = &rd2GHz;
            channelBM = rd->chan11g_turbo;
            freqs = &regDmn2Ghz11gTurboFreq[0];
            ctl = rd->conformance_test_limit | CTL_108G;
            break;
#ifndef ATH_NO_5G_SUPPORT
        case HAL_MODE_11A_TURBO:
            rd = &rd5GHz;
            channelBM = rd->chan11a_dyn_turbo;
            freqs = &regDmn5GhzTurboFreq[0];
            ctl = rd->conformance_test_limit | CTL_108G;
            break;
#endif
        default:
            HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: Unkonwn HAL mode 0x%x\n",
                    __func__, cm->mode);
            continue;
        }
        if (isChanBitMaskZero(channelBM))
            continue;


        if ((cm->mode == HAL_MODE_11NA_HT40PLUS) ||
            (cm->mode == HAL_MODE_11NG_HT40PLUS)) {
                hi_adj = -20;
        }

        if ((cm->mode == HAL_MODE_11NA_HT40MINUS) ||
            (cm->mode == HAL_MODE_11NG_HT40MINUS)) {
                low_adj = 20;
        }

        for (b=0;b<64*BMLEN; b++) {
            if (IS_BIT_SET(b,channelBM)) {
                fband = &freqs[b];

#ifndef ATH_NO_5G_SUPPORT
                if (block_dfs_enable)
                {
                    if (fband->useDfs != NO_DFS)
                        continue;
                }
                /* 
                 * MKK capability check. U1 ODD, U1 EVEN, U2, and MIDBAND bits are checked here. 
                 * Don't add the band to channel list if the corresponding bit is not set.
                 */
                if ((cm->flags & CHANNEL_5GHZ) && (rd5GHz.regDmnEnum == MKK1 || rd5GHz.regDmnEnum == MKK2)) {
                    int i, skipband=0;
                    u_int32_t regcap;

                    for (i = 0; i < N(j_bandcheck); i++) {
                        if (j_bandcheck[i].freqbandbit == b) {
                            ath_hal_getcapability(ah, HAL_CAP_REG_FLAG, 0, &regcap);
                            if ((j_bandcheck[i].eepromflagtocheck & regcap) == 0) {
                                skipband = 1;
                            }
                            break;
                        }
                    }
                    if (skipband) {
                        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: Skipping %d freq band.\n",
                                __func__, j_bandcheck[i].freqbandbit);
                        continue;
                    }
                }
#endif

                ath_add_regclassid(regclassids, maxregids,
                                   nregids, fband->regClassId);

                if ((rd == &rd5GHz) && (fband->lowChannel < 5180) && (!is_49ghz_cap))
                    continue;

                if (IS_HT40_MODE(cm->mode) && (rd == &rd5GHz)) {
                    /* For 5G HT40 mode, channel seperation should be 40. */
                    chanSep = 40;

                    /*
                     * For all 4.9G Hz and Unii1 odd channels, HT40 is not allowed. 
                     * Except for CTRY_DEBUG.
                     */
                    if ((fband->lowChannel < 5180) && (cc != CTRY_DEBUG)) {
                        continue;
                    }

                    /*
                     * This is mainly for F1_5280_5320, where 5280 is not a HT40_PLUS channel.
                     * The way we generate HT40 channels is assuming the freq band starts from 
                     * a HT40_PLUS channel. Shift the low_adj by 20 to make it starts from 5300.
                     */
                    if ((fband->lowChannel == 5280) || (fband->lowChannel == 4920)) {
                        low_adj += 20;
                    }
                }
                else {
                    chanSep = fband->channelSep;

                    if (cm->mode == HAL_MODE_11NA_HT20) {
                        /*
                         * For all 4.9G Hz and Unii1 odd channels, HT20 is not allowed either. 
                         * Except for CTRY_DEBUG.
                         */
                        if ((fband->lowChannel < 5180) && (cc != CTRY_DEBUG)) {
                            continue;
                        }
                    }
                }

                for (c=fband->lowChannel + low_adj;
                     ((c <= (fband->highChannel + hi_adj)) &&
                      (c >= (fband->lowChannel + low_adj)));
                     c += chanSep) {
                    HAL_CHANNEL_INTERNAL icv;

                    if (!(c_lo <= c && c <= c_hi)) {
                        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: c %u out of range [%u..%u]\n",
                             __func__, c, c_lo, c_hi);
                        continue;
                    }
                    if ((fband->channelBW ==
                            CHANNEL_HALF_BW) &&
                        !is_halfchan_cap) {
                        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: Skipping %u half rate channel\n",
                                __func__, c);
                        continue;
                    }

                    if ((fband->channelBW ==
                        CHANNEL_QUARTER_BW) &&
                        !is_quarterchan_cap) {
                        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: Skipping %u quarter rate channel\n",
                                __func__, c);
                        continue;
                    }

                    if (((c+fband->channelSep)/2) > (maxChan+HALF_MAXCHANBW)) {
                        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: c %u > maxChan %u\n",
                             __func__, c, maxChan);
                        continue;
                    }
                    if (next >= maxchans){
                        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: too many channels for channel table\n",
                             __func__);
                        goto done;
                    }
                    if ((fband->usePassScan & IS_ECM_CHAN) &&
                        !enableExtendedChannels && (c >= 2467)) {
                        HDPRINTF(ah, HAL_DBG_REGULATORY, "Skipping ecm channel\n");
                        continue;
                    }
                    if ((rd->flags & NO_HOSTAP) &&
                        (AH_PRIVATE(ah)->ah_opmode == HAL_M_HOSTAP)) {
                        HDPRINTF(ah, HAL_DBG_REGULATORY, "Skipping HOSTAP channel\n");
                        continue;
                    }
                    // This code makes sense for only the AP side. The following if's deal specifically
                    // with Owl since it does not support radar detection on extension channels.
                    // For STA Mode, the AP takes care of switching channels when radar detection
                    // happens. Also, for SWAP mode, Maverick does not allow DFS channels, and so allowing
                    // HT40 operation on these channels is OK. 
                    if (IS_HT40_MODE(cm->mode) &&
                        !(getEepromRegExtBits(ah,REG_EXT_FCC_DFS_HT40)) &&
                        (fband->useDfs) && (rd->conformance_test_limit != MKK)) {
                        /*
                         * Only MKK CTL checks REG_EXT_JAPAN_DFS_HT40 for DFS HT40 support.
                         * All other CTLs should check REG_EXT_FCC_DFS_HT40
                         */
                        HDPRINTF(ah, HAL_DBG_REGULATORY, "Skipping HT40 channel (en_fcc_dfs_ht40 = 0)\n");
                        continue;
                    }
                    if (IS_HT40_MODE(cm->mode) &&
                        !(getEepromRegExtBits(ah,REG_EXT_JAPAN_NONDFS_HT40)) &&
                        !(fband->useDfs) && (rd->conformance_test_limit == MKK)) {
                        HDPRINTF(ah, HAL_DBG_REGULATORY, "Skipping HT40 channel (en_jap_ht40 = 0)\n");
                        continue;
                    }
                    if (IS_HT40_MODE(cm->mode) &&
                        !(getEepromRegExtBits(ah,REG_EXT_JAPAN_DFS_HT40)) &&
                        (fband->useDfs) && (rd->conformance_test_limit == MKK) ) {
                        HDPRINTF(ah, HAL_DBG_REGULATORY, "Skipping HT40 channel (en_jap_dfs_ht40 = 0)\n");
                        continue;
                    }

#ifndef ATH_NO_5G_SUPPORT
                    if(rd == &rd5GHz)
                    {
                        /*
                            This is for enabling channel 144 for FCC3 and FCC6 according to
                            RD extension bit 5 and software 144 setting
                            RD extension bit 5 == 0. Ch. 144 is not certified. No Tx allowed in
                            Ch. 144 even Ch. 144 is extension channel.
                            
                        */

                        if((b == F1_5500_5720) || (b == F2_5660_5720) || (b == F3_5660_5720)) {

                            ch144 = AH_PRIVATE(ah)->ah_config.ath_hal_ch144;

                            if((c == 5700) && (cm->mode & (HAL_MODE_11NA_HT40PLUS))
                                && (!getEepromRegExtBits(ah, REG_EXT_FCC_CH_144) | !(ch144 & REG_CH144_SEC20_ALLOWED))) {
                                /*
                                    Skipping 140 VHT40 and VHT80 channel when RD bit 5 == 0 or
                                    Ch. 144 sec20 is not allowed
                                */
                                continue;                                 
                            }
                    
                            if(c == 5720 && (!getEepromRegExtBits(ah, REG_EXT_FCC_CH_144) |
                                !(ch144 & REG_CH144_PRI20_ALLOWED))) {
                                /*
                                    Skipping all 144 channel. when RD bit 5 == 0 or Ch. 144 pri20 
                                    is not allowed 
                                */
                                continue;
                            }
                        }
                    }
#endif 

                    OS_MEMZERO(&icv, sizeof(icv));
                    icv.channel = c;
                    icv.channel_flags = cm->flags;

                    switch (fband->channelBW) {
                        case CHANNEL_HALF_BW:
                            icv.channel_flags |= CHANNEL_HALF;
                            break;
                        case CHANNEL_QUARTER_BW:
                            icv.channel_flags |= CHANNEL_QUARTER;
                            break;
                    }

                    icv.max_reg_tx_power = fband->powerDfs;
                    icv.antennaMax = fband->antennaMax;
                    icv.regDmnFlags = rd->flags;
                    icv.conformance_test_limit = ctl;
                    icv.regClassId = fband->regClassId;
                    if (fband->usePassScan & rd->pscan)
                    {
                        /* For WG1_2412_2472, the whole band marked as PSCAN_WWR, but only channel 12-13 need to be passive. */
                        if (!(fband->usePassScan & PSCAN_EXT_CHAN) ||
                            (icv.channel >= 2467))
                        {
                            icv.channel_flags |= CHANNEL_PASSIVE;
                        }
						else if ((fband->usePassScan & PSCAN_MKKA_G) && (rd->pscan & PSCAN_MKKA_G))
						{
							icv.channel_flags |= CHANNEL_PASSIVE;
						}
                        else
                        {
                            icv.channel_flags &= ~CHANNEL_PASSIVE;
                        }
                    }
                    else
                    {
                        icv.channel_flags &= ~CHANNEL_PASSIVE;
                    }
                    if (fband->useDfs & rd->dfsMask)
                        icv.priv_flags = CHANNEL_DFS;
                    else
                        icv.priv_flags = 0;
                    #if 0 /* Enabling 60 sec startup listen for ETSI */
                    /* Don't use 60 sec startup listen for ETSI yet */
                    if (fband->useDfs & rd->dfsMask & DFS_ETSI)
                        icv.priv_flags |= CHANNEL_DFS_CLEAR;
                    #endif
                    if (rd->flags & LIMIT_FRAME_4MS)
                        icv.priv_flags |= CHANNEL_4MS_LIMIT;

                    /* Check for ad-hoc allowableness */
                    /* To be done: DISALLOW_ADHOC_11A_TURB should allow ad-hoc */
                    if (icv.priv_flags & CHANNEL_DFS) {
                        icv.priv_flags |= CHANNEL_DISALLOW_ADHOC;
                    }

                    if (icv.regDmnFlags & CHANNEL_NO_HOSTAP) {
                        icv.priv_flags |= CHANNEL_NO_HOSTAP;
                    }

                    if (icv.regDmnFlags & ADHOC_PER_11D) {
                        icv.priv_flags |= CHANNEL_PER_11D_ADHOC;
                    }
                    if (icv.channel_flags & CHANNEL_PASSIVE) {
                        /* Allow channel 1-11 as ad-hoc channel even marked as passive */
                        if ((icv.channel < 2412) || (icv.channel > 2462)) {
                            if (rd5GHz.regDmnEnum == MKK1 || rd5GHz.regDmnEnum == MKK2) {
                                u_int32_t regcap;

                                ath_hal_getcapability(ah, HAL_CAP_REG_FLAG, 0, &regcap);
                                if (!(regcap & (AR_EEPROM_EEREGCAP_EN_KK_U1_EVEN |
                                        AR_EEPROM_EEREGCAP_EN_KK_U2 |
                                        AR_EEPROM_EEREGCAP_EN_KK_MIDBAND)) 
                                        && isUNII1OddChan(icv.channel)) {
                                    /* 
                                     * There is no RD bit in EEPROM. Unii 1 odd channels
                                     * should be active scan for MKK1 and MKK2.
                                     */
                                    icv.channel_flags &= ~CHANNEL_PASSIVE;
                                }
                                else {
                                    icv.priv_flags |= CHANNEL_DISALLOW_ADHOC;
                                }
                            }
                            else {
                                icv.priv_flags |= CHANNEL_DISALLOW_ADHOC;
                            }
                        }
                    }
#ifndef ATH_NO_5G_SUPPORT
                    if (cm->mode & (HAL_MODE_TURBO | HAL_MODE_11A_TURBO)) {
                        if( icv.regDmnFlags & DISALLOW_ADHOC_11A_TURB) {
                            icv.priv_flags |= CHANNEL_DISALLOW_ADHOC;
                        }
                    }
#endif
                    else if (cm->mode & (HAL_MODE_11A | HAL_MODE_11NA_HT20 |
                        HAL_MODE_11NA_HT40PLUS | HAL_MODE_11NA_HT40MINUS)) {
                        if( icv.regDmnFlags & (ADHOC_NO_11A | DISALLOW_ADHOC_11A)) {
                            icv.priv_flags |= CHANNEL_DISALLOW_ADHOC;
                        }
                    }

                    if (rd == &rd2GHz) {

                        if ((icv.channel < (fband->lowChannel + 10)) ||
                            (icv.channel > (fband->highChannel - 10))) {
                            icv.priv_flags |= CHANNEL_EDGE_CH;
                        }

                        if ((icv.channel_flags & CHANNEL_HT40PLUS) &&
                            (icv.channel > (fband->highChannel - 30))) {
                            icv.priv_flags |= CHANNEL_EDGE_CH; /* Extn Channel */
                        }

                        if ((icv.channel_flags & CHANNEL_HT40MINUS) &&
                            (icv.channel < (fband->lowChannel + 30))) {
                            icv.priv_flags |= CHANNEL_EDGE_CH; /* Extn Channel */
                        }
                    }

        #ifdef ATH_IBSS_DFS_CHANNEL_SUPPORT
                    /* EV 83788, 83790 support 11A adhoc in ETSI domain. Other
                     * domain will remain as their original settings for now since there
                     * is no requirement.
                     * This will open all 11A adhoc in ETSI, no matter DFS/non-DFS
                     */
                    if (dfsDomain == DFS_ETSI_DOMAIN) {
                        icv.priv_flags &= ~CHANNEL_DISALLOW_ADHOC;
                    }
        #endif

                    OS_MEMCPY(&ichans[next++], &icv, sizeof(HAL_CHANNEL_INTERNAL));
                }
                /* Restore normal low_adj value. */
                if (IS_HT40_MODE(cm->mode)) {
                    if (fband->lowChannel == 5280 || fband->lowChannel == 4920) {
                        low_adj -= 20;
                    }
                }
            }
        }
    }
done:    if (next != 0) {
        int i;

        /* XXX maxchans set above so this cannot happen? */
        if (next > N(AH_TABLES(ah)->ah_channels)) {
            HDPRINTF(ah, HAL_DBG_REGULATORY,
                 "%s: too many channels %u; truncating to %u\n",
                 __func__, next,
                 (unsigned) N(AH_TABLES(ah)->ah_channels));
            next = N(AH_TABLES(ah)->ah_channels);
        }

        /* Initialize NF cal history buffer */
        ath_hal_init_NF_buffer(ah, ichans, next);

#if ATH_SUPPORT_RADIO_RETENTION
        /* Initialize radio retention table */
        for (i = 0; i < next; i++) {
            ichans[i].rtt.saved = 0;
            ichans[i].rtt.last = 0;
        }
#endif

        /*
         * Keep a private copy of the channel list so we can
         * constrain future requests to only these channels
         */
        ath_hal_sort(ichans, next, sizeof(HAL_CHANNEL_INTERNAL), chansort);
        AH_PRIVATE(ah)->ah_nchan = next;

        /*
         * Copy the channel list to the public channel list
         */
        HDPRINTF(ah, HAL_DBG_REGULATORY, "Channel list:\n");
        for (i=0; i<next; i++) {
            HDPRINTF(ah, HAL_DBG_REGULATORY, "chan: %d flags: 0x%x\n", ichans[i].channel, ichans[i].channel_flags);
            chans[i].channel = ichans[i].channel;
            chans[i].channel_flags = ichans[i].channel_flags;
            chans[i].priv_flags = ichans[i].priv_flags;
            chans[i].max_reg_tx_power = ichans[i].max_reg_tx_power;
            chans[i].regClassId = ichans[i].regClassId;
        }
        /*
         * Retrieve power limits.
         */
        ath_hal_getpowerlimits(ah, chans, next);
        for (i=0; i<next; i++) {
            ichans[i].max_tx_power = chans[i].max_tx_power;
            ichans[i].min_tx_power = chans[i].min_tx_power;
            /*
             * Retrieve power limit for all rates
             */
            AH_PRIVATE(ah)->ah_get_rate_power_limit_from_eeprom(ah, chans[i].channel,
                                                    &chans[i].max_rate_power,
                                                    &chans[i].min_rate_power);
            ichans[i].max_rate_power = chans[i].max_rate_power;
            ichans[i].min_rate_power = chans[i].min_rate_power;
        }
    }
    else {
        AH_PRIVATE(ah)->ah_nchan = next;
    }
    *nchans = next;

    /* save for later query */
    AH_PRIVATE(ah)->ah_currentRDInUse = regdmn;
    AH_PRIVATE(ah)->ah_currentRD5G = rd5GHz.regDmnEnum;
    AH_PRIVATE(ah)->ah_currentRD2G = rd2GHz.regDmnEnum;
    if(country == AH_NULL) {
        AH_PRIVATE(ah)->ah_iso[0] = 0;
        AH_PRIVATE(ah)->ah_iso[1] = 0;
    }
    else {
        AH_PRIVATE(ah)->ah_iso[0] = country->isoName[0];
        AH_PRIVATE(ah)->ah_iso[1] = country->isoName[1];
    }

#if ICHAN_WAR_SYNCH
    AH_PRIVATE(ah)->ah_ichan_set = false;
    spin_unlock(&AH_PRIVATE(ah)->ah_ichan_lock);
#endif

    return (next != 0);
#undef CHANNEL_HALF_BW
#undef CHANNEL_QUARTER_BW
}

#endif
/*
 * Return whether or not the specified channel is ok to use
 * based on the current regulatory domain constraints and 
 * DFS interference.
 */
HAL_CHANNEL_INTERNAL *
ath_hal_checkchannel(struct ath_hal *ah, const HAL_CHANNEL *c)
{
#define CHAN_FLAGS    (CHANNEL_ALL|CHANNEL_HALF|CHANNEL_QUARTER)
    HAL_CHANNEL_INTERNAL *base, *cc = AH_NULL;
    /* NB: be wary of user-specified channel flags */
    int flags = c->channel_flags & CHAN_FLAGS;
    int n, lim;

    HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: channel %u/0x%x (0x%x) requested\n",
        __func__, c->channel, c->channel_flags, flags);

#if ICHAN_WAR_SYNCH
    if (AH_PRIVATE(ah)->ah_ichan_set == true) {
        printk("Calling %s while ath_hal_init_channels in progress!!! Investigate!!\n", __func__);
        HALASSERT(0);
    }

    spin_lock(&AH_PRIVATE(ah)->ah_ichan_lock);
#endif

    /*
     * Check current channel to avoid the lookup.
     */
    cc = AH_PRIVATE(ah)->ah_curchan;
    if (cc != AH_NULL && cc->channel == c->channel &&
        (cc->channel_flags & CHAN_FLAGS) == flags) {
        if ((cc->priv_flags & CHANNEL_INTERFERENCE) &&
            (cc->priv_flags & CHANNEL_DFS)) {
            cc = AH_NULL;
        }   
        goto done;
    }

    /* binary search based on known sorting order */
    base = AH_TABLES(ah)->ah_channels;
    n = AH_PRIVATE(ah)->ah_nchan;
    /* binary search based on known sorting order */
    for (lim = n; lim != 0; lim >>= 1) {
        int d;
        cc = &base[lim>>1];
        d = c->channel - cc->channel;
        if (d == 0) {
            if ((cc->channel_flags & CHAN_FLAGS) == flags) {
                if ((cc->priv_flags & CHANNEL_INTERFERENCE) &&
                    (cc->priv_flags & CHANNEL_DFS)) {
                    cc = AH_NULL;
                }
                goto done;
            }
            d = flags - (cc->channel_flags & CHAN_FLAGS);
        }
        HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: channel %u/0x%x d %d\n", __func__,
            cc->channel, cc->channel_flags, d);
        if (d > 0) {
            base = cc + 1;
            lim--;
        }
    }
    HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: no match for %u/0x%x\n",
        __func__, c->channel, c->channel_flags);

    cc = AH_NULL;

done:

#if ICHAN_WAR_SYNCH
    spin_unlock(&AH_PRIVATE(ah)->ah_ichan_lock);
#endif

    return cc;
#undef CHAN_FLAGS
}

/*
 * Return the max allowed antenna gain and apply any regulatory
 * domain specific changes.
 *
 * NOTE: a negative reduction is possible in RD's that only
 * measure radiated power (e.g., ETSI) which would increase
 * that actual conducted output power (though never beyond
 * the calibrated target power).
 */
u_int
ath_hal_getantennareduction(struct ath_hal *ah, HAL_CHANNEL *chan, u_int twiceGain)
{
    HAL_CHANNEL_INTERNAL *ichan=AH_NULL;
    int8_t antennaMax;

    if ((ichan = ath_hal_checkchannel(ah, chan)) != AH_NULL) {
        antennaMax = twiceGain - ichan->antennaMax*2;
        return (antennaMax < 0) ? 0 : antennaMax;
    } else {
        /* Failed to find the correct index - may be a debug channel */
        return 0;
    }
}

u_int
ath_hal_getantennaallowed(struct ath_hal *ah, HAL_CHANNEL *chan)
{
            HAL_CHANNEL_INTERNAL *ichan = AH_NULL;

                ichan = ath_hal_checkchannel(ah, chan);
                    if (!ichan)
                                return 0;

                        return ichan->antennaMax;
}


/* XXX - KCYU - fix this code ... maybe move ctl decision into channel set area or
 into the tables so no decision is needed in the code */

/*
 * Return the test group from the specified channel from
 * the regulatory table.
 *
 * TODO: CTL for 11B CommonMode when regulatory domain is unknown
 */
u_int
ath_hal_getctl(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int ctl=NO_CTL;
    HAL_CHANNEL_INTERNAL *ichan;

    /* Special CTL to signify WWR SKU without a known country */
    if (AH_PRIVATE(ah)->ah_countryCode == CTRY_DEFAULT && isWwrSKU(ah)) {
        if (IS_CHAN_B(chan)) {
            ctl = SD_NO_CTL | CTL_11B;
        } else if (IS_CHAN_G(chan)) {
            ctl = SD_NO_CTL | CTL_11G;
        } else if (IS_CHAN_108G(chan)) {
            ctl = SD_NO_CTL | CTL_108G;
        } else if (IS_CHAN_T(chan)) {
            ctl = SD_NO_CTL | CTL_TURBO;
        } else {
            ctl = SD_NO_CTL | CTL_11A;
        }
    } else {
        if ((ichan = ath_hal_checkchannel(ah, chan)) != AH_NULL) {
            ctl = ichan->conformance_test_limit;
            /* Atheros change# 73449: limit 11G OFDM power */
            if (IS_CHAN_PUREG(chan) && (ctl & 0xf) == CTL_11B)
                ctl = (ctl &~ 0xf) | CTL_11G;
        }
    }
    return ctl;
}

/*
 * Return whether or not a noise floor check is required in
 * the current regulatory domain for the specified channel.
 */

bool
ath_hal_getnfcheckrequired(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    HAL_CHANNEL_INTERNAL *ichan;

    if ((ichan = ath_hal_checkchannel(ah, chan)) != AH_NULL) {
        return ((ichan->regDmnFlags & NEED_NFC) ? true : false);
    }
    return false;
}


/*
 * Insertion sort.
 */
#define ath_hal_swap(_a, _b, _size) {            \
    u_int8_t *s = _b;                    \
    int i = _size;                       \
    do {                                 \
        u_int8_t tmp = *_a;              \
        *_a++ = *s;                      \
        *s++ = tmp;                      \
    } while (--i);                       \
    _a -= _size;                         \
}

#if !NO_HAL
static void
ath_hal_sort(void *a, u_int32_t n, u_int32_t size, ath_hal_cmp_t *cmp)
{
    u_int8_t *aa = a;
    u_int8_t *ai, *t;

    for (ai = aa+size; --n >= 1; ai += size)
        for (t = ai; t > aa; t -= size) {
            u_int8_t *u = t - size;
            if (cmp(u, t) <= 0)
                break;
            ath_hal_swap(u, t, size);
        }
}
#endif

HAL_CTRY_CODE __ahdecl findCountryCodeByRegDomain(HAL_REG_DOMAIN regdmn)
{
    int i;

    for (i = 0; i < Rdt.allCountriesCt; i++) {
        if (Rdt.allCountries[i].regDmnEnum == regdmn)
            return (Rdt.allCountries[i].countryCode);
    }
    return (0);        /* Not found */
}

#if !NO_HAL
void __ahdecl ath_hal_getCurrentCountry(void *ah, HAL_COUNTRY_ENTRY* ctry)
{
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);

    u_int16_t rd = getEepromRD(ah);

    ctry->isMultidomain = false;
    if(!(rd & COUNTRY_ERD_FLAG)) {
        ctry->isMultidomain = isWwrSKU(ah);
    }

    ctry->countryCode = ahpriv->ah_countryCode;
    ctry->regDmnEnum = ahpriv->ah_current_rd;
    ctry->regDmn5G = AH_PRIVATE(ah)->ah_currentRD5G;
    ctry->regDmn2G = AH_PRIVATE(ah)->ah_currentRD2G;
    ctry->iso[0] = AH_PRIVATE(ah)->ah_iso[0];
    ctry->iso[1] = AH_PRIVATE(ah)->ah_iso[1];
}
u_int8_t 
__ahdecl getCommonPower(u_int16_t freq)
{
    int i;

    for (i = 0; i < N(common_mode_pwrtbl); i++) {
         if (freq >= common_mode_pwrtbl[i].lchan && freq < common_mode_pwrtbl[i].hchan)
             return common_mode_pwrtbl[i].pwrlvl;
    }
    return 0;
}
#endif

int8_t
ath_hal_get_twice_max_regpower(struct ath_hal_private *ahpriv, HAL_CHANNEL_INTERNAL *ichan,
                            HAL_CHANNEL *chan)
{
    int8_t twice_max_reg_power =  ichan->max_reg_tx_power * 2;

    /*
     * ETSI UNII-II Ext band has different limits for STA and AP.
     * The software reg domain table contains the STA limits(23dBm).
     * For AP we adjust the max power(30dBm) dynamically based on
     * the mode of operation. 
     * Skipping this for ETSI8/ETSI9 (except for Maverick) as MaxRegPower is 20dB for all 
     * channels(EV# 89129)
     */
    
    if ((ahpriv->ah_opmode == HAL_M_HOSTAP) &&
        is_reg_dmn_etsi(ichan->conformance_test_limit ) &&
        (!((ahpriv->ah_currentRD5G == ETSI8)|| 
               (ahpriv->ah_currentRD5G == ETSI9))) &&
        (ichan->channel >= 5500) && (ichan->channel <= 5700)){
        twice_max_reg_power += 14;  /* (23 * 2) + (7 * 2) = 60 */
    }

    return twice_max_reg_power;
}


#ifdef _UNINET_REGDMN_

HAL_BOOL ath_hal_is_valid_country_code(struct ath_hal *ah, HAL_CTRY_CODE cc)
{
	return isCountryCodeValid(ah, cc);
}

void ath_hal_set_regdmn_country(struct ath_hal *ah, uint16_t regdomain, HAL_CTRY_CODE cc, uint16_t dfs_domain)
{
    // should really translate between ieee and hal reg domain but hal reg domain shouldn't matter anymore 
	AH_PRIVATE(ah)->ah_countryCode = cc;
	AH_PRIVATE(ah)->ah_dfsDomain = (HAL_DFS_DOMAIN)dfs_domain;
	HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: RegDmn 0x%x Country code: %d DFS domain: %d\n", __func__, 
             AH_PRIVATE(ah)->ah_current_rd, AH_PRIVATE(ah)->ah_countryCode, AH_PRIVATE(ah)->ah_dfsDomain);
}

int ath_hal_set_channels(struct ath_hal *ah, int num_chan, HAL_CHANNEL *hchan)
{
	struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    struct ath_hal_private_tables *ah_chantable = AH_TABLES(ah);

	int i = 0;
	ahpriv->ah_nchan = num_chan;
	if (ahpriv->ah_nchan >= AH_MAX_CHANNELS)
	{
		HDPRINTF(ah, HAL_DBG_REGULATORY, "%s list too big %d, truncating\n", __func__, ahpriv->ah_nchan);
		ahpriv->ah_nchan = AH_MAX_CHANNELS;
	}
	for (i = 0; i < ahpriv->ah_nchan; i++, hchan++) {
		ah_chantable->ah_channels[i].channel       = hchan->channel;
		ah_chantable->ah_channels[i].channel_flags  = hchan->channel_flags;
		ah_chantable->ah_channels[i].priv_flags     = hchan->priv_flags;
		ah_chantable->ah_channels[i].max_reg_tx_power = hchan->max_reg_tx_power;
		ah_chantable->ah_channels[i].max_tx_power    = hchan->max_tx_power;
		ah_chantable->ah_channels[i].min_tx_power    = hchan->min_tx_power;
        /* Mark all calibrations on this channel as being invalid */
        ah_chantable->ah_channels[i].cal_valid      = 0;
	}
	// sort for use by ath_hal_checkchannel
	ath_hal_sort(&ah_chantable->ah_channels, ahpriv->ah_nchan, sizeof(HAL_CHANNEL_INTERNAL), chansort);
	/* Initialize NF cal history buffer */
	ath_hal_init_NF_buffer(ah, ah_chantable->ah_channels, ahpriv->ah_nchan);
	return 0;
}

#endif /* _UNINET_REGDMN_ */

uint32_t ath_hal_get_chip_mode(struct ath_hal *ah)
{
    uint32_t modesAvail;

    modesAvail = ath_hal_getWirelessModes(ah);

    return modesAvail;
}

void ath_hal_get_freq_range(struct ath_hal *ah,
        uint32_t flags,
        uint32_t *low_freq,
        uint32_t *high_freq)
{
    uint16_t tmp_low_freq, tmp_high_freq;

    ath_hal_getChannelEdges(ah, (uint16_t)flags, &tmp_low_freq, &tmp_high_freq);
    *low_freq = (uint32_t)tmp_low_freq;
    *high_freq = (uint32_t)tmp_high_freq;
}


#undef N
