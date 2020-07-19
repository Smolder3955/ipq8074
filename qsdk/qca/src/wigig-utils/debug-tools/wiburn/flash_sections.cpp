/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/
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

#include "flash_sections.h"
#include "ini_parser.h"
#include "flash_image.h"
#include <fstream>
#include <iostream>
#include <iomanip>

//extern class ini_parser g_parser;
//extern ini_parser g_parser;

u_int64_t current_fw_build = 999999;

void format_version_tag_t::disp(const char *name)
{
    printf("%s : %d\n", name, format_version);
}

void version_tag_t::disp(const char *name)
{
    printf("%s : %02d.%02d.%02d.%02d\n",name, major, minor, sub_minor, build);
}

void timestamp_tag_t::disp(const char *name)
{
    printf("%s (D/M/Y H:M:S) : %02d/%02d/%04d %02d:%02d:%02d\n"
           ,name, day, month, year, hour, min, sec );
}

void configuration_id_tag_t::disp(const char *name)
{
    //unused param
    (void)name;
    BYTE *null_terminated_buffer = new BYTE [sizeof (id) + 1];
    if (!null_terminated_buffer)
    {
        return;
    }

    memcpy(null_terminated_buffer, id, sizeof (id));
    null_terminated_buffer[sizeof (id)] = 0;
    printf("CONFIGURATION_ID = %s\n", null_terminated_buffer);
    delete[] null_terminated_buffer;

    //printf("CONFIGURATION_ID = ");
    //print_buffer(id, sizeof id);
    //printf("\n");
}

void device_id_tag_t::disp(const char *name)
{
    //unused param
    (void)name;
    cout << "device_id :" << endl;
    cout << " device_id = " << (int)device_id << endl;
    cout << " revision_id = " << (int)revision_id << endl;
    cout << " misc = " << (int)misc << endl;
}

void hw_id_tag_t::disp(const char *name)
{
    //unused param
    (void)name;
    cout << "hw_id :" << endl;
    cout << " digital_soc_id = " << (int)digital_soc_id << endl;
    cout << " board_id = " << (int)board_id<< endl;
    cout << " antenna_id = " << (int)antenna_id<< endl;
    cout << " rf_id = " << (int)rf_id<< endl;
    cout << " serial_id = " << (int)serial_id<< endl;
}

void end_tag_t::disp(const char *name)
{
    //unused param
    (void)name;
}

//
// TAG
//
tag_base_t::tag_base_t (BYTE id, const char *name)
{
    m_header.reserved = 0;
    m_header.tag_id = id;
    this->m_name = name;
}

template <typename Tag>
tag_t <Tag> ::tag_t (BYTE id, const char* name)
    :tag_base_t(id, name)
{
    m_header.tag_size = sizeof(Tag);
    memset(&m_tag, 0, sizeof(Tag));
}

template <typename Tag>
char *tag_t <Tag>::get_tag()
{
    return (char*)&m_tag;
};

template <typename Tag>
void tag_t<Tag>::disp()
{
    m_tag.disp(this->m_name);
}


//
// FLASH section
//
flash_section::flash_section (const char *name)
{
    this->m_name = name;
    this->m_size = 0;
    m_offset = 0;
    this->m_crc = 0;
    this->m_max_size = 0;
}

int flash_section::size () const
{
    return this->m_size;
}

int flash_section::get_max_size () const
{
    return this->m_max_size;
}

void flash_section::set_max_size ( u_int32_t max_size ) {
    this->m_max_size = max_size;
}

void flash_section::set_offset( u_int32_t offset )
{
    m_offset = offset;
}

u_int32_t flash_section::get_offset () const
{
    return m_offset;
}

//
// POINTER section
//
template <class PRODUCT>
pointer_section_t<PRODUCT>::pointer_section_t (const char *name)
    : flash_section(name)
{
    memset(&m_pointers, -1, sizeof (m_pointers));
    m_pointers.signature = 0x40;
    this->m_size = sizeof (m_pointers);
}

template <class PRODUCT>
void pointer_section_t<PRODUCT>::init (flash_base *fl)
{
    fl->read(0, this->m_size, (BYTE*)&m_pointers);
}

template <class PRODUCT>
bool pointer_section_t<PRODUCT>::operator == (const pointer_section_t <PRODUCT> &r_side) const
{
    int res = memcmp(&m_pointers, &(r_side.m_pointers), sizeof (m_pointers));
    return (res == 0);
}

template <class PRODUCT>
int pointer_section_t<PRODUCT>::write_to_buffer( BYTE *buffer )
{
    INFO("%s: Writing section of size 0x%04x at offset 0x%04x\n",this->m_name, this->m_size, m_offset);
    int current_offset = m_offset;
    memcpy(buffer + current_offset, &m_pointers, sizeof (m_pointers));
    current_offset += sizeof (m_pointers);

//   this->m_crc = crc_16((u_int32_t*)(buffer+m_offset), this->m_size - sizeof (this->m_crc));
    this->m_crc = m_CalcCRC.CalcCRC((UCHAR*)(buffer+m_offset), this->m_size - sizeof (this->m_crc));
    memcpy(buffer+current_offset, &this->m_crc, sizeof (this->m_crc));
    current_offset += sizeof(this->m_crc);

    // Current_offset would be the new ptr from which we continue to write to the file (should be used
    // when baseband is Talyn)
    return current_offset;
}

//
// ID section
//
//id_section::id_section (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t* ptr_high)
id_section::id_section (const char *name, BYTE id, ADDRESS32 *ptr)
    : flash_section(name)
{
    this->header.reserved = 0;
    this->header.section_id = id;
//   m_ptr_low = ptr_low;
//   m_ptr_high = ptr_high;
    this->m_ptr = ptr;
    this->m_size = sizeof(section_header_t);
};

void id_section::set_offset (int offset)
{
    flash_section::set_offset(offset);
    offset += sizeof(section_header_t);
    //*m_ptr_low = (u_int16_t)offset;
    //*m_ptr_high = (u_int16_t)(offset>>16);
    *this->m_ptr = offset;
}

//
// VARIABLE section
//variable_section_t::variable_section_t (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t* ptr_high)
//: id_section( name, id, ptr_low, ptr_high)
variable_section_t::variable_section_t (const char *name, BYTE id, ADDRESS32* ptr)
    : id_section( name, id, ptr)
{
    //nothing to do
}

variable_section_t::~variable_section_t ()
{
    //nothing to do
}

