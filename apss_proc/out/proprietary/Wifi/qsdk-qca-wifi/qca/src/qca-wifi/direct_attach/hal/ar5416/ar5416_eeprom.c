/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2008-2010, Atheros Communications Inc.
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
 *
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5416

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"
#ifdef AH_DEBUG
#include "ah_desc.h"                    /* NB: for HAL_PHYERR* */
#endif
#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"

static u_int16_t ar5416EepromGetSpurChan(struct ath_hal *ah, u_int16_t spurChan,HAL_BOOL is2GHz);
static inline HAL_BOOL ar5416FillEeprom(struct ath_hal *ah);
static inline HAL_STATUS ar5416CheckEeprom(struct ath_hal *ah);

int __ahdecl ath_hal_fs_read(char *filename,
                loff_t offset,
                unsigned int size,
                uint8_t *buffer);

#ifdef AH_PRIVATE_DIAG
static inline void ar5416FillEmuEeprom(struct ath_hal_5416 *ahp);
#else
#define ar5416FillEmuEeprom(_ahp)
#endif /* AH_PRIVATE_DIAG */

#define FIXED_CCA_THRESHOLD 15

/*****************************
 * Eeprom APIs for CB/XB only
 ****************************/

#ifdef ART_BUILD
/*
 * This is where we look for the calibration data.
 * must be set before ath_attach() is called
 */
#include "mEepStruct9287.h"
static int calibration_data_try = calibration_data_none;
static int calibration_data_try_address = 0;

/*
 * Set the type of memory used to store calibration data.
 * Used by nart to force reading/writing of a specific type.
 * The driver can normally allow autodetection
 * by setting source to calibration_data_none=0.
 */
void ar5416_calibration_data_set(struct ath_hal *ah, int32_t source)
{
    if (ah != 0) {
        AH5416(ah)->calibration_data_source = source;
    } else {
        calibration_data_try = source;
    }
}

int32_t ar5416_calibration_data_get(struct ath_hal *ah)
{
    if (ah != 0) {
        return AH5416(ah)->calibration_data_source;
    } else {
        return calibration_data_try;
    }
}

/*
 * Set the address of first byte used to store calibration data.
 * Used by nart to force reading/writing at a specific address.
 * The driver can normally allow autodetection by setting size=0.
 */
void ar5416_calibration_data_address_set(struct ath_hal *ah, int32_t size)
{
    if (ah != 0) {
        AH5416(ah)->calibration_data_source_address = size;
    } else {
        calibration_data_try_address = size;
    }
}

int32_t ar5416_calibration_data_address_get(struct ath_hal *ah)
{
    if (ah != 0) {
        return AH5416(ah)->calibration_data_source_address;
    } else {
        return calibration_data_try_address;
    }
}

/*
 * returns size of the physical eeprom in bytes.
 * 1024 and 2048 are normal sizes.
 * 0 means there is no eeprom.
 */
int32_t
ar5416_eeprom_size(struct ath_hal *ah)
{
    u_int16_t data;
    /*
     * first we'll try for 4096 bytes eeprom
     */
    if (ar5416EepromRead(ah, 2047, &data)) {
        if (data != 0) {
            return 4096;
        }
    }
    /*
     * then we'll try for 2048 bytes eeprom
     */
    if (ar5416EepromRead(ah, 1023, &data)) {
        if (data != 0) {
            return 2048;
        }
    }
    /*
     * then we'll try for 1024 bytes eeprom
     */
    if (ar5416EepromRead(ah, 511, &data)) {
        if (data != 0) {
            return 1024;
        }
    }
    return 0;
}
#endif /* ART_BUILD */

/*
 * Read 16 bits of data from offset into *data
 */
HAL_BOOL
ar5416EepromRead(struct ath_hal *ah, u_int off, u_int16_t *data)
{
    (void)OS_REG_READ(ah, AR5416_EEPROM_OFFSET + (off << AR5416_EEPROM_S));
    if (!ath_hal_wait(ah, AR_EEPROM_STATUS_DATA, AR_EEPROM_STATUS_DATA_BUSY
        | AR_EEPROM_STATUS_DATA_PROT_ACCESS, 0, AH_WAIT_TIMEOUT))
    {
        return AH_FALSE;
    }

    *data = MS(OS_REG_READ(ah, AR_EEPROM_STATUS_DATA), AR_EEPROM_STATUS_DATA_VAL);
    return AH_TRUE;
}

