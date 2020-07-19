#ifndef __MSMHWIOREG_H__
#define __MSMHWIOREG_H__
/*
===========================================================================
*/
/**
  @file msmhwioreg.h
  @brief Auto-generated HWIO interface include file.

  This file contains HWIO register definitions for the following modules:
    MPM2_WDOG
    MPM2_PSHOLD
    MPM2_SLP_CNTR

  'Include' filters applied: <none>
  'Exclude' filters applied: RESERVED DUMMY 
*/
/*
  ===========================================================================

  Copyright (c) 2017 Qualcomm Technologies, Inc.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  ===========================================================================

  $Header: //components/rel/boot.bf/3.3.1/boot_images/core/storage/tools/deviceprogrammer_ddr/src/bsp/msmhwioreg.h#6 $
  $DateTime: 2017/07/04 05:31:19 $
  $Author: pwbldsvc $

  ===========================================================================
*/

#include "msmhwiobase.h"



/*----------------------------------------------------------------------------
 * MODULE: MPM2_SLP_CNTR
 *--------------------------------------------------------------------------*/

#define MPM2_SLP_CNTR_REG_BASE                                    (MPM2_MPM_BASE      + 0x00003000)

#define HWIO_MPM2_MPM_SLEEP_TIMETICK_COUNT_VAL_ADDR               (MPM2_SLP_CNTR_REG_BASE      + 0x00000000)
#define HWIO_MPM2_MPM_SLEEP_TIMETICK_COUNT_VAL_RMSK               0xffffffff
#define HWIO_MPM2_MPM_SLEEP_TIMETICK_COUNT_VAL_IN          \
        in_dword_masked(HWIO_MPM2_MPM_SLEEP_TIMETICK_COUNT_VAL_ADDR, HWIO_MPM2_MPM_SLEEP_TIMETICK_COUNT_VAL_RMSK)
#define HWIO_MPM2_MPM_SLEEP_TIMETICK_COUNT_VAL_INM(m)      \
        in_dword_masked(HWIO_MPM2_MPM_SLEEP_TIMETICK_COUNT_VAL_ADDR, m)
#define HWIO_MPM2_MPM_SLEEP_TIMETICK_COUNT_VAL_DATA_BMSK          0xffffffff
#define HWIO_MPM2_MPM_SLEEP_TIMETICK_COUNT_VAL_DATA_SHFT                 0x0

/*----------------------------------------------------------------------------
 * MODULE: MPM2_WDOG
 *--------------------------------------------------------------------------*/

#define MPM2_WDOG_REG_BASE                                      (MPM2_MPM_BASE      + 0x0000a000)

#define HWIO_MPM2_WDOG_RESET_REG_ADDR                           (MPM2_WDOG_REG_BASE      + 0x00000000)
#define HWIO_MPM2_WDOG_RESET_REG_RMSK                                  0x1
#define HWIO_MPM2_WDOG_RESET_REG_OUT(v)      \
        out_dword(HWIO_MPM2_WDOG_RESET_REG_ADDR,v)
#define HWIO_MPM2_WDOG_RESET_REG_WDOG_RESET_BMSK                       0x1
#define HWIO_MPM2_WDOG_RESET_REG_WDOG_RESET_SHFT                       0x0

#define HWIO_MPM2_WDOG_CTL_REG_ADDR                             (MPM2_WDOG_REG_BASE      + 0x00000004)
#define HWIO_MPM2_WDOG_CTL_REG_RMSK                             0x80000007
#define HWIO_MPM2_WDOG_CTL_REG_IN          \
        in_dword_masked(HWIO_MPM2_WDOG_CTL_REG_ADDR, HWIO_MPM2_WDOG_CTL_REG_RMSK)
#define HWIO_MPM2_WDOG_CTL_REG_INM(m)      \
        in_dword_masked(HWIO_MPM2_WDOG_CTL_REG_ADDR, m)
#define HWIO_MPM2_WDOG_CTL_REG_OUT(v)      \
        out_dword(HWIO_MPM2_WDOG_CTL_REG_ADDR,v)
#define HWIO_MPM2_WDOG_CTL_REG_OUTM(m,v) \
        out_dword_masked_ns(HWIO_MPM2_WDOG_CTL_REG_ADDR,m,v,HWIO_MPM2_WDOG_CTL_REG_IN)
#define HWIO_MPM2_WDOG_CTL_REG_WDOG_CLK_EN_BMSK                 0x80000000
#define HWIO_MPM2_WDOG_CTL_REG_WDOG_CLK_EN_SHFT                       0x1f
#define HWIO_MPM2_WDOG_CTL_REG_CHIP_AUTOPET_EN_BMSK                    0x4
#define HWIO_MPM2_WDOG_CTL_REG_CHIP_AUTOPET_EN_SHFT                    0x2
#define HWIO_MPM2_WDOG_CTL_REG_HW_SLEEP_WAKEUP_EN_BMSK                 0x2
#define HWIO_MPM2_WDOG_CTL_REG_HW_SLEEP_WAKEUP_EN_SHFT                 0x1
#define HWIO_MPM2_WDOG_CTL_REG_WDOG_EN_BMSK                            0x1
#define HWIO_MPM2_WDOG_CTL_REG_WDOG_EN_SHFT                            0x0

