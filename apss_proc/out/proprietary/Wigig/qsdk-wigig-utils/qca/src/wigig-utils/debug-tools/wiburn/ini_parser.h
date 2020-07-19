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

#ifndef _INI_PARSER_H_
#define _INI_PARSER_H_

#include "wiburn.h"
#include "ini_parser_types.h"
//#include "flash_sections.h"
#include "translation_map.h"

extern translation_map g_reg_tree_t_map;
extern translation_map g_ucode_t_map;
extern translation_map g_fw_t_map;

typedef enum _TRANSLATION_MAP_BITS
{
    TRANSLATION_MAP_NONE    = 0,
    TRANSLATION_MAP_REGTREE = 1,
    TRANSLATION_MAP_FW      = 2,
    TRANSLATION_MAP_UCODE    = 4,

} TRANSLATION_MAP_BITS;

bool get_value(const ini_section_t &ini_section,
               const string &key,
               u_int64_t *value,
               bool must_be = true);

bool get_string(const ini_section_t &ini_section,
                const string &key,
                void *string,
                u_int32_t max_length,
                bool must_be = true);



void get_resolved_data (ini_section_t::const_iterator sec_iter,
                        IMAGE *value);


class ini_parser_base
{
public:
    ini_parser_base () {};
    virtual ~ini_parser_base () {};
    virtual bool init (const char *filename, u_int32_t option = 0) = 0;
    virtual bool get_section(const string &section_name, u_int8_t translation_map_bits, ini_file_t::iterator *iter1, bool is_string, bool must_be) = 0;
    virtual void update_section(u_int8_t translation_map_bits, ini_section_t *orig, const ini_section_t &update, bool is_string) = 0;
};

template <class PRODUCT>
class ini_parser: public ini_parser_base
{
private:
    ini_file_t ini_file;
    ini_file_t::iterator current_ini_section;

public:
    ini_parser ();
    bool init (const char *filename, u_int32_t option = 0);
    bool get_section(const string &section_name, u_int8_t translation_map_bits, ini_file_t::iterator *iter1, bool is_string, bool must_be);

    //bool get_value(const ini_section_t &ini_section,
    //               const string &key,
    //               u_int64_t *value,
    //               bool must_be = true) const;
    //bool get_string(const ini_section_t &ini_section,
    //                  const string &key,
    //                  void *string,
    //                  u_int32_t max_length,
    //                  bool must_be = true) const;

    static void get_resolved_address_data (
        u_int8_t                        translation_map_bits,
        ini_section_t::const_iterator    sec_iter,
        typename PRODUCT::ADDRESS        *address,
        typename PRODUCT::REG            *value,
        bool                            word_addresses = false);

    static void get_resolved_address_data (
        u_int8_t                        translation_map_bits,
        ini_section_t::const_iterator    sec_iter,
        typename PRODUCT::ADDRESS        *address,
        typename PRODUCT::REG            *value,
        int                                *start,
        int                                *end,
        bool                                word_addresses = false);

    //void get_resolved_data (ini_section_t::const_iterator sec_iter,
    //                             IMAGE *value) const;

    void update_section(u_int8_t translation_map_bits, ini_section_t *orig, const ini_section_t &update, bool is_string);
private:
    ini_section_t::iterator find(
        u_int8_t                        translation_map_bits,
        ini_section_t::const_iterator    &lside,
        ini_section_t                    &ini_section,
        bool                            is_string) const;
    int get_clean_line (FILE *stream, char *buffer, const int buffer_size) const;

    bool hadProductionSeciton;

};


#endif// #ifndef _INI_PARSER_H_