#ifdef AH_SUPPORT_WRITE_EEPROM
/*
 * Write 16 bits of data from data to the specified EEPROM offset.
 */
HAL_BOOL
ar5416EepromWrite(struct ath_hal *ah, u_int off, u_int16_t data)
{
    u_int32_t status;
    u_int32_t write_to = 50000;      /* write timeout */
    u_int32_t eepromValue;

    eepromValue = (u_int32_t)data & 0xffff;

    /* Setup EEPROM device to write */
    OS_REG_RMW(ah, AR_GPIO_INPUT_EN_VAL, AR_GPIO_JTAG_DISABLE, 0);
    OS_REG_RMW(ah, AR_GPIO_OUTPUT_MUX1, 0, 0x1f << 15);   /* Mux GPIO-3 as GPIO */
    OS_DELAY(1);
    OS_REG_RMW(ah, AR_GPIO_OE_OUT, 0xc0, 0xc0);     /* Configure GPIO-3 as output */
    OS_DELAY(1);
    OS_REG_RMW(ah, AR_GPIO_IN_OUT, 0, 1 << 3);       /* drive GPIO-3 low */
    OS_DELAY(1);

    /* Send write data, as 32 bit data */
    OS_REG_WRITE(ah, AR5416_EEPROM_OFFSET + (off << AR5416_EEPROM_S), eepromValue);

    /* check busy bit to see if eeprom write succeeded */
    while (write_to > 0) {
        status = OS_REG_READ(ah, AR_EEPROM_STATUS_DATA) &
                                    (AR_EEPROM_STATUS_DATA_BUSY |
                                     AR_EEPROM_STATUS_DATA_BUSY_ACCESS |
                                     AR_EEPROM_STATUS_DATA_PROT_ACCESS |
                                     AR_EEPROM_STATUS_DATA_ABSENT_ACCESS);
        if (status == 0) {
            OS_REG_RMW(ah, AR_GPIO_IN_OUT, 1<<3, 1<<3);       /* drive GPIO-3 hi */
            return AH_TRUE;
        }
        OS_DELAY(1);
        write_to--;
    }

    OS_REG_RMW(ah, AR_GPIO_IN_OUT, 1<<3, 1<<3);       /* drive GPIO-3 hi */
    return AH_FALSE;
}
#endif /* AH_SUPPORT_WRITE_EEPROM */

HAL_STATUS
ar5416GetRatePowerLimitFromEeprom(struct ath_hal *ah, u_int16_t freq,
                                  int8_t *max_rate_power, int8_t *min_rate_power)
{
    /* no support for 5416 HAL based chips */
    *max_rate_power = 0;
    *min_rate_power = 0;

    return HAL_OK;
}

#ifndef WIN32
/*************************
 * Flash APIs for AP only
 *************************/

static HAL_STATUS
ar5416FlashMap(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
#if defined(AR9100)
    ahp->ah_cal_mem = OS_REMAP(ah, AR5416_EEPROM_START_ADDR, AR5416_EEPROM_MAX);
#else
    ahp->ah_cal_mem = OS_REMAP((uintptr_t)AH_PRIVATE(ah)->ah_st, AR5416_EEPROM_MAX);
#endif
    if (!ahp->ah_cal_mem)
    {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: cannot remap eeprom region \n", __func__);
        return HAL_EIO;
    }

    return HAL_OK;
}

HAL_BOOL
ar5416FlashRead(struct ath_hal *ah, u_int off, u_int16_t *data)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    *data = ((u_int16_t *)ahp->ah_cal_mem)[off];
    return AH_TRUE;
}

HAL_BOOL
ar5416FlashWrite(struct ath_hal *ah, u_int off, u_int16_t data)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    ((u_int16_t *)ahp->ah_cal_mem)[off] = data;
    return AH_TRUE;
}
#endif /* WIN32 */



/***************************
 * Common APIs for AP/CB/XB
 ***************************/
#ifdef ART_BUILD
#include "instance.h"
#endif
#define MOUTPUT 2048
#define AR9287_FLASH_SIZE 16*1024  // byte addressable
#define FLASH_BASE_CALDATA_OFFSET  0x1000

