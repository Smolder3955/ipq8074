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

#ifndef _INI_PARSER_TYPES_H_
#define _INI_PARSER_TYPES_H_

#include <string>
#include <map>
#include <vector>

using namespace std;

typedef pair <string,string> key_value_t;
typedef vector <key_value_t> ini_section_t;
typedef multimap <string, ini_section_t*> ini_file_t;

enum section_id_t {
    production_section_id = 0,
    ids_section_id = 1,
    hw_conf_section_id = 2,
    fw1_code_section_id = 3,
    fw1_data_section_id = 4,
    fw2_code_section_id = 5,
    fw2_data_section_id = 6,
    fw1_static_conf_section_id = 7,
    fw2_static_conf_section_id = 8,
    config_section_id = 9,
    image_info_section_id = 10,
    radio_tx_conf_section_id = 11,
    radio_rx_conf_section_id = 12,
    radio_tx_conf2_section_id = 13,
    radio_rx_conf2_section_id = 14,
    raw_data_section_id = 15,

    // The Following is unused !
    usb_info_section_id = 16,
    user_conf_section_id = 17,
    usb_section_id = 18,
};

#endif //#ifndef _INI_PARSER_TYPES_H_
