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

#include "flash.h"
#include "translation_map.h"
#include "flash_image.h"
#include <signal.h>
#include "ParameterParser.h"
#include "product.h"
#include <iostream>

#ifdef _WINDOWS
bool g_debug = false;
#else
extern bool g_debug;
#endif
translation_map g_reg_tree_t_map;
translation_map g_ucode_t_map;
translation_map g_fw_t_map;

extern u_int64_t current_fw_build;

ini_parser_base *g_parser = 0;
flash_base *g_flash = 0;

volatile bool g_exit = false;

int g_signals_for_termination[] = {
    SIGINT,
    SIGTERM
};

bool IsReduced();

void EXIT (int val) {
    delete g_flash;
    exit (val);
}

bool get_interface (string *name) {
    INFO("Acquiring available interfaces. Please wait...\n");
    INTERFACE_LIST interfaces;
    int num_items;
    int res;
    res = GetInterfaces(&interfaces, &num_items);
    INFO("\n");

    if (res != 0 || num_items == 0) {
        return false;
    }

    if (num_items == 1 ) {
        *name = interfaces.list[0].ifName;
        INFO("Using the single available interface %s\n", interfaces.list[0].ifName);
        return true;
    }

    printf("Please choose interface index:\n");
    for (int i = 0; i < num_items; i++) {
        std::cout << "[" << i << "] " << interfaces.list[i].ifName << std::endl;
    }

    u_int32_t index;
    std::cin >> index;

    if (!std::cin.good() || index >= (u_int32_t)num_items ) {
        ERR("Invalid interface chosen\n");
        EXIT (-1);
    }

    *name = interfaces.list[index].ifName;
    return true;
}

void print_buffer(const void *buffer, int length)
{
    for(int i=0; i< length; i++) {
        putchar(*((char*)(buffer) + i));
    }
}

void set_exit_flag(bool bExit)
{
    g_exit = bExit;
    if (g_flash)
    {
        g_flash->set_exit_flag(bExit);
    }
}

void TerminationHandler (int signum)
{
    static int termination_cnt = 0;
    if (signum == 0) {
        INFO ("\nTrying to exit...\n");
        set_exit_flag(true);
        return;
    }

    termination_cnt++;

    if (termination_cnt == 2) {
        INFO ("\nTrying to exit...\n");
        set_exit_flag(true);
    }
    else {
        INFO ("\nWarning: Please wait for program normal termination or press ^C again to exit.\n");
        signal(signum, TerminationHandler);
        return;
    }
}

void compatibility_check() {
    /* const u_int64_t minimal_ver_supported = 2026;
       if (current_fw_build < minimal_ver_supported) {
       ERR("Current INI file is older than %lu and is not supported by this wiburn version !\n", (long unsigned int)minimal_ver_supported);
       //EXIT (-1);
       }*/
}

void param_parsing (ParameterParser *opt)
{
    opt->addFlag(  "-debug" ); /* a flag (takes no argument), supporting long and short form */
    opt->addFlag(  "-verify" ); /* a flag (takes no argument), supporting long and short form */
    opt->addFlag(  "-help" ); /* a flag (takes no argument), supporting long and short form */
    opt->addFlag(  "-save"); /* a flag (takes no argument), supporting long and short form */
    opt->addFlag(  "-burn" ); /* a flag (takes no argument), supporting long and short form */
    opt->addFlag(  "-read" ); /* a flag (takes no argument), supporting long and short form */
    opt->addFlag(  "-read_formatted" ); /* a flag (takes no argument), supporting long and short form */
    opt->addFlag(  "-query" ); /* a flag (takes no argument), supporting long and short form */
    opt->addFlag(  "-erase"); /* a flag (takes no argument), supporting long and short form */
    opt->addFlag(  "-force"); /* a flag (takes no argument), supporting long and short form */
    opt->addFlag(  "-ignore_lock"); /* a flag (takes no argument), supporting long and short form */
    opt->addFlag(  "-read_ids_to_file"); /* a flag (takes no argument), supporting long and short form */
    opt->addFlag(  "-burn_ids_section");
    opt->addFlag(  "-burn_id");

    opt->addParam( "-bin" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-fw" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-board" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-board2" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-board3" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-setup_ini" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-ids" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-production" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-usb" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-interface" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-device" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-offset" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-length" );/* an option (takes an argument), supporting long and short form */
    opt->addParam( "-id_name");
    opt->addParam( "-id_value");
}