//
// HW SECTION
//
template <class PRODUCT>
//hw_section_t<PRODUCT>::hw_section_t (const char *name, BYTE id, u_int16_t *ptr_low , u_int16_t *ptr_high)
//: variable_section_t(name, id, ptr_low, ptr_high)
hw_section_t<PRODUCT>::hw_section_t (const char *name, BYTE id, ADDRESS32 *ptr)
    : variable_section_t(name, id, ptr)
{
    m_termination[0] = -1;
    m_termination[1] = -1;
    m_buffer = 0;
}

template <class PRODUCT>
int hw_section_t<PRODUCT>::write_to_buffer( BYTE *buffer )
{
    INFO("%s: Writing section of size 0x%04x at offset 0x%04x\n",this->m_name, this->m_size, m_offset);

    int current_offset = m_offset;
    memcpy(buffer + current_offset, &this->header, sizeof(this->header));
    current_offset += sizeof(this->header);
    memcpy(buffer + current_offset, m_buffer, m_buffer_size);
    current_offset += m_buffer_size;

    memcpy(buffer + current_offset, (char*)&m_termination, sizeof(m_termination));
    current_offset += sizeof (m_termination);

//   this->m_crc = crc_16((u_int32_t*)(buffer+m_offset), this->m_size - sizeof (this->m_crc));
    this->m_crc = m_CalcCRC.CalcCRC((UCHAR*)(buffer+m_offset), this->m_size - sizeof (this->m_crc));
    memcpy(buffer+current_offset, &this->m_crc, sizeof (this->m_crc));
    current_offset += sizeof(this->m_crc);

    // Current_offset would be the new ptr from which we continue to write to the file (should be used
    // when baseband is Talyn)
    return current_offset;
}

template <class PRODUCT>
void hw_section_t<PRODUCT>::handle_ini_section (u_int8_t translation_map_bits, const ini_section_t &ini_section)
{
    int size = ini_section.size();
    m_buffer = new address_value_t<PRODUCT> [size];
    m_buffer_size = size * (sizeof (address_value_t<PRODUCT>));
    memset((void*)m_buffer, -1, m_buffer_size);
    // buffer + this->header + FFFFs
    this->m_size += m_buffer_size + sizeof (m_termination) + sizeof (this->m_crc);

    u_int32_t section_size = this->m_size - sizeof(this->m_crc); //TBD remove - FW does not support CRC yet
    this->header.section_size = static_cast<u_int16_t>(section_size); // lower 16 bits
    this->header.reserved = (section_size & 0x00FF0000) >> 16; // 3rd byte goes to reserved

    ini_section_t::const_iterator sec_iter;
    sec_iter = ini_section.begin();
    int index = 0;
    typename PRODUCT::ADDRESS temp_address;
    typename PRODUCT::REG temp_value;
    while (sec_iter != ini_section.end())
    {
        ini_parser<PRODUCT>::get_resolved_address_data(translation_map_bits,
                                                       sec_iter,
                                                       &temp_address,
                                                       &temp_value);

        m_buffer[index].address = temp_address;
        m_buffer[index].value = temp_value;

        index++;
        sec_iter++;
    }

    // This check was removed due to the reason that the ptr is increased by the size read from
    // the ini file. There is no more check for the max size, it is not the responsibility of
    // wiburn anymore. Consider removing this check for all the sections (the best would be
    // removing it only for Talyn, because in Talyn there is no flash)
    //if (this->m_size > this->m_max_size) {
    //    ERR("Section %s with size %d exceeds maximum section size of %d\n",
    //        this->m_name, this->m_size, this->m_max_size );
    //    EXIT (-1);
    //}
}

template <class PRODUCT>
void hw_section_t<PRODUCT>::init (flash_base *fl)
{
    //u_int32_t section_address = (*m_ptr_low | ((*m_ptr_high) << 16));
    u_int32_t section_address = *this->m_ptr;
    u_int32_t header_address = section_address - sizeof (section_header_t);

    // get the section this->header
    fl->read(header_address, sizeof (section_header_t), (BYTE*)&this->header);

    // get the section
    m_buffer_size = this->header.section_size - sizeof (section_header_t) - sizeof (this->m_crc) - sizeof (m_termination);
    m_buffer_size += sizeof (this->m_crc); // TBD remove when CRC is supported by FW
    m_buffer = new address_value_t<PRODUCT> [m_buffer_size / (sizeof (address_value_t<PRODUCT>))];
    fl->read(section_address, m_buffer_size, (BYTE*)m_buffer);

    // get the termination
    fl->read(section_address+m_buffer_size, sizeof (m_termination), (BYTE*)&m_termination);
    // get the CRC ???

}

template <class PRODUCT>
bool hw_section_t<PRODUCT>::operator == (const hw_section_t <PRODUCT> &r_side) const
{
    int res = memcmp(&this->header, &r_side.header, sizeof (section_header_t));
    res |= memcmp(m_buffer, r_side.m_buffer, m_buffer_size);
    res |= memcmp(m_termination, r_side.m_termination, sizeof (m_termination));
    return (res == 0);
}

template <class PRODUCT>
hw_section_t<PRODUCT>::~hw_section_t ()
{
    delete m_buffer;
};


//
// System Config Section
//
template <class PRODUCT>
system_config_section_t<PRODUCT>::system_config_section_t (const char *name, ADDRESS32 *ptr)
    : flash_section(name)
{
    this->m_ptr = ptr;
}

template <class PRODUCT>
int system_config_section_t<PRODUCT>::write_to_buffer( BYTE *buffer )
{
    //unused param
    (void)buffer;
    INFO("%s: Preparing empty section at offset 0x%04x\n",this->m_name, m_offset);
    return 0;
}

template <class PRODUCT>
void system_config_section_t<PRODUCT>::set_offset (int offset)
{
    this->m_offset = offset;

    *this->m_ptr = offset;
    //pointer_section.m_pointers.config_section_pointer = ptr;
}


//
// FW section
//
template <class PRODUCT>
//fw_section_t<PRODUCT>::fw_section_t (const char *name, BYTE id, u_int16_t *ptr_low , u_int16_t *ptr_high)
//: variable_section_t(name, id, ptr_low , ptr_high)
fw_section_t<PRODUCT>::fw_section_t (const char *name, BYTE id, ADDRESS32 *ptr)
    : variable_section_t(name, id, ptr)
{
    m_buffer = 0;
}

