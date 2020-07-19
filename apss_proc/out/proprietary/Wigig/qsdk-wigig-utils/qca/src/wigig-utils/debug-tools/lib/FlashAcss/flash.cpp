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

#include "flash.h"
#include "wlct_os.h"

bool g_debug = false;

void flash_base::set_exit_flag(bool bExit)
{
    m_bExit = bExit;
}

flash::flash(DType device_type)
    :flash_base(device_type)
{
    m_pDeviceAccss = NULL;
    m_pFlashAccss = NULL;
    if (device_type == MST_SPARROW)
        m_size = FLASH_SIZE_SPARROW;
    if (device_type == MST_MARLON)
        m_size = FLASH_SIZE_MARLON;
    if (device_type == MST_TALYN)
        m_size = FLASH_SIZE_TALYN;
    m_hw_read_chunk_size = 256;
    m_hw_write_chunk_size = 256;
    m_page_erased = new bool [m_size / FLASH_PAGE_SIZE];

    if (!m_page_erased)
    {
        ERR("Cannot allocate page_erased buffer of size %d\n", m_size / FLASH_PAGE_SIZE);
        exit(1);
}

    memset(m_page_erased, 0, m_size / FLASH_PAGE_SIZE);
    m_buffer = NULL;
    update_erased_flash(false);

    flag_reading_not_done = false;
    m_address = 0;
    m_length = 0;
}

flash::~flash(void)
{
    if (m_pFlashAccss != NULL)
    {
        wfa_release_lock(m_pFlashAccss);
        close ();
    }

    free_buffer();

    delete m_page_erased;
}

void flash::free_buffer ()
{
    if (m_buffer != NULL)
    {
        delete [] m_buffer;
    }
}

#ifdef _WINDOWS
void readThread(void* pFlashDevice)
#else
    void *readThread(void* pFlashDevice)
#endif
{
    flash *pFlash = (flash*)pFlashDevice;
    pFlash->set_flag_reading_not_done(true);

    u_int32_t address = pFlash->get_address();
    u_int32_t length = pFlash->get_length();
    BYTE* buffer = (BYTE*)pFlash->get_buffer_ptr();

    pFlash->read( address , length , buffer);

    pFlash->set_flag_reading_not_done(false);
#ifndef _WINDOWS
    return NULL;
#endif
}

int flash::read_async( const u_int32_t address, const u_int32_t length )
{
    if (flag_reading_not_done)
    {
        return BUFFER_NOT_READY;
    }

    m_address = address;
    m_length = length;

    free_buffer(); //free memory if already allocated before
    m_buffer = new BYTE[length];

#ifdef _WINDOWS
    m_thread = (HANDLE)_beginthread(readThread, 0 , this);
#else
    if (0 != pthread_create(&m_thread, NULL, readThread, this))
    {
        ERR("Failed to create Read thread\n");
        free_buffer();
        return BUFFER_NOT_READY;
    }
#endif

    return BUFFER_OK;
}

int flash::open (const char *device_name, DType dtype, bool ignore_lock)
{
    int res = 0;
    res =  CreateDeviceAccessHandler( device_name, dtype, &m_pDeviceAccss);
    if (res != 0)
        return res;
    deviceType = dtype;
    res =  wfa_create_flash_access_handler(dtype, m_pDeviceAccss, &m_pFlashAccss);
    if (res != 0)
        return res;

    res = wfa_boot_done(m_pFlashAccss);
    if (res != 0)
        return res;

    if (ignore_lock) {
        res = wfa_force_lock(m_pFlashAccss);
    } else {
        if (wfa_get_lock(m_pFlashAccss) == false) {
            ERR("Failed getting Flash lock."
                "Use -ignore_lock option to ignore locking mechanism\n");
            return (-1);
        }
    }

    res = retrive_info();
    return res;
}

