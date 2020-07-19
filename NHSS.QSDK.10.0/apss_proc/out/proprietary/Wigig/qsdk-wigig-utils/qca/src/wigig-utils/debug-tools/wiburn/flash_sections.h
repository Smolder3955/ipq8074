/*
* Copyright (c) 2018 Qualcomm Technologies, Inc.
*
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

#ifndef _FLASH_SECTIONS_H_
#define _FLASH_SECTIONS_H_

#include "wiburn.h"
#include "ini_parser_types.h"
#include "flash.h"
#include "CCRC32.h"

enum tag_ids {
    image_format_version_tag_id = 0x1,
    fw_version_tag_id = 0x2,
    fw_timestamp_tag_id = 0x3,
    usb_format_version_tag_id = 0x1,
    usb_version_tag_id = 0x2,
    usb_timestamp_tag_id = 0x3,
    configuration_id_tag_id = 0x4,
    device_id_tag_id = 0x5,
    hw_id_tag_id = 0x6,
    end_tag_id =    0xff
};

#pragma pack(push, 1)
template <class PRODUCT>
struct pointers_t
{
    typename PRODUCT::REG   signature;
    ADDRESS32               hw_pointer;
    ADDRESS32               fw1_pointer;
    typename PRODUCT::REG   fw1_length;
    ADDRESS32               fw1_data_pointer;
    typename PRODUCT::REG   fw1_data_length;
    ADDRESS32               fw2_pointer;
    typename PRODUCT::REG   fw2_length;
    ADDRESS32               fw2_data_pointer;
    typename PRODUCT::REG   fw2_data_length;
    ADDRESS32               production_pointer;
    ADDRESS32               ids_pointer;
    typename PRODUCT::REG   pointer_section_version;
    ADDRESS32               fw1_static_conf_pointer;
    ADDRESS32               fw2_static_conf_pointer;
    ADDRESS32               config_section_pointer;
    ADDRESS32               image_info_pointer;
    ADDRESS32               radio_tx_cnf_pointer;
    ADDRESS32               radio_rx_cnf_pointer;
    ADDRESS32               radio_tx_cnf2_pointer;
    ADDRESS32               radio_rx_cnf2_pointer;
    ADDRESS32               raw_data_pointer;
    typename PRODUCT::REG   raw_data_length;

    // Unused
    ADDRESS32               usb_pointer;
    ADDRESS32               usb_info_pointer;
    ADDRESS32               user_pointer;
};
#pragma pack(pop)

#pragma pack(push, 1)
template <class PRODUCT>
struct address_value_t
{
    typename PRODUCT::ADDRESS address;
    typename PRODUCT::REG value;
};
#pragma pack(pop)

#pragma pack(push, 1)
template <class PRODUCT>
struct address_value_mask_t
{
    typename PRODUCT::ADDRESS address;
    typename PRODUCT::REG value;
    typename PRODUCT::REG mask;
};
#pragma pack(pop)


#pragma pack(push, 1)
struct tag_header_t
{
    BYTE reserved;
    BYTE tag_id;
    u_int16_t tag_size;
    void disp(const char *name);
};
#pragma pack(pop)

#pragma pack(push, 1)
struct format_version_tag_t
{
    BYTE format_version;
    BYTE reserved [3];
    void disp(const char *name);
};
#pragma pack(pop)

#pragma pack(push, 1)
struct version_tag_t
{
    u_int32_t minor : 8;
    u_int32_t major : 8;
    u_int32_t build : 13;
    u_int32_t sub_minor : 3;
    void disp(const char *name);
};
#pragma pack(pop)

#pragma pack(push, 1)
struct timestamp_tag_t
{
    BYTE min;
    BYTE hour;
    BYTE reserved1;
    BYTE sec;
    BYTE month;
    BYTE day;
    u_int16_t year;
    void disp(const char *name);
};
#pragma pack(pop)

#pragma pack(push, 1)
struct configuration_id_tag_t
{
    BYTE id [16];
    void disp(const char *name);
};
#pragma pack(pop)

#pragma pack(push, 1)
struct device_id_tag_t
{
    u_int16_t device_id;
    BYTE revision_id;
    BYTE misc;
    void disp(const char *name);
};
#pragma pack(pop)

#pragma pack(push, 1)
struct hw_id_tag_t
{
    BYTE digital_soc_id;
    BYTE board_id;
    BYTE antenna_id;
    BYTE rf_id;
    u_int16_t serial_id;
    u_int16_t reserved;
    void disp(const char *name);
};
#pragma pack(pop)

#pragma pack(push, 1)
struct end_tag_t
{
    u_int16_t reserved1;
    u_int16_t reserved2;
    void disp(const char *name);
};
#pragma pack(pop)

class tag_base_t
{
public:
    virtual char *get_tag() = 0;
    tag_base_t (BYTE id, const char* name);
    virtual ~tag_base_t() {};
    virtual void disp() = 0;
public:
    tag_header_t m_header;
    const char *m_name;
};

template <class Tag>
class tag_t: public tag_base_t
{
public:
    char *get_tag();
    tag_t (BYTE id, const char* name = 0);
    virtual ~tag_t() {};
    void disp();
public:
    Tag m_tag;
};


#pragma pack(push, 1)
struct section_header_t
{
    BYTE reserved;
    BYTE section_id;
    /*
    Although section size can be larger than 16bit number, this casting should stay.
    This section size is relevant only when using the '.bin' file with Boot Loader (when compiling FW using wiburn, it
    doesn't matter because DriverRecordsBuilder doesn't use this section size). This situation happens only on sparrow
    devices (and below). In this case the flash size can be at most 64K and then this section size is OK.
    Don't change it u_int32_t, because probably, in the Boot Loader, it looks for a section size of type uint16.
    Changing it uint32 could cause problems with burning FW using Boot Loader.
    */
    u_int16_t section_size;
};
#pragma pack(pop)

