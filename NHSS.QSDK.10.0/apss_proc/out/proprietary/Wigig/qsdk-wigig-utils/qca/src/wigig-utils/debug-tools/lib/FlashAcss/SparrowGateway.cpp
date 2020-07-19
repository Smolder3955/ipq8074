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

#include <stdio.h>
#include "SparrowGateway.h"

#define ONES32(size)                    ((size)?(0xffffffff>>(32-(size))):0)
#define MASK32(offset,size)             (ONES32(size)<<(offset))

#define EXTRACT_C32(source,offset,size)   ((((unsigned)(source))>>(offset)) & ONES32(size))
#define EXTRACT32(src,start,len)          (((len)==32)?(src):EXTRACT_C32(src,start,len))

#define MERGE_C32(rsrc1,rsrc2,start,len)  ((((rsrc2)<<(start)) & (MASK32((start),(len)))) | ((rsrc1) & (~MASK32((start),(len)))))
#define MERGE32(rsrc1,rsrc2,start,len)    (((len)==32)?(rsrc2):MERGE_C32(rsrc1,rsrc2,start,len))

SparrowGateway::SparrowGateway(const char *device_name)
{
    m_got_lock = false;
    int res;
    res =  CreateDeviceAccessHandler( device_name, MST_SPARROW, &m_handler );
    WLCT_UNREFERENCED_PARAM(res);
}

SparrowGateway::SparrowGateway(void* flashHandle)
{
    m_got_lock = false;
    m_handler = flashHandle;
}

SparrowGateway::~SparrowGateway(void)
{
    int res = CloseDeviceAccessHandler(m_handler);

    WLCT_UNREFERENCED_PARAM(res);
//     if (m_got_lock)
//     {
//       release_lock();
//     }
}

int SparrowGateway::runCommand(UCHAR flash_cmd, FLASH_PHASE phase, bool bWrite, DWORD addr, DWORD ulSize)
{
    int rc = 0;
    // write the address if it needed (according to the instruction phase)
    switch (phase)
    {
    case INSTRUCTION_PHASE:
        break;
    case INSTRUCTION_ADDRESS_PHASE:
        rc = setAddr(addr);
        break;
    case INSTRUCTION_DATA_PHASE:
        break;
    case INSTRUCTION_ADDRESS_DATA_PHASE:
        rc = setAddr(addr);
        break;
    }
    if (rc != 0)
    {
        return rc;
    }

    u_int32_t opcode_n_len = 0;

    // write the number of bytes to read or write
    opcode_n_len = MERGE32(opcode_n_len, ulSize, 0, 16);

    if (bWrite)
    {
        opcode_n_len = MERGE32(opcode_n_len, 1, BIT_OFFSET_WRITE_OP, 1);
    }

    switch (phase)
    {
    case INSTRUCTION_PHASE:
        break;
    case INSTRUCTION_ADDRESS_PHASE:
        opcode_n_len = MERGE32(opcode_n_len, 1, BIT_OFFSET_ADDR_PHASE, 1);
        break;
    case INSTRUCTION_DATA_PHASE:
        opcode_n_len = MERGE32(opcode_n_len, 1, BIT_OFFSET_DATA_PHASE, 1);
        break;
    case INSTRUCTION_ADDRESS_DATA_PHASE:
        opcode_n_len = MERGE32(opcode_n_len, 1, BIT_OFFSET_DATA_PHASE, 1);
        opcode_n_len = MERGE32(opcode_n_len, 1, BIT_OFFSET_ADDR_PHASE, 1);
        break;
    }
    opcode_n_len = MERGE32(opcode_n_len, flash_cmd, BIT_OFFSET_INST, BIT_SIZE_INST);

    rc = w(CR_FLASH_OPCODE, opcode_n_len);
    if (rc != 0)
    {
        return rc;
    }
    rc = execute_cmd();
    return rc;
}

int SparrowGateway::setAddr(DWORD addr)
{
    int rc = 0;
    rc = w(CR_FLASH_ADDR, addr);
    return rc;
}


int SparrowGateway::execute_cmd () const
{
    int rc = 0;
    // BIT_GO
    u_int32_t cmd = 0x40000000;
    if (m_got_lock)
    {
        // BIT_GO & BIT_LOCK
        cmd = 0xC0000000;
    }

    rc = w(CR_FLASH_STAT_CTL, cmd);
    if (rc != 0)
    {
        return rc;
    }
    rc = wait_done();
    return rc;
}

typedef struct _FOUR_BYTES_ {
    union {
        struct {
            u_int8_t b[4];
        }dummyStrct;

        u_int32_t _dword_;
    }dummyUnion;
}FOUR_BYTES;