#define HWIO_MPM2_WDOG_STATUS_REG_ADDR                          (MPM2_WDOG_REG_BASE      + 0x00000008)
#define HWIO_MPM2_WDOG_STATUS_REG_RMSK                             0xfffff
#define HWIO_MPM2_WDOG_STATUS_REG_IN          \
        in_dword_masked(HWIO_MPM2_WDOG_STATUS_REG_ADDR, HWIO_MPM2_WDOG_STATUS_REG_RMSK)
#define HWIO_MPM2_WDOG_STATUS_REG_INM(m)      \
        in_dword_masked(HWIO_MPM2_WDOG_STATUS_REG_ADDR, m)
#define HWIO_MPM2_WDOG_STATUS_REG_WDOG_CNT_BMSK                    0xfffff
#define HWIO_MPM2_WDOG_STATUS_REG_WDOG_CNT_SHFT                        0x0

#define HWIO_MPM2_WDOG_BARK_VAL_REG_ADDR                        (MPM2_WDOG_REG_BASE      + 0x0000000c)
#define HWIO_MPM2_WDOG_BARK_VAL_REG_RMSK                        0x800fffff
#define HWIO_MPM2_WDOG_BARK_VAL_REG_IN          \
        in_dword_masked(HWIO_MPM2_WDOG_BARK_VAL_REG_ADDR, HWIO_MPM2_WDOG_BARK_VAL_REG_RMSK)
#define HWIO_MPM2_WDOG_BARK_VAL_REG_INM(m)      \
        in_dword_masked(HWIO_MPM2_WDOG_BARK_VAL_REG_ADDR, m)
#define HWIO_MPM2_WDOG_BARK_VAL_REG_OUT(v)      \
        out_dword(HWIO_MPM2_WDOG_BARK_VAL_REG_ADDR,v)
#define HWIO_MPM2_WDOG_BARK_VAL_REG_OUTM(m,v) \
        out_dword_masked_ns(HWIO_MPM2_WDOG_BARK_VAL_REG_ADDR,m,v,HWIO_MPM2_WDOG_BARK_VAL_REG_IN)
#define HWIO_MPM2_WDOG_BARK_VAL_REG_SYNC_STATUS_BMSK            0x80000000
#define HWIO_MPM2_WDOG_BARK_VAL_REG_SYNC_STATUS_SHFT                  0x1f
#define HWIO_MPM2_WDOG_BARK_VAL_REG_WDOG_BARK_VAL_BMSK             0xfffff
#define HWIO_MPM2_WDOG_BARK_VAL_REG_WDOG_BARK_VAL_SHFT                 0x0

#define HWIO_MPM2_WDOG_BITE_VAL_REG_ADDR                        (MPM2_WDOG_REG_BASE      + 0x00000010)
#define HWIO_MPM2_WDOG_BITE_VAL_REG_RMSK                        0x800fffff
#define HWIO_MPM2_WDOG_BITE_VAL_REG_IN          \
        in_dword_masked(HWIO_MPM2_WDOG_BITE_VAL_REG_ADDR, HWIO_MPM2_WDOG_BITE_VAL_REG_RMSK)
#define HWIO_MPM2_WDOG_BITE_VAL_REG_INM(m)      \
        in_dword_masked(HWIO_MPM2_WDOG_BITE_VAL_REG_ADDR, m)
#define HWIO_MPM2_WDOG_BITE_VAL_REG_OUT(v)      \
        out_dword(HWIO_MPM2_WDOG_BITE_VAL_REG_ADDR,v)
#define HWIO_MPM2_WDOG_BITE_VAL_REG_OUTM(m,v) \
        out_dword_masked_ns(HWIO_MPM2_WDOG_BITE_VAL_REG_ADDR,m,v,HWIO_MPM2_WDOG_BITE_VAL_REG_IN)
#define HWIO_MPM2_WDOG_BITE_VAL_REG_SYNC_STATUS_BMSK            0x80000000
#define HWIO_MPM2_WDOG_BITE_VAL_REG_SYNC_STATUS_SHFT                  0x1f
#define HWIO_MPM2_WDOG_BITE_VAL_REG_WDOG_BITE_VAL_BMSK             0xfffff
#define HWIO_MPM2_WDOG_BITE_VAL_REG_WDOG_BITE_VAL_SHFT                 0x0

