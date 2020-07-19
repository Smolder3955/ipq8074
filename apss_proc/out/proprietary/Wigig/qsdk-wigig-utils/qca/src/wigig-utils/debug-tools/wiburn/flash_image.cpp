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

#include "flash_image.h"
#include <memory.h>
#include <iostream>


//extern ini_parser g_parser;

#define NEXT_PTR(ptr) (((ptr) & SUB_SECTOR_MASK) + SUB_SECTOR )

u_int16_t crc_16 (const u_int32_t *buffer, int size)
{
    return 0;
    u_int16_t      crc;
    u_int32_t element;

    crc = 0xffff;

    for (int j = 0; j < size; j++) {
        element = buffer[j];
        for (int i=0; i<32; i++) {
            if (crc & 0x8000) {
                crc = (u_int16_t) ((((crc<<1) | (element>>31)) ^  0x100b) & 0xffff);
            }
            else {
                crc= (u_int16_t) (((crc<<1) | (element>>31)) & 0xffff);
            }
            element = (element<<1) & 0xffffffff;
        }
    }

    for (int k=0; k<16; k++) {
        if (crc & 0x8000)
            crc=((crc<<1)  ^  0x100b) & 0xffff;
        else
            crc=(crc<<1) & 0xffff;
    }

    // Revert 16 low bits
    crc = crc ^ 0xffff;
    return crc;
}


template <class PRODUCT>
flash_image<PRODUCT>::flash_image () :
    pointer_section ("pointer_section"),
    hw_conf_section ("hw_conf_section", hw_conf_section_id, &pointer_section.m_pointers.hw_pointer),

    fw1_code_section ("fw1_code_section", fw1_code_section_id, &pointer_section.m_pointers.fw1_pointer, &pointer_section.m_pointers.fw1_length),
    fw1_data_section ("fw1_data_section",fw1_data_section_id, &pointer_section.m_pointers.fw1_data_pointer, &pointer_section.m_pointers.fw1_data_length),
    fw1_static_conf_section ("fw1_static_conf_section",fw1_static_conf_section_id, &pointer_section.m_pointers.fw1_static_conf_pointer),

    fw2_code_section ("fw2_code_section",fw2_code_section_id, &pointer_section.m_pointers.fw2_pointer, &pointer_section.m_pointers.fw2_length),
    fw2_data_section ("fw2_data_section",fw2_data_section_id, &pointer_section.m_pointers.fw2_data_pointer, &pointer_section.m_pointers.fw2_data_length),
    fw2_static_conf_section ( "fw2_static_conf_section",fw2_static_conf_section_id, &pointer_section.m_pointers.fw2_static_conf_pointer),

    system_config_section ( "config_section", &pointer_section.m_pointers.config_section_pointer),

    production_section ("production_section",production_section_id, &pointer_section.m_pointers.production_pointer),
    user_conf_section ("user_conf_section",user_conf_section_id, &pointer_section.m_pointers.user_pointer),

    image_info_section ("image_info_section",image_info_section_id, &pointer_section.m_pointers.image_info_pointer),
    ids_section("ids_section", ids_section_id, &pointer_section.m_pointers.ids_pointer),

    usb_section("usb_section", &pointer_section.m_pointers.usb_pointer),
    usb_info_section("usb_info_section", usb_info_section_id, &pointer_section.m_pointers.usb_info_pointer),

    radio_tx_conf_section ("radio_tx_conf_section", radio_tx_conf_section_id, &pointer_section.m_pointers.radio_tx_cnf_pointer),
    radio_rx_conf_section ("radio_rx_conf_section", radio_rx_conf_section_id, &pointer_section.m_pointers.radio_rx_cnf_pointer),
    radio_tx_conf2_section ("radio_tx_conf2_section", radio_tx_conf2_section_id, &pointer_section.m_pointers.radio_tx_cnf2_pointer),
    radio_rx_conf2_section ("radio_rx_conf2_section", radio_rx_conf2_section_id, &pointer_section.m_pointers.radio_rx_cnf2_pointer),

    // The following is unused!
    raw_data_section ("raw_data_section", raw_data_section_id, &pointer_section.m_pointers.raw_data_pointer, &pointer_section.m_pointers.raw_data_length),
    proxy_product(NULL)
{
#ifdef FLASH_256KB
    m_imageSize = (1 << 21) / 8; //2Mbits - 256KB
#else
    m_imageSize = 1024 * 512;

    if (PRODUCT::id == talyn::id)
    {
        m_imageSize = 1024 * 4 * 512;
    }
    else if ((PRODUCT::id == sparrow::id))
    {
        m_imageSize = 1024 * 512;
    }

    m_image = new BYTE[m_imageSize];
    if (NULL == m_image)
    {
        return;
    }

    //    proxy_product = new PRODUCT;
    memset(m_image, -1, m_imageSize);
#endif
}

