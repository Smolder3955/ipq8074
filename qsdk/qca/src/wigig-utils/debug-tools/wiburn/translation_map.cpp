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

#include "translation_map.h"

#include "wlct_os.h"

translation_map::translation_map ()
{
    //nothing to do
}

translation_map::~translation_map ()
{
    //nothing to do
}

bool translation_map::add_to_translation_map (const char* key, const char* address, const char* start, const char* end)
{
    u_int32_t int_address;
    int int_start, int_end;
    char *end_ptr;

    int_address = (u_int32_t)strtoul(address, &end_ptr, 0);
    if (0 != *end_ptr) {
        ERR("Error converting %s to int\n", address);
        EXIT (-1);
    }

    int_start = strtoul(start, &end_ptr, 0);
    if (0 != *end_ptr) {
        ERR("Error converting %s to int\n", start);
        EXIT (-1);
    }

    int_end = strtoul(end, &end_ptr, 0);
    if (0 != *end_ptr) {
        ERR("Error converting %s to int\n", end);
        EXIT (-1);
    }

    return add_to_translation_map(key, int_address, int_start, int_end);
}


bool translation_map::add_to_translation_map (const char* key, const u_int32_t address, const int start, const int end)
{
    full_address_t temp_address = {address, start, end};
    string temp_string (key);
    translation_map_t::value_type entry (key, temp_address);
    pair <translation_map_t::iterator, bool> ret;
    ret = m_map.insert(entry);
    return (ret.second);
}

bool translation_map::get_from_translation_map (const char* key, u_int32_t *address, int *start, int *end) const
{
    translation_map_t::const_iterator iter;
    string temp_string (key);
    iter = m_map.find(temp_string);
    if ( iter == m_map.end( ) )
    {
        return false;
    } else {
        *address = iter->second.address;
        *start = iter->second.start;
        *end = iter->second.end;
        return true;
    }
}