int flash::open (void *m_pDeviceAccss, DType dtype, bool ignore_lock)
{
    int res = 0;
    if (m_pDeviceAccss == NULL)
        return -1;
    deviceType = dtype;
    res =  wfa_create_flash_access_handler(dtype, m_pDeviceAccss, &m_pFlashAccss);
    if (res != 0)
        return res;

    res = wfa_boot_done(m_pFlashAccss);
    if (res != 0)
        return res;

    if (ignore_lock) {
        res = wfa_force_lock(m_pFlashAccss);
    } else {
        if (wfa_get_lock(m_pFlashAccss) == false) {
            ERR("Failed getting Flash lock."
                "Use -ignore_lock option to ignore locking mechanism\n");
            return (-1);
        }
    }

    res = retrive_info();
    return res;
}

void flash::close ()
{
    if (m_pFlashAccss)
    {
        wfa_close_flash_access_handler(m_pFlashAccss);
        m_pFlashAccss = NULL;
    }
    int res = CloseDeviceAccessHandler(m_pDeviceAccss);

    WLCT_UNREFERENCED_PARAM(res);
    if (g_debug ) DBG("Closed device\n");
}

int flash::retrive_info()
{
    int res = 0;
    res = wfa_run_command(m_pFlashAccss, FC_RDID, INSTRUCTION_DATA_PHASE, false, 0, 20);
    if (res != 0)
    {
#ifdef _WINDOWS /* TODO: Handle error in linux */
        // 1 - OK
        // 2 = cancel
        //int mb = MessageBox(NULL, L"Failed access to flash.\nPress OK to continue or\nCancel to abort.", L"Error", MB_OKCANCEL );
        //if (mb == 2)
        {
            return res;
        }
        //res = 0;
#endif
    }

    memset((void*)&m_info, 0, sizeof(m_info));
    res = wfa_get_data(m_pFlashAccss,(BYTE*)&m_info, sizeof(m_info) - 4);
    if (res != 0)
        return 0;

    INFO ("Flash Manufactured by ");
    switch (m_info.manufacturer_id)
    {
    case 0x20:
    {
        INFO("Numonyx ");
        switch (m_info.device_id1)
        {
        case 0x80 :
        {
            INFO("M25PE ");
            break;
        }
        case 0x20 :
        {
            INFO("M25P ");
            break;
        }
        default : INFO("Unknown device ID ");
        }

        switch (m_info.device_id2)
        {
        case 0x11 :
        {
            INFO("Capacity of 1Mbits ");
            break;
        }
        case 0x12 :
        {
            INFO("Capacity of 2Mbits ");
            break;
        }
        case 0x14 :
        {
            INFO("Capacity of 8Mbits ");
            break;
        }
        default : INFO("Unknown capacity ");
        }

        break;
    }

    case 0xC2:
    {
        INFO("Macronix ");
        switch (m_info.device_id1)
        {
        case 0x20 :
        {
            INFO("MX25L8006E ");
            break;
        }
        default : INFO("Unknown device ID ");
        }

        switch (m_info.device_id2)
        {
        case 0x11 :
        {
            INFO("Capacity of 1Mbits ");
            break;
        }
        case 0x12 :
        {
            INFO("Capacity of 2Mbits ");
            break;
        }
        case 0x13 :
        {
            INFO("Capacity of 4Mbits ");
            break;
        }
        case 0x14 :
        {
            INFO("Capacity of 8Mbits ");
            break;
        }
        default : INFO("Unknown capacity ");
        }

        break;
    }

    case 0x1F:
    {
        INFO("Atmel ");
        switch (m_info.device_id1)
        {
        case 0x43 :
        {
            INFO("AT25DF021 ");
            break;
        }
        default : INFO("Unknown device ID ");
        }
        break;
    }
    default:
    {
        INFO("Unknown manufacturer");
    }
    }

    //if (m_info.extended_string_length != 0) {
    //INFO("Extended INFO %s ", m_info.extended_string);
    //}
    INFO("\n --------------------------- \n");
    return res;
}