template <class PRODUCT>
flash_image<PRODUCT>::~flash_image ()
{
    delete[] m_image;
}

template <class PRODUCT>
const BYTE* flash_image<PRODUCT>::get_image (void) const
{
    return m_image;
}

template <class PRODUCT>
u_int32_t flash_image<PRODUCT>::get_image_size (void) const
{
    return m_imageSize;
}

template <class PRODUCT>
void flash_image<PRODUCT>::init( const char *filename)
{
    FILE *stream;
    stream = fopen(filename, "r");
    if( NULL == stream  )
    {
        ERR("Failed opening file %s for reading\n", filename);
        EXIT (-1);
    }

    // Changed to support direct burn of bin file without generating it from ini
    // m_imageSize is set onece and exactly matches the size of the buffer m_image
    u_int32_t cnt = fread(m_image, size_t(m_imageSize), 1, stream);
    if (cnt < sizeof m_image) {
        INFO("Initialized from partial image file of size 0x%04x\n", cnt);
    }
    fclose(stream);
}

template <class PRODUCT>
void flash_image<PRODUCT>::init_pointer_section (flash_base *fl)
{
    pointer_section.init(fl);

    // In order to prevent reading from the flash over the signature
    // when the flash is un-initiated, we reset the signature
    // after reading from flash.
    pointer_section.m_pointers.signature = 0x40;
}

template <class PRODUCT>
void flash_image<PRODUCT>::init_hw_conf_section (flash_base *fl)
{
    hw_conf_section.init(fl);
}

// template <class PRODUCT>
// void flash_image<PRODUCT>::init_radio_tx_conf_section (flash_base *fl)
// {
//     radio_tx_conf_section.init(fl);
// }
//
// template <class PRODUCT>
// void flash_image<PRODUCT>::init_radio_rx_conf_section (flash_base *fl)
// {
//     radio_rx_conf_section.init(fl);
// }
//
// template <class PRODUCT>
// void flash_image<PRODUCT>::init_radio_tx_conf2_section (flash_base *fl)
// {
//     radio_tx_conf2_section.init(fl);
// }
//
// template <class PRODUCT>
// void flash_image<PRODUCT>::init_radio_rx_conf2_section (flash_base *fl)
// {
//     radio_rx_conf2_section.init(fl);
// }

template <class PRODUCT>
void flash_image<PRODUCT>::init_image_info_section (flash_base *fl)
{
    image_info_section.init(fl);
}

template <class PRODUCT>
void flash_image<PRODUCT>::init_ids_section (flash_base *fl, bool isReduced)
{
    if (isReduced)
    {
        *ids_section.m_ptr = 4 * 1024 + sizeof(section_header_t);
    }
    else
    {
        *ids_section.m_ptr = 44 * 1024 + sizeof(section_header_t);
    }

    ids_section.init(fl);
}

template <class PRODUCT>
void flash_image<PRODUCT>::init_usb_info_section (flash_base *fl)
{
    usb_info_section.init(fl);
}

template <class PRODUCT>
const pointer_section_t<PRODUCT>& flash_image<PRODUCT>::get_pointer_section(void) const
{
    return pointer_section;
}

template <class PRODUCT>
const hw_section_t<PRODUCT>& flash_image<PRODUCT>::get_hw_conf_section(void) const
{
    return hw_conf_section;
}

// template <class PRODUCT>
// const hw_section_t<marlon>& flash_image<PRODUCT>::get_radio_tx_conf_section(void) const
// {
//     return radio_tx_conf_section;
// }
//
// template <class PRODUCT>
// const hw_section_t<marlon>& flash_image<PRODUCT>::get_radio_rx_conf_section(void) const
// {
//     return radio_rx_conf_section;
// }
//
// template <class PRODUCT>
// const hw_section_t<marlon>& flash_image<PRODUCT>::get_radio_tx_conf2_section(void) const
// {
//     return radio_tx_conf2_section;
// }
//
// template <class PRODUCT>
// const hw_section_t<marlon>& flash_image<PRODUCT>::get_radio_rx_conf2_section(void) const
// {
//     return radio_rx_conf2_section;
// }

