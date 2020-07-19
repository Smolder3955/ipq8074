/*
* Copyright (c) 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "opt_ah.h"
#include <stdarg.h>

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

/*
** Linux Includes
*/
#ifdef AR9100
#include <ar9100.h>
#endif

/*
** HAL Includes
*/

#include "ah.h"
#include "ah_internal.h"

/*
** Internal static variables
** These are used to set GLOBAL HAL operating modes.  Any
** configuration variable that is instance specific SHOULD NOT
** be defined here
*/

static struct ath_hal_callback    ath_hal_callback_table;

#ifdef AR9100
void
ath_hal_ahb_mac_reset(void)
{
    ar9100_ahb2wmac_reset();
}

void ath_hal_get_chip_revisionid(u_int32_t *val)
{
    ar9100_get_rst_revisionid(*val);
}
#endif

/******************************************************************************/
/*!
**  \brief Create a HAL instance
**
**  This function creates an instance of a HAL object for a specific device
**  type.  This is in essence the constructor function for the HAL.
**
**  \param devid 16 bit PCI Device ID value
**  \param sc Pointer to the ATH Soft C Object
**  \param bus_context Pointer to the PCI bus information structure.
**  \param callbackTable Pointer to ATH object callback table.
**  \param s Void pointer to area to pass Hal Status back to caller
**
**  \return Pointer to ath_hal object, HAL public object reference
**  \return NULL if unsuccessful
*/


struct ath_hal *
_ath_hal_attach(u_int16_t               devid,
                HAL_ADAPTER_HANDLE      osdev,
                HAL_SOFTC               sc,
                HAL_BUS_CONTEXT         *bus_context,
                struct hal_reg_parm     *hal_conf_parm,
                const struct ath_hal_callback *callbackTable,
                void                    *s)
{
    HAL_STATUS status;
    struct ath_hal *ah;

	/* 
	 * save callback table locally; 
         * can be made dynamically;
         * other handles are passed to the chip-specific routine.
	 */
	ath_hal_callback_table = *callbackTable;
	ah = ath_hal_attach(devid, osdev, sc, 
                        bus_context->bc_tag, bus_context->bc_handle, bus_context->bc_bustype,
                        hal_conf_parm, &status);

    *(HAL_STATUS *)s = status;
    return ah;
}


/******************************************************************************/
/*!
**  \brief Decrement the use count for this module
**
**  This decrements the module use count for each HAL instance that is detached.
**  Note that the ah reference is freed in the detach routine, so the ah
**  pointer is not valid on return.
**
**  \param ah Pointer to HAL instance structure
**
**  \return N/A
*/

void
ath_hal_detach(struct ath_hal *ah)
{
    (*ah->ah_detach)(ah);
}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

void __ahdecl
ath_hal_printf(struct ath_hal *ah, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ath_hal_vprintf(ah, fmt, ap);
    va_end(ap);
}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

/*
 * Print/log message support.
 */

void __ahdecl
ath_hal_vprintf(struct ath_hal *ah, const char* fmt, va_list ap)
{
    char buf[1024];                 /* XXX */
    vsnprintf(buf, sizeof(buf), fmt, ap);
    printf("HAL:: %s", buf);
}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

/*
 * Format an Ethernet MAC for printing.
 */