template <class PRODUCT>
int fw_section_t<PRODUCT>::write_to_buffer(BYTE *buffer )
{
    INFO("%s: Writing section of size 0x%04x at offset 0x%04x\n",this->m_name, this->m_size, m_offset);

    int current_offset = m_offset;
    memcpy(buffer + current_offset, &this->header, sizeof (this->header));
    current_offset += sizeof (this->header);
    memcpy(buffer + current_offset, m_buffer, m_buffer_size);
    current_offset += m_buffer_size;

//   this->m_crc = crc_16((u_int32_t*)(buffer+m_offset), this->m_size - sizeof (this->m_crc));
    this->m_crc = m_CalcCRC.CalcCRC((UCHAR*)(buffer+m_offset), this->m_size - sizeof (this->m_crc));
    memcpy(buffer+current_offset, &this->m_crc, sizeof (this->m_crc));
    current_offset += sizeof(this->m_crc);

    // Current_offset would be the new ptr from which we continue to write to the file (should be used
    // when baseband is Talyn)
    return current_offset;
}

template <class PRODUCT>
void fw_section_t<PRODUCT>::handle_ini_section (u_int8_t translation_map_bits, const ini_section_t &ini_section)
{
    int size = ini_section.size();
    m_buffer = new address_value_mask_t<PRODUCT> [size];
    m_buffer_size = size * (sizeof (address_value_mask_t<PRODUCT>));
    memset((void*)m_buffer, -1, m_buffer_size);
    // buffer + this->header + CRC
    this->m_size += m_buffer_size + sizeof (this->m_crc);
    this->header.section_size = (u_int16_t)this->m_size;
//   this->header.section_size -= sizeof (this->m_crc); //TBD remove - FW does not support CRC yet

    ini_section_t::const_iterator sec_iter;
    sec_iter = ini_section.begin();
    int index = 0;
    typename PRODUCT::ADDRESS temp_address;
    typename PRODUCT::REG temp_value;
    int start, end;
    while (sec_iter != ini_section.end())
    {
        ini_parser<PRODUCT>::get_resolved_address_data(
            translation_map_bits,
            sec_iter,
            &temp_address,
            &temp_value,
            &start,
            &end);
        m_buffer[index].address = temp_address;
        m_buffer[index].value = temp_value;
        m_buffer[index].value <<= start;
        m_buffer[index].mask = ~(((2 << (end - start)) - 1) << start);
        index++;
        sec_iter++;
    }
    if (this->m_size > this->m_max_size) {
        ERR("Section %s with size %d exceeds maximum section size of %d\n",
            this->m_name, this->m_size, this->m_max_size );
        EXIT (-1);
    }
}

template <class PRODUCT>
fw_section_t<PRODUCT>::~fw_section_t ()
{
    delete m_buffer;
};

//
// Image section
//
//image_section_t::image_section_t (const char *name, BYTE id, u_int16_t *ptr_low,
//                                  u_int16_t *ptr_high, u_int16_t *length)
//: variable_section_t(name, id, ptr_low , ptr_high)
template <class PRODUCT>
image_section_t<PRODUCT>::image_section_t (const char *name, BYTE id, ADDRESS32 *ptr, typename PRODUCT::REG *length)
    : variable_section_t(name, id, ptr)
{
    m_length = length;
    m_buffer = 0;
}

template <class PRODUCT>
void image_section_t<PRODUCT>::set_offset (int offset)
{
    id_section::set_offset(offset);
    // length in pointer section is w/o the section this->header and w/o the CRC
    *m_length = ( typename PRODUCT::REG)(this->m_size - sizeof (section_header_t) - sizeof (this->m_crc));
}

template <class PRODUCT>
int image_section_t<PRODUCT>::write_to_buffer(BYTE *buffer)
{
    INFO("%s: Writing section of size 0x%04x at offset 0x%04x\n",this->m_name, this->m_size, m_offset);

    int current_offset = m_offset;
    memcpy(buffer + current_offset, &this->header, sizeof (this->header));
    current_offset += sizeof (this->header);
    memcpy(buffer + current_offset, m_buffer, m_buffer_size);
    current_offset += m_buffer_size;

//   this->m_crc = crc_16((u_int32_t*)(buffer+m_offset), this->m_size - sizeof (this->m_crc));
    this->m_crc = m_CalcCRC.CalcCRC((UCHAR*)(buffer+m_offset+sizeof (this->header)), m_buffer_size);
    memcpy(buffer+current_offset, &this->m_crc, sizeof (this->m_crc));
    current_offset += sizeof(this->m_crc);

    // Current_offset would be the new ptr from which we continue to write to the file (should be used
    // when baseband is Talyn)
    return current_offset;
}

template <class PRODUCT>
void image_section_t<PRODUCT>::handle_ini_section (const ini_section_t &ini_section)
{
    int size = ini_section.size();
    m_buffer = new IMAGE [size];
    m_buffer_size = size * (sizeof (IMAGE));
    memset((void*)m_buffer, -1, m_buffer_size);
    // buffer + this->header + CRC
    this->m_size += m_buffer_size + sizeof (this->m_crc);
    /*
        Although section size can be larger than 16bit number, this casting should stay.
        This section size is relevant only when using the '.bin' file with Boot Loader (when compiling FW using wiburn, it
        doesn't matter because DriverRecordsBuilder doesn't use this section size). This situation happens only on sparrow
        devices (and below). In this case the flash size can be at most 64K and then this section size is OK.
        Don't change it u_int32_t, because probably, in the Boot Loader, it looks for a section size of type uint16.
        Changing it uint32 could cause problems with burning FW using Boot Loader.
    */
    this->header.section_size = (u_int16_t)this->m_size;
//   this->header.section_size -= sizeof (this->m_crc); //TBD remove - FW does not support CRC yet

    ini_section_t::const_iterator sec_iter;
    sec_iter = ini_section.begin();
    int index = 0;
    while (sec_iter != ini_section.end())
    {
        get_resolved_data(sec_iter, &m_buffer[index]);
        index++;
        sec_iter++;
    }

    if (this->m_size > this->m_max_size) {
        ERR("Section %s with size %d exceeds maximum section size of %d\n",
            this->m_name, this->m_size, this->m_max_size );
        EXIT (-1);
    }
}

template <class PRODUCT>
image_section_t<PRODUCT>::~image_section_t ()
{
    delete m_buffer;
};

//
// TAG section
//