template <class PRODUCT>
const image_info_section_t<PRODUCT>& flash_image<PRODUCT>::get_image_info_section(void) const
{
    return image_info_section;
}

template <class PRODUCT>
const usb_info_section_t<PRODUCT>& flash_image<PRODUCT>::get_usb_info_section(void) const
{
    return usb_info_section;
}

template <class PRODUCT>
const ids_section_t<PRODUCT>& flash_image<PRODUCT>::get_ids_section(void) const
{
    return ids_section;
}


// template <class PRODUCT>
// const image_section_t<PRODUCT>& flash_image<PRODUCT>::get_fw1_code_section () const
// {
//     return fw1_code_section;
// }
//
// template <class PRODUCT>
// const image_section_t<PRODUCT>& flash_image<PRODUCT>::get_fw2_code_section () const
// {
//     return fw2_code_section;
// }

template <class PRODUCT>
void flash_image<PRODUCT>::save( const char *filename)
{
    FILE *stream;
    stream = fopen(filename, "wb");
    if( NULL == stream  )
    {
        ERR("Failed opening file %s for writing\n", filename);
        EXIT (-1);
    }

    fwrite(m_image, sizeof m_image, 1, stream);
    fclose(stream);
}

/*
        MARLON:                                                            SPARROW:                                               SPARROW REDUCED:
        +-------------------------------------------+   0k                +-------------------------------------------+   0k       +-------------------------------------------+  0k
        |    POINTER                                |                     |    POINTER                                |            |    POINTER                                |
        +-------------------------------------------+   4k                +-------------------------------------------+   4k       +-------------------------------------------+  4k
        |    production         (40K)               |                     |    production        (40K)                |            |    IDS                (4K)                |
        +-------------------------------------------+  48k                +-------------------------------------------+  48k       +-------------------------------------------+
        |    IDS                (4K)                |                     |    IDS                (4K)                |            +-------------------------------------------+  8k
        +-------------------------------------------+                     +-------------------------------------------+            |    HW                (4k)                 |
        +-------------------------------------------+  52k                +-------------------------------------------+  52k       +-------------------------------------------+  12k
        |    HW                 (4k)                |                     |    HW                (4k)                 |            |    FW CODE            (44K)               |
        +-------------------------------------------+  56k                +-------------------------------------------+  56k       +-------------------------------------------+  56k
        |    FW CODE            (256K)              |                     |    FW CODE            (256K)              |            |    FW DATA            (8K)                |
        +-------------------------------------------+  312k               +-------------------------------------------+  312k      +-------------------------------------------+  64k
        |    FW DATA            (32K)               |                     |    FW DATA            (32K)               |
        +-------------------------------------------+  344k               +-------------------------------------------+  344k
        |    uCode CODE         (64K)               |                     |    uCode CODE        (128K)               |
        +-------------------------------------------+  408k               +-------------------------------------------+  472k
        |    uCode DATA         (8K)                |                     |    uCode DATA         (8K)                |
        +-------------------------------------------+  416k               +-------------------------------------------+  480k
        |    FW STATICS         (4K)                |                     |    FW STATICS        (4K)                 |
        +-------------------------------------------+  420k               +-------------------------------------------+  484k
        |    uCode STATICS      (4K)                |                     |    uCode STATICS    (4K)                  |
        +-------------------------------------------+                     +-------------------------------------------+
        +-------------------------------------------+  424k               +-------------------------------------------+  488k
        |    System Config Selector    (4K)         |                     |    System Config Selector    (4K)         |
        +-------------------------------------------+  428k               +-------------------------------------------+  492k
        |    System Config Factory    (4K)          |                     |    System Config Factory    (4K)          |
        +-------------------------------------------+  432k               +-------------------------------------------+  496k
        |    System Config User1        (4K)        |                     |    System Config User1        (4K)        |
        +-------------------------------------------+  436k               +-------------------------------------------+  500k
        |    System Config User2        (4K)        |                     |    System Config User2        (4K)        |
        +-------------------------------------------+                     +-------------------------------------------+
        +-------------------------------------------+  440k               +-------------------------------------------+  504k
        |    IMAGE INFO        (4K)                 |                     |    IMAGE INFO        (4K)                 |
        |            image format version           |                     |            image format version           |
        |            FW VERSION                     |                     |            FW VERSION                     |
        |            fw_timestamp                   |                     |            fw_timestamp                   |
        |            ucode_version                  |                     |            ucode_version                  |
        |            ucode_timestamp                |                     |            ucode_timestamp                |
        |            configuration_id               |                     |            configuration_id               |
        |            device_id                      |                     |            device_id                      |
        |            hw_id                          |                     |            hw_id                          |
        +-------------------------------------------+  444k               +-------------------------------------------+  508k
        |    radio_tx_conf    (8K)                  |
        +-------------------------------------------+  452k
        |    radio_rx_conf    (8K)                  |
        +-------------------------------------------+  460k
        |    radio_tx_conf2    (8K)                 |
        +-------------------------------------------+  468k
        |    radio_rx_conf2    (8K)                 |
        +-------------------------------------------+  476k
        |    RAW DATA        (4K)                   |
        +-------------------------------------------+  480k
*/
template <class PRODUCT>
void flash_image<PRODUCT>::init(ini_parser_base *parser, bool full_image, bool isReducedSip)
{
    ini_file_t::iterator iter1;
    ini_file_t::const_iterator iter2;
    int ptr;
    ptr = NEXT_PTR( 0 );
    string section_name;
    bool reducedSip = isReducedSip && ((PRODUCT::id == sparrow::id) || (PRODUCT::id == talyn::id));

    if (!reducedSip)
    {
        section_name = "production";
        if (parser->get_section(section_name, 0, &iter1, false, false) )
        {
            // This set_max_size was removed due to the reason that the ptr is increased by the size read from
            // the ini file. There is no more check for the max size, it is not the responsibility of
            // wiburn anymore. Consider removing this check for all the sections (the best would be
            // removing it only for Talyn, because in Talyn there is no flash)
            //production_section.set_max_size( flash::SUB_SECTOR_SIZE * 10 );
            production_section.handle_ini_section(0, *(iter1->second));
            production_section.set_offset( ptr );
            ptr = production_section.write_to_buffer(m_image);
        }
        else if (PRODUCT::id == sparrow::id)
        {
            // This update to the ptr was removed due to the reason that the ptr is increased by the size read from
            // the ini file. Consider doing this increasing for all the sections (the best would be
            // doing it only for Talyn, because in Talyn there is no flash)
            // Update: The update has been returned to support burning boot loader on sparrow. Thus it happens only on sparrow
            // devices, when production section is not needed (production section is needed for creating ".brd" file.
            ////Force the section size to be 40K
            ptr += NEXT_PTR( (1<<15) -1 );        // 32K
            ptr += NEXT_PTR( (1<<13) -1 );        //  8K
        }
    }

    section_name = "ids";

    // Always set ids section. The existing values remain if input file is not provided
    ids_section.set_max_size( flash::SUB_SECTOR_SIZE );
    if (parser->get_section(section_name, 0, &iter1, true, false))
    {
        ids_section.handle_ini_section( *(iter1->second));
    }

    ids_section.set_offset( ptr );
    ids_section.write_to_buffer(m_image);

    ptr += NEXT_PTR( 0 );

    section_name = "hw_config";
    if (parser->get_section(section_name, TRANSLATION_MAP_REGTREE, &iter1, false, full_image))
    {
        hw_conf_section.set_max_size( flash::SUB_SECTOR_SIZE );
        hw_conf_section.handle_ini_section(TRANSLATION_MAP_REGTREE, *(iter1->second));
        hw_conf_section.set_offset( ptr );
        hw_conf_section.write_to_buffer(m_image);
    }
    ptr += NEXT_PTR( 0 );

    section_name = "fw_code";
    if (parser->get_section(section_name, 0, &iter1, false, full_image))
    {
        if (reducedSip)
        {
            fw1_code_section.set_max_size( flash::SUB_SECTOR_SIZE * 11);
        }
        else
        {
            // FW code limit to 256K
#ifdef FLASH_256KB
            fw1_code_section.set_max_size( flash::SUB_SECTOR_SIZE * 16);
#else
            if (PRODUCT::id == sparrow::id)
            {
                // FW code limit to 256K - Sparrow
                fw1_code_section.set_max_size( flash::SUB_SECTOR_SIZE * 64);
            }
            else if (PRODUCT::id == talyn::id)
            {
                // FW code limit to 1M - Talyn
                fw1_code_section.set_max_size( flash::SUB_SECTOR_SIZE * 256);
            }
            else
            {
                // FW code limit to 256K - Marlon, Default
                fw1_code_section.set_max_size( flash::SUB_SECTOR_SIZE * 64);
            }
#endif
        }

        fw1_code_section.handle_ini_section( *(iter1->second));
        fw1_code_section.set_offset( ptr );
        fw1_code_section.write_to_buffer(m_image);
    }

    if (reducedSip)
    {
        ptr += NEXT_PTR( (0xB000) -1 ); //Force the Code size to be 44K (including header+CRC)
    }
    else
    {
#ifdef FLASH_256KB
        ptr += NEXT_PTR( (1<<16) -1 ); //Force the Code size to be 64K (including header+CRC)
#else
        if (PRODUCT::id == sparrow::id)
        {
            ptr += NEXT_PTR( (1<<18) -1 ); //Force the Code size to be 256K (including header+CRC)
        }
        else if (PRODUCT::id == talyn::id)
        {
            ptr += NEXT_PTR( (1<<20) -1 ); //Force the Code size to be 1M (including header+CRC)
        }
        else
        {
            ptr += NEXT_PTR( (1<<18) -1 ); //Force the Code size to be 256K (including header+CRC)
        }
#endif
    }

    section_name = "fw_data";
    if (parser->get_section(section_name, 0, &iter1, false, full_image))
    {
        if (reducedSip)
        {
            // FW data limit to 8K
            fw1_data_section.set_max_size( flash::SUB_SECTOR_SIZE * 2 );
        }
        else
        {
            if (PRODUCT::id == sparrow::id)
            {
                // FW data limit to 32K
                fw1_data_section.set_max_size( flash::SUB_SECTOR_SIZE * 8 );
            }
            else if (PRODUCT::id == talyn::id)
            {
                // FW data limit to 128K
                fw1_data_section.set_max_size( flash::SUB_SECTOR_SIZE * 32 );
            }
            else
            {
                // FW data limit to 8K
                fw1_data_section.set_max_size( flash::SUB_SECTOR_SIZE * 2 );
            }
        }
        fw1_data_section.handle_ini_section( *(iter1->second));
        fw1_data_section.set_offset( ptr );
        fw1_data_section.write_to_buffer(m_image);
    }
    if (reducedSip)
    {
        ptr += NEXT_PTR( (1<<13) -1 ); //Force the Data size to be 8K (including header+CRC)
    }
    else
    {
        if (PRODUCT::id == sparrow::id)
        {
            ptr += NEXT_PTR((1 << 15) - 1); //Force the Data size to be 32K - Sparrow (including header+CRC)
        }
        else if (PRODUCT::id == talyn::id)
        {
            ptr += NEXT_PTR((1 << 17) - 1); //Force the Data size to be 128K - Talyn (including header+CRC)
        }
        else
        {
            ptr += NEXT_PTR((1 << 13) - 1); //Force the Data size to be 8K - Marlon, default (including header+CRC)
        }
    }

    if (!reducedSip)
    {
        section_name = "ucode_code";
        if (parser->get_section(section_name, 0, &iter1, false, full_image))
        {
            if (PRODUCT::id == sparrow::id)
            {
                // ucode code limit to 128K - Sparrow
                fw2_code_section.set_max_size( flash::SUB_SECTOR_SIZE * 32 );
            }
            else if (PRODUCT::id == talyn::id)
            {
                // ucode code limit to 256K - Talyn
                fw2_code_section.set_max_size( flash::SUB_SECTOR_SIZE * 64 );
            }
            else
            {
                // ucode code limit to 64K - Marlon, default
                fw2_code_section.set_max_size( flash::SUB_SECTOR_SIZE * 16 );
            }

            fw2_code_section.handle_ini_section( *(iter1->second));
            fw2_code_section.set_offset( ptr );
            fw2_code_section.write_to_buffer(m_image);
        }

        if (PRODUCT::id == sparrow::id)
        {
            ptr += NEXT_PTR( (1<<17) -1 ); //Force the Code size to be 128K - Sparrow (including header+CRC)
        }
        else if (PRODUCT::id == talyn::id)
        {
            ptr += NEXT_PTR( (1<<18) -1 ); //Force the Code size to be 256K - Talyn (including header+CRC)
        }
        else
        {
            ptr += NEXT_PTR( (1<<16) -1 ); //Force the Code size to be 64K - Marlon, default (including header+CRC)
        }

        section_name = "ucode_data";
        if (parser->get_section(section_name, 0, &iter1, false, full_image))
        {
            if (PRODUCT::id == sparrow::id)
            {
                // ucode data limit to 16K
                fw2_data_section.set_max_size( flash::SUB_SECTOR_SIZE * 4);
            }
            else if (PRODUCT::id == talyn::id)
            {
                // ucode data limit to 32K
                fw2_data_section.set_max_size( flash::SUB_SECTOR_SIZE * 8);
            }
            else
            {
                // ucode data limit to 8K
                fw2_data_section.set_max_size( flash::SUB_SECTOR_SIZE * 2);
            }

            fw2_data_section.handle_ini_section( *(iter1->second));
            fw2_data_section.set_offset( ptr );
            fw2_data_section.write_to_buffer(m_image);
        }

        if (PRODUCT::id == sparrow::id)
        {
            ptr += NEXT_PTR((1 << 14) - 1); //Force the Data size to be 16K - Sparrow (including header+CRC)
        }
        else if (PRODUCT::id == talyn::id)
        {
            ptr += NEXT_PTR((1 << 15) - 1); //Force the Data size to be 32K - Talyn (including header+CRC)
        }
        else
        {
            ptr += NEXT_PTR((1 << 13) - 1); //Force the Data size to be 8K - Marlon, default (including header+CRC)
        }

        section_name = "fw_static";
        if (full_image) {
            if (parser->get_section(section_name, TRANSLATION_MAP_REGTREE | TRANSLATION_MAP_FW, &iter1, false, full_image))
            {
                fw1_static_conf_section.set_max_size( flash::SUB_SECTOR_SIZE );
                fw1_static_conf_section.handle_ini_section(TRANSLATION_MAP_REGTREE | TRANSLATION_MAP_FW, *(iter1->second));
                fw1_static_conf_section.set_offset( ptr );
                fw1_static_conf_section.write_to_buffer(m_image);
            }
            else
            {
                //ERR("Could not find mandatory section %s\n", section_name.c_str() );
                //EXIT (-1);
            }
        }
        ptr += NEXT_PTR( 0 );

        if (full_image) {
            section_name = "ucode_static";
            if (parser->get_section(section_name, TRANSLATION_MAP_REGTREE | TRANSLATION_MAP_UCODE, &iter1, false, full_image))
            {
                fw2_static_conf_section.set_max_size( flash::SUB_SECTOR_SIZE );
                fw2_static_conf_section.handle_ini_section(TRANSLATION_MAP_REGTREE | TRANSLATION_MAP_UCODE, *(iter1->second));
                fw2_static_conf_section.set_offset( ptr );
                fw2_static_conf_section.write_to_buffer(m_image);
            }else{
                //ERR("Could not find mandatory section %s\n", section_name.c_str() );
                //EXIT (-1);
            }
        }
        ptr += NEXT_PTR( 0 );

        section_name = "system_config_section";
        system_config_section.set_max_size( flash::SUB_SECTOR_SIZE * 4 );
        if (full_image) {
            system_config_section.set_offset( ptr );
            system_config_section.write_to_buffer(m_image);
        }

        ptr += NEXT_PTR( (1<<14) -1 );

        /*section_name = "user";
          if (parser->get_section(section_name, TRANSLATION_MAP_REGTREE | TRANSLATION_MAP_FW, &iter1, false, full_image))
          {
          user_conf_section.set_max_size( flash::SUB_SECTOR_SIZE );
          user_conf_section.handle_ini_section(TRANSLATION_MAP_REGTREE | TRANSLATION_MAP_FW, *(iter1->second));
          user_conf_section.set_offset( ptr );
          user_conf_section.write_to_buffer(m_image);
          }
          ptr += NEXT_PTR( 0 ); */

        bool image_info_section_exist = false;
        section_name = "image_format_version";
        if (parser->get_section(section_name, 0, &iter1, true, full_image))
        {
            image_info_section.set_max_size( flash::SUB_SECTOR_SIZE );
            image_info_section.handle_ini_section( *(iter1->second), section_name);
            image_info_section_exist = true;
        }

        section_name = "fw_version";
        if (parser->get_section(section_name, 0, &iter1, true, full_image))
        {
            image_info_section.handle_ini_section( *(iter1->second), section_name);
            image_info_section_exist = true;
        }

        section_name = "fw_timestamp";
        if (parser->get_section(section_name, 0, &iter1, true, full_image))
        {
            image_info_section.handle_ini_section( *(iter1->second), section_name);
            image_info_section_exist = true;
        }

        section_name = "ucode_version";
        if (parser->get_section(section_name, 0, &iter1, true, full_image))
        {
            image_info_section.handle_ini_section( *(iter1->second), section_name);
            image_info_section_exist = true;
        }

        section_name = "ucode_timestamp";
        if (parser->get_section(section_name, 0, &iter1, true, full_image))
        {
            image_info_section.handle_ini_section( *(iter1->second), section_name);
            image_info_section_exist = true;
        }

        section_name = "configuration_id";
        if (parser->get_section(section_name, 0, &iter1, true, full_image))
        {
            image_info_section.handle_ini_section( *(iter1->second), section_name);
            image_info_section_exist = true;
        }

        section_name = "device_id";
        if (parser->get_section(section_name, 0, &iter1, true, full_image))
        {
            image_info_section.handle_ini_section( *(iter1->second), section_name);
            image_info_section_exist = true;
        }

        section_name = "hw_id";
        if (parser->get_section(section_name, 0, &iter1, true, full_image))
        {
            image_info_section.handle_ini_section( *(iter1->second), section_name);
            image_info_section_exist = true;
        }

        if ( image_info_section_exist ) {
            image_info_section.set_offset( ptr );
            image_info_section.write_to_buffer(m_image);
        }
        ptr += NEXT_PTR( 0 );

        if (PRODUCT::id == marlon::id) {

            section_name = "radio_tx_conf";
            if (parser->get_section(section_name, 0, &iter1, false, false) )
            {
                //        parser->update_section(iter1->second, *(iter2->second), false);
                radio_tx_conf_section.set_max_size( flash::SUB_SECTOR_SIZE * 2);
                radio_tx_conf_section.handle_ini_section(0, *(iter1->second));
                radio_tx_conf_section.set_offset( ptr );
                radio_tx_conf_section.write_to_buffer(m_image);
            }

            ptr += NEXT_PTR( (1<<13) -1 ); //Force the section size to be 8K
            section_name = "radio_rx_conf";
            if (parser->get_section(section_name, 0, &iter1, false, false) )
            {
                //        parser->update_section(iter1->second, *(iter2->second), false);
                radio_rx_conf_section.set_max_size( flash::SUB_SECTOR_SIZE * 2);
                radio_rx_conf_section.handle_ini_section(0, *(iter1->second));
                radio_rx_conf_section.set_offset( ptr );
                radio_rx_conf_section.write_to_buffer(m_image);
            }

            ptr += NEXT_PTR( (1<<13) -1 ); //Force the section size to be 8K
            section_name = "radio_tx_conf2";
            if (parser->get_section(section_name, 0, &iter1, false, false) )
            {
                //        parser->update_section(iter1->second, *(iter2->second), false);
                radio_tx_conf2_section.set_max_size( flash::SUB_SECTOR_SIZE * 2);
                radio_tx_conf2_section.handle_ini_section(0, *(iter1->second));
                radio_tx_conf2_section.set_offset( ptr );
                radio_tx_conf2_section.write_to_buffer(m_image);
            }

            ptr += NEXT_PTR( (1<<13) -1 ); //Force the section size to be 8K
            section_name = "radio_rx_conf2";
            if (parser->get_section(section_name, 0, &iter1, false, false) )
            {
                //        parser->update_section(iter1->second, *(iter2->second), false);
                radio_rx_conf2_section.set_max_size( flash::SUB_SECTOR_SIZE * 2);
                radio_rx_conf2_section.handle_ini_section(0, *(iter1->second));
                radio_rx_conf2_section.set_offset( ptr );
                radio_rx_conf2_section.write_to_buffer(m_image);
            }

            ptr += NEXT_PTR( (1<<13) -1 ); //Force the section size to be 8K
        }
        section_name = "raw_data";
        if (parser->get_section(section_name, 0, &iter1, false, false) )
        {
            // data_raw limit to 4K
            raw_data_section.set_max_size( flash::SUB_SECTOR_SIZE);
            raw_data_section.handle_ini_section( *(iter1->second));
            raw_data_section.set_offset( ptr );
            raw_data_section.write_to_buffer(m_image);

        }

    }

    // At the end write the pointer section
    pointer_section.set_offset( 0 );

    if (full_image)
        pointer_section.write_to_buffer(m_image);

}