#ifndef CALDATA0_FILE_PATH
#define CALDATA0_FILE_PATH	"/tmp/wifi0.caldata"
#endif

#ifndef CALDATA1_FILE_PATH
#define CALDATA1_FILE_PATH	"/tmp/wifi1.caldata"
#endif

#ifndef CALDATA2_FILE_PATH
#define CALDATA2_FILE_PATH	"/tmp/wifi2.caldata"
#endif

static HAL_BOOL ar5416_calibration_data_read_file(struct ath_hal *ah, u_int8_t *buffer, int many)
{
    int ret_val = -1;
    char * devname = NULL;
    char * filename;

    ath_hal_printf(ah, "Restoring Cal data from FS\n");

    devname = (char *)(AH_PRIVATE(ah)->ah_st);

    if(0 == strncmp("wifi0", devname, 5))
    {
        filename = CALDATA0_FILE_PATH;
    }
    else if(0 == strncmp("wifi1", devname, 5))
    {
        filename = CALDATA1_FILE_PATH;
    }
    else if(0 == strncmp("wifi2", devname, 5))
    {
        filename = CALDATA2_FILE_PATH;
    }
    else
    {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s[%d], Error: Please check why none of wifi0, wifi1 or wifi2 is your device name (%s).\n", __func__, __LINE__, devname);
        return -1;
    }

#if AH_DEBUG
    HDPRINTF(ah, HAL_DBG_EEPROM, "%s[%d] Get Caldata for %s.\n", __func__, __LINE__, devname);
#endif

    if(NULL == filename)
    {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s[%d], Error: File name is null, please assign right caldata file name.\n", __func__, __LINE__);
        return -1;
    }

    ret_val = ath_hal_fs_read(filename, 0, (unsigned int)many, buffer);

    return ( (-1 == ret_val) ? AH_FALSE : AH_TRUE);
}


static HAL_BOOL ar5416EepromRestoreFromFlash(struct ath_hal *ah, ar9287_eeprom_t *mptr, int mdataSize)
{
	int nptr;

	nptr=-1;

#ifndef WIN32
#ifdef MDK_AP //MDK_AP is defined only in NART AP build
        {
            #define AR9287_PCIE_CONFIG_SIZE 0x100  // byte addressable

            /*
             * When calibration data is saved in flash, read
             * uncompressed eeprom structure from flash and return
             */
            int fd;
            int offset;
            u_int8_t word[MOUTPUT];
            //printf("Reading flash for calibration data\n");
            if ((fd = open("/dev/caldata", O_RDWR)) > 0) {
            /*
            * First 0x1000 are reserved for pcie config writes.
            * offset = devIndex*AR9287_FLASH_SIZE+FLASH_BASE_CALDATA_OFFSET;
            *     Need for boards with more than one radio
            */
            //offset = FLASH_BASE_CALDATA_OFFSET;
            offset = instance*AR9287_FLASH_SIZE+FLASH_BASE_CALDATA_OFFSET+AR9287_PCIE_CONFIG_SIZE;
            lseek(fd, offset, SEEK_SET);
            if (read(fd, mptr, mdataSize) != mdataSize) {
                perror("\nRead\n");
		        close(fd);
                return AH_FALSE;
                }
	    nptr = mdataSize;
            close(fd);
            }
        return AH_TRUE;
        }
#endif
#endif		/* WIN32 */


        if(AH_TRUE == ar5416_calibration_data_read_file(
            ah,(u_int8_t *)mptr, mdataSize) )
        {
            nptr = mdataSize;
            return AH_TRUE;
        }
	if(nptr>=0)
	{
            if (mptr->baseEepHeader.version==0xff ||  mptr->baseEepHeader.version==0)
	    {
                // The board is uncalibrated
                nptr = -1;
            }
       }
       return ( (-1 == nptr) ? AH_FALSE : AH_TRUE);
}


/*
 * Read the configuration data from the eeprom.
 * The data can be put in any specified memory buffer.
 *
 * Returns -1 on error.
 * Returns address of next memory location on success.
 */