//info_section_t::info_section_t (const char *name, BYTE id, u_int16_t *ptr_low , u_int16_t *ptr_high)
//: variable_section_t(name, id, ptr_low , ptr_high)
template <class PRODUCT>
info_section_t<PRODUCT>::info_section_t (const char *name, BYTE id, ADDRESS32* ptr)
    : variable_section_t(name, id, ptr)
{
    this->m_size += sizeof (this->m_crc); //for CRC
}

template <class PRODUCT>
int info_section_t<PRODUCT>::write_to_buffer(BYTE *buffer)
{
    tag_t<end_tag_t> *tag = new tag_t<end_tag_t> (end_tag_id);
    this->m_size += sizeof (end_tag_t) + sizeof (tag_header_t);
    this->m_tags.insert(this->m_tags.end(), tag);
    this->header.section_size = (u_int16_t)this->m_size;
//    this->header.section_size -= sizeof (this->m_crc); //TBD remove - FW does not support CRC yet

    INFO("%s: Writing section of size 0x%04x at offset 0x%04x\n",this->m_name, this->m_size, m_offset);

    int current_offset = m_offset;
    memcpy(buffer + current_offset, &this->header, sizeof (this->header));
    current_offset += sizeof (this->header);

    tag_vector_t::const_iterator iter = this->m_tags.begin();
    while (iter != this->m_tags.end()) {
        tag_base_t *tag_base = *iter;
        memcpy(buffer + current_offset, &(tag_base->m_header), sizeof (tag_header_t));
        current_offset += sizeof (tag_header_t);

        memcpy(buffer + current_offset, tag_base->get_tag(), tag_base->m_header.tag_size);
        current_offset += tag_base->m_header.tag_size;
        iter++;
    }

//   this->m_crc = crc_16((u_int32_t*)(buffer+m_offset), this->m_size - sizeof (this->m_crc));
    this->m_crc = m_CalcCRC.CalcCRC((UCHAR*)(buffer+m_offset), this->m_size - sizeof (this->m_crc));
    memcpy(buffer+current_offset, &this->m_crc, sizeof (this->m_crc));
    current_offset += sizeof(this->m_crc);

    // Current_offset would be the new ptr from which we continue to write to the file (should be used
    // when baseband is Talyn)
    return current_offset;
}

template <class PRODUCT>
void info_section_t<PRODUCT>::disp() const
{
    tag_vector_t::const_iterator iter = this->m_tags.begin();
    while (iter != this->m_tags.end()) {
        tag_base_t *tag_base = *iter;
        tag_base->disp();
        iter++;
    }
}

template <class PRODUCT>
info_section_t<PRODUCT>::~info_section_t ()
{
    tag_vector_t::const_iterator iter = this->m_tags.begin();
    while (iter != this->m_tags.end()) {
        tag_base_t *tag_base = *iter;
        delete tag_base;
        iter++;
    }
}

//
// Image_info
//
//image_info_section_t::image_info_section_t (const char *name, BYTE id, u_int16_t *ptr_low , u_int16_t *ptr_high)
//:info_section_t(name, id, ptr_low, ptr_high)
template <class PRODUCT>
image_info_section_t<PRODUCT>::image_info_section_t (const char *name, BYTE id, ADDRESS32 *ptr)
    :info_section_t<PRODUCT>(name, id, ptr)
{
    // nothing to do
}

template <class PRODUCT>
void image_info_section_t<PRODUCT>::handle_ini_section (const ini_section_t &ini_section,
                                                        const string &name)
{
    string key;
    u_int64_t value;
    if( "image_format_version" == name ) {
        tag_t<format_version_tag_t> *tag = new tag_t<format_version_tag_t> (image_format_version_tag_id, name.data());
        this->m_size += sizeof (format_version_tag_t) + sizeof (tag_header_t);
        this->m_tags.insert(this->m_tags.end(), tag);
        key = "format_version";
        get_value(ini_section, key, &value );
        tag->m_tag.format_version = (BYTE)value;
    }
    else if (( "fw_version" == name ) || ( "ucode_version" == name )){
        tag_t<version_tag_t> *tag = new tag_t<version_tag_t> (fw_version_tag_id, name.data());
        this->m_size += sizeof (version_tag_t) + sizeof (tag_header_t);
        this->m_tags.insert(this->m_tags.end(), tag);
        key = "build";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.build = value;
            if ("fw_version" == name) {
                // Remember the current fw build number for later use
                current_fw_build = value;
            }
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "major";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.major = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "minor";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.minor = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "sub_minor";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.sub_minor = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
    }
    else if (("fw_timestamp" == name ) || ( "ucode_timestamp" == name )) {
        tag_t<timestamp_tag_t> *tag = new tag_t<timestamp_tag_t> (fw_timestamp_tag_id, name.data());
        this->m_size += sizeof (timestamp_tag_t) + sizeof (tag_header_t);
        this->m_tags.insert(this->m_tags.end(), tag);
        key = "sec";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.sec = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "min";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.min = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "hour";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.hour = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "day";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.day = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "month";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.month = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "year";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.year = (u_int16_t)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
    }
    else if ("configuration_id" == name ) {
        tag_t<configuration_id_tag_t> *tag = new tag_t<configuration_id_tag_t> (configuration_id_tag_id, name.data());
        this->m_size += sizeof (configuration_id_tag_t) + sizeof (tag_header_t);
        this->m_tags.insert(this->m_tags.end(), tag);
        key = "configuration_id";
        get_string(ini_section, key, tag->m_tag.id, sizeof (tag->m_tag.id));
    }
    else if ("device_id" == name ) {
        tag_t<device_id_tag_t> *tag = new tag_t<device_id_tag_t> (device_id_tag_id, name.data());
        this->m_size += sizeof (device_id_tag_t) + sizeof (tag_header_t);
        this->m_tags.insert(this->m_tags.end(), tag);
        key = "device_id";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.device_id = (u_int16_t)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "misc";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.misc = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "revision_id";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.revision_id = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
    }
    else if ("hw_id" == name ) {
        tag_t<hw_id_tag_t> *tag = new tag_t<hw_id_tag_t> (hw_id_tag_id, name.data());
        this->m_size += sizeof (hw_id_tag_t) + sizeof (tag_header_t);
        this->m_tags.insert(this->m_tags.end(), tag);
        key = "antenna_id";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.antenna_id = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "board_id";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.board_id = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "digital_soc_id";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.digital_soc_id = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "rf_id";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.rf_id = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "serial_id";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.serial_id = (u_int16_t)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
    } else {
        ERR("Unknown tag name %s\n", name.c_str());
        EXIT (-1);
    }

    this->header.section_size = (u_int16_t)this->m_size;
    this->header.section_size -= sizeof (this->m_crc); //TBD remove - FW does not support CRC yet

    if (this->m_size > this->m_max_size) {
        ERR("Section %s with size %d exceeds maximum section size of %d\n",
            this->m_name, this->m_size, this->m_max_size );
        EXIT (-1);
    }
}