template <typename PRODUCT>
void sub_main(bool burn,
              bool erase,
              bool burn_full_image,
              bool query,
              bool verify,
              bool read,
              bool read_formatted,
              bool read_ids_to_file,
              bool force,
              const char* offset,
              const char* length,
              const char* bin_file,
              const char* fw_ini_file,
              const char* ids_ini_file,
              const char* production_ini_file,
              const char* board_ini_file,
              const char* board_ini_file2,
              const char* board_ini_file3,
              const char* setup_ini_file,
              const char* usb_ini_file,
              bool burn_ids_section, // burn IDs section only
              bool burn_id, // burn specific ID
              const char* id_name, // used with burn_id
              const char* id_value // used with burn_id
    )
{
    flash_image<PRODUCT>* new_flash_image = 0;
    flash_image<PRODUCT>* old_flash_image = 0;

    bool reducedSip = IsReduced();

    if (reducedSip)
    {
        INFO("SIP type: REDUCED");
    }
    else
    {
        INFO("SIP type: FULL");
    }

    old_flash_image = new flash_image<PRODUCT>;
    old_flash_image->init_pointer_section(g_flash);
    old_flash_image->init_ids_section(g_flash, reducedSip);
    const ids_section_t<PRODUCT> &ids = old_flash_image->get_ids_section();
    const pointer_section_t<PRODUCT> &pointers = old_flash_image->get_pointer_section();

    // If requested to replace the entirety of the IDS or just a single one
    if (burn_ids_section || burn_id)
    {
        // Initialize IDS section from existing flash
        ids_section_t<PRODUCT> &new_ids = const_cast<ids_section_t<PRODUCT>&>(ids);

        // Limit IDS section to 4K
        new_ids.set_max_size(flash::SUB_SECTOR_SIZE);

        if (burn_id)
        {
            // Set ID value
            if (!new_ids.set_value(id_name, id_value))
            {
                ERR("\nFailed to set ID - check ID name and value\n\n");
                EXIT(-1);
            }
        }
        else
        {
            ////////// IDs only burn /////////////

            // Parse ids.ini
            g_parser->init(ids_ini_file);
            ini_file_t::iterator iter;
            g_parser->get_section("ids", 0, &iter, true, false);

            // Retrieve ids content from ini parser
            new_ids.handle_ini_section(*(iter->second));

            ////////////IDs only burn [END]///////
        }

        BYTE buffer[flash::SUB_SECTOR_SIZE];
        new_ids.write_to_buffer(buffer);

        // Erase ids sector
        g_flash->sub_sector_erase(pointers.m_pointers.ids_pointer & 0xFFFFF000);

        // Write new ids content to ids sector
        g_flash->program(pointers.m_pointers.ids_pointer & 0xFFFFF000 /* only pointer section is before ids section*/, flash::SUB_SECTOR_SIZE /* ids section size (in bytes) is a single sector*/, buffer, true);

        return;
    }

    if( read_ids_to_file ) {
        INFO("\nReading current IDS section into file: %s\n", ids_ini_file);
        ids.disp();
        ids.disp_to_file( ids_ini_file );
    }

    // parse INI file if needed
    if( burn ) {
        new_flash_image = new flash_image<PRODUCT>;
        if( bin_file ) {
            INFO("\n######### Initializing from BIN file %s ###########\n", bin_file);
            new_flash_image->init(bin_file);
        } else {
            if( fw_ini_file ) {
                INFO("\n######### Initializing from FW INI file %s ###########\n", fw_ini_file);
                g_parser->init(fw_ini_file);
            }
            if( ids_ini_file ) {
                INFO("\n######### Initializing from IDs INI file %s ###########\n", ids_ini_file);
                g_parser->init(ids_ini_file);
            }
            else
            {
                INFO("\n######### Taking IDs section from flash ###########\n");
                new_flash_image->init_ids_section( g_flash, reducedSip );
            }

            if( production_ini_file ) {
                INFO("\n######### Initializing from Production INI file %s ###########\n", production_ini_file);
                g_parser->init(production_ini_file);
            }
            if( board_ini_file ) {
                INFO("\n######### Initializing from Board INI file %s ###########\n", board_ini_file);
                g_parser->init(board_ini_file);
            }
            if( board_ini_file2 ) {
                INFO("\n######### Initializing from Board2 INI file %s ###########\n", board_ini_file2);
                g_parser->init(board_ini_file2, 1);
            }
            if( board_ini_file3 ) {
                INFO("\n######### Initializing from Board3 INI file %s ###########\n", board_ini_file3);
                g_parser->init(board_ini_file3, 1);
            }
            if( setup_ini_file ) {
                INFO("\n######### Initializing from setup_ini INI file %s ###########\n", setup_ini_file);
                g_parser->init(setup_ini_file);
            }
            if( usb_ini_file ) {
                INFO("\n######### Initializing from USB INI file %s ###########\n", usb_ini_file);
                g_parser->init(usb_ini_file);
            }
            INFO("\n#########    Building flash sections    ###########\n");

            if (!erase) {
                new_flash_image->init_pointer_section(g_flash);
            }

            new_flash_image->init(g_parser, burn_full_image, reducedSip);
            INFO("\n#########    Flash sections are ready   ###########\n");
        }
    }

    if( query ) {
        old_flash_image = new flash_image<PRODUCT>;
        old_flash_image->init_pointer_section( g_flash );
        old_flash_image->init_image_info_section( g_flash );
        old_flash_image->init_ids_section( g_flash, reducedSip );
//        old_flash_image->init_usb_info_section( g_flash );
        const image_info_section_t<PRODUCT> &info = old_flash_image->get_image_info_section();
        info.disp();
        const ids_section_t<PRODUCT> &ids = old_flash_image->get_ids_section();
        ids.disp();
        const usb_info_section_t<PRODUCT> &usb_info = old_flash_image->get_usb_info_section();
        usb_info.disp();
    }

    if( erase ) {
        compatibility_check();
        INFO("Erasing flash ...");
        g_flash->erase();
        INFO("done\n");
    }

    if( burn ) {
        compatibility_check();
        if (erase) {// Full image burn
            INFO("Burning image...\n");
            int res = g_flash->program(4, new_flash_image->get_image_size() - 4, new_flash_image->get_image() + 4, verify);

            if (res != 0)  {
                ERR("\nBurning failed !\n\n");
                EXIT (-1);
            }

            INFO("Burning signature...\n");
            g_flash->program(0, 4, new_flash_image->get_image(), verify);
            INFO("Burning done\n");

        } else {
            flash_image<PRODUCT> *old_flash_image = new flash_image<PRODUCT>;
            old_flash_image->init_pointer_section( g_flash );
            const pointer_section_t<PRODUCT> &new_pointer_section = new_flash_image->get_pointer_section();
            if( !(old_flash_image->get_pointer_section() == new_pointer_section )) {
                ERR("Current INI file will cause changes in the pointer section."
                    " Changes in pointer section are not supported\n");
                //EXIT (-1);
            }

            ini_file_t::iterator dummy1;
            ini_file_t::const_iterator dummy2;

            const hw_section_t<PRODUCT> &new_hw_conf_section = new_flash_image->get_hw_conf_section();
            const hw_section_t<PRODUCT> &old_hw_conf_section = old_flash_image->get_hw_conf_section();
            if (!force ) {
                string section_name = "hw_config";
                if (g_parser->get_section(section_name, TRANSLATION_MAP_REGTREE, &dummy1, false, false)) {
                    //if (g_parser->get_section(section_name, &dummy1, &dummy2, false)) {
                    old_flash_image->init_hw_conf_section( g_flash );
                    if( !(old_hw_conf_section == new_hw_conf_section ) ) {
                        ERR("Current INI file will cause changes in the hw_configuration "
                            "section. These changes may leave the device in an unstable state."
                            " Use -force option to force the operation.\n");
                        EXIT(-1);
                    }
                }
            }

            INFO("Burning image...\n");
            // write all sections following the pointer_section.
            // Only modified sections will be written.
            // Unmodified sections will be skipped
            u_int32_t start_offset = new_pointer_section.size() + sizeof(u_int32_t); //For CRC
            g_flash->program(start_offset,
                             new_flash_image->get_image_size() - start_offset,
                             new_flash_image->get_image() + start_offset,
                             verify);

            INFO("Burning done\n");
        }
    }

    if(read | read_formatted) {
        char *end_ptr;
        int int_offset = strtoul(offset, &end_ptr, 0);
        if (*end_ptr != 0 ) {
            ERR("Expected offset and got %s\n", offset);
            EXIT(-1);
        }
        int int_length = strtoul(length, &end_ptr, 0);
        if (*end_ptr != 0 ) {
            ERR("Expected length and got %s\n", length);
            EXIT(-1);
        }

        BYTE *tmp_buffer = new BYTE [int_length];
        if (!tmp_buffer)
        {
            ERR("Cannot allocate temp buffer of size %d\n", int_length);
            EXIT(-1);
        }

        g_flash->read(int_offset, int_length, tmp_buffer);

        if (read_formatted) {
            int step = 16;
            for(int i = 0; i < int_length; i += step) {
                printf("0x%05x:", int_offset+i);
                for( int j = 0; j < min(step, int_length - i); j += 2 ) {
                    printf(" 0x%04x", *(u_int16_t*)(tmp_buffer+i+j));
                }
                printf("\n");
            }
        } else {
            for(int i = 0; i < int_length; i += 2) {
                printf("0x%04x ", *(u_int16_t*)(tmp_buffer+i));
            }
            printf("\n");
        }
        delete[] tmp_buffer;

    }

    delete old_flash_image;
    delete new_flash_image;

    EXIT(0);
}

