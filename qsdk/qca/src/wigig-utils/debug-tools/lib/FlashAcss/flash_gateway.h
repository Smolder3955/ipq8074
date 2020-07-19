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

#pragma once
#include "flashacss.h"
#include "WlctPciAcss.h"

//using namespace std;

#define ONES16(size)                    ((size)?(0xffff>>(16-(size))):0)
#define MASK16(offset,size)             (ONES16(size)<<(offset))

#define EXTRACT_C(source,offset,size)   ((((unsigned)(source))>>(offset)) & ONES16(size))
#define EXTRACT(src,start,len)          (((len)==16)?(src):EXTRACT_C(src,start,len))

#define MERGE_C(rsrc1,rsrc2,start,len)  ((((rsrc2)<<(start)) & (MASK16((start),(len)))) | ((rsrc1) & (~MASK16((start),(len)))))
#define MERGE(rsrc1,rsrc2,start,len)    (((len)==16)?(rsrc2):MERGE_C(rsrc1,rsrc2,start,len))

typedef enum _FlashGatewayType
{
    FPT_SPARROW,
    FPT_MARLON

}FlashGatewayType;

typedef enum _FLASH_PHASE
{
    INSTRUCTION_PHASE                = 0,
    INSTRUCTION_ADDRESS_PHASE        = 1,
    INSTRUCTION_DATA_PHASE           = 2,
    INSTRUCTION_ADDRESS_DATA_PHASE   = 3
}FLASH_PHASE;

class flash_gateway
{
protected:

public:
    flash_gateway(const char *device_name);
    flash_gateway(void* flashHandle);
    flash_gateway(){}
    virtual ~flash_gateway(){};
    virtual int runCommand(UCHAR flash_cmd, FLASH_PHASE phase, bool bWrite, DWORD addr, DWORD ulSize) = 0;
    virtual int setData(const BYTE *buffer, DWORD size) = 0;
    virtual int getData(BYTE *buffer, DWORD size) = 0;
    virtual int wait_done (void) const = 0;
    virtual int boot_done(void) const = 0;
    virtual bool get_lock () = 0;
    virtual int force_lock () = 0;
    virtual int release_lock() = 0;

protected:
    void *m_handler;
};