template <class PRODUCT>
void image_info_section_t<PRODUCT>::init(flash_base *fl)
{
    //u_int32_t section_address = (*m_ptr_low | ((*m_ptr_high) << 16));
    u_int32_t section_address = *this->m_ptr;
    u_int32_t header_address = section_address - sizeof (section_header_t);

    // get the section header
    fl->read(header_address, sizeof (section_header_t), (BYTE*)&this->header);
    this->m_size = this->header.section_size;

    u_int16_t offset = sizeof (section_header_t);
    while (offset < this->header.section_size) {
        tag_header_t tag_header;
        fl->read(header_address+offset, sizeof (tag_header_t), (BYTE*)&tag_header);
        offset += sizeof (tag_header_t);

        tag_base_t *tag = NULL;
        switch (tag_header.tag_id) {
        case image_format_version_tag_id : {
            tag = new tag_t<format_version_tag_t> (tag_header.tag_id, "image_format_version");
            break;
        }
        case fw_version_tag_id : {
            tag = new tag_t<version_tag_t> (tag_header.tag_id, "fw_version");
            break;
        }
        case fw_timestamp_tag_id : {
            tag = new tag_t<timestamp_tag_t> (tag_header.tag_id, "fw_timestamp");
            break;
        }
        case configuration_id_tag_id : {
            tag = new tag_t<configuration_id_tag_t> (tag_header.tag_id, "configuration_id");
            break;
        }
        case device_id_tag_id : {
            tag = new tag_t<device_id_tag_t> (tag_header.tag_id, "device_id");
            break;
        }
        case hw_id_tag_id : {
            tag = new tag_t<hw_id_tag_t> (tag_header.tag_id, "hw_id");
            break;
        }
        case end_tag_id : {
            tag = new tag_t<end_tag_t> (tag_header.tag_id);
            break;
        }
        default: {
            ERR("Unknown tag id %d\n", tag_header.tag_id);
//            EXIT (-1);
        }
        }

        if (tag != NULL)
        {
            fl->read(header_address+offset, tag_header.tag_size, (BYTE*)tag->get_tag());
            offset += tag_header.tag_size;

            this->m_tags.insert(this->m_tags.end(), tag);
        }
    }
}

template <class PRODUCT>
image_info_section_t<PRODUCT>::~image_info_section_t ()
{
    // nothing to do
}

//
// USB_Info
//
//usb_info_section_t::usb_info_section_t (const char *name, BYTE id, u_int16_t *ptr_low , u_int16_t *ptr_high)
//:info_section_t(name, id, ptr_low, ptr_high)
template <class PRODUCT>
usb_info_section_t<PRODUCT>::usb_info_section_t (const char *name, BYTE id, ADDRESS32 *ptr)
    :info_section_t<PRODUCT>(name, id, ptr)
{
    // nothing to do
}

template <class PRODUCT>
void usb_info_section_t<PRODUCT>::handle_ini_section (const ini_section_t &ini_section,
                                                      const string &name)
{
    string key;
    u_int64_t value;
    if( "usb_image_format_version" == name ) {
        tag_t<format_version_tag_t> *tag = new tag_t<format_version_tag_t> (usb_format_version_tag_id, name.data());
        this->m_size += sizeof (format_version_tag_t) + sizeof (tag_header_t);
        this->m_tags.insert(this->m_tags.end(), tag);
        key = "format_version";
        get_value(ini_section, key, &value );
        tag->m_tag.format_version = (BYTE)value;
    }
    else if( "usb_version" == name ) {
        tag_t<version_tag_t> *tag = new tag_t<version_tag_t> (usb_version_tag_id, name.data());
        this->m_size += sizeof (version_tag_t) + sizeof (tag_header_t);
        this->m_tags.insert(this->m_tags.end(), tag);
        key = "build";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.build = value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "major";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.major = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "minor";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.minor = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "sub_minor";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.sub_minor = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
    }
    else if ("usb_timestamp" == name ) {
        tag_t<timestamp_tag_t> *tag = new tag_t<timestamp_tag_t> (usb_timestamp_tag_id, name.data());
        this->m_size += sizeof (timestamp_tag_t) + sizeof (tag_header_t);
        this->m_tags.insert(this->m_tags.end(), tag);
        key = "sec";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.sec = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "min";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.min = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "hour";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.hour = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "day";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.day = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "month";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.month = (BYTE)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
        key = "year";
        if( get_value(ini_section, key, &value )) {
            tag->m_tag.year = (u_int16_t)value;
        }
        else {
            ERR("Missing key %s in tag %s\n", key.c_str(), name.c_str());
            EXIT (-1);
        }
    } else {
        ERR("Unknown tag name %s\n", name.c_str());
        EXIT (-1);
    }

    this->header.section_size = (u_int16_t)this->m_size;
    this->header.section_size -= sizeof (this->m_crc); //TBD remove - FW does not support CRC yet

    if (this->m_size > this->m_max_size) {
        ERR("Section %s with size %d exceeds maximum section size of %d\n",
            this->m_name, this->m_size, this->m_max_size );
        EXIT (-1);
    }
}

