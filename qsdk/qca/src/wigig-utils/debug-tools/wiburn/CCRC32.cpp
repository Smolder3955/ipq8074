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

#include "wlct_os.h"
#include <stdio.h>
#include <stdlib.h>
#include "CCRC32.h"

CCRC32::CCRC32(void)
{
    int index = 0;
    while (index <= BYTE_MASK)
    {
        lookup[index] = Reflect(index, 8);
        lookup[index] = lookup[index] << 24;

        // Repeat 8 times
        int subindex = 0;
        while(subindex < 8)
        {
            subindex++;

            int check;
            if (lookup[index] & (1 << 31))
            {
                check = POLYNOMIAL;
            }
            else
            {
                check = 0;
            }

            lookup[index] = (lookup[index] << 1);
            lookup[index] = lookup[index] ^ check;
        }

        lookup[index] = Reflect(lookup[index], 32);
        index++;
    }
}

uint32_t CCRC32::Reflect(uint32_t reflected, const char bitsToReflect)
{
    uint32_t retVal = 0;

    int index = 1;

    while(index < (bitsToReflect + 1))
    {
        if((reflected % 2) == 1)
        {
            retVal |= (1 << (bitsToReflect - index));
        }
        else
        {
            // do nothing
        }
        reflected >>= 1;
        index++;
    }

    return retVal;
}

void CCRC32::CalcCRCStep(uint32_t *crc, const unsigned char *data, uint32_t length)
{
    uint32_t tempCRC = *crc;

    for (uint32_t index = 0; index < length; index++)
    {
        tempCRC = (tempCRC >> 8) ^ lookup[(tempCRC & BYTE_MASK) ^ *data++];
    }

    *crc = tempCRC;
}

uint32_t CCRC32::CalcCRC(const unsigned char *data, uint32_t length)
{
    uint32_t crc = CRC_MASK;
    CalcCRCStep(&crc, data, length);
    return(crc ^ CRC_MASK);
}
