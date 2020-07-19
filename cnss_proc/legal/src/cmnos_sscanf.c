/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * $ATH_LICENSE_TARGET_C$
 */

#include "osapi.h"

LOCAL int sscanf(const char *s, const char *format, ... )
{
    return 0;
} 

LOCAL A_ULONG cmnos_atoul(char *string)
{
    return 0;
}

void cmnos_sccanf_init(void)
{
}

#if defined(CONFIG_SSCANF_INDIRECT_SUPPORT)
void cmnos_sscanf_module_install(struct sscanf_api *tbl)
{
    tbl->_init = cmnos_sccanf_init;
    tbl->_sscanf = sscanf;
    tbl->_atoul = cmnos_atoul;
}
#endif /* CONFIG_SSCANF_INDIRECT_SUPPORT */
