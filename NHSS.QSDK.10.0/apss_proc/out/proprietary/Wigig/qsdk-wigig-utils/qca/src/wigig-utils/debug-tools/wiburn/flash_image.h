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

#ifndef _FLASH_IMAGE_H_
#define _FLASH_IMAGE_H_

#include "wiburn.h"
#include "flash_sections.h"
#include "ini_parser.h"
#include "product.h"

u_int16_t crc_16 (const u_int32_t *buffer, int size);

template <class PRODUCT>
class flash_image
{
public:
    flash_image ();
    ~flash_image ();
    void init (ini_parser_base *parser, bool full_image, bool isReducedSip); // init from ini file
    void init (const char *filename); // init from image file
    void init_pointer_section (flash_base *fl); // init from flash
    void init_hw_conf_section (flash_base *fl); // init from flash
    //    void init_radio_tx_conf_section (flash_base *fl); // init from flash
    //    void init_radio_rx_conf_section (flash_base *fl); // init from flash
    //    void init_radio_tx_conf2_section (flash_base *fl); // init from flash
    //    void init_radio_rx_conf2_section (flash_base *fl); // init from flash
    void init_image_info_section (flash_base *fl); // init from flash
    void init_ids_section (flash_base *fl, bool isReduced); // init from flash
    void init_usb_info_section (flash_base *fl); // init from flash
    void save (const char *filename);
    const BYTE* get_image (void) const;
    u_int32_t get_image_size (void) const;
    const pointer_section_t <PRODUCT> & get_pointer_section(void) const;
    const hw_section_t <PRODUCT> & get_hw_conf_section(void) const;
    //    const hw_section_t <marlon> & get_radio_tx_conf_section(void) const;
    //    const hw_section_t <marlon> & get_radio_rx_conf_section(void) const;
    //    const hw_section_t <marlon> & get_radio_tx_conf2_section(void) const;
    //    const hw_section_t <marlon> & get_radio_rx_conf2_section(void) const;
    const image_info_section_t<PRODUCT>& get_image_info_section(void) const;
    const usb_info_section_t<PRODUCT>& get_usb_info_section(void) const;
    const ids_section_t<PRODUCT>& get_ids_section(void) const;
    //    const image_section_t<PRODUCT>& get_fw1_code_section () const;
    //    const image_section_t<PRODUCT>& get_fw2_code_section () const;
private:
    pointer_section_t <PRODUCT>         pointer_section;
    hw_section_t <PRODUCT>              hw_conf_section;

    image_section_t<PRODUCT>            fw1_code_section;
    image_section_t<PRODUCT>            fw1_data_section;
    fw_section_t <PRODUCT>              fw1_static_conf_section;

    image_section_t<PRODUCT>            fw2_code_section;
    image_section_t<PRODUCT>            fw2_data_section;
    fw_section_t <PRODUCT>              fw2_static_conf_section;

    system_config_section_t <PRODUCT>   system_config_section;

    hw_section_t <PRODUCT>              production_section;
    fw_section_t <PRODUCT>              user_conf_section;

    image_info_section_t<PRODUCT>       image_info_section;
    ids_section_t<PRODUCT>              ids_section;

    usb_section_t                       usb_section;
    usb_info_section_t<PRODUCT>         usb_info_section;

    hw_section_t <marlon> radio_tx_conf_section;
    hw_section_t <marlon> radio_rx_conf_section;
    hw_section_t <marlon> radio_tx_conf2_section;
    hw_section_t <marlon> radio_rx_conf2_section;

    image_section_t<PRODUCT> raw_data_section;

    IProduct* proxy_product;

private:
#ifdef FLASH_256KB
    BYTE m_image [(1<<21)/8]; //2Mbits - 256KB
#else
    BYTE* m_image;
    int m_imageSize;
#endif
};

#endif //#ifndef _FLASH_IMAGE_H_