const char* __ahdecl
ath_hal_ether_sprintf(const u_int8_t *mac)
{
    static char etherbuf[18];
    snprintf(etherbuf, sizeof(etherbuf), "%02x:%02x:%02x:%02x:%02x:%02x",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return etherbuf;
}

#ifdef AH_ASSERT
/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

void __ahdecl
ath_hal_assert_failed(const char* filename, int lineno, const char *msg)
{
    printf("Atheros HAL assertion failure: %s: line %u: %s\n",
        filename, lineno, msg);
    panic("ath_hal_assert");
}
#endif /* AH_ASSERT */

#ifdef AH_DEBUG_ALQ
/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

/*
 * ALQ register tracing support.
 *
 * Setting hw.ath.hal.alq=1 enables tracing of all register reads and
 * writes to the file /tmp/ath_hal.log.  The file format is a simple
 * fixed-size array of records.  When done logging set hw.ath.hal.alq=0
 * and then decode the file with the ardecode program (that is part of the
 * HAL).  If you start+stop tracing the data will be appended to an
 * existing file.
 *
 * NB: doesn't handle multiple devices properly; only one DEVICE record
 *     is emitted and the different devices are not identified.
 */
#include "alq/alq.h"
#include "ah_decode.h"

static  struct alq *ath_hal_alq;
static  int ath_hal_alq_emitdev;    /* need to emit DEVICE record */
static  u_int ath_hal_alq_lost;     /* count of lost records */
static  const char *ath_hal_logfile = "/tmp/ath_hal.log";
static  u_int ath_hal_alq_qsize = 8*1024;

static int
ath_hal_setlogging(int enable)
{
    int error;

    if (enable) {
        if (!capable(CAP_NET_ADMIN))
            return -EPERM;
        error = alq_open(&ath_hal_alq, ath_hal_logfile,
                sizeof (struct athregrec), ath_hal_alq_qsize);
        ath_hal_alq_lost = 0;
        ath_hal_alq_emitdev = 1;
        printf("ath_hal: logging to %s %s\n", ath_hal_logfile,
            error == 0 ? "enabled" : "could not be setup");
    } else {
        if (ath_hal_alq)
            alq_close(ath_hal_alq);
        ath_hal_alq = NULL;
        printf("ath_hal: logging disabled\n");
        error = 0;
    }
    return error;
}


/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

/*
 * Deal with the sysctl handler api changing.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8)
#define AH_SYSCTL_ARGS_DECL \
    ctl_table *ctl, int write, struct file *filp, void *buffer, \
        size_t *lenp
#define AH_SYSCTL_ARGS      ctl, write, filp, buffer, lenp
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8) */
#define AH_SYSCTL_ARGS_DECL \
    ctl_table *ctl, int write, struct file *filp, void *buffer,\
        size_t *lenp, loff_t *ppos
#define AH_SYSCTL_ARGS      ctl, write, filp, buffer, lenp, ppos
#endif

static int
sysctl_hw_ath_hal_log(AH_SYSCTL_ARGS_DECL)
{
    int error, enable;

    ctl->data = &enable;
    ctl->maxlen = sizeof(enable);
    enable = (ath_hal_alq != NULL);
        error = proc_dointvec(AH_SYSCTL_ARGS);
        if (error || !write)
                return error;
    else
        return ath_hal_setlogging(enable);
}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

static struct ale *
ath_hal_alq_get(struct ath_hal *ah)
{
    struct ale *ale;

    if (ath_hal_alq_emitdev) {
        ale = alq_get(ath_hal_alq, ALQ_NOWAIT);
        if (ale) {
            struct athregrec *r =
                (struct athregrec *) ale->ae_data;
            r->op = OP_DEVICE;
            r->reg = 0;
            r->val = AH_PRIVATE(ah)->ah_devid;
            alq_post(ath_hal_alq, ale);
            ath_hal_alq_emitdev = 0;
        } else
            ath_hal_alq_lost++;
    }
    ale = alq_get(ath_hal_alq, ALQ_NOWAIT);
    if (!ale)
        ath_hal_alq_lost++;
    return ale;
}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

void __ahdecl
ath_hal_reg_write(struct ath_hal *ah, u_int32_t reg, u_int32_t val)
{
    if (ath_hal_alq) {
        unsigned long flags;
        struct ale *ale;

        local_irq_save(flags);
        ale = ath_hal_alq_get(ah);
        if (ale) {
            struct athregrec *r = (struct athregrec *) ale->ae_data;
            r->op = OP_WRITE;
            r->reg = reg;
            r->val = val;
            alq_post(ath_hal_alq, ale);
        }
        local_irq_restore(flags);
    }
    _OS_REG_WRITE(ah, reg, val);
}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

u_int32_t __ahdecl
ath_hal_reg_read(struct ath_hal *ah, u_int32_t reg)
{
    u_int32_t val;

    val = _OS_REG_READ(ah, reg);
    if (ath_hal_alq) {
        unsigned long flags;
        struct ale *ale;

        local_irq_save(flags);
        ale = ath_hal_alq_get(ah);
        if (ale) {
            struct athregrec *r = (struct athregrec *) ale->ae_data;
            r->op = OP_READ;
            r->reg = reg;
            r->val = val;
            alq_post(ath_hal_alq, ale);
        }
        local_irq_restore(flags);
    }
    return val;
}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

void __ahdecl
OS_MARK(struct ath_hal *ah, u_int id, u_int32_t v)
{
    if (ath_hal_alq) {
        unsigned long flags;
        struct ale *ale;

        local_irq_save(flags);
        ale = ath_hal_alq_get(ah);
        if (ale) {
            struct athregrec *r = (struct athregrec *) ale->ae_data;
            r->op = OP_MARK;
            r->reg = id;
            r->val = v;
            alq_post(ath_hal_alq, ale);
        }
        local_irq_restore(flags);
    }
}

#elif defined(AH_DEBUG) || defined(AH_REGOPS_FUNC)
/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

/*
 * Memory-mapped device register read/write.  These are here
 * as routines when debugging support is enabled and/or when
 * explicitly configured to use function calls.  The latter is
 * for architectures that might need to do something before
 * referencing memory (e.g. remap an i/o window).
 *
 * NB: see the comments in ah_osdep.h about byte-swapping register
 *     reads and writes to understand what's going on below.
 */
#ifdef AH_ANALOG_SHADOW_READ
#define RF_BEGIN 0x7800
#define RF_END 0x7900
static u_int32_t rfshadow[(RF_END-RF_BEGIN)/4+1];
#endif
void __ahdecl
ath_hal_reg_write(struct ath_hal *ah, u_int reg, u_int32_t val)
{
    HDPRINTF(ah, HAL_DBG_REG_IO, "WRITE 0x%x <= 0x%x\n", reg, val);

#if defined(AH_ANALOG_SHADOW_READ)
    if (reg<=RF_END && reg>=RF_BEGIN) rfshadow[(reg-RF_BEGIN)/4] = val;
#endif
#if defined(AH_ANALOG_SHADOW_WRITE)
    if (reg<=RF_END && reg>=RF_BEGIN) return;
#endif

    _OS_REG_WRITE(ah, reg, val);
}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

u_int32_t __ahdecl
ath_hal_reg_read(struct ath_hal *ah, u_int reg)
{
    u_int32_t val;

#ifdef AH_ANALOG_SHADOW_READ
    if (reg<=RF_END && reg>=RF_BEGIN) val = rfshadow[(reg-RF_BEGIN)/4];
    else
#endif
    val = _OS_REG_READ(ah, reg);
    HDPRINTF(ah, HAL_DBG_REG_IO, "READ 0x%x => 0x%x\n", reg, val);
    return val;
}
#endif /* AH_DEBUG || AH_REGOPS_FUNC */

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

/*
 * Delay n microseconds.
 */
void ath_hal_delay(int n)
{
    usleep(n);
}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

/*
 * Allocate/free memory.
 */

#if 0
void * __ahdecl ath_hal_malloc(HAL_ADAPTER_HANDLE osdev, size_t size)
{
    void *p;
    p = malloc(size);
    if (p)
        OS_MEMZERO(p, size);
    return p;

}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

void __ahdecl
ath_hal_free(void* p)
{
    free(p);
}
#endif

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

u_int32_t
ath_hal_read_pci_config_space(struct ath_hal *ah, u_int32_t offset, void *pBuffer, u_int32_t length)
{
    u_int32_t bytesRead = 0;

    // if (ath_hal_callback_table.read_pci_config_space != NULL) {
    if (ath_hal_callback_table.read_pci_config_space != 0) {
        bytesRead = ath_hal_callback_table.read_pci_config_space(
            AH_PRIVATE(ah)->ah_sc, offset, pBuffer, length);
    }

    return (bytesRead);
}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

void __ahdecl
ath_hal_memzero(void *dst, size_t n)
{
    memset(dst, 0, n);
}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

void * __ahdecl
ath_hal_memcpy(void *dst, const void *src, size_t n)
{
    return memcpy(dst, src, n);
}

/******************************************************************************/
/*!
**  \brief Set Factory Defaults for the HAL instance
**
**  This function will set factory default values for the HAL operations
**  configuration structure, used in the operation of each HAL instance.
**  Specific values can be overriden using IOCTL interfaces, as required.
**
**  \param ahp  Pointer to HAL private data structure
**
**  \return N/A
*/

void __ahdecl
ath_hal_factory_defaults(struct ath_hal_private *ap, struct hal_reg_parm *hal_conf_parm)
{
    int i;
    
    ap->ah_config.ath_hal_dma_beacon_response_time  = 512;   /* microseconds */
    ap->ah_config.ath_hal_sw_beacon_response_time   = 10240; /* microseconds */
    ap->ah_config.ath_hal_additional_swba_backoff   = 0;     /* microseconds */
    ap->ah_config.ath_hal_6mb_ack                   = 0x0;
    ap->ah_config.ath_hal_cwm_ignore_ext_cca        = 0;
    ap->ah_config.ath_hal_rifs_enable         	    = 0;
    ap->ah_config.ath_hal_soft_eeprom               = 0;
    ap->ah_config.ath_hal_desc_tpc                  = 0;
    ap->ah_config.ath_hal_redchn_maxpwr             = 0;
    
#ifdef ATH_FORCE_BIAS
    ap->ah_config.ath_hal_forceBias                 = hal_conf_parm->forceBias;
    ap->ah_config.ath_hal_forceBiasAuto             = hal_conf_parm->forceBiasAuto;
#endif
    ap->ah_config.ath_hal_pcie_power_save_enable    = hal_conf_parm->halPciePowerSaveEnable;
    ap->ah_config.ath_hal_pcieL1SKPEnable           = hal_conf_parm->halPcieL1SKPEnable;
    ap->ah_config.ath_hal_pcie_clock_req            = hal_conf_parm->halPcieClockReq;
    ap->ah_config.ath_hal_pll_pwr_save              = 0;
    ap->ah_config.ath_hal_pciePowerReset            = hal_conf_parm->halPciePowerReset;
    ap->ah_config.ath_hal_pcie_waen                 = hal_conf_parm->halPcieWaen;
    ap->ah_config.ath_hal_pcieRestore               = hal_conf_parm->halPcieRestore;
    ap->ah_config.ath_hal_ht_enable                 = hal_conf_parm->htEnable;
#ifdef ATH_SUPERG_DYNTURBO
    ap->ah_config.ath_hal_disableTurboG             = hal_conf_parm->disableTurboG;
#endif
    ap->ah_config.ath_hal_ofdmTrigLow               = hal_conf_parm->ofdmTrigLow;
    ap->ah_config.ath_hal_ofdmTrigHigh              = hal_conf_parm->ofdmTrigHigh;
    ap->ah_config.ath_hal_cckTrigHigh               = hal_conf_parm->cckTrigHigh;
    ap->ah_config.ath_hal_cckTrigLow                = hal_conf_parm->cckTrigLow;
    ap->ah_config.ath_hal_enable_ani                = hal_conf_parm->enableANI;
    ap->ah_config.ath_hal_noiseImmunityLvl          = hal_conf_parm->noiseImmunityLvl;
    ap->ah_config.ath_hal_ofdmWeakSigDet            = hal_conf_parm->ofdmWeakSigDet;
    ap->ah_config.ath_hal_cckWeakSigThr             = hal_conf_parm->cckWeakSigThr;
    ap->ah_config.ath_hal_spurImmunityLvl           = hal_conf_parm->spurImmunityLvl;
    ap->ah_config.ath_hal_firStepLvl                = hal_conf_parm->firStepLvl;
    ap->ah_config.ath_hal_rssiThrHigh               = hal_conf_parm->rssiThrHigh;
    ap->ah_config.ath_hal_rssiThrLow                = hal_conf_parm->rssiThrLow;
    ap->ah_config.ath_hal_diversity_control         = hal_conf_parm->diversityControl;
    ap->ah_config.ath_hal_antenna_switch_swap       = hal_conf_parm->antennaSwitchSwap;
    for (i=0; i< AR_EEPROM_MODAL_SPURS; i++) {
        ap->ah_config.ath_hal_spur_chans[i][0] = AR_NO_SPUR;
        ap->ah_config.ath_hal_spur_chans[i][1] = AR_NO_SPUR;
    }
    ap->ah_config.ath_hal_serialize_reg_mode          = hal_conf_parm->serializeRegMode;
    ap->ah_config.ath_hal_defaultAntCfg             = hal_conf_parm->defaultAntCfg;
    ap->ah_config.ath_hal_fastClockEnable           = hal_conf_parm->fastClockEnable;
    ap->ah_config.ath_hal_mfp_support                = hal_conf_parm->hwMfpSupport;
    ap->ah_config.ath_hal_enable_msi                 = 0;
    ap->ah_config.ath_hal_keep_alive_timeout          = 60000;  /* 60 seconds */
}

static char *dev_info = "ath_hal";

/******************************************************************************/
/*!
**  \brief Load and Initialize HAL "layer"
**
**  This function installs the HAL module into memory, and performs minimal
**  initialization.  The main purpose of this initialization is to make all
**  of the HAL symbols accessible by other modules, as required.  This init
**  function DOES NOT allocate any HAL instances.
**
**  \return N/A, This is a module initialization function
*/

static int init_ath_hal(void)
{
    const char *sep;
    int i;

    printf("HAL:: %s: %s (", dev_info, ath_hal_version);
    sep = "";
    // for (i = 0; ath_hal_buildopts[i] != NULL; i++) {
    for (i = 0; ath_hal_buildopts[i] != 0; i++) {
        printf("%s%s", sep, ath_hal_buildopts[i]);
        sep = ", ";
    }
    printf(")\n");
    
    return (0);
}

/******************************************************************************/
/*!
**  \brief b
**
**  XXXX
**
**  \param P1 d
**
**  \return RV
*/

exit_ath_hal(void)
{
    printf("HAL:: %s: driver unloaded\n", dev_info);
}

typedef struct regAccess {
    struct ath_hal  *ah;
    u_int32_t       off;
    u_int32_t       val;
} REG_ACCESS;



static void (*_RegisterDebug)(unsigned long address, unsigned long before, unsigned long after);

void HalRegisterDebug(void (*f)(unsigned long address, unsigned long before, unsigned long after))
{
    _RegisterDebug=f;
}


void * __ahdecl ath_hal_ioremap(uintptr_t addr, u_int32_t len)
{
    return (void *)addr;
}

