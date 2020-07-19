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

#include "ParameterParser.h"

#include "wlct_os.h"

ParameterParser::ParameterParser()
{
}

void ParameterParser::setVerbose()
{
    verbose = true ;
}

void ParameterParser::printVerbose()
{
    if( verbose )
        cout << endl  ;
}
void ParameterParser::printVerbose( const char *INFO )
{
    if( verbose )
        cout << INFO  ;
}

int ParameterParser::processCommandArgs( int _argc, char **_argv )
{
    for (int index = 1; index < _argc; index++)
    {
        if (IsFlag(_argv[index]))
        {
            Flags[_argv[index]] = true;
        }
        else if (IsParam(_argv[index]))
        {
            Parameters[_argv[index]] = _argv[index + 1];
            index++;
        }
        else
        {
            printUsage();
            return -1;
        }
    }

    return 0;
}

bool ParameterParser::IsFlag(string flag)
{
    for (list<string>::iterator iterator = flags.begin(); iterator != flags.end(); iterator++)
    {
        if (flag.compare(*iterator) == 0)
        {
            return true;
        }
    }

    return false;
}

bool ParameterParser::IsParam(string param)
{
    for (list<string>::iterator iterator = params.begin(); iterator != params.end(); iterator++)
    {
        if (param.compare(*iterator) == 0)
        {
            return true;
        }
    }

    return false;
}

bool ParameterParser::ParamFound(string param)
{
    return (Parameters.find(param) != Parameters.end());
}

bool ParameterParser::FlagFound(string flag)
{
    return (Flags.find(flag) != Flags.end());
}

void ParameterParser::addFlag(string flag)
{
    flags.push_front(flag);
}

void ParameterParser::addParam(string param)
{
    params.push_front(param);
}

char* ParameterParser::getValue(string option)
{
    if (IsParam(option) && ParamFound(option))
    {
        return Parameters[option];
    }
    else
    {
        return NULL;
    }
}

bool ParameterParser::getFlag(string option)
{
    if (IsFlag(option) && FlagFound(option))
    {
        return Flags[option];
    }
    else
    {
        return false;
    }
}

void ParameterParser::printUsage()
{
    cout<<
        "***************\n"\
        "Usage \n" \
        "***************\n" \
        "Commands:\n" \
        " -help              --- Prints this help \n" \
        " -burn              --- Burn to flash\n" \
        " -query             --- disaply the image info section from flash\n" \
        " -read              --- read image from flash\n" \
        " -read_formatted    --- read image from flash and display in formatted manner\n" \
        " -erase             --- Erase the flash\n" \
        " -debug             --- Prints debug messages\n" \
        " -verify            --- Verify burning\n" \
        " -force             --- Force burning even if it is unsafe\n" \
        " -ignore_lock       --- Ignore flash lock mechanism\n" \
        " -read_ids_to_file  --- Read the IDS section in the Flash, and write it to file\n" \
        " -burn_ids_section  --- Burn IDs section only (ignore contardicting flags, ids section is taken from -ids file)\n" \
        " -burn_id             --- Burn only one id (id name is taken from -id_name and its value is taken from -id_value)\n" \
        " -read_ids_to_file  --- Read the IDS section in the Flash, and write it to file\n" \

        "\n" \
        "Parameters:\n" \
        " -bin binary_image_filename      --- The binary image filename to use\n" \
        " -fw fw_ini_filename             --- The FW INI filename to use\n" \
        " -ids ids_ini_filename           --- The IDs INI filename to use\n" \
        " -production prod_ini_filename   --- The Production INI filename to use\n" \
        " -board board_ini_filename       --- The board INI filename to use\n" \
        " -board2 board_ini_filename      --- The board 2 INI filename to use\n" \
        " -board3 board_ini_filename      --- The board 3 INI filename to use\n" \
        " -setup_ini setup_ini_filename   --- The setup_ini INI filename to use\n" \
        " -usb usb_ini_filename           --- The USB INI filename to use\n" \
        " -interface interface_name       --- The interface to use\n" \
        " -device device_type             --- The type of device\n" \
        " -offset                         --- offset address\n" \
        " -length                         --- Length (in bytes)\n" \
        " -id_name                        --- ID to be burnt (see -burn_id command)\n" \
        " -id_value                       --- Value to be burnt (see -burn_id command)\n" \
        "\n" \
        "Examples\n" \
        "To burn FW INI file to flash using PCI on MARLON with ignore_loc\n" \
        "   wiburn -erase -burn -fw fw.ini -interface wpci2l!malon -ignore_lock -device MARLO\n" \
        "\n" \
        "To burn FW INI file to binary image file out.bi\n" \
        "   wiburn -burn -erase -fw fw.ini -interface out.bin -device MARLO\n" \
        "\n" \
        "To burn FW INI file to flash using automatically selected interfac\n" \
        "   wiburn -burn -erase -fw fw.ini -device MARLO\n" \
        "\n" \
        "To read image from flash on Sparrow using Jtag\n" \
        "   wiburn -read_formatted -length 0x80000 -offset 0 -interface wjtag0!sparrow -device SPARRO\n" \
        "\n" \
        "To burn IDS section to flash using JTAG\n" \
        "   wiburn -section -ids ids_file.ini -interface wjtag0!sparrow -device SPARRO\n" \
        "\n" \
        "To erase the flash using JTA\n" \
        "   wiburn -interface wjtag0!sparrow -erase -device SPARRO\n" \
        "\n" \
        "To burn ids section only\n" \
        "   wiburn -burn_ids_section -force -ignore_lock -device SPARROW -ids ids.ini\n" \
        "\n" \
            "To burn a specific id\n" \
            "   wiburn -force -ignore_lock -device SPARROW -burn_id -id_name mac_address -id_value 0x123456789AB\n";
}