int
ar5416EepromRestoreInternal(struct ath_hal *ah, ar9287_eeprom_t *mptr, int mdataSize)
{
    int nptr;
    nptr = -1;
    nptr = ar5416EepromRestoreFromFlash(ah, mptr, mdataSize);
    return nptr;
}

int valid_flash_caldata(struct ath_hal *ah)
{
    u_int32_t sum;
    int i;
    u_int16_t eeval;
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar9287_eeprom_t *eep = &ahp->ah_eeprom.map.mapAr9287;
    BASE_EEPAR9287_HEADER  *pBase  = &eep->baseEepHeader;
    u_int32_t EEP_MAC [] = { EEP_MAC_LSW, EEP_MAC_MID, EEP_MAC_MSW };
    sum = 0;

    for (i = 0; i < 3; i++) {
        eeval = ar9287EepromGet(ahp, EEP_MAC[i]);
        sum += eeval;
        ahp->ah_macaddr[2*i] = eeval >> 8;
        ahp->ah_macaddr[2*i + 1] = eeval & 0xff;
    }

    if (pBase->version == 0x00 || pBase->version == 0xFF) {
        HDPRINTF(ah, HAL_DBG_EEPROM,"%s: Flash signature  read failed: %s\n",
               __func__, ath_hal_ether_sprintf(ahp->ah_macaddr));
        return AH_FALSE;
    }

    if (sum == 0 || sum == 0xffff*3) {
        HDPRINTF(ah, HAL_DBG_EEPROM,"%s: mac address read failed: %s\n",
              __func__, ath_hal_ether_sprintf(ahp->ah_macaddr));
        return AH_FALSE;
    }
    return AH_TRUE;
}

/*
 * Restore the configuration structure by reading the eeprom.
 * This function destroys any existing in-memory structure content.
 */
HAL_BOOL
ar5416EepromRestore(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar9287_eeprom_t *mptr;
    int mdataSize;
    HAL_BOOL status = AH_FALSE;
    int ret = 0;

    mptr = &ahp->ah_eeprom.map.mapAr9287;
    mdataSize = sizeof(ar9287_eeprom_t);

    if (mptr!=0 && mdataSize>0)
    {
#if AH_BYTE_ORDER == AH_BIG_ENDIAN
        //ar9287SwapEeprom(mptr);
#endif
        //
        // At this point, mptr points to the eeprom data structure in it's "default"
        // state.  If this is big endian, swap the data structures back to "little endian"
        // form.
        //
        if (ar5416EepromRestoreInternal(ah,mptr,mdataSize) > 0)
        {
            ret = valid_flash_caldata(ah);
            if (ret == AH_TRUE)
                status = AH_TRUE;
            else
                status = AH_FALSE;
        }

#if AH_BYTE_ORDER == AH_BIG_ENDIAN
        // Second Swap, back to Big Endian
        //ar9287SwapEeprom(mptr);
#endif
    }
    return status;
}


HAL_STATUS
ar5416EepromAttach(struct ath_hal *ah)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
    HAL_STATUS rv;

#ifndef WIN32
    if (ar5416EepDataInFlash(ah))
        ar5416FlashMap(ah);
#endif

	if (AR_SREV_KIWI(ah) )
		ahp->ah_eep_map = EEP_MAP_AR9287;
	else if (AR_SREV_KITE(ah) || AR_SREV_K2(ah))
		ahp->ah_eep_map = EEP_MAP_4KBITS;
	else
		ahp->ah_eep_map = EEP_MAP_DEFAULT;

    AH_PRIVATE(ah)->ah_eeprom_get_spur_chan = ar5416EepromGetSpurChan;

    AH_PRIVATE(ah)->ah_get_rate_power_limit_from_eeprom = ar5416GetRatePowerLimitFromEeprom;

    if (AH_TRUE == ar5416EepromRestore(ah))
    {
            return HAL_OK;
    }
#ifdef ART_BUILD
    if(AH_PRIVATE(ah)->ah_flags == AH_USE_EEPROM)
    {
#endif /* ART_BUILD */

        if (!ar5416FillEeprom(ah)) {
        /* eeprom read failure => assume emulation board */
            if (ahp->ah_priv.priv.ah_config.ath_hal_soft_eeprom) {
                ar5416FillEmuEeprom(ahp);
                ahp->ah_emu_eeprom = 1;
                return HAL_OK;
            } else {
                return HAL_EIO;
            }
        }
#ifdef ART_BUILD
    }