template <class PRODUCT>
void usb_info_section_t<PRODUCT>::init(flash_base *fl)
{
    //u_int32_t section_address = (*m_ptr_low | ((*m_ptr_high) << 16));
    u_int32_t section_address = *this->m_ptr;
    u_int32_t header_address = section_address - sizeof (section_header_t);

    // get the section header
    fl->read(header_address, sizeof (section_header_t), (BYTE*)&this->header);
    this->m_size = this->header.section_size;

    u_int16_t offset = sizeof (section_header_t);
    while (offset < this->header.section_size) {
        tag_header_t tag_header;
        fl->read(header_address+offset, sizeof (tag_header_t), (BYTE*)&tag_header);
        offset += sizeof (tag_header_t);

        tag_base_t *tag = NULL;
        switch (tag_header.tag_id) {
        case usb_format_version_tag_id: {
            tag = new tag_t<format_version_tag_t> (tag_header.tag_id, "usb_format_version");
            break;
        }
        case usb_version_tag_id : {
            tag = new tag_t<version_tag_t> (tag_header.tag_id, "usb_version");
            break;
        }
        case usb_timestamp_tag_id : {
            tag = new tag_t<timestamp_tag_t> (tag_header.tag_id, "usb_timestamp");
            break;
        }
        case end_tag_id : {
            tag = new tag_t<end_tag_t> (tag_header.tag_id);
            break;
        }
        default: {
            ERR("Unknown tag id %d\n", tag_header.tag_id);
            return;
        }
        }

        fl->read(header_address+offset, tag_header.tag_size, (BYTE*)tag->get_tag());
        offset += tag_header.tag_size;

        this->m_tags.insert(this->m_tags.end(), tag);
    }
}

template <class PRODUCT>
usb_info_section_t<PRODUCT>::~usb_info_section_t ()
{
    // nothing to do
}


//
// Const section
//
template <typename T, class PRODUCT>
//const_section_t<T>::const_section_t (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t* ptr_high)
//: id_section(name, id, ptr_low, ptr_high)
const_section_t<T,PRODUCT>::const_section_t (const char *name, BYTE id, ADDRESS32 *ptr)
    : id_section(name, id, ptr)
{
    memset(&this->m_section, 0, sizeof (T));
    this->m_size += sizeof (T) + sizeof (this->m_crc);
    this->header.section_size = (u_int16_t)this->m_size;
}

template <typename T, class PRODUCT>
int const_section_t<T,PRODUCT>::write_to_buffer(BYTE *buffer )
{
    INFO("%s: Writing section of size 0x%04x at offset 0x%04x\n",this->m_name, this->m_size, m_offset);
    int current_offset = m_offset;

    memcpy(buffer + current_offset, &this->header, sizeof (this->header));
    current_offset += sizeof (this->header);

    memcpy(buffer + current_offset, &this->m_section, sizeof (T));
    current_offset += sizeof (T);

    this->m_crc = crc_16((u_int32_t*)(buffer+m_offset), this->m_size - sizeof (this->m_crc));
    memcpy(buffer+current_offset, &this->m_crc, sizeof (this->m_crc));
    current_offset += sizeof(this->m_crc);

    // Current_offset would be the new ptr from which we continue to write to the file (should be used
    // when baseband is Talyn)
    return current_offset;
}

template <typename T, class PRODUCT>
void const_section_t<T,PRODUCT>::init(flash_base *fl)
{
    //u_int32_t section_address = (*m_ptr_low | ((*m_ptr_high) << 16));
    u_int32_t section_address = *this->m_ptr;
    fl->read(section_address, sizeof (this->m_section), (BYTE*)&this->m_section);
}

template <typename T, class PRODUCT>
void const_section_t<T,PRODUCT>::initReduced(flash_base *fl, int reductionSize)
{
    //u_int32_t section_address = (*m_ptr_low | ((*m_ptr_high) << 16));
    u_int32_t section_address = *this->m_ptr - reductionSize;
    fl->read(section_address, sizeof (this->m_section), (BYTE*)&this->m_section);
}


//
// IDs section
//
//ids_section_t::ids_section_t (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t* ptr_high)
//: const_section_t(name, id, ptr_low, ptr_high)
template <class PRODUCT>
ids_section_t<PRODUCT>::ids_section_t (const char *name, BYTE id, ADDRESS32 *ptr)
    : const_section_t<ids_section,PRODUCT>(name, id, ptr)
{
    //nothing to do
}

template <class PRODUCT>
void ids_section_t<PRODUCT>::handle_ini_section (const ini_section_t &ini_section)
{
    u_int64_t value;

    get_value(ini_section, "version", &value);
    this->m_section.version = (BYTE)value;

    get_value(ini_section, "mac_address" , &value);
    memcpy(&this->m_section.mac_address, &value, sizeof (this->m_section.mac_address));

    get_string(ini_section, "ssid" , &this->m_section.ssid, sizeof (this->m_section.ssid));

    get_value(ini_section, "local", &value);
    this->m_section.local = (u_int16_t)value;

    get_value(ini_section, "ppm", &value);
    this->m_section.ppm = (u_int16_t)value;

    get_value(ini_section, "board", &value);
    this->m_section.board_type = (u_int32_t)value;

    get_value(ini_section, "lo_power_xif_gc", &value);
    this->m_section.lo_power_xif_gc = (u_int16_t)value;

    get_value(ini_section, "lo_power_stg2_bias", &value);
    this->m_section.lo_power_stg2_bias = (u_int16_t)value;

    get_value(ini_section, "vga_bias", &value);
    this->m_section.vga_bias = (u_int16_t)value;

    get_value(ini_section, "vga_stg1_fine_bias", &value);
    this->m_section.vga_stg1_fine_bias = (u_int16_t)value;

    get_value(ini_section, "ats_ver", &value);
    this->m_section.ats_ver = (u_int16_t)value;

    get_value(ini_section, "mlt_ver", &value);
    this->m_section.mlt_ver = (u_int16_t)value;

    get_value(ini_section, "bl_ver", &value);
    this->m_section.bl_ver = (u_int16_t)value;

    get_value(ini_section, "lo_power_gc_ctrl", &value);
    this->m_section.lo_power_gc_ctrl = (u_int16_t)value;

    get_value(ini_section, "production1", &value);
    this->m_section.production1 = (u_int16_t)value;

    get_value(ini_section, "production2", &value);
    this->m_section.production2 = (u_int16_t)value;

    get_value(ini_section, "production3", &value);
    this->m_section.production3 = (u_int16_t)value;

    get_value(ini_section, "production4", &value);
    this->m_section.production4 = (u_int16_t)value;

    get_value(ini_section, "production5", &value);
    this->m_section.production5 = (u_int16_t)value;

    get_value(ini_section, "production6", &value);
    this->m_section.production6 = (u_int16_t)value;

    get_value(ini_section, "production7", &value);
    this->m_section.production7 = (u_int16_t)value;

    get_value(ini_section, "production8", &value);
    this->m_section.production8 = (u_int16_t)value;

    get_value(ini_section, "production9", &value);
    this->m_section.production9 = (u_int16_t)value;

    get_value(ini_section, "production10", &value);
    this->m_section.production10 = (u_int16_t)value;

    get_value(ini_section, "production11", &value);
    this->m_section.production11 = (u_int16_t)value;

    get_value(ini_section, "production12", &value);
    this->m_section.production12 = (u_int16_t)value;

    get_value(ini_section, "production13", &value);
    this->m_section.production13 = (u_int16_t)value;

    get_value(ini_section, "production14", &value);
    this->m_section.production14 = (u_int16_t)value;

    get_value(ini_section, "production15", &value);
    this->m_section.production15 = (u_int16_t)value;

    get_value(ini_section, "production16", &value);
    this->m_section.production16 = (u_int16_t)value;


    if (this->m_size > this->m_max_size) {
        ERR("Section %s with size %d exceeds maximum section size of %d\n",
            this->m_name, this->m_size, this->m_max_size );
        EXIT (-1);
    }
}

