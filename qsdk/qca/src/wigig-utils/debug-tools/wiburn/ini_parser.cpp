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

#include "ini_parser.h"
#include <algorithm>
#include <stdio.h>
#include <errno.h>
#include <string.h>

template <class PRODUCT>
void ini_parser<PRODUCT>::update_section(u_int8_t translation_map_bits, ini_section_t *orig, const ini_section_t &update, bool is_string)
{
    if (*orig == update) {
        return;
    }

    ini_section_t::const_iterator update_iter = update.begin();
    ini_section_t::iterator orig_iter;
    while (update_iter != update.end() ) {
        orig_iter = find(translation_map_bits, update_iter, *orig, is_string);
        if (orig_iter == orig->end() ) {
            if (g_debug) DBG("Appending key %s value %s\n", update_iter->first.c_str(), update_iter->second.c_str());
            orig->insert(orig->end(), *update_iter); // append entry from the update section
        } else {
            if (g_debug) DBG("Replacing Key %s value %s with key %s value %s\n"
                             , orig_iter->first.c_str()
                             , orig_iter->second.c_str()
                             , update_iter->first.c_str()
                             , update_iter->second.c_str());
            *orig_iter = *update_iter; // replace the original entry with the entry from the update section
        }
        update_iter++;
    }
}

template <class PRODUCT>
ini_section_t::iterator ini_parser<PRODUCT>::find(
    u_int8_t                        translation_map_bits,
    ini_section_t::const_iterator   &lside,
    ini_section_t                   &ini_section,
    bool                            is_string) const
{
    //u_int8_t                        translation_map_bits = 1;
    typename PRODUCT::ADDRESS laddress,raddress;
    typename PRODUCT::REG lvalue, rvalue;
    int lstart, lend, rstart, rend;
    if (!is_string) {
        get_resolved_address_data(translation_map_bits, lside, &laddress, &lvalue, &lstart, &lend);
    }

    ini_section_t::iterator rside = ini_section.begin();
    while (rside != ini_section.end() ) {
        if (is_string) {
            if (lside->first == rside->first) {
                break;
            }
        } else {
            get_resolved_address_data(translation_map_bits, rside, &raddress, &rvalue, &rstart, &rend);
            if (laddress == raddress && lstart == rstart && lend == rend ) {
                break;
            }
        }
        rside++;
    }
    return rside;
}

//bool ini_parser::get_value (const ini_section_t &ini_section,
//const string &key,
//u_int64_t *value,
//bool must_be) const
bool get_value (const ini_section_t &ini_section,
                const string &key,
                u_int64_t *value,
                bool must_be)
{
    ini_section_t::const_iterator iter = ini_section.begin();
    while( ini_section.end() != iter ) {
        if( iter->first == key ) {
            char *end_ptr;
            *value = STRTOULL(iter->second.c_str(), &end_ptr, 0);
            if(*end_ptr != 0) {
                ERR("Key %s: Failed converting value %s\n", key.c_str(), iter->second.c_str());
                EXIT (-1);
            }
            return true;
        }
        iter++;
    }

    if(must_be) {
        ERR("Could not find key %s\n", key.c_str());
        EXIT (-1);
    }

    return false;
}


//bool ini_parser::get_string(const ini_section_t &ini_section,
//const string &key,
//void *str,
//u_int32_t max_length,
//bool must_be) const
bool get_string(const ini_section_t &ini_section,
                const string &key,
                void *str,
                u_int32_t max_length,
                bool must_be)
{
    ini_section_t::const_iterator iter = ini_section.begin();
    while( ini_section.end() != iter ) {
        if( iter->first == key ) {
            if (iter->second.size() > max_length ) {
                ERR("Key %s: length %lu exceeded maximal length of %u\n", key.c_str(), (unsigned long)iter->second.size(), max_length);
                EXIT (-1);
            }

            iter->second.copy((char*)str, max_length);

            return true;
        }
        iter++;
    }

    if(must_be) {
        ERR("Could not find key %s\n", key.c_str());
        EXIT (-1);
    }

    return false;
}

template <class PRODUCT>
void ini_parser<PRODUCT>::get_resolved_address_data (
    u_int8_t                        translation_map_bits,
    ini_section_t::const_iterator    sec_iter,
    typename PRODUCT::ADDRESS        *address,
    typename PRODUCT::REG            *value,
    bool                            word_addresses)
{
    int start, end;
    get_resolved_address_data(translation_map_bits, sec_iter, address, value, &start, &end, word_addresses);
}

