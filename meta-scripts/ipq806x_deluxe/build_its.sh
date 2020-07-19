#/*
# * Copyright (c) 2015, The Linux Foundation. All rights reserved.
# *
# * Permission to use, copy, modify, and/or distribute this software for any
# * purpose with or without fee is hereby granted, provided that the above
# * copyright notice and this permission notice appear in all copies.
# *
# * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
# */

#!/bin/bash

ITS="qca_ipq806x_ap.akxx.its";
# size of HLOS partition
# Reducing size of HLOS partition to accomodate MBN header size as well in case of signed image
FIT_MAX_SIZE="4063232";
ITB="qca_ipq806x_ap.akxx";
IPQ_DIR=$PWD/ipq

DTB_STR="qcom-ipq8064-ap148"

build_itb()
{
	for dtb in $DTB_STR
	do
		gzip -9 -c $IPQ_DIR/$dtb.dtb >  $IPQ_DIR/$dtb.dtb.gz
	done

	gzip -9 -c  $IPQ_DIR/openwrt-ipq-ipq806x-vmlinux.bin >  $IPQ_DIR/Image.gz
	mkimage -f $IPQ_DIR/$ITS -D "-I dts -O dtb -S $FIT_MAX_SIZE" $IPQ_DIR/$ITB.itb

	ubinize -m 2048 -p 128KiB -o $IPQ_DIR/root.ubi $IPQ_DIR/ubi-ipq806x-ap-akxx.cfg

        dd if=$IPQ_DIR/root.ubi of=$IPQ_DIR/ipq806x-ubi-root.img bs=2k conv=sync

}
build_itb