template <class PRODUCT>
bool ids_section_t<PRODUCT>::set_value(const char* id_name, const char* id_value)
{
    u_int64_t value;

    if (strcmp("ssid", id_name) == 0)
    {
        ERR("Setting SSID field is not supported\n");
        //memcpy(&this->m_section.ssid, id_value, sizeof(this->m_section.ssid));

        return false;
    }

    char *end_ptr;
    value = STRTOULL(id_value, &end_ptr, 0);
    if (*end_ptr != 0) {
        ERR("Key %s: Failed converting value %s\n", id_name, id_value);
        return false;
    }

    if (strcmp("version", id_name) == 0)
    {
        this->m_section.version = (BYTE)value;
    }
    else if (strcmp("mac_address", id_name) == 0)
    {
        memcpy(&this->m_section.mac_address, &value, sizeof(this->m_section.mac_address));
    }
    else if (strcmp("local", id_name) == 0)
    {
        this->m_section.local = (u_int16_t)value;
    }
    else if (strcmp("ppm", id_name) == 0)
    {
        this->m_section.ppm = (u_int16_t)value;
    }
    else if (strcmp("board", id_name) == 0)
    {
        this->m_section.board_type = (u_int32_t)value;
    }
    else if (strcmp("lo_power_xif_gc", id_name) == 0)
    {
        this->m_section.lo_power_xif_gc = (u_int16_t)value;
    }
    else if (strcmp("lo_power_stg2_bias", id_name) == 0)
    {
        this->m_section.lo_power_stg2_bias = (u_int16_t)value;
    }
    else if (strcmp("vga_bias", id_name) == 0)
    {
        this->m_section.vga_bias = (u_int16_t)value;
    }
    else if (strcmp("vga_stg1_fine_bias", id_name) == 0)
    {
        this->m_section.vga_stg1_fine_bias = (u_int16_t)value;
    }
    else if (strcmp("ats_ver", id_name) == 0)
    {
        this->m_section.ats_ver = (u_int16_t)value;
    }
    else if (strcmp("mlt_ver", id_name) == 0)
    {
        this->m_section.mlt_ver = (u_int16_t)value;
    }
    else if (strcmp("bl_ver", id_name) == 0)
    {
        this->m_section.bl_ver = (u_int16_t)value;
    }
    else if (strcmp("lo_power_gc_ctrl", id_name) == 0)
    {
        this->m_section.lo_power_gc_ctrl = (u_int16_t)value;
    }
    else if (strcmp("production1", id_name) == 0)
    {
        this->m_section.production1 = (u_int16_t)value;
    }
    else if (strcmp("production2", id_name) == 0)
    {
        this->m_section.production2 = (u_int16_t)value;
    }
    else if (strcmp("production3", id_name) == 0)
    {
        this->m_section.production3 = (u_int16_t)value;
    }
    else if (strcmp("production4", id_name) == 0)
    {
        this->m_section.production4 = (u_int16_t)value;
    }
    else if (strcmp("production5", id_name) == 0)
    {
        this->m_section.production5 = (u_int16_t)value;
    }
    else if (strcmp("production6", id_name) == 0)
    {
        this->m_section.production6 = (u_int16_t)value;
    }
    else if (strcmp("production7", id_name) == 0)
    {
        this->m_section.production7 = (u_int16_t)value;
    }
    else if (strcmp("production8", id_name) == 0)
    {
        this->m_section.production8 = (u_int16_t)value;
    }
    else if (strcmp("production9", id_name) == 0)
    {
        this->m_section.production9 = (u_int16_t)value;
    }
    else if (strcmp("production10", id_name) == 0)
    {
        this->m_section.production10 = (u_int16_t)value;
    }
    else if (strcmp("production11", id_name) == 0)
    {
        this->m_section.production11 = (u_int16_t)value;
    }
    else if (strcmp("production12", id_name) == 0)
    {
        this->m_section.production12 = (u_int16_t)value;
    }
    else if (strcmp("production13", id_name) == 0)
    {
        this->m_section.production13 = (u_int16_t)value;
    }
    else if (strcmp("production14", id_name) == 0)
    {
        this->m_section.production14 = (u_int16_t)value;
    }
    else if (strcmp("production15", id_name) == 0)
    {
        this->m_section.production15 = (u_int16_t)value;
    }
    else if (strcmp("production16", id_name) == 0)
    {
        this->m_section.production16 = (u_int16_t)value;
    }
    else
    {
        return false;
    }

    return true;
}

template <class PRODUCT>
void ids_section_t<PRODUCT>::disp() const
{
    printf("version = %d\n",this->m_section.version);
    printf("MAC address = 0x%02x%02x%02x%02x%02x%02x\n", this->m_section.mac_address[5],
           this->m_section.mac_address[4],
           this->m_section.mac_address[3],
           this->m_section.mac_address[2],
           this->m_section.mac_address[1],
           this->m_section.mac_address[0]);
    printf("SSID = ");
    print_buffer(&this->m_section.ssid, sizeof (this->m_section.ssid));
    printf("\n");
    printf("local = %d\n",this->m_section.local);
    printf("ppm = %d\n",this->m_section.ppm);
    printf("board = %d\n",this->m_section.board_type);
    printf("lo_power_xif_gc = %d\n",this->m_section.lo_power_xif_gc);
    printf("lo_power_stg2_bias = %d\n",this->m_section.lo_power_stg2_bias);
    printf("vga_bias = %d\n",this->m_section.vga_bias);
    printf("vga_stg1_fine_bias = %d\n",this->m_section.vga_stg1_fine_bias);
    printf("ats_ver = %d\n",this->m_section.ats_ver);
    printf("mlt_ver = %d\n",this->m_section.mlt_ver);
    printf("bl_ver = %d\n",this->m_section.bl_ver);
    printf("lo_power_gc_ctrl = %d\n",this->m_section.lo_power_gc_ctrl);
    printf("production1 = %d\n",this->m_section.production1);
    printf("production2 = %d\n",this->m_section.production2);
    printf("production3 = %d\n",this->m_section.production3);
    printf("production4 = %d\n",this->m_section.production4);
    printf("production5 = %d\n",this->m_section.production5);
    printf("production6 = %d\n",this->m_section.production6);
    printf("production7 = %d\n",this->m_section.production7);
    printf("production8 = %d\n",this->m_section.production8);
    printf("production9 = %d\n",this->m_section.production9);
    printf("production10 = %d\n",this->m_section.production10);
    printf("production11 = %d\n",this->m_section.production11);
    printf("production12 = %d\n",this->m_section.production12);
    printf("production13 = %d\n",this->m_section.production13);
    printf("production14 = %d\n",this->m_section.production14);
    printf("production15 = %d\n",this->m_section.production15);
    printf("production16 = %d\n",this->m_section.production16);
}