template <class PRODUCT>
void ini_parser<PRODUCT>::get_resolved_address_data (
    u_int8_t                        translation_map_bits,
    ini_section_t::const_iterator    sec_iter,
    typename PRODUCT::ADDRESS        *address,
    typename PRODUCT::REG            *value,
    int                                *start,
    int                                *end,
    bool                            word_addresses
    )
{
    char *end_ptr;
    if(g_debug) DBG( "Dumping address %s  value %s\n", sec_iter->first.c_str(),sec_iter->second.c_str());
    *address = ( typename PRODUCT::ADDRESS)strtoul(sec_iter->first.c_str(), &end_ptr, 0);
    if (0 != *end_ptr) // it was a pathname
    {
        if(g_debug) DBG("Converting %s to ", sec_iter->first.c_str());
        u_int32_t temp_address;
        bool found = false;
        if (!found && (translation_map_bits & TRANSLATION_MAP_REGTREE))
        {
            found = g_reg_tree_t_map.get_from_translation_map(sec_iter->first.c_str(), &temp_address, start, end);
        }
        if (!found && (translation_map_bits & TRANSLATION_MAP_FW))
        {
            found = g_fw_t_map.get_from_translation_map(sec_iter->first.c_str(), &temp_address, start, end);
        }
        if (!found && (translation_map_bits & TRANSLATION_MAP_UCODE))
        {
            found = g_ucode_t_map.get_from_translation_map(sec_iter->first.c_str(), &temp_address, start, end);
        }

//      bool found = ::g_t_map.get_from_translation_map(sec_iter->first.c_str(), &temp_address, start, end);
        *address = ( typename PRODUCT::ADDRESS)temp_address;
        if (!found) {
            ERR("Could not find %s in translation map. Make sure that it is in the REG_TREE section in your INI file\n",
                sec_iter->first.c_str());
            EXIT (-1);
        }
        if (word_addresses) {
            if (*address & 0x1) {
                ERR("%s was converted to 0x%04x, which is not dword aligned\n",
                    sec_iter->first.c_str(),
                    *address );
                EXIT (-1);
            }

            *address = *address / 2;
        }
    }
    else
    {
        *start = 0;
        *end = sizeof ( typename PRODUCT::REG) * 8 - 1;
    }

    *value = ( typename PRODUCT::REG)strtoul(sec_iter->second.c_str(), &end_ptr, 0);

    if(g_debug) DBG( "Address %0#4x value %0#4x start %d end %d\n", *address, *value, *start, *end);
}

//void ini_parser::get_resolved_data (ini_section_t::const_iterator sec_iter,
//                                IMAGE *value) const
void get_resolved_data (ini_section_t::const_iterator sec_iter,
                        IMAGE *value)
{
    char *end_ptr;
    *value = strtoul(sec_iter->second.c_str(), &end_ptr, 0);
    if(g_debug) DBG( "Image %0#4x\n", *value);
}

template <class PRODUCT>
ini_parser<PRODUCT>::ini_parser ()
{
    this ->hadProductionSeciton = false;
    //nothing to do
}