class flash_section
{
public:
    flash_section (const char *name);
    virtual int write_to_buffer(BYTE *buffer ) = 0;
    int size () const;
    int get_max_size () const;
    void set_max_size ( u_int32_t max_size );
    void set_offset (u_int32_t offset);
    u_int32_t get_offset () const;
    virtual ~flash_section() {};
protected:
    u_int32_t m_size;
    u_int32_t m_max_size;
    u_int32_t m_offset;
    const char *m_name;
    u_int32_t m_crc;
    CCRC32 m_CalcCRC;
};

class usb_section_t : public flash_section
{
public:
    //usb_section_t (const char *name, u_int16_t *ptr_low, u_int16_t* ptr_high);
    usb_section_t (const char *name, ADDRESS32 *ptr);
    int write_to_buffer(BYTE *buffer ) ;
    void handle_ini_section (const ini_section_t &ini_section);
    void set_offset (int offset);
    ~usb_section_t();
public:
    IMAGE *m_buffer;
//   u_int16_t *m_ptr_low;
//   u_int16_t *m_ptr_high;
    ADDRESS32 *m_ptr;
};

class id_section : public flash_section
{
public:
    //id_section (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t* ptr_high);
    id_section (const char *name, BYTE id, ADDRESS32 *ptr);
    void set_offset (int offset);

public:
    section_header_t header;
//   u_int16_t *m_ptr_low;
//   u_int16_t *m_ptr_high;
    ADDRESS32 *m_ptr;
};

class variable_section_t : public id_section
{
public:
//   variable_section_t (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t *ptr_high);
    variable_section_t (const char *name, BYTE id, ADDRESS32 *ptr);
    ~variable_section_t ();
protected:
    int m_buffer_size;
};

template <class PRODUCT>
class hw_section_t : public variable_section_t
{
public:
//   hw_section_t (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t *ptr_high);
    hw_section_t (const char *name, BYTE id, ADDRESS32 *ptr);
public:
    void handle_ini_section (u_int8_t translation_map_bits, const ini_section_t &ini_section);
    void init (flash_base *flash);
    bool operator == (const hw_section_t &r_side) const;
    int write_to_buffer(BYTE *buffer ) ;
    ~hw_section_t ();
    address_value_t <PRODUCT> *m_buffer;
    u_int16_t *m_length;
    u_int32_t m_termination [2];

};

template <class PRODUCT>
class fw_section_t : public variable_section_t
{
public:
    //fw_section_t (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t *ptr_high);
    fw_section_t (const char *name, BYTE id, ADDRESS32 *ptr);
public:
    void handle_ini_section (u_int8_t translation_map_bits, const ini_section_t &ini_section);
    int write_to_buffer(BYTE *buffer ) ;
    ~fw_section_t ();
    address_value_mask_t <PRODUCT> *m_buffer;
};

template <class PRODUCT>
class image_section_t : public variable_section_t
{
public:
    //image_section_t (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t *ptr_high, u_int16_t *length);
    image_section_t (const char *name, BYTE id, ADDRESS32 *ptr, typename PRODUCT::REG *length);
    void handle_ini_section (const ini_section_t &ini_section);
    int write_to_buffer(BYTE *buffer ) ;
    void set_offset (int offset);
    ~image_section_t ();
public:
    IMAGE *m_buffer;
    //u_int16_t *m_length;
    typename PRODUCT::REG *m_length;
};