#endif /* ART_BUILD */

    if ((rv = ar5416CheckEeprom(ah)) == HAL_OK)
    {
        return HAL_OK;
    }
    else
    {
#ifndef WIN32
        if (rv == HAL_EEBADSUM) {
            /* Retry again with flash/eeprom if the other one fails */
            if (ar5416EepDataInFlash(ah)) {
                HDPRINTF(ah, HAL_DBG_EEPROM, "%s: CalData FLASH is unavaible. Trying from EEPROM\n", __func__);
                AH_PRIVATE(ah)->ah_flags |= AH_USE_EEPROM;
                ahp->ah_priv.priv.ah_eeprom_read  = ar5416EepromRead;
#ifdef AH_SUPPORT_WRITE_EEPROM
                ahp->ah_priv.priv.ah_eeprom_write = ar5416EepromWrite;
#endif
            }
            else {
                HDPRINTF(ah, HAL_DBG_EEPROM, "%s: CalData EEPROM is unavaible. Trying from FLASH\n", __func__);
                AH_PRIVATE(ah)->ah_flags &= ~AH_USE_EEPROM;
                ahp->ah_priv.priv.ah_eeprom_read  = ar5416FlashRead;
                ahp->ah_priv.priv.ah_eeprom_dump  = AH_NULL;
#ifdef AH_SUPPORT_WRITE_EEPROM
                ahp->ah_priv.priv.ah_eeprom_write = ar5416FlashWrite;
#endif
            }
            if (!ar5416FillEeprom(ah)) {
                return HAL_EIO;
            }
            if ((rv = ar5416CheckEeprom(ah)) == HAL_OK)
            {
                return HAL_OK;
            }
        }
#endif /* WIN32 */

#ifndef ART_BUILD
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: CalData read failed from both EEPROM/FLASH\n", __func__);
        return rv;
#else
        //The data of EEPROM is unavailible
        if (AR_SREV_KIWI(ah))
        {
            //Use default eeprom's data
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: EEPROM is unavaible. Load EEPROM default value\n", __func__);
            ar9287EepromLoadDefaults(ah);
            return HAL_OK;
        }
        else
        {
            return HAL_EEBADSUM;
        }
#endif
    }
}

u_int32_t
ar5416EepromGet(struct ath_hal_5416 *ahp, EEPROM_PARAM param)
{
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromGet(ahp, param);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kGet(ahp, param);
	else
		return ar5416EepromDefGet(ahp, param);
}

#ifdef AH_SUPPORT_WRITE_EEPROM
/**************************************************************
 * ar5416EepromSetParam
 */
HAL_BOOL
ar5416EepromSetParam(struct ath_hal *ah, EEPROM_PARAM param, u_int32_t value)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromSetParam(ah, param, value);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kSetParam(ah, param, value);
	else
		return ar5416EepromDefSetParam(ah, param, value);
}
#endif //#ifdef AH_SUPPORT_WRITE_EEPROM

/*
 * Read EEPROM header info and program the device for correct operation
 * given the channel value.
 */
HAL_BOOL
ar5416EepromSetBoardValues(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromSetBoardValues(ah, chan);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kSetBoardValues(ah, chan);
	else
		return ar5416EepromDefSetBoardValues(ah, chan);
}

/******************************************************************************/
/*!
**  \brief EEPROM fixup code for INI values
**
** This routine provides a place to insert "fixup" code for specific devices
** that need to modify INI values based on EEPROM values, BEFORE the INI values
** are written.  Certain registers in the INI file can only be written once without
** undesired side effects, and this provides a place for EEPROM overrides in these
** cases.
**
** This is called at attach time once.  It should not affect run time performance
** at all
**
**  \param ah       Pointer to HAL object (this)
**  \param pEepData Pointer to (filled in) eeprom data structure
**  \param reg      register being inspected on this call
**  \param value    value in INI file
**
**  \return Updated value for INI file.
*/