/*----------------------------------------------------------------------------
 * MODULE: MPM2_PSHOLD
 *--------------------------------------------------------------------------*/

#define MPM2_PSHOLD_REG_BASE                                                    (MPM2_MPM_BASE      + 0x0000b000)

#define HWIO_MPM2_MPM_PS_HOLD_ADDR                                              (MPM2_PSHOLD_REG_BASE      + 0x00000000)
#define HWIO_MPM2_MPM_PS_HOLD_RMSK                                                     0x1
#define HWIO_MPM2_MPM_PS_HOLD_IN          \
        in_dword_masked(HWIO_MPM2_MPM_PS_HOLD_ADDR, HWIO_MPM2_MPM_PS_HOLD_RMSK)
#define HWIO_MPM2_MPM_PS_HOLD_INM(m)      \
        in_dword_masked(HWIO_MPM2_MPM_PS_HOLD_ADDR, m)
#define HWIO_MPM2_MPM_PS_HOLD_OUT(v)      \
        out_dword(HWIO_MPM2_MPM_PS_HOLD_ADDR,v)
#define HWIO_MPM2_MPM_PS_HOLD_OUTM(m,v) \
        out_dword_masked_ns(HWIO_MPM2_MPM_PS_HOLD_ADDR,m,v,HWIO_MPM2_MPM_PS_HOLD_IN)
#define HWIO_MPM2_MPM_PS_HOLD_PSHOLD_BMSK                                              0x1
#define HWIO_MPM2_MPM_PS_HOLD_PSHOLD_SHFT                                              0x0

#define HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_ADDR                                (MPM2_PSHOLD_REG_BASE      + 0x00000004)
#define HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_RMSK                                       0x1
#define HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_IN          \
        in_dword_masked(HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_ADDR, HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_RMSK)
#define HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_INM(m)      \
        in_dword_masked(HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_ADDR, m)
#define HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_OUT(v)      \
        out_dword(HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_ADDR,v)
#define HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_OUTM(m,v) \
        out_dword_masked_ns(HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_ADDR,m,v,HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_IN)
#define HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_DDR_PHY_FREEZEIO_EBI1_BMSK                 0x1
#define HWIO_MPM2_MPM_DDR_PHY_FREEZEIO_EBI1_DDR_PHY_FREEZEIO_EBI1_SHFT                 0x0

#define HWIO_MPM2_MPM_SSCAON_CONFIG_ADDR                                        (MPM2_PSHOLD_REG_BASE      + 0x00000008)
#define HWIO_MPM2_MPM_SSCAON_CONFIG_RMSK                                        0xffffffff
#define HWIO_MPM2_MPM_SSCAON_CONFIG_IN          \
        in_dword_masked(HWIO_MPM2_MPM_SSCAON_CONFIG_ADDR, HWIO_MPM2_MPM_SSCAON_CONFIG_RMSK)
#define HWIO_MPM2_MPM_SSCAON_CONFIG_INM(m)      \
        in_dword_masked(HWIO_MPM2_MPM_SSCAON_CONFIG_ADDR, m)
#define HWIO_MPM2_MPM_SSCAON_CONFIG_OUT(v)      \
        out_dword(HWIO_MPM2_MPM_SSCAON_CONFIG_ADDR,v)
#define HWIO_MPM2_MPM_SSCAON_CONFIG_OUTM(m,v) \
        out_dword_masked_ns(HWIO_MPM2_MPM_SSCAON_CONFIG_ADDR,m,v,HWIO_MPM2_MPM_SSCAON_CONFIG_IN)
#define HWIO_MPM2_MPM_SSCAON_CONFIG_SSCAON_CONFIG_BMSK                          0xffffffff
#define HWIO_MPM2_MPM_SSCAON_CONFIG_SSCAON_CONFIG_SHFT                                 0x0

#define HWIO_MPM2_MPM_SSCAON_STATUS_ADDR                                        (MPM2_PSHOLD_REG_BASE      + 0x0000000c)
#define HWIO_MPM2_MPM_SSCAON_STATUS_RMSK                                        0xffffffff
#define HWIO_MPM2_MPM_SSCAON_STATUS_IN          \
        in_dword_masked(HWIO_MPM2_MPM_SSCAON_STATUS_ADDR, HWIO_MPM2_MPM_SSCAON_STATUS_RMSK)
#define HWIO_MPM2_MPM_SSCAON_STATUS_INM(m)      \
        in_dword_masked(HWIO_MPM2_MPM_SSCAON_STATUS_ADDR, m)
#define HWIO_MPM2_MPM_SSCAON_STATUS_SSCAON_STATUS_BMSK                          0xffffffff
#define HWIO_MPM2_MPM_SSCAON_STATUS_SSCAON_STATUS_SHFT                                 0x0


#endif /* __MSMHWIOREG_H__ */
