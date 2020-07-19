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

#include <string>
#include "WlctPciAcss.h"
#include "flashacss.h"

#ifdef _WINDOWS
#ifdef FLSHACSS_EXPORTS
#define FLSHACSS_CPP_API __declspec(dllexport)
#else
#define FLSHACSS_CPP_API __declspec(dllimport)
#endif
#else //#ifdef _WINDOWS
#define FLSHACSS_CPP_API
#endif //#ifdef _WINDOWS

#define BUFFER_NOT_READY -1
#define BUFFER_OK 0

extern "C"
{
class FLSHACSS_CPP_API flash_base
{
public:
    enum {
        //FLASH_SIZE_SWIFT = 256*1024,
        FLASH_SIZE_SPARROW = 1024*512,
        //FLASH_SIZE_MARLON = 1024*1024,
        FLASH_SIZE_MARLON = 1024*512,
        FLASH_SIZE_TALYN = 1024*512*4
    };
    flash_base(DType device_type)
        : m_bExit(false)
        , deviceType(device_type)
        , m_size(0)
    {
        if (device_type == MST_SPARROW)
            m_size = FLASH_SIZE_SPARROW;
        if (device_type == MST_MARLON)
            m_size = FLASH_SIZE_MARLON;
        if (device_type == MST_TALYN)
            m_size = FLASH_SIZE_TALYN;
        else
            m_size = FLASH_SIZE_TALYN;
    };
    virtual ~flash_base(void) {};
    virtual int open (const char *device_name, DType dtype, bool ignore_lock) = 0;
    virtual int open (void *m_pDeviceAccss, DType dtype, bool ignore_lock) = 0;
    virtual void close () = 0;
    virtual int read( const u_int32_t address, const u_int32_t length, BYTE *buffer ) const = 0;
    virtual int write( const u_int32_t address, const u_int32_t length, const BYTE *buffer, bool verify ) = 0;
    virtual int program( const u_int32_t address, const u_int32_t length, const BYTE *buffer, bool verify ) = 0;
    virtual int erase (void) = 0;
    virtual void clear_erased( u_int32_t page_offset ) = 0;
    virtual int sub_sector_erase(u_int32_t address) = 0;

        void set_exit_flag(bool bExit);
        DType GetDeviceType()
        {
            return deviceType;
        }

    protected:
        bool m_bExit;
        DType deviceType;
        u_int32_t m_size;
    };

    class FLSHACSS_CPP_API flash: public flash_base
    {
    public:
        flash(DType device_type);
        virtual ~flash(void);
        virtual int open (const char *device_name, DType dtype, bool ignore_lock);
        virtual int open (void *m_pDeviceAccss, DType dtype, bool ignore_lock);
        virtual void close ();
        virtual int read( const u_int32_t address, const u_int32_t length, BYTE *buffer ) const;
        virtual int read_async( const u_int32_t address, const u_int32_t length);
        int write( const u_int32_t address, const u_int32_t length, const BYTE *buffer, bool verify );
        int program( const u_int32_t address, const u_int32_t length, const BYTE *buffer, bool verify );
        virtual int erase (void);
        bool get_flag_reading_not_done () {return flag_reading_not_done; };
        void set_flag_reading_not_done (bool new_val) {flag_reading_not_done = new_val;};
        void* get_buffer_ptr () {return m_buffer; };
        void free_buffer ();

        virtual int sub_sector_erase(u_int32_t address);

        u_int32_t get_address () {return m_address; };
        u_int32_t get_length () {return m_length; };


    public:
        enum {
            FLASH_PAGE_SIZE = 256,
            PAGES_PER_SUB_SECTOR = 16,
            SUB_SECTOR_SIZE = PAGES_PER_SUB_SECTOR * FLASH_PAGE_SIZE,
            SUB_SECTORS_PER_SECTOR = 16,
            SECTOR_SIZE = SUB_SECTORS_PER_SECTOR * SUB_SECTOR_SIZE,
#ifdef FLASH_256KB
            SECTORS_PER_FLASH = 4, // support 1MB flash
#else
            //SECTORS_PER_FLASH = 16, // support 1MB flash
            SECTORS_PER_FLASH = 8, // support 0.5MB flash
#endif
        };

    public:
        struct {
            BYTE manufacturer_id;
            BYTE device_id1;
            BYTE device_id2;
            BYTE extended_string_length;
            char extended_string [256]; // maximal size
        } m_info; // according to JEDEC standard

    protected:
        int page_erase(u_int32_t offset);
        int sector_erase(u_int32_t address);
        virtual int page_write_program( const u_int32_t address, const u_int16_t length, const BYTE *buffer, u_int8_t type );
        int wait_wip (void) const;
        int write_wen (void) const;
        u_int8_t get_status(void) const;
        void update_erased_page (u_int32_t offset, bool erased );
        void update_erased_sub_sector (u_int32_t offset, bool erased );
        void update_erased_sector (u_int32_t offset, bool erased );
        void update_erased_flash ( bool erased );
        bool is_erased( u_int32_t page_offset ) const;
        void clear_erased( u_int32_t page_offset );
        int retrive_info();

    protected:
        bool *m_page_erased;
        u_int32_t m_hw_read_chunk_size;
        u_int32_t m_hw_write_chunk_size;

#if _WINDOWS
        HANDLE m_thread;
#else
        pthread_t m_thread;
#endif

    protected:
//    string m_device_name;

    protected:
        enum FlashCommand {
            FC_WREN         = 0x06,
            FC_WRDI         = 0x04,
            FC_RDID         = 0x9F,
            FC_RDSR         = 0x05,
            FC_WRLR         = 0xE5,
            FC_WRSR         = 0x01,
            FC_RDLR         = 0xE8,
            FC_READ         = 0x03,
            FC_FAST_READ    = 0x0B,
            FC_PW           = 0x0A,
            FC_PP           = 0x02,
            FC_PE           = 0xDB,
            FC_SSE          = 0x20,
            FC_SE           = 0xD8,
            FC_BE           = 0xC7,
            FC_DP           = 0xB9,
            FC_RDP          = 0xAB
        };
    private:
//    flash_gateway* m_flash_gateway;
        void* m_pDeviceAccss;
        void* m_pFlashAccss;

        bool flag_reading_not_done;
        u_int32_t m_address;
        u_int32_t m_length;
        BYTE* m_buffer;
    };


    class FLSHACSS_CPP_API flash_file: public flash
    {
    public:
        flash_file(DType device_type);
        ~flash_file(void);
        virtual int read( const u_int32_t address, const u_int32_t length, BYTE *buffer ) const;
        virtual int erase ();
        virtual int open (const char *device_name, DType dtype, bool ignore_lock);
        virtual int open (void *m_pDeviceAccss, DType dtype, bool ignore_lock)
        {
            //do something with params
            (void)m_pDeviceAccss;
            (void)dtype;
            (void)ignore_lock;
            return -1;
        }
        virtual void close ();
        virtual int sub_sector_erase(u_int32_t address);

    private:
        virtual int page_write_program( const u_int32_t address, const u_int16_t length, const BYTE *buffer, u_int8_t type );
    private:
        mutable char *m_buffer;
        void *m_handler;
    };
};