u_int32_t
ar5416INIFixup(struct ath_hal *ah,ar5416_eeprom_t *pEepData, u_int32_t reg, u_int32_t value)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromINIFixup(ah, &pEepData->map.mapAr9287, reg, value);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kINIFixup(ah, &pEepData->map.map4k, reg, value);
	else
		return ar5416EepromDefINIFixup(ah, &pEepData->map.def, reg, value);
}

/**************************************************************
 * ar5416EepromSetTransmitPower
 *
 * Set the transmit power in the baseband for the given
 * operating channel and mode.
 */
HAL_STATUS
ar5416EepromSetTransmitPower(struct ath_hal *ah,
    ar5416_eeprom_t *pEepData, HAL_CHANNEL_INTERNAL *chan, u_int16_t cfgCtl,
    u_int16_t twiceAntennaReduction, u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromSetTransmitPower(ah, &pEepData->map.mapAr9287,
					chan, cfgCtl, twiceAntennaReduction, twiceMaxRegulatoryPower, powerLimit);

	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kSetTransmitPower(ah, &pEepData->map.map4k,
					chan, cfgCtl, twiceAntennaReduction, twiceMaxRegulatoryPower, powerLimit);
	else
		return ar5416EepromDefSetTransmitPower(ah, &pEepData->map.def,
					chan, cfgCtl, twiceAntennaReduction, twiceMaxRegulatoryPower, powerLimit);
}

/**************************************************************
 * ar5416EepromSetAddac
 *
 * Set the ADDAC from eeprom for Sowl.
 */
void
ar5416EepromSetAddac(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		ar9287EepromSetAddac(ah, chan);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		ar5416Eeprom4kSetAddac(ah, chan);
	else
		ar5416EepromDefSetAddac(ah, chan);
}

u_int
ar5416EepromDumpSupport(struct ath_hal *ah, void **pp_e)
{
    *pp_e = &(AH5416(ah)->ah_eeprom);
    return sizeof(ar5416_eeprom_t);
}

u_int8_t
ar5416EepromGetNumAntConfig(struct ath_hal_5416 *ahp, HAL_FREQ_BAND freq_band)
{
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromGetNumAntConfig(ahp, freq_band);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kGetNumAntConfig(ahp, freq_band);
	else
		return ar5416EepromDefGetNumAntConfig(ahp, freq_band);
}

HAL_STATUS
ar5416EepromGetAntCfg(struct ath_hal_5416 *ahp, HAL_CHANNEL_INTERNAL *chan,
                   u_int8_t index, u_int32_t *config)
{
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287EepromGetAntCfg(ahp, chan, index, config);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416Eeprom4kGetAntCfg(ahp, chan, index, config);
	else
		return ar5416EepromDefGetAntCfg(ahp, chan, index, config);
}

u_int8_t*
ar5416EepromGetCustData(struct ath_hal_5416 *ahp)
{
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ahp->ah_eeprom.map.mapAr9287.custData;
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ahp->ah_eeprom.map.map4k.custData;
	else
		return ahp->ah_eeprom.map.def.custData;
}

/***************************************
 * Helper functions common for AP/CB/XB
 **************************************/

/**************************************************************
 * ar5416GetTargetPowers
 *
 * Return the rates of target power for the given target power table
 * channel, and number of channels
 */
