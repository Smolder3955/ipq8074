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

class SparrowGateway : public flash_gateway
{
public:
    SparrowGateway(const char *device_name);
    SparrowGateway(void* flashHandle);
    virtual ~SparrowGateway(void);
    virtual int runCommand(UCHAR flash_cmd, FLASH_PHASE phase, bool bWrite, DWORD addr, DWORD ulSize);
    virtual int setData(const BYTE *buffer, DWORD size);
    virtual int getData(BYTE *buffer, DWORD size);
    virtual int wait_done (void) const;
    virtual int boot_done(void) const;
    virtual bool get_lock ();
    virtual int force_lock ();
    virtual int release_lock();

private:

    int setAddr(DWORD addr);
//    void setSize(DWORD size);
    int setCommand(UCHAR cmd, FLASH_PHASE phase, bool bWrite);

    int wb(u_int32_t offset, DWORD size, const BYTE *buffer);
    int rb(u_int32_t offset, DWORD size, const BYTE *buffer);
    int r(u_int32_t offset, DWORD & val) const;
    int w(u_int32_t offset, DWORD val) const;
    int execute_cmd() const;

    bool m_got_lock;

    enum constants{
        CR_USER                 = 0x880000,
#ifndef MARLON_B0
        CR_FLASH_DATA           = CR_USER + 0xA8,
        CR_FLASH_ADDR           = CR_USER + 0x1B0,
        CR_FLASH_OPCODE         = CR_USER + 0x1AC,
        CR_FLASH_STAT_CTL       = CR_USER + 0x1A8,
#else
        CR_FLASH_DATA           = CR_USER + 0x5C,
        CR_FLASH_ADDR           = CR_USER + 0x164,
        CR_FLASH_OPCODE         = CR_USER + 0x160,
        CR_FLASH_STAT_CTL       = CR_USER + 0x15C,
#endif // MARLON_B0

        // CR_FLASH_OPCODE
        BIT_OFFSET_INST         = 16,
        BIT_SIZE_INST           = 8,
        BIT_OFFSET_ADDR_PHASE   = 24,
        BIT_OFFSET_DATA_PHASE   = 25,
        BIT_OFFSET_WRITE_OP     = 31,

        BIT_FLASH_STATUS_OFFSET = 0,
        BIT_FLASH_STATUS_SIZE   = 8,
        BIT_OFFSET_BUSY         = 29,
        BIT_GO                  = 30,
        BIT_LOCK                = 31,
    };

};
