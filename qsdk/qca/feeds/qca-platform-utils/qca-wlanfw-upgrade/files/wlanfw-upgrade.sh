#!/bin/bash
#
#  Copyright (c) 2018 Qualcomm Technologies, Inc.
#
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.
#
. /lib/ipq806x.sh
. /lib/functions.sh
. /lib/upgrade/platform.sh

# exit when any command fails
set -e

device="$(cat /tmp/sysinfo/model)"
fwpath="$1"
tmpdir="$(mktemp -d)"
tmpfile="$(mktemp)"
img="$2"

flashfw=/lib/firmware/IPQ8074/WIFI_FW
part_name="0:WIFIFW"
status="failed"

emmc_part=$(find_mmc_part $part_name 2> /dev/null)
nand_part=$(find_mtd_part $part_name 2> /dev/null)

if ! echo "$device" | grep -q "IPQ807"; then
	echo "Unrecognized Device...\nCurrently Devices Supported: ipq807x...\n"
	exit 0;
fi


if [ -z "$fwpath" ]; then
	echo "Error:Firmware path is empty\n"
	echo "Usages:Please run as below command:\n"
	echo "wlanfw-upgrade.sh path_to_fw_files fw_type(q6/m3/all)\n"
	exit 0;
fi

printf " Copying firmware files to $tmpdir..\n"
cp -r $flashfw/* $tmpdir/
if [ "$img" == "q6" ]; then
	printf " Copying Q6 fw files from $fwpath..\n"
	rm -rf $tmpdir/q6_*
	cp -r $fwpath/q6_* $tmpdir/
elif [ "$img" == "m3" ]; then
	printf " Copying M3 fw files from $fwpath..\n"
	rm -rf $tmpdir/m3_*
	cp -r $fwpath/m3_* $tmpdir/
else
	printf " Copying Q6 & M3 fw files from $fwpath..\n"
	rm -rf $tmpdir/q6_*
	rm -rf $tmpdir/m3_*
	cp -r $fwpath/q6_* $tmpdir/
	cp -r $fwpath/m3_* $tmpdir/
fi

printf " Preparing squashfs image..\n"
mksquashfs4  $tmpdir/  $tmpfile -nopad -noappend -root-owned -comp xz -Xpreset 9 -Xe -Xlc 0 -Xlp 2 -Xpb 2 -Xbcj arm -b 256k -processors 1

if [ -n "$emmc_part" ]; then
	emmc_blk=$( echo $emmc_part | cut -c 6-15)
	emmc_partsize=$( cat /sys/class/block/$emmc_blk/size )

	printf " Unmounting $emmc_part \n"
	umount $emmc_part || true

	printf " Flashing squashfs image on $emmc_part ..\n"
	dd if=/dev/zero of=$emmc_part bs=512 count=$emmc_partsize
	dd if=$tmpfile of=$emmc_part

	printf " Mounting $emmc_part \n"
	/bin/mount -t squashfs $emmc_part /lib/firmware/IPQ8074/WIFI_FW
	status="completed"
elif [ -n "$nand_part" ]; then
	mtdpart=$(grep "\"$part_name\"" /proc/mtd | awk -F: '{print $1}')

	printf " Flash squashfs image on $nand_part\n"
	size=$(ls -s $tmpfile | awk '{print $1 "KiB"}')

	printf " Unmounting /lib/firmware/IPQ8074/WIFI_FW \n"
	umount /lib/firmware/IPQ8074/WIFI_FW/ || true
	ubidetach -f -p /dev/$mtdpart || true
	sync

	printf " Formatting /dev/$mtdpart \n"
	ubiformat /dev/$mtdpart

	printf " Attaching ubi on /dev/$mtdpart \n"
	ubiattach -p /dev/${mtdpart}
	ubimkvol /dev/ubi1 -N wifi_fw -s $size
	ubiupdatevol /dev/ubi1_0 $tmpfile
	ubi_part=$(find_mtd_part wifi_fw 2> /dev/null)

	printf " Mounting $ubi_part\n"
	/bin/mount -t squashfs $ubi_part /lib/firmware/IPQ8074/WIFI_FW
	status="completed"
else
	printf " Flash Type Not Supported \n"
fi

printf " Cleaning up temp files..\n"
rm -rf  $tmpfile $tmpdir/
printf " wlan fw upgrade $status !\n"