void
ar5416GetTargetPowers(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan,
    CAL_TARGET_POWER_HT *powInfo, u_int16_t numChannels,
    CAL_TARGET_POWER_HT *pNewPower, u_int16_t numRates,
    HAL_BOOL isHt40Target)
{
    u_int16_t clo, chi;
    int i;
    int matchIndex = -1, lowIndex = -1;
    u_int16_t freq;
    CHAN_CENTERS centers;

    ar5416GetChannelCenters(ah, chan, &centers);
    freq = isHt40Target ? centers.synth_center : centers.ctl_center;

    /* Copy the target powers into the temp channel list */
    if (freq <= fbin2freq(powInfo[0].bChannel, IS_CHAN_2GHZ(chan)))
    {
        matchIndex = 0;
    }
    else
    {
        for (i = 0; (i < numChannels) && (powInfo[i].bChannel != AR5416_BCHAN_UNUSED); i++)
        {
            if (freq == fbin2freq(powInfo[i].bChannel, IS_CHAN_2GHZ(chan)))
            {
                matchIndex = i;
                break;
            }
            else if ((freq < fbin2freq(powInfo[i].bChannel, IS_CHAN_2GHZ(chan))) &&
                (freq > fbin2freq(powInfo[i - 1].bChannel, IS_CHAN_2GHZ(chan))))
            {
                lowIndex = i - 1;
                break;
            }
        }
        if ((matchIndex == -1) && (lowIndex == -1))
        {
            HALASSERT(freq > fbin2freq(powInfo[i - 1].bChannel, IS_CHAN_2GHZ(chan)));
            matchIndex = i - 1;
        }
    }

    if (matchIndex != -1)
    {
        *pNewPower = powInfo[matchIndex];
    }
    else
    {
        HALASSERT(lowIndex != -1);
        /*
        * Get the lower and upper channels, target powers,
        * and interpolate between them.
        */
        clo = fbin2freq(powInfo[lowIndex].bChannel, IS_CHAN_2GHZ(chan));
        chi = fbin2freq(powInfo[lowIndex + 1].bChannel, IS_CHAN_2GHZ(chan));

        for (i = 0; i < numRates; i++)
        {
            pNewPower->tPow2x[i] = (u_int8_t)interpolate(freq, clo, chi,
                powInfo[lowIndex].tPow2x[i], powInfo[lowIndex + 1].tPow2x[i]);
        }
    }
}

/**************************************************************
 * ar5416GetTargetPowersLeg
 *
 * Return the four rates of target power for the given target power table
 * channel, and number of channels
 */
void
ar5416GetTargetPowersLeg(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan,
    CAL_TARGET_POWER_LEG *powInfo, u_int16_t numChannels,
    CAL_TARGET_POWER_LEG *pNewPower, u_int16_t numRates,
    HAL_BOOL isExtTarget)
{
    u_int16_t clo, chi;
    int i;
    int matchIndex = -1, lowIndex = -1;
    u_int16_t freq;
    CHAN_CENTERS centers;

    ar5416GetChannelCenters(ah, chan, &centers);
    freq = (isExtTarget) ? centers.ext_center : centers.ctl_center;

    /* Copy the target powers into the temp channel list */
    if (freq <= fbin2freq(powInfo[0].bChannel, IS_CHAN_2GHZ(chan)))
    {
        matchIndex = 0;
    }
    else
    {
        for (i = 0; (i < numChannels) && (powInfo[i].bChannel != AR5416_BCHAN_UNUSED); i++)
        {
            if (freq == fbin2freq(powInfo[i].bChannel, IS_CHAN_2GHZ(chan)))
            {
                matchIndex = i;
                break;
            }
            else if ((freq < fbin2freq(powInfo[i].bChannel, IS_CHAN_2GHZ(chan))) &&
                (freq > fbin2freq(powInfo[i - 1].bChannel, IS_CHAN_2GHZ(chan))))
            {
                lowIndex = i - 1;
                break;
            }
        }
        if ((matchIndex == -1) && (lowIndex == -1))
        {
            HALASSERT(freq > fbin2freq(powInfo[i - 1].bChannel, IS_CHAN_2GHZ(chan)));
            matchIndex = i - 1;
        }
    }

    if (matchIndex != -1)
    {
        *pNewPower = powInfo[matchIndex];
    }
    else
    {
        HALASSERT(lowIndex != -1);
        /*
        * Get the lower and upper channels, target powers,
        * and interpolate between them.
        */
        clo = fbin2freq(powInfo[lowIndex].bChannel, IS_CHAN_2GHZ(chan));
        chi = fbin2freq(powInfo[lowIndex + 1].bChannel, IS_CHAN_2GHZ(chan));

        for (i = 0; i < numRates; i++)
        {
            pNewPower->tPow2x[i] = (u_int8_t)interpolate(freq, clo, chi,
                powInfo[lowIndex].tPow2x[i], powInfo[lowIndex + 1].tPow2x[i]);
        }
    }
}

static inline HAL_STATUS
ar5416CheckEeprom(struct ath_hal *ah)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287CheckEeprom(ah);
	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416CheckEeprom4k(ah);
	else
		return ar5416CheckEepromDef(ah);
}