template <class PRODUCT>
void ids_section_t<PRODUCT>::disp_to_file(const char* ids_ini_file) const
{
    std::ofstream fs(ids_ini_file);

    if(!fs)
    {
        printf("Cannot open the output file\n");
        return;
    }


    fs << "[IDS]" << endl;
    fs << "version = " << (int)(this->m_section.version) << endl;

    // Mac address
    fs << "mac_address = 0x" ;
    fs << hex << std::setw(2) << std::setfill('0') << (int)(this->m_section.mac_address[5]);
    fs << hex << std::setw(2) << std::setfill('0') << (int)(this->m_section.mac_address[4]);
    fs << hex << std::setw(2) << std::setfill('0') << (int)(this->m_section.mac_address[3]);
    fs << hex << std::setw(2) << std::setfill('0') << (int)(this->m_section.mac_address[2]);
    fs << hex << std::setw(2) << std::setfill('0') << (int)(this->m_section.mac_address[1]);
    fs << hex << std::setw(2) << std::setfill('0') << (int)(this->m_section.mac_address[0]);
    fs << endl;


    fs << std::setbase(10);
    fs << "local = " << (int)(this->m_section.local) << endl;
    fs << "ssid = " << this->m_section.ssid << endl;
    fs << "ppm = " << this->m_section.ppm << endl;
    fs << "board = " << this->m_section.board_type << endl;
    fs << "lo_power_xif_gc = " << this->m_section.lo_power_xif_gc << endl;
    fs << "lo_power_stg2_bias = " << this->m_section.lo_power_stg2_bias << endl;
    fs << "vga_bias = " << this->m_section.vga_bias << endl;
    fs << "vga_stg1_fine_bias = " << this->m_section.vga_stg1_fine_bias << endl;
    fs << "ats_ver = " << this->m_section.ats_ver << endl;
    fs << "mlt_ver = " << this->m_section.mlt_ver << endl;
    fs << "bl_ver = " << this->m_section.bl_ver << endl;
    fs << "lo_power_gc_ctrl = " << this->m_section.lo_power_gc_ctrl << endl;
    fs << "production1 = " << this->m_section.production1 << endl;
    fs << "production2 = " << this->m_section.production2 << endl;
    fs << "production3 = " << this->m_section.production3 << endl;
    fs << "production4 = " << this->m_section.production4 << endl;
    fs << "production5 = " << this->m_section.production5 << endl;
    fs << "production6 = " << this->m_section.production6 << endl;
    fs << "production7 = " << this->m_section.production7 << endl;
    fs << "production8 = " << this->m_section.production8 << endl;
    fs << "production9 = " << this->m_section.production9 << endl;
    fs << "production10 = " << this->m_section.production10 << endl;
    fs << "production11 = " << this->m_section.production11 << endl;
    fs << "production12 = " << this->m_section.production12 << endl;
    fs << "production13 = " << this->m_section.production13 << endl;
    fs << "production14 = " << this->m_section.production14 << endl;
    fs << "production15 = " << this->m_section.production15 << endl;
    fs << "production16 = " << this->m_section.production16 << endl;
    fs << std::setbase(16);
    fs.close();
}

//usb_section_t::usb_section_t (const char *name, u_int16_t *ptr_low, u_int16_t* ptr_high)
usb_section_t::usb_section_t (const char *name, ADDRESS32 *ptr)
    :flash_section(name)
{
    //m_ptr_low = ptr_low;
    //m_ptr_high = ptr_high;
    this->m_ptr = ptr;
    this->m_size = 0;
    m_buffer = 0;
}

int usb_section_t::write_to_buffer(BYTE *buffer )
{
    INFO("%s: Writing section of size 0x%04x at offset 0x%04x\n",this->m_name, this->m_size, m_offset);

    memcpy(buffer + m_offset, m_buffer, this->m_size);

    // Current_offset would be the new ptr from which we continue to write to the file (should be used
    // when baseband is Talyn)
    return m_offset + this->m_size;
}

void usb_section_t::handle_ini_section (const ini_section_t &ini_section)
{
    this->m_size = ini_section.size() * sizeof (IMAGE);
    m_buffer = new IMAGE [ini_section.size()];
    if (!m_buffer)
    {
        ERR("Cannot allocate a buffer of size %u\n", static_cast<unsigned>(ini_section.size()));
        EXIT(-1);
    }

    memset((void*)m_buffer, -1, this->m_size);

    ini_section_t::const_iterator sec_iter;
    sec_iter = ini_section.begin();
    int index = 0;
    while (sec_iter != ini_section.end())
    {
        get_resolved_data(sec_iter, &m_buffer[index]);
        index++;
        sec_iter++;
    }

    if (this->m_size > this->m_max_size) {
        ERR("Section %s with size %d exceeds maximum section size of %d\n",
            this->m_name, this->m_size, this->m_max_size );
        EXIT (-1);
    }
}

void usb_section_t::set_offset (int offset)
{
    flash_section::set_offset(offset);
    //*m_ptr_low = (u_int16_t)offset;
    //*m_ptr_high = (u_int16_t)(offset>>16);
    *this->m_ptr = offset;
}

usb_section_t::~usb_section_t()
{
    delete m_buffer;
}

//template class pointer_section_t<u_int16_t>;
//template class pointer_section_t<u_int32_t>;