int flash::read( const u_int32_t address, const u_int32_t length, BYTE *buffer ) const
{
    int res = 0;
    if (address & 0x1) {
        ERR("address 0x%04x is not word aligned\n",address);
        return -1;
    }

    if (length & 0x1) {
        ERR("length 0x%04x is not word aligned\n",length);
        return -1;
    }
    if (address + length > this->m_size) {
        ERR("address 0x%04x + length 0x%04x exceeds flash size of 0x%04x\n",address,length,this->m_size);
        //return -1;
    }

    u_int32_t offset = 0;
    while (!m_bExit && offset < length )
    {
        u_int16_t current_size = FLASH_PAGE_SIZE - ((address + offset) & 0xff); // Part of a page
        if (current_size > length ) {
            current_size = (u_int16_t)length;
        }

        if (offset + current_size > length ) {
            current_size = (u_int16_t)(length - offset);
        }
        //while ( offset < length ) {
        //   if( m_bExit ) {
        //      reruen(-1);
        //   }

        //   u_int32_t curr_length = min(m_hw_read_chunk_size, length - offset);
        u_int32_t curr_address = address + offset;
        if (g_debug) DBG("Reading 0x%04x bytes from address 0x%04x\n", current_size, curr_address);

        res = wfa_run_command(m_pFlashAccss, FC_READ, INSTRUCTION_ADDRESS_DATA_PHASE, false, curr_address, current_size);
        if (res != 0)
            break;
        res = wfa_get_data(m_pFlashAccss,buffer + offset, current_size);
        if (res != 0)
            break;
        offset += current_size;
    }
    return res;
}

int flash::write( const u_int32_t address, const u_int32_t length, const BYTE *buffer, bool verify )
{

    //unused param
    (void)verify;
    int res = -1;

    res = write_wen();
    if (res != 0)
        return res;

    u_int32_t offset = 0;
    while (!m_bExit && offset < length )
    {
        u_int16_t current_size = FLASH_PAGE_SIZE - ((address + offset) & 0xff); // Part of a page
        if (current_size > length ) {
            current_size = (u_int16_t)length;
        }

        if (offset + current_size > length ) {
            current_size = (u_int16_t)(length - offset);
        }

        //   u_int32_t curr_length = min(m_hw_read_chunk_size, length - offset);
        u_int32_t curr_address = address + offset;
        if (g_debug) DBG("Reading 0x%04x bytes from address 0x%04x\n", current_size, curr_address);

        res = wfa_set_data(m_pFlashAccss,buffer + offset, current_size);
        if (res != 0)
            break;
        res = wfa_run_command(m_pFlashAccss, FC_PW, INSTRUCTION_ADDRESS_DATA_PHASE, true, curr_address, current_size);
        if (res != 0)
            break;
        offset += current_size;
    }
    return res;
}