int SparrowGateway::setData(const BYTE *buffer, DWORD size)
{
    int rc = 0;
    u_int32_t word;
    if ((size & 0x3) == 0)
    {    // size is module 4
        rc = wb(CR_FLASH_DATA, size, buffer);
    }
    else
    {
        for (u_int32_t i = 0 ; i < size ; i += 4)
        {
            // check the case that size is not DWORD align
            if (i+4 > size)
            {
                FOUR_BYTES fb;
                fb.dummyUnion._dword_ = 0;
                // copy the entire array (1, 2, or 3 bytes)
                for (DWORD j = 0; j < (size-i); j++)
                {
                    fb.dummyUnion.dummyStrct.b[j] = buffer[i+j];
                }
                word = fb.dummyUnion._dword_;
            }
            else
            {
                word = *((u_int32_t*)(&buffer[i]));
            }
            rc = w(CR_FLASH_DATA + i, word );
            if (rc != 0)
            {
                return rc;
            }
        }
    }
    return rc;
}

int SparrowGateway::getData(BYTE *buffer, DWORD size)
{
    int rc = 0;
    DWORD val;
    DWORD size2 = (size >> 2) << 2;
    DWORD remainder = size - size2;
    rc = rb(CR_FLASH_DATA, size2, buffer);
    return rc;

    WLCT_UNREFERENCED_PARAM(val);
    WLCT_UNREFERENCED_PARAM(remainder);
}

int SparrowGateway::wait_done (void) const
{
    int rc = 0;
    DWORD val;
    u_int32_t timeout = 0;
    do {
        rc = r(CR_FLASH_STAT_CTL, val);
        if (rc != 0)
        {
            return rc;
        }
        if (EXTRACT32(val, BIT_OFFSET_BUSY, 1) == 0) {
            return rc;
        }
        sleep_ms( 10 );
        timeout+= 10;
    } while (timeout <= 100);

    ERR("wait_done:: timeout reached\n");
    return (-1);
}

int SparrowGateway::boot_done(void) const
{
    int rc = 0;
    return rc;
}

bool SparrowGateway::get_lock ()
{
    if (g_debug) DBG("Trying to get flash lock\n");
    int rc = 0;
    u_int32_t timeout = 0;
    DWORD value;

    do {
        rc = r(CR_FLASH_STAT_CTL, value);
        if (rc != 0)
        {
            return false;
        }
        // bit BIT_LOCK
        if ( (value & 0x80000000) == 0) {
            m_got_lock = true;
            if (g_debug) DBG("Done\n");
            return true;
        }
        sleep_ms( 10 );
        timeout++;
    } while (timeout <= 10 );

    if (g_debug) DBG("Failed. Semaphore is 0x%04x\n", (unsigned int)value);
    return false;
}

int SparrowGateway::force_lock ()
{
    int rc = 0;
    //w (CR_FLASH_STAT_CTL, 1<<15);
    m_got_lock = true;
    if (g_debug) DBG("Forced flash lock\n");
    return rc;
}

int SparrowGateway::release_lock()
{
    int rc = 0;
    DWORD val;
    if ( m_got_lock )
    {
        rc = r(CR_FLASH_STAT_CTL, val);
        if (rc != 0)
        {
            return rc;
        }
        rc = w(CR_FLASH_STAT_CTL, val & 0x7fffffff);
        m_got_lock = false;
        if (g_debug) DBG("Released flash lock\n");
    }
    return rc;
}

int SparrowGateway::w(u_int32_t offset, DWORD val) const
{
    int rc = 0;
    rc = WlctAccssWrite(m_handler, offset, val);
    if( 0 != rc ) {
        ERR("Failed writing offset 0x%04x rc=%d\n", offset, rc);
    }
    return rc;
}

int SparrowGateway::r(u_int32_t offset, DWORD & val) const
{
    int rc = 0;
    rc = WlctAccssRead(m_handler, offset, val);
    if( 0 != rc ) {
        ERR("Failed reading offset 0x%04x rc=%d\n", offset, rc);
    }
    return rc;
}

int SparrowGateway::wb(u_int32_t offset, DWORD size, const BYTE *buffer)
{
    int rc = 0;
    rc = writeBlock(m_handler, offset, size, (char*)buffer);
    if( 0 != rc ) {
        ERR("Failed writing offset 0x%04x rc=%d\n", offset, rc);
    }
    return rc;
}

int SparrowGateway::rb(u_int32_t offset, DWORD size, const BYTE *buffer)
{
    int rc = 0;
    rc = readBlock(m_handler, offset, size, (char*)buffer);
    if( 0 != rc ) {
        ERR("Failed reading offset 0x%04x rc=%d\n", offset, rc);
    }
    return rc;
}