template <class PRODUCT>
bool ini_parser<PRODUCT>::init (const char *filename, u_int32_t option)
{
    FILE *stream;
    printf("%s\n", filename);
    stream = fopen( filename, "r" );
    if( stream == NULL )
    {
//        char cwd[1024];
//        if (getcwd(cwd, sizeof(cwd)) != NULL)
//                   fprintf(stdout, "Current working dir: %s\n", cwd);
        printf("%s\n", strerror(errno));
        ERR( "Failed opening file %s \n", filename );
        EXIT (-1);
    }
    else
    {
        int reg_tree_section_index = 0;
        bool skip_section = false;
        char buffer [1100];
        char a1 [1000];
        char a2 [256];
        char a3 [256];
        char a4 [256];

        int ch;
        int num;
        u_int8_t translation_map_bits = TRANSLATION_MAP_NONE;

        do
        {
            ch = get_clean_line (stream, buffer, sizeof buffer);

            num = sscanf(buffer, "%999s %256s %256s %256s", a1, a2, a3, a4);
            switch (num)
            {
            case 1:
            {
                INFO( "Section [%s]\n", a1 );
                string section = a1;

                if (section == "production")
                {
                    if (hadProductionSeciton == true)
                    {
                        ERR( "Only one PRODUCTION Section is allowed in one burn command ! \n");
                        EXIT (-1);
                    }
                    hadProductionSeciton = true;
                }

                if (option == 1)
                {
                    if (section == "radio_tx_conf")
                    {
                        section += "2";
                    }
                    if (section == "radio_rx_conf")
                    {
                        section += "2";
                    }
                }

                skip_section = false;

                if (section == "fw_symbols")
                {
                    translation_map_bits = TRANSLATION_MAP_FW;
                }
                else if (section == "ucode_symbols")
                {
                    translation_map_bits = TRANSLATION_MAP_UCODE;
                }
                else if (section == "reg_tree")
                {
                    translation_map_bits = TRANSLATION_MAP_REGTREE;
                    // if [REG_TREE] section appear more then once then do not process
                    // its key, values
                    reg_tree_section_index++;
                    if (reg_tree_section_index > 1)
                    {
                        skip_section = true;
                    }
                }
                if (section != "reg_tree")
                {
                    if(g_debug) DBG("Adding ini section\n");
                    ini_section_t *sec = new ini_section_t;
                    pair <string, ini_section_t*> p (section, sec);
                    current_ini_section = ini_file.insert(p);
                }
                break;
            }
            case 2:
            {
                if(g_debug) DBG( "Address %s Value %s\n", a1, a2 );
                key_value_t entry (a1, a2);
                pair <ini_section_t::iterator, bool> ret;
                ini_section_t *sec = this->current_ini_section->second;
                sec->push_back(entry);
                break;
            }
            case 3:
            {
                if(g_debug) DBG( "Address %s[%s] Value %s\n", a1, a3, a2 );
                string temp = a1;
                temp += '#';
                temp.append(a3);
                key_value_t entry (temp, a2);
                pair <ini_section_t::iterator, bool> ret;
                ini_section_t *sec = this->current_ini_section->second;
                sec->push_back(entry);
                break;
            }

            case 4:
            {
                if (!skip_section)
                {
                    bool bInsert = false;

                    if(g_debug) DBG ("path %s address %s start %s end %s\n", a1, a2, a3, a4);

                    if (translation_map_bits == TRANSLATION_MAP_REGTREE)
                    {
                        bInsert = g_reg_tree_t_map.add_to_translation_map(a1, a2, a3, a4);
                    }
                    else if (translation_map_bits == TRANSLATION_MAP_FW)
                    {
                        bInsert = g_fw_t_map.add_to_translation_map(a1, a2, a3, a4);
                    }
                    else if (translation_map_bits == TRANSLATION_MAP_UCODE)
                    {
                        bInsert = g_ucode_t_map.add_to_translation_map(a1, a2, a3, a4);
                    }

                    //                        if( !g_t_map.add_to_translation_map(a1, a2, a3, a4) )
                    if (!bInsert)
                    {
                        ERR("Failed adding to translation map: path %s address %s start %s end %s\n", a1, a2, a3, a4);
                        EXIT (-1);
                    };
                }
                break;
            }
            default : if(g_debug) DBG( "empty line or comment line\n" );break;
            }
        } while (EOF != ch);

        fclose(stream);
    }
    return true;
}

template <class PRODUCT>
int ini_parser<PRODUCT>::get_clean_line (FILE *stream, char *buffer, const int buffer_size) const
{
    int i;
    int ch=0;
    bool alpha_detected = false;
    bool section_detected = false;

    for (i = 0; i < buffer_size; i++)
    {
        ch = getc(stream);
        if( ch >= 'A' && ch <= 'Z' )
        {
            ch = ch - 'A' + 'a'; // change to lower case
        }

        alpha_detected  = alpha_detected | (ch >= 'a' && ch <= 'z') ;

        if (EOF == ch || '\n' == ch)
        {
            buffer[i] = '\0';
            break;
        }
        else if ('[' == ch && !alpha_detected)
        {
            section_detected = true;
            buffer[i] = ' ';
        }
        else if (']' == ch && section_detected)
        {
            buffer[i] = ' ';
        }
        else if ( '=' == ch)
        {
            buffer[i] = ' ';
        }
        else if ('#' == ch || ';' == ch)
        {
            buffer[i] = '\0';
        } else
        {
            buffer[i] = (char)ch;
        }
    }
    return ch;
}
template <class PRODUCT>
bool ini_parser<PRODUCT>::get_section(const string &section_name, u_int8_t translation_map_bits, ini_file_t::iterator *iter1, bool is_string, bool must_be)
{
    //unused param
    (void)must_be;

    ini_file_t::iterator iter = ini_file.begin();
    *iter1 = ini_file.end();
    while (iter != ini_file.end() ) {
        if (iter->first == section_name ) {
            if (*iter1 == ini_file.end()){
                *iter1 = iter;
            }else{
                this->update_section(translation_map_bits, (*iter1)->second, *(iter->second), is_string);
            }
        }
        iter++;
    }

    //
    // adding this lines cause the application stop running if the section must_be and did not find
    // in other words, this function always return true
    // if you decide to remove these lines, you should handle the return code in the caller function
    //
    /*if (must_be && *iter1 == ini_file.end()) {
      ERR("Could not find mandatory section %s\n", section_name.c_str() );
      EXIT (-1);
      }*/

    return (*iter1 != ini_file.end() );
}