int flash::program( const u_int32_t address, const u_int32_t length, const BYTE *buffer, bool verify )
{
    int res = -1;

    //    u_int8_t type = m_erased ? FC_PP : FC_PW;
    //    u_int8_t type = FC_PW;
    u_int8_t type = FC_PP;

    if (address & 0x1) {
        ERR("address 0x%04x is not word aligned\n",address);
        return -1;
    }

    if (length & 0x1) {
        ERR("length 0x%04x is not word aligned\n",length);
        return -1;
    }
    if (address + length > this->m_size) {
        ERR("address 0x%04x + length 0x%04x exceeds flash size of 0x%04x\n",address,length,this->m_size);
        return -1;
    }

    u_int32_t offset = 0;
    BYTE *verify_buffer = new BYTE[FLASH_PAGE_SIZE];
    if (!verify_buffer) {
        ERR("Cannot allocate verify buffer\n");
        return -1;
    }

    u_int32_t loop = 0;
    bool dbg_verify = false;
    while (!m_bExit && offset < length )
    {
        loop++;
        if (loop%50 == 0 || loop < 10)
            dbg_verify = true;
        else
            dbg_verify = false;

        u_int16_t current_size = FLASH_PAGE_SIZE - ((address + offset) & 0xff); // Part of a page
        if (current_size > length ) {
            current_size = (u_int16_t)length;
        }

        if (offset + current_size > length ) {
            current_size = (u_int16_t)(length - offset);
        }

        if (type == FC_PP) {
            bool all_ff = true;
            for (u_int32_t i = 0; i < current_size ; i+=2) {
                if ( (*(u_int16_t*)(buffer + offset + i)) != (u_int16_t)(-1) ) {
                    all_ff = false;
                    break;
                }
            }

            if (all_ff) {
                if (g_debug) {
                    DBG(" - skipped empty page 0x%04x - ", address+offset);
                } else {
                    INFO("\rSkip   - ");
                }
                offset += current_size;
                INFO("%03.2f%%", (float)offset / (float)length * 100);
                if (g_debug) DBG("\n");
                continue;
            }
        }

        INFO("\rWrite  - ");
        res = page_write_program(address+offset, current_size, &buffer[offset], type);
        if (res != 0)
            break;

        if (verify || dbg_verify) {
            INFO("\rRead   - ");
            res = read(address+offset, current_size, verify_buffer);
            if (res != 0)
                break;
            INFO("\rVerify - ");
            if( memcmp(&buffer[offset], verify_buffer, current_size) != 0 ) {
                ERR("Data verification failed on address 0x%04x length 0x%03x\n", address+offset, current_size);
                res = -1;
                break;
            }
        }

        offset += current_size;
        INFO("%03.2f%%", (float)offset / (float)length * 100);
        if (g_debug) DBG("\n");
    }
    delete[] verify_buffer;
    printf("\n");
    return res;
}

int flash::page_write_program( const u_int32_t address, const u_int16_t length, const BYTE *buffer, u_int8_t type )
{
    int res = 0;
    if( m_bExit ) {
        return (-1);
    }

    if (type == FC_PP) {
        if ( !is_erased(address) ) {
            res = sub_sector_erase( address );
//            sector_erase( address );
        }
        if (g_debug) DBG("Programing page address 0x%04x length 0x%03x\n", address, length);
    }
    else {
        if (g_debug) DBG("Writing page address 0x%04x length 0x%03x\n", address, length);
    }

    res = write_wen();
    if (res != 0)
        return res;
    res = wfa_set_data(m_pFlashAccss, buffer, length);
    if (res != 0)
        return res;
    res = wfa_run_command(m_pFlashAccss, type, INSTRUCTION_ADDRESS_DATA_PHASE, true, address, length);
    if (res != 0)
        return res;

    res = wait_wip();
    return res;
}

int flash::write_wen (void) const
{
    int res = 0;
//    execute_cmd(0x0106);
    res = wfa_run_command(m_pFlashAccss, FC_WREN, INSTRUCTION_PHASE, true, 0, 0);
    return res;
}

int flash::erase (void)
{
    int res = 0;
    res = wfa_boot_done(m_pFlashAccss);
    if (res != 0)
        return res;
    res = write_wen();
    if (res != 0)
        return res;

    //erase
    res = wfa_run_command(m_pFlashAccss, FC_BE, INSTRUCTION_PHASE, true, 0, 0);
    if (res != 0)
        return res;

    res = wait_wip();
    if (res != 0)
        return res;
    update_erased_flash( true );
    return res;
}

int flash::page_erase(u_int32_t address)
{
    int res = 0;
    write_wen();
    if (res != 0)
        return res;

    res = wfa_run_command(m_pFlashAccss, FC_PE, INSTRUCTION_ADDRESS_PHASE, true, address, 0);
    if (res != 0)
        return res;
    res = wait_wip();
    if (res != 0)
        return res;
    if (g_debug) DBG("Cmd is 0x%04x, Status is 0x%04x\n",FC_PE, get_status() );
    if (g_debug) DBG("Erased page address 0x%04x\n", address);
    update_erased_page(address, true);
    return res;
}