template <class PRODUCT>
class pointer_section_t : public flash_section
{
public:
    pointer_section_t (const char *name);
    void init (flash_base *fl);
    int write_to_buffer(BYTE *buffer ) ;
    bool operator == (const pointer_section_t <PRODUCT> &r_side) const;
public:
    pointers_t <PRODUCT> m_pointers;
};

template <class PRODUCT>
class info_section_t : public variable_section_t
{
public:
    typedef vector <tag_base_t*> tag_vector_t;
public:
//   info_section_t (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t *ptr_high);
    info_section_t (const char *name, BYTE id, ADDRESS32 *ptr);
    virtual void handle_ini_section (const ini_section_t &ini_section,
                                     const string &name) = 0;
    virtual void init (flash_base *flash) = 0;
    int write_to_buffer(BYTE *buffer ) ;
    ~info_section_t ();
    void disp() const;

public:
    tag_vector_t m_tags;
    u_int16_t *m_length;
};

template <class PRODUCT>
class image_info_section_t : public info_section_t<PRODUCT>
{
public:
    //image_info_section_t (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t *ptr_high);
    image_info_section_t (const char *name, BYTE id, ADDRESS32 *ptr);
    void handle_ini_section (const ini_section_t &ini_section,
                             const string &name);
    void init (flash_base *fl);
    ~image_info_section_t ();
};

template <class PRODUCT>
class usb_info_section_t : public info_section_t<PRODUCT>
{
public:
//   usb_info_section_t (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t *ptr_high);
    usb_info_section_t (const char *name, BYTE id, ADDRESS32 *ptr);
    void handle_ini_section (const ini_section_t &ini_section,
                             const string &name);
    void init (flash_base *fl);
    ~usb_info_section_t ();
};

template <class PRODUCT>
class system_config_section_t : public flash_section
{
public:
    system_config_section_t (const char *name, ADDRESS32 *ptr);
    void set_offset (int offset);
    int write_to_buffer(BYTE *buffer ) ;

public:
    ADDRESS32 *m_ptr;
};


#pragma pack(push, 1)
struct ids_section
{
    BYTE reserved1;
    BYTE version;
    BYTE mac_address [6];
    BYTE ssid [32];
    u_int16_t local;
    u_int16_t reserved2;
    u_int16_t ppm;
    u_int16_t reserved3;
    u_int32_t board_type;
    u_int16_t lo_power_xif_gc;
    u_int16_t lo_power_stg2_bias;
    u_int16_t vga_bias;
    u_int16_t vga_stg1_fine_bias;
    u_int16_t ats_ver;
    u_int16_t mlt_ver;
    u_int16_t bl_ver;
    u_int16_t lo_power_gc_ctrl;
    u_int16_t production1;
    u_int16_t production2;
    u_int16_t production3;
    u_int16_t production4;
    u_int16_t production5;
    u_int16_t production6;
    u_int16_t production7;
    u_int16_t production8;
    u_int16_t production9;
    u_int16_t production10;
    u_int16_t production11;
    u_int16_t production12;
    u_int16_t production13;
    u_int16_t production14;
    u_int16_t production15;
    u_int16_t production16;
};
#pragma pack(pop)

template <typename T, class PRODUCT>
    struct const_section_t: public id_section
{
public:
//   const_section_t (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t* ptr_high);
    const_section_t (const char *name, BYTE id, ADDRESS32 *ptr);
    virtual void handle_ini_section (const ini_section_t &ini_section) = 0;
    int write_to_buffer(BYTE *buffer ) ;
    virtual void init (flash_base *fl);
    virtual void initReduced (flash_base *fl, int reductionSize);
public:
    T m_section;
};


template <class PRODUCT>
struct ids_section_t: public const_section_t <ids_section, PRODUCT>
{
public:
//   ids_section_t (const char *name, BYTE id, u_int16_t *ptr_low, u_int16_t* ptr_high);
    ids_section_t (const char *name, BYTE id, ADDRESS32 *ptr);
    void handle_ini_section (const ini_section_t &ini_section);
    void disp() const;
    void disp_to_file(const char* ids_ini_file) const;
    bool set_value(const char* id_name, const char* id_value);
};
#endif //#ifndef _FLASH_SECTIONS_H_