static u_int16_t
ar5416EepromGetSpurChan(struct ath_hal *ah, u_int16_t i,HAL_BOOL is2GHz)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_t *eep = (ar5416_eeprom_t *)&ahp->ah_eeprom;
    u_int16_t   spur_val = AR_NO_SPUR;

    HALASSERT(i <  AR_EEPROM_MODAL_SPURS );

    HDPRINTF(ah, HAL_DBG_ANI,
             "Getting spur idx %d is2Ghz. %d val %x\n",
             i, is2GHz, AH_PRIVATE(ah)->ah_config.ath_hal_spur_chans[i][is2GHz]);

    switch(AH_PRIVATE(ah)->ah_config.ath_hal_spur_mode)
    {
    case SPUR_DISABLE:
        /* returns AR_NO_SPUR */
        break;
    case SPUR_ENABLE_IOCTL:
        spur_val = AH_PRIVATE(ah)->ah_config.ath_hal_spur_chans[i][is2GHz];
        HDPRINTF(ah, HAL_DBG_ANI, "Getting spur val from new loc. %d\n", spur_val);
        break;
    case SPUR_ENABLE_EEPROM:
		if (ahp->ah_eep_map == EEP_MAP_AR9287)
			spur_val = eep->map.mapAr9287.modalHeader.spurChans[i].spurChan;
		else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
			spur_val = eep->map.map4k.modalHeader.spurChans[i].spurChan;
		else
			spur_val = eep->map.def.modalHeader[is2GHz].spurChans[i].spurChan;
        break;

    }
    return spur_val;
}

#ifdef AH_PRIVATE_DIAG
static inline void
ar5416FillEmuEeprom(struct ath_hal_5416 *ahp)
{
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		ar9287FillEmuEeprom(ahp);
    else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		ar5416FillEmuEeprom4k(ahp);
	else ar5416FillEmuEepromDef(ahp);
}
#endif /* AH_PRIVATE_DIAG */

static inline HAL_BOOL
ar5416FillEeprom(struct ath_hal *ah)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287FillEeprom(ah);
    else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
		return ar5416FillEeprom4k(ah);
	else
		return ar5416FillEepromDef(ah);
}


#ifdef ART_BUILD
/*added by eguo*/

extern HAL_STATUS ar9287SetTargetPowerFromEeprom(struct ath_hal *ah,
    ar9287_eeprom_t *pEepData, HAL_CHANNEL_INTERNAL *chan, u_int16_t cfgCtl,
    u_int16_t twiceAntennaReduction, u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit);

/**************************************************************
 * ar5416 set Target power from Eeprom
 *
 * Set the transmit target power in the baseband for the given
 * operating channel and mode.
 */
HAL_STATUS
ar5416SetTargetPower(struct ath_hal *ah,
    ar5416_eeprom_t *pEepData, HAL_CHANNEL_INTERNAL *chan, u_int16_t cfgCtl,
    u_int16_t twiceAntennaReduction, u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	if (ahp->ah_eep_map == EEP_MAP_AR9287)
		return ar9287SetTargetPowerFromEeprom(ah, &pEepData->map.mapAr9287,
					chan, cfgCtl, twiceAntennaReduction, twiceMaxRegulatoryPower, powerLimit);

	else if (ahp->ah_eep_map == EEP_MAP_4KBITS)
	{
	    HDPRINTF(ah, HAL_DBG_ANI, "error:ahp->ah_eep_map == EEP_MAP_4KBITS\n");
	    return 0;
	}
	else
	{
	    HDPRINTF(ah, HAL_DBG_ANI, "error: unkonwn ah_eep_map \n");
	    return 0;
	}
}

ar5416SetTargetPowerFromEeprom(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CHANNEL_INTERNAL *ichan = ahpriv->ah_curchan;
    HAL_CHANNEL *chan = (HAL_CHANNEL *)ichan;

    if (ar5416SetTargetPower(ah, &ahp->ah_eeprom, ichan,
        ath_hal_getctl(ah, chan), ath_hal_getantennaallowed(ah, chan),
        chan->max_reg_tx_power * 2,
        AH_MIN(MAX_RATE_POWER, ahpriv->ah_power_limit)) != HAL_OK)
        return AH_FALSE;

    return AH_TRUE;
}
#endif /* ART_BUILD */

#endif /* AH_SUPPORT_AR5416 */