int flash::sector_erase(u_int32_t address)
{
    int res = 0;
    res = write_wen();
    if (res != 0)
        return res;

    res = wfa_run_command(m_pFlashAccss, FC_SE, INSTRUCTION_ADDRESS_PHASE, true, address, 0);
    if (res != 0)
        return res;
    res = wait_wip();
    if (res != 0)
        return res;
    if (g_debug) DBG("Cmd is 0x%04x, Status is 0x%04x\n",FC_SE, get_status() );
    if (g_debug) DBG("Erased sector address 0x%04x\n", address);
    update_erased_sector(address, true);
    return res;
}

int flash::sub_sector_erase(u_int32_t address)
{
    int res = 0;
    printf("Erasing sub_sector %d\n", address / SUB_SECTOR_SIZE);
    res = write_wen();
    if (res != 0)
        return res;

    res = wfa_run_command(m_pFlashAccss, FC_SSE, INSTRUCTION_ADDRESS_PHASE, true, address, 0);
    if (res != 0)
        return res;
    res = wait_wip();
    if (res != 0)
        return res;
    if (g_debug) DBG("Cmd is 0x%04x, Status is 0x%04x\n",FC_SSE, get_status() );
    if (g_debug) {
        DBG(" - Erased sub_sector address 0x%04x - ", address);
    } else {
        INFO("\rErase  - ");
    }
    update_erased_sub_sector(address, true);
    return res;
}

u_int8_t flash::get_status() const
{
    int res = 0;
    res = wfa_run_command(m_pFlashAccss, FC_RDSR, INSTRUCTION_DATA_PHASE, false, 0, 1);

    u_int32_t value;
    unsigned long size = sizeof(u_int32_t);
    res = wfa_get_data(m_pFlashAccss, (BYTE*)&value, size);

    WLCT_UNREFERENCED_PARAM(res);
    return (u_int8_t)value;
}

int flash::wait_wip (void) const
{
    int res = 0;
    u_int16_t value;
    u_int32_t timeout = 0;
    WLCT_UNREFERENCED_PARAM(res);

    do {
        value = get_status();
        if (EXTRACT(value, 0, 1) == 0) {
            if (EXTRACT(value, 1, 1) == 1 ) {
                ERR("WIP bit is 0 but WEL bit is 1. Status = 0x%04x\n", value);
                return (-1);
            }
            return 0;
        }
        timeout++;
    } while (timeout <= 1000000 );

    ERR("wait_wip:: timeout reached\n");
    return -1;
}

void flash::update_erased_page (u_int32_t offset, bool erased )
{
    m_page_erased[offset/FLASH_PAGE_SIZE] = erased;
}

void flash::update_erased_sub_sector (u_int32_t offset, bool erased )
{
    u_int32_t page_offset = offset / SUB_SECTOR_SIZE * SUB_SECTOR_SIZE;
    for (u_int32_t i =0; i < PAGES_PER_SUB_SECTOR; i++) {
        update_erased_page( page_offset + i * FLASH_PAGE_SIZE, erased );
    }
}

void flash::update_erased_sector (u_int32_t offset, bool erased )
{
    u_int32_t page_offset = offset / SECTOR_SIZE * SECTOR_SIZE;
    for (u_int32_t i =0; i < SUB_SECTORS_PER_SECTOR; i++) {
        update_erased_sub_sector( page_offset + i * SUB_SECTOR_SIZE, erased );
    }
}

void flash::update_erased_flash (bool erased )
{
    for (u_int32_t i =0; i < SECTORS_PER_FLASH; i++) {
        update_erased_sector( i * SECTOR_SIZE, erased );
    }
}

bool flash::is_erased( u_int32_t page_offset ) const{
    u_int32_t page_index = page_offset/FLASH_PAGE_SIZE;
    bool erased = m_page_erased[page_index];
    return erased;
}

void flash::clear_erased( u_int32_t page_offset ) {
    u_int32_t page_index = page_offset/FLASH_PAGE_SIZE;
    m_page_erased[page_index] = false;
}

