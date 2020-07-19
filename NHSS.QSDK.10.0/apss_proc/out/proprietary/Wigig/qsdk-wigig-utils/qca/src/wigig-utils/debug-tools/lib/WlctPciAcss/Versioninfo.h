/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
 */

#ifndef _VERSION_INFO_H_
#define _VERSION_INFO_H_

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

// The only place for version definition/editing
#define TOOL_VERSION_MAJOR 0
#define TOOL_VERSION_MINOR 1
#define TOOL_VERSION_MAINT 0
#define TOOL_VERSION_INTERM 36
#define TOOL_DESCRIPTION AUTO_GENERATED_DATE

#define TOOL_VERSION            TOOL_VERSION_MAJOR,TOOL_VERSION_MINOR,TOOL_VERSION_MAINT,TOOL_VERSION_INTERM
#define TOOL_VERSION_STR        STR(TOOL_VERSION_MAJOR.TOOL_VERSION_MINOR.TOOL_VERSION_MAINT.TOOL_VERSION_INTERM)

#define TOOL_FILE_DESCRIPTION   "WlctPciAcss Dynamic Link Library"
#define TOOL_INTERNAL_NAME      "WlctPciAcss"
#define TOOL_ORIGINAL_FILE_NAME "WlctPciAcss.dll"
#define TOOL_PRODUCT_NAME       "WlctPciA Dynamic Link Library"

#endif // _VERSION_INFO_H_
