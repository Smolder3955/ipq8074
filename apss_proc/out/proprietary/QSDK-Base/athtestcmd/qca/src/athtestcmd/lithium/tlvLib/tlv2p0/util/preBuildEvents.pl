#!/usr/bin/perl -w

# Copyright (c) 2017 Qualcomm Technologies, Inc.
#
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.

use File::Copy;

if (-d "output")
{
	my @oldFiles = <output/*>;
	unlink @oldFiles;
}
else
{
	mkdir "output";
}

my @list = <../common/src/*.s>;
foreach my $file(@list)
{
	copy ($file, "output/");
}
my @newFiles = <output/*>;
chmod 0600, @newFiles;