int flash_file::open (const char *device_name, DType dtype, bool ignore_lock = true)
{
    //unused param
    (void)ignore_lock;

//    m_device_name = device_name;
    m_handler = fopen( device_name, "rb+");
    m_buffer = new char [m_size];
    if (!m_buffer) {
        ERR("Cannot allocate a buffer of size %u\n", m_size);
        return (-1);
    }

    if (0 == m_handler) { // File doesn't exist
        if (g_debug) DBG("file %s type %d does not exist. Creating...\n", device_name, dtype);
        m_handler = fopen( device_name, "wb");
        if (0 == m_handler) {
            ERR("Failed creating file %s type %d\n", device_name, dtype);
            return (-1);
        }
        memset(m_buffer, 0, m_size);
    } else {
        int res = fread(m_buffer, 1, m_size, (FILE*)m_handler);
        if( (u_int32_t)res != m_size ) {
            ERR("Expected %d bytes and read %d bytes\n", m_size, res);
            return (-1);
        }
        rewind((FILE*)m_handler);
        //res = fclose((FILE*)m_handler);
        //if (0 != res) {
        //   ERR("Failed closing file\n");
        //   return (-1);
        //}
        if (g_debug) DBG("Opened file %s type %d\n", device_name, dtype);
    }
    return 0;
}

void flash_file::close ()
{
    //m_handler = fopen( this->m_device_name.c_str(), "w");
    //if (0 == m_handler) {
    //   ERR("Failed opening file for writing\n");
    //   return (-1);
    //}


    int res = fwrite(m_buffer, 1, m_size, (FILE*)m_handler);
    if( (u_int32_t)res != m_size ) {
        ERR("Expected %d bytes and wrote %d bytes\n", m_size, res);
        return;
    }

    res = fclose((FILE*)m_handler);

    if (0 != res) {
        ERR("Failed closing device\n");
        return;
    }

    if (g_debug ) DBG("Closed device\n");
}

int flash_file::erase (void)
{
    memset(m_buffer, 0xff, m_size);
    this->update_erased_flash( true );
    return 0;
}

flash_file::flash_file(DType device_type)
    :flash(device_type)
{
    m_buffer = 0;
}

flash_file::~flash_file(void)
{
    if (0 != m_handler) {
        close ();
        m_handler = 0;
    }
    if (0 != m_buffer) {
        delete m_buffer;
        m_buffer = 0;
    }
}

int flash_file::page_write_program( const u_int32_t address, const u_int16_t length, const BYTE *buffer, u_int8_t type )
{
    if( m_bExit ) {
        return(-1);
    }

    if (type == FC_PP) {
        if ( !this->is_erased(address) ) {
            sub_sector_erase( address );
            //         sector_erase( address );
        }
        if (g_debug) DBG("Programing page address 0x%04x length 0x%03x\n", address, length);
    }
    else {
        if (g_debug) DBG("Writing page address 0x%04x length 0x%03x\n", address, length);
    }

    memcpy(m_buffer+address, buffer, length);
    return 0;
}

int flash_file::sub_sector_erase(u_int32_t address)
{
    printf("Erasing sub_sector %d\n", address / SUB_SECTOR_SIZE);

    memset(m_buffer+address, 0xff, SUB_SECTOR_SIZE);
    if (g_debug) {
        DBG(" - Erased sub_sector address 0x%04x - ", address);
    } else {
        INFO("\rErase  - ");
    }
    this->update_erased_sub_sector(address, true);
    return 0;
}

int flash_file::read( const u_int32_t address, const u_int32_t length, BYTE *buffer ) const
{
    if (address & 0x1) {
        ERR("address 0x%04x is not word aligned\n",address);
        return(-1);
    }

    if (length & 0x1) {
        ERR("length 0x%04x is not word aligned\n",length);
        return(-1);
    }

    if (address + length > this->m_size) {
        ERR("address 0x%04x + length 0x%04x exceeds flash size of 0x%04x\n",address,length,this->m_size);
        return(-1);
    }

    memcpy(buffer, m_buffer+address, length);
    return 0;
}