bool IsReduced()
{
    u_int32_t verifyAddress = 256 * 1024;

    BYTE testSequence[8] = {0x52, 0x65, 0x64, 0x75, 0x63, 0x65, 0x64, 0x21}; // ASCII for "Reduced!"
    BYTE verifySequence[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    BYTE originalData[4096] = {1};
    BYTE manipulatedData[4096] = {2};

    // Save the original data to be restored later
    g_flash->read(verifyAddress, sizeof(originalData), originalData);
    //INFO("1. %d %d %d\n", verifyAddress, sizeof(originalData), originalData);

    memcpy(manipulatedData, originalData, sizeof(originalData));
    memcpy(manipulatedData, testSequence, sizeof(testSequence));
    //INFO("%s\n",manipulatedData);

    // We are writing the testSequence to an address which will not exist in reduced SIP (which is 64K) with verify set to true
    g_flash->program(verifyAddress /*This address will not exist in reduced SIP*/, sizeof(manipulatedData), manipulatedData, true);

    //INFO("2. %d %d %d\n", verifyAddress, sizeof(manipulatedData), manipulatedData);
    g_flash->read(verifyAddress & ((64 * 1024) - 1), sizeof(verifySequence), verifySequence);

    // Restore the original data
    g_flash->clear_erased(verifyAddress);
    g_flash->program(verifyAddress, sizeof(originalData), originalData, true);
    //INFO("3. %d %d %d\n", verifyAddress, sizeof(originalData), originalData);

    if (0 == memcmp(testSequence, verifySequence, sizeof(testSequence)))
    {
        return true;
    }
    //else
    {
        return false;
    }
}

int main(int argc, char *argv[])
{
    const char *fw_ini_file;
    const char *bin_file;
    const char *board_ini_file;
    const char *board_ini_file2;
    const char *board_ini_file3;
    const char *setup_ini_file;
    const char *ids_ini_file;
    const char *production_ini_file;
    const char *usb_ini_file;
    const char *flash_image_file;
    const char *interface_name;
    const char *device_type_string;
    DType device_type = MST_NONE;
    string str_interface_name;
    const char *offset;
    const char *length;
    bool erase, verify, save, burn, read, read_formatted, query, force, read_ids_to_file;
    bool ignore_lock, burn_full_image;
    bool burn_ids_section;
    bool burn_id;
    const char *id_name;
    const char *id_value;

//    printf("Press enter to debug ErezK..");
//    getchar();

    //
    // Map termination signal handlers
    //
    //int i;
//     for (i = 0 ; i < (int)(sizeof(g_signals_for_termination)/sizeof(g_signals_for_termination[0])) ; i++ ) {
//         signal (g_signals_for_termination[i], TerminationHandler);
//     }

    //
    // command line parsing
    //
    ParameterParser *opt = new ParameterParser();
    param_parsing(opt);
    if (opt->processCommandArgs( argc, argv ) != 0) {
        EXIT (-1);
    }

    if( opt->getFlag( "-help" ) ) {
        opt->printUsage();
        EXIT(0);
    }

    flash_image_file = opt->getValue( "-bin" );
    fw_ini_file = opt->getValue( "-fw" );
    bin_file = opt->getValue( "-bin" );
    ids_ini_file = opt->getValue( "-ids" );
    production_ini_file = opt->getValue( "-production" );
    board_ini_file = opt->getValue( "-board" );
    board_ini_file2 = opt->getValue( "-board2" );
    board_ini_file3 = opt->getValue( "-board3" );
    setup_ini_file = opt->getValue( "-setup_ini" );
    usb_ini_file = opt->getValue( "-usb" );
    interface_name = opt->getValue( "-interface" );
    device_type_string = opt->getValue( "-device" );
    offset = opt->getValue( "-offset" );
    length = opt->getValue( "-length" );
    g_debug = opt->getFlag( "-debug" );
    erase = opt->getFlag( "-erase" );
    verify = opt->getFlag( "-verify" );// | opt->getFlag( 'v' );
    save = opt->getFlag( "-save" );
    burn = opt->getFlag( "-burn" );// | opt->getFlag( 'b' );
    read = opt->getFlag( "-read" ); //| opt->getFlag( 'r' );
    read_formatted = opt->getFlag( "-read_formatted" ); //| opt->getFlag( 'r' );
    query = opt->getFlag( "-query" );
    force = opt->getFlag( "-force" );
    ignore_lock = opt->getFlag ("-ignore_lock" );
    read_ids_to_file = opt->getFlag ("-read_ids_to_file" );
    burn_ids_section = opt->getFlag("-burn_ids_section");
    burn_id = opt->getFlag("-burn_id");
    id_name = opt->getValue("-id_name");
    id_value = opt->getValue("-id_value");


    WLCT_UNREFERENCED_PARAM(flash_image_file);
    WLCT_UNREFERENCED_PARAM(save);

    //
    // check missing/extra params
    //
    bool has_ini_file = fw_ini_file != 0 || ids_ini_file != 0 || usb_ini_file != 0 || board_ini_file != 0 || board_ini_file2 != 0 || board_ini_file3 != 0 || production_ini_file != 0;
    bool has_bin_file = bin_file != 0;
    bool missing_ini_file = board_ini_file == 0 && board_ini_file2 != 0 && board_ini_file3 != 0;

    if ( burn ) {
        if ( !(has_ini_file | has_bin_file)) {
            ERR("option -burn require INI or BIN file name\nUse wiburn -h for usage info\n");
            EXIT(-1);
        }
        if ( has_ini_file && has_bin_file) {
            ERR("option -burn require INI or BIN file name. Not both\nUse wiburn -h for usage info\n");
            EXIT(-1);
        }
        if ( missing_ini_file ) {
            ERR("option -board2/-board3 require -board.\nUse wiburn -h for usage info\n");
            EXIT(-1);
        }
    }

    if (read_ids_to_file) {
        if (ids_ini_file == 0) {
            ERR("option -read_ids_to_file require IDS file name.\nUse wiburn -h for usage info\n");
            EXIT(-1);
        }
        if(burn){
            INFO("\nCurrent IDS section will not be changed !\n\n");
        }
    }

    if ( burn | erase | read | read_formatted | query | read_ids_to_file | burn_ids_section | burn_id) {
        if (interface_name == 0) {
            if( get_interface(&str_interface_name) == false ) {
                ERR("Missing interface name\nUse wiburn -h for usage info\n");
                EXIT(-1);
            } else {
                interface_name = str_interface_name.c_str();
            }
        }
    }

    if (read | read_formatted) {
        if( length == 0 || offset == 0 ) {
            ERR("Missing length or offset\nUse wiburn -h for usage info\n");
            EXIT(-1);
        }
    }

    if (burn_ids_section && burn_id)
    {
        ERR("cannot set both -burn_ids_section and -burn_id.\nUse wiburn -h for usage info\n");
        EXIT(-1);
    }

    if (burn_ids_section)
    {
        if (ids_ini_file == 0) {
            ERR("option -burn_ids_section require IDS file name.\nUse wiburn -h for usage info\n");
            EXIT(-1);
        }
    }

    if (burn_id)
    {
        if (id_name == 0 || id_value == 0)
        {
            ERR("option -burn_id require id name and id value.\nUse wiburn -h for usage info\n");
            EXIT(-1);
        }
    }

    burn_full_image = burn && erase;

    //
    // DLL info
    //
    WLCT_DLL_VERSION dll_ver;
    GetMyVersion(&dll_ver);
    INFO("Using DLL version %d.%d.%d.%d\n",
         dll_ver.major,
         dll_ver.minor,
         dll_ver.maintenance,
         dll_ver.build );

    if (device_type_string == 0) {
        ERR("Missing device type\nUse wiburn -h for usage info\n");
        EXIT(-1);
    }
    else
    {
        if( strcmp("MARLON", device_type_string) == 0 )
        {
            device_type = MST_MARLON;
        }
        else if( strcmp("SPARROW", device_type_string) == 0 )
        {
            device_type = MST_SPARROW;
        }
        else if( strcmp("TALYN", device_type_string) == 0 )
        {
            device_type = MST_TALYN;
        }
        else
        {
            ERR("Unknown device type %s. Supported device types are SPARROW and MARLON\n", device_type_string);
            EXIT(-1);
        }
    }

    //
    // Start executing
    //
    if ( burn | erase | query | burn_ids_section | burn_id)
    {
        if( device_type == MST_MARLON )
        {
            g_parser = new ini_parser<marlon>;
        }
        else if( device_type == MST_SPARROW )
        {
            g_parser = new ini_parser<sparrow>;
        }
        else if( device_type == MST_TALYN )
        {
            g_parser = new ini_parser<talyn>;
        }
        else {
            ERR("Unknown device type %s. Supported device types are SPARROW and MARLON\n", device_type_string);
            EXIT(-1);
        }
    }

    if (burn | erase | read | read_formatted | query | read_ids_to_file | burn_ids_section | burn_id) {
        if (strchr(interface_name, '.') != NULL) {
            g_flash = new flash_file(device_type);
        } else {
            g_flash = new flash(device_type);
        }
        if (g_flash->open(interface_name, device_type, ignore_lock) != 0)
        {
            ERR("Can't open device %s with specified type %s.\n", interface_name, device_type_string);
            EXIT(-1);
        }
    }

    if (device_type == MST_TALYN) {
        sub_main<talyn> (burn,
                         erase,
                         burn_full_image,
                         query,
                         verify,
                         read,
                         read_formatted,
                         read_ids_to_file,
                         force,
                         offset,
                         length,
                         bin_file,
                         fw_ini_file,
                         ids_ini_file,
                         production_ini_file,
                         board_ini_file,
                         board_ini_file2,
                         board_ini_file3,
                         setup_ini_file,
                         usb_ini_file,
                         burn_ids_section,
                         burn_id,
                         id_name,
                         id_value);
    }
    if (device_type == MST_SPARROW) {
        sub_main<sparrow> (burn,
                           erase,
                           burn_full_image,
                           query,
                           verify,
                           read,
                           read_formatted,
                           read_ids_to_file,
                           force,
                           offset,
                           length,
                           bin_file,
                           fw_ini_file,
                           ids_ini_file,
                           production_ini_file,
                           board_ini_file,
                           board_ini_file2,
                           board_ini_file3,
                           setup_ini_file,
                           usb_ini_file,
                           burn_ids_section,
                           burn_id,
                           id_name,
                           id_value);
    } else {
        sub_main<marlon> (burn,
                          erase,
                          burn_full_image,
                          query,
                          verify,
                          read,
                          read_formatted,
                          read_ids_to_file,
                          force,
                          offset,
                          length,
                          bin_file,
                          fw_ini_file,
                          ids_ini_file,
                          production_ini_file,
                          board_ini_file,
                          board_ini_file2,
                          board_ini_file3,
                          setup_ini_file,
                          usb_ini_file,
                          burn_ids_section,
                          burn_id,
                          id_name,
                          id_value);
    }

    return 0;
}

