#!/usr/bin/perl -w

# Copyright (c) 2017 Qualcomm Technologies, Inc.
#
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.

use File::Copy;

if (!(-d "bin"))
{
	mkdir "bin";
}
copy ("$ARGV[0]/cmdRspDictGenSrc.exe", "bin/cmdRspDictGenSrc.exe") or print "Error: cannot copy cmdRspDictGenSrc.exe file to bin\n";

