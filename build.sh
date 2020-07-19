#!/bin/bash

export LC_ALL=C
export LANG=en_US

export TOPDIR=${PWD}
export QSDKDIR=${TOPDIR}/qsdk
export QSDKCONFIG=${QSDKDIR}/.config

init_time_info()
{
	export YE="`date '+%G'`"
	export MO="`date '+%m'`"
	export DY="`date '+%d'`"
	export HR="`date '+%H'`"
	export MI="`date '+%M'`"
	export SE="`date '+%S'`"

	export IMAGE_TIMEINFO="${YE}${MO}${DY}${HR}${MI}"
}

restore_configuration()
{
	local model=$1
	local newconfig=$2

	cp -rvf ${newconfig} ${QSDKCONFIG} || exit 1
	make -C ${QSDKDIR} defconfig || exit 1
	echo "${model}" > ${TOPDIR}/.model
}

prepare_environment()
{
	local oldmodel
	local curmodel=$1
	local defconfig=$2

	[ -z ${curmodel} ] && {
		echo "Unknow model: ${curmodel}"
		exit 1
	}

	[ ! -f ${defconfig} ] && {
		echo "Invalid default config: ${defconfig}"
		exit 1
	}

	[ ! -e ${QSDKCONFIG} ] && {
		restore_configuration ${curmodel} ${defconfig}
	} || {	
		oldmodel=`cat ${TOPDIR}/.model 2>/dev/null`
		[ "x${oldmodel}" != "x${curmodel}" ] && {
			restore_configuration ${curmodel} ${defconfig}
		} || {
			echo "=============== nochanged ==============="
		}
	}
}

build_ipq807x()
{
	local MODEL="ipq807x"
	local TGTDIR=${TOPDIR}/target_images/${MODEL}
	local TMPBINDIR=${TOPDIR}/package_tools/common/build/bin

	################## BUILD ##################
	prepare_environment ${MODEL} ${TOPDIR}/defconfig/ipq807x_premium_32bit.config
	make -C ${QSDKDIR} V=s || exit 1

	################### START PACKAGING ##################
	#rm -rf ${TOPDIR}/package_tools/common/build/ipq/openwrt*
	#cp -rf ${QSDKDIR}/bin/ipq806x/openwrt* ${TOPDIR}/package_tools/common/build/ipq/
	#cd ${TOPDIR}/package_tools/common/build || exit 1
	#python update_common_info.py || exit 1
	#rm -rf ${TOPDIR}/package_tools/common/build/ipq/openwrt*

	#################### PREPARE THE FIRMWARE ##################
	#TMPBINLIST=`find ${TMPBINDIR} -type f 2>/dev/null`
	#[ ! -z "${TMPBINLIST}" ] && {
	#	IMAGEFILES="ipq40xx-nor-apps.img"
	#	IMAGEFILES+=" ipq40xx-nornand-apps.img"
	#	IMAGEFILES+=" nor-ipq40xx-single.img"
	#	IMAGEFILES+=" nornand-ipq40xx-single.img"
    #
	#	mkdir -p ${TGTDIR}
	#	cd ${TMPBINDIR} && tar -jcvf ${TGTDIR}/${MODEL}-${IMAGE_TIMEINFO}.tar.bz2 ${IMAGEFILES}
	#}
}

init_time_info

echo "========================================================="
echo "=                                                       ="
echo "=             Welcome to Project Maker Menu             ="
echo "=                                                       ="
echo "========================================================="
echo "  1.  Build ipq807x                                      "
echo "========================================================="
echo -n "==> "

read action
case ${action} in 
	1)
		build_ipq807x
	;;
	
	*)
		echo " ERROR: Please enter the correct number."
	;;
esac
