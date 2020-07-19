/*
 * FST Manager interface definitions
 *
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __FST_MANAGER_H__
#define __FST_MANAGER_H__

#include "utils/common.h"
#include "common/ieee802_11_defs.h"

#ifndef FST_MGR_COMPONENT
#error "FST_MGR_COMPONENT has to be defined"
#endif

#define FST_MANAGER_VERSION "0.02"

#define _fst_mgr_printf(format, ...) wpa_printf(MSG_ERROR, format, ##__VA_ARGS__)

extern unsigned int fst_debug_level;
extern char *fst_l2da_default_slave;

#include <sys/time.h>

#define fst_mgr_printf(level, format, ...) \
	do { \
		if ((level) >= fst_debug_level) { \
			struct timeval tv = {0}; \
			gettimeofday(&tv, NULL); \
			_fst_mgr_printf("[%08lu.%06lu] FST: " FST_MGR_COMPONENT \
				": %s: " format "\n", \
				(unsigned long)tv.tv_sec, \
				(unsigned long)tv.tv_usec, \
				__func__, ##__VA_ARGS__); \
		} \
	} while (0)


int  fst_manager_init(void);
void fst_manager_deinit(void);
const u8 *fst_mgr_get_addr_from_mbie(struct multi_band_ie *mbie);
#endif /* __FST_MANAGER_H__ */
