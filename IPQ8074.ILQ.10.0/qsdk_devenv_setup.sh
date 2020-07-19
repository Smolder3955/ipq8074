#!/bin/bash
#
# Copyright (c) 2013 Qualcomm Atheros, Inc.
#
#   **************************************
#    DEVELOPMENT ENVIRONMENT SETUP SCRIPT
#   **************************************
#
#   DONOT RUN THIS SCRIPT IN ALREADY INSTALLAED FOLDER
#
#   name: qsk_devenv_setup.sh
#   version: v5
#   Changes from v4:
#	1. Add copyright information
#	2. Added RELEASE_TAG varibale to use select specific release
#	
#   Steps:
#   ******
#     1. Please configure login info for git operations to https://chipcode.qti.qualcomm.com/ as per below link
#	 https://chipcode.qti.qualcomm.com/projects/help/wiki/Avoiding_Entering_Username_and_Password_for_Git_Operations
#
#     2. Configure of the chipcode.qti.qualcomm.com git repositary name in CHIPCODE_PACKAGE_NAME
#
#     3. Download the patches from support.atheros.com and configure the path ATHEROS_PATCH_DIR
#
#     4. Configure the qsdk build config (retail, enterprise, carrier) QSDK_CONFIG 
#
#     5. Configure the release tag to be checkout in RELEASE_TAG, leave it empty for latest
#
#     6. Execute the script with option( all/setup/build/fit)
#	    all	  --> to do all in sequence setup->build->fit
#	    setup --> for dev setup(cloning/unpacking/pacthing)
#	    build --> to build qsdk
#	    fit	  --> to create single FIT image
#
#
#
####################### USER CONFIGS ################################
#Download the patches from support.atheros.com 
#and place in a folder and configure the below path
#please configure full path
ATHEROS_PATCH_DIR=$PWD

#Configure the QSDK configuration (retail, enterprise, carrier)
QSDK_CONFIG=enterprise

#Configure the chipcode git package name to be cloned
#CHIPCODE_PACKAGE_NAME="qualcomm_ipq8066-ln-1-0_qca_oem_retail"
#CHIPCODE_PACKAGE_NAME="qualcomm_ipq8066-ln-1-0_qca_oem_carrier"
#CHIPCODE_PACKAGE_NAME="qualcomm_ipq8066-ln-1-0_qca_oem_enterprise"
CHIPCODE_PACKAGE_NAME="qualcomm_ipq8066-ln-1-0_qca_oem_internal"
#Configure the package version name
VERSION="2.0.259"
RELEASE_TAG="r00015.1"
####################### USER CONFIGS ################################
ROOT_DIR=$PWD
TOP_DIR=$PWD/$CHIPCODE_PACKAGE_NAME
NSS_IMAGE_DIR=$TOP_DIR/nss_proc/bin
QSDK_DIR=$TOP_DIR/qsdk
QSDK_BIN_DIR=$QSDK_DIR/bin/ipq806x
RSRC_WIFI_DIR=$TOP_DIR/apss_proc/out/proprietary/RSRC-Wifi
STREAMBOOST_DIR=$TOP_DIR/apss_proc/out/proprietary/RBIN-Streamboost-generic
FIT_BUILD_DIR=$TOP_DIR/common/build
FIT_BUILD_SCR=update_common_info.py
#Individial atheros patch names
QSDK_BASE_PATCH=qsdk-base-"$VERSION".patch
QSDK_PACK_PATCH=qsdk-packages-"$VERSION".patch
QSDK_CONF_TAR=qsdk-configs-qsdk-"$VERSION".tar.bz2
QSDK_LUCI_PATCH=qsdk-luci-"$VERSION".patch
QSDK_LINUX_PATCH=qsdk-linux-"$VERSION".patch
QSDK_UBOOT_PATCH=qsdk-u-boot-"$VERSION".patch
QCA_NSS_CFI_TAR=qca-nss-cfi-"$VERSION".tar.bz2
QCA_NSS_CRYPTO_TAR=qca-nss-crypto-"$VERSION".tar.bz2
QCA_NSS_DRV_TAR=qca-nss-drv-"$VERSION".tar.bz2
QCA_NSS_GMAC_TAR=qca-nss-gmac-"$VERSION".tar.bz2
QSDK_QCA_TAR=qsdk-qca-"$VERSION".tar.bz2
STREAMBOOST_TAR=streamboost-ipq-"$VERSION"-template.tar.bz2

ALL_PATCHES="$ATHEROS_PATCH_DIR/$QSDK_BASE_PATCH
$ATHEROS_PATCH_DIR/$QSDK_PACK_PATCH
$ATHEROS_PATCH_DIR/$QSDK_CONF_TAR
$ATHEROS_PATCH_DIR/$QSDK_LUCI_PATCH
$ATHEROS_PATCH_DIR/$QSDK_LINUX_PATCH
$ATHEROS_PATCH_DIR/$QSDK_UBOOT_PATCH
$ATHEROS_PATCH_DIR/$QCA_NSS_CFI_TAR
$ATHEROS_PATCH_DIR/$QCA_NSS_CRYPTO_TAR
$ATHEROS_PATCH_DIR/$QCA_NSS_DRV_TAR
$ATHEROS_PATCH_DIR/$QCA_NSS_GMAC_TAR
$RSRC_WIFI_DIR/$QSDK_QCA_TAR"

QSDK_BUILD_OUT="openwrt-ipq806x-3.4-uImage
openwrt-ipq806x-cdp-u-boot.mbn 
openwrt-ipq806x-squashfs-root.img
openwrt-ipq806x-ubi-root.img"

TMP_DELIM="router" && [ "$QSDK_CONFIG" = "enterprise" ] && TMP_DELIM="ap"
NSS_IMAGE0=$NSS_IMAGE_DIR/"$QSDK_CONFIG"_"$TMP_DELIM"0.uImage
NSS_IMAGE1=$NSS_IMAGE_DIR/"$QSDK_CONFIG"_"$TMP_DELIM"1.uImage

function check_release_tag()
{
    #check if RELEASE_TAG empty	
    if [ -z "$RELEASE_TAG" ]
    then
        return
    fi

#check tag present in the repo otherwise exist
    REPO_TAGS=$(git tag)
    for tags in $REPO_TAGS
    do
    if [[ "$tags" == "$RELEASE_TAG" ]]
    then
	echo "####### checkout the release tag $RELEASE_TAG"
        git checkout $RELEASE_TAG
	return
    fi
    done

    if [[ "$RELEASE_TAG" != "master" ]]
    then
	echo "ERROR: $RELEASE_TAG not found Exiting...."
	exit
    fi
}

function dev_setup()
{

    DIRSTOCHECK=$CHIPCODE_PACKAGE_NAME
    for tmp_dir in $DIRSTOCHECK
    do
	if [ -e $tmp_dir ]
	then
	    echo "ERROR: $tmp_dir already exists/cloned"
	    echo "please remove $tmp_dir"
	    exit
	fi
    done

#check necessary if commands exist for dev setup
    CMDSTOCHECK="git patch svn bzip2"
    for tmp_cmds in $CMDSTOCHECK
    do
	which $tmp_cmds &>/dev/null
	if [ $? -ne 0 ]
	then
	    echo "ERROR: $tmp_cmds command not found, cannnot continue"
	    exit
	fi
    done

    echo "######## CLONING FROM $CHIPCODE_PACKAGE_NAME.git"
    echo "Enter username/password if asked"
    echo "To avoide Entering Username and Password (for ex non-GUI ssh sessions) Git Operations follow"
    echo "https://chipcode.qti.qualcomm.com/projects/help/wiki/Avoiding_Entering_Username_and_Password_for_Git_Operations"
    time git clone https://git.chipcode.qti.qualcomm.com/$CHIPCODE_PACKAGE_NAME.git	

    echo "######## ENTERING $CHIPCODE_PACKAGE_NAME"

#check if git.chipcode.qti.qualcomm.com clone successful    
    if [ ! -e $CHIPCODE_PACKAGE_NAME ]
    then
	echo "ERROR: dir $CHIPCODE_PACKAGE_NAME not found"
	exit
    fi

    cd $CHIPCODE_PACKAGE_NAME

#check and checkout the release tag
	
    check_release_tag

#check all necessary patch and tars available
    for tmp_files in $ALL_PATCHES
    do
	if [ ! -e $tmp_files ]
	then
	   echo "ERROR: $tmp_files not found"
	    exit
	fi
    done

    echo "######## CLONING QSDK ###################"
    git clone git://git.openwrt.org/12.09/openwrt.git qsdk
    mkdir -p qsdk/qca/configs qsdk/qca/feeds qsdk/dl
    qsdkgitopt="--git-dir=qsdk/.git --work-tree=qsdk/"
    git ${qsdkgitopt} checkout b812c102d1382e7073d164255ffe070b668033d4
	
    echo "####### PATCH QSDK BASE $ATHEROS_PATCH_DIR/$QSDK_BASE_PATCH"
    patch -p1 -d qsdk < $ATHEROS_PATCH_DIR/$QSDK_BASE_PATCH 

    echo "####### CLONING QSDK PACKAGES ############"
    git clone git://git.openwrt.org/12.09/packages.git qsdk/qca/feeds/packages
    pkgsgitopt="--git-dir=qsdk/qca/feeds/packages/.git --work-tree=qsdk/qca/feeds/packages"
    git ${pkgsgitopt} checkout d7775e01872b4fdc96edb514833f92c9b2f8a95b
	
    echo "###### PATCH QSDK PACKAGES $ATHEROS_PATCH_DIR/$QSDK_PACK_PATCH"
    patch -p1 -d qsdk/qca/feeds/packages < $ATHEROS_PATCH_DIR/$QSDK_PACK_PATCH 

    echo "###### UNTAR $RSRC_WIFI_DIR/$QSDK_QCA_TAR"
    tar xjvf $RSRC_WIFI_DIR/$QSDK_QCA_TAR -C qsdk

    echo "###### CLONING LUCI #####################"	

    git clone git://git.openwrt.org/project/luci.git luci-tmp

    lucigitopt="--git-dir=luci-tmp/.git --work-tree=luci-tmp"
    git ${lucigitopt} checkout a95d1cc646e96b43cf985abf9205a2790aa8a13c
    
    mv -v luci-tmp/contrib/package qsdk/qca/feeds/luci && rm -rf luci-tmp
    sed -i -e 's,\(PKG_SOURCE_URL\):=.*,\1:=git://git.openwrt.org/project/luci.git,' -e 's,\(PKG_SOURCE_PROTO\):=.*,\1:=git,' -e 's,\(PKG_SOURCE_VERSION\):=.*,\1:=a95d1cc646e96b43cf985abf9205a2790aa8a13c,' qsdk/qca/feeds/luci/luci/Makefile

    echo "###### PATCH QSDK LUCI $ATHEROS_PATCH_DIR/$QSDK_LUCI_PATCH"
    patch -p1 -d qsdk/qca/feeds/luci < $ATHEROS_PATCH_DIR/$QSDK_LUCI_PATCH 
	
    echo "###### UNTAR $ATHEROS_PATCH_DIR/$QSDK_CONF_TAR"
    tar xjvf $ATHEROS_PATCH_DIR/$QSDK_CONF_TAR --xform 's,configs-,,' -C qsdk/qca/configs
	
    echo "###### CLONING LINUX ####################"
    git clone git://codeaurora.org/quic/la/kernel/msm qsdk/qca/src/linux
    linuxgitopt="--git-dir=qsdk/qca/src/linux/.git --work-tree=qsdk/qca/src/linux"
    git ${linuxgitopt} checkout 7fc5534da4f340239fe8a36e48ef9554ca10519a

    echo "###### PATCH QSDK LINUX $ATHEROS_PATCH_DIR/$QSDK_LINUX_PATCH"
    patch -p1 -d qsdk/qca/src/linux < $ATHEROS_PATCH_DIR/$QSDK_LINUX_PATCH 

    echo "###### CLONING U-BOOT ###################"
    git clone git://git.denx.de/u-boot.git qsdk/qca/src/u-boot
    ubootgitopt="--git-dir=qsdk/qca/src/u-boot/.git --work-tree=qsdk/qca/src/u-boot"
    git ${ubootgitopt} checkout 190649fb4309d1bc0fe7732fd0f951cb6440f935

    echo "##### PATCH QSDK U-BOOT $ATHEROS_PATCH_DIR/$QSDK_UBOOT_PATCH"
    patch -p1 -d qsdk/qca/src/u-boot < $ATHEROS_PATCH_DIR/$QSDK_UBOOT_PATCH 

    sed -i '/CONFIG_LOCALMIRROR=.*/d' qsdk/qca/configs/*/*
	
    mkdir -p qsdk/qca/src/qca-nss-gmac qsdk/qca/src/qca-nss-cfi qsdk/qca/src/qca-nss-crypto qsdk/qca/src/qca-nss-drv
	
    echo "###### UNTARRING QCA NSS TARS ###########"
    tar jxvf $ATHEROS_PATCH_DIR/$QCA_NSS_GMAC_TAR -C qsdk/qca/src/qca-nss-gmac --strip-components=1
    tar jxvf $ATHEROS_PATCH_DIR/$QCA_NSS_CFI_TAR -C qsdk/qca/src/qca-nss-cfi --strip-components=1
    tar jxvf $ATHEROS_PATCH_DIR/$QCA_NSS_CRYPTO_TAR -C qsdk/qca/src/qca-nss-crypto --strip-components=1
    tar jxvf $ATHEROS_PATCH_DIR/$QCA_NSS_DRV_TAR -C qsdk/qca/src/qca-nss-drv --strip-components=1

#check and unpack stream package if exist
    if [ -e $STREAMBOOST_DIR/$STREAMBOOST_TAR ]
    then
	echo "##### SRTEAMBOOST PACKAGE FOUND $STREAMBOOST_DIR/$STREAMBOOST_TAR"
	echo "UNTARRING $STREAMBOOST_DIR/$STREAMBOOST_TAR"
	tar xjvf $STREAMBOOST_DIR/$STREAMBOOST_TAR -C qsdk
	sed -e 's,g6d6c616,g4a80312,g' -e 's,gc4ae93f,g7906f52,g' -e 's,g7efe29d,g9f966ad,g' -e 's,g0285a3c,ga5c3fc4,g' -e 's,g6d2b4fc,g33315e2,g' -i qsdk/qca/feeds/streamboost/*/Makefile
    fi

#ECHO OFF
#set +v

    echo "###### ENV SETUP DONE#####################"
}

function print_need_pkg_info()
{

#Print the needed build PC package info
PKGTOCHECK="gcc g++ binutils patch bzip2 flex make gettext 
pkg-config unzip zlib1g-dev libc6-dev subversion 
libncurses5-dev gawk sharutils curl libxml-parser-perl"

    echo "########PLEASE ENSURE THE BELOW PACKAGES"
    echo "$PKGTOCHECK"
    echo "available in the build PC other wise qsdk build may fail"
}

function qsdk_build()
{

    cd $ROOT_DIR

    if [ ! -e $CHIPCODE_PACKAGE_NAME ]
    then
      echo "ERROR: $CHIPCODE_PACKAGE_NAME not found/cloned"
      exit
    fi

    print_need_pkg_info

    echo "###### BUILDING QSDK_$QSDK_CONFIG"
	
    if [ ! -e $QSDK_DIR ]
    then
	echo "$QSDK_DIR not found cannot continue"
	exit
    fi

    set -v
	cd $QSDK_DIR
	make package/symlinks
	cp -v qca/configs/qsdk/ipq806x_$QSDK_CONFIG.config .config
	make defconfig
	make V=s
    set +v

    echo "###### BUILDING QSDK_$QSDK_CONFIG DONE"
}

function build_fit_image()
{
    cd $ROOT_DIR

    if [ ! -e $CHIPCODE_PACKAGE_NAME ]
    then
      echo "ERROR: $CHIPCODE_PACKAGE_NAME not found/cloned"
      exit
    fi  

    echo "##### Building single FIT images for board "

CMDSTOCHECK_FIT="mkimage dtc python"
    for tmp_cmds in $CMDSTOCHECK_FIT
    do
    	which $tmp_cmds &>/dev/null
    	if [ $? -ne 0 ]
	then
	    echo "ERROR: $tmp_cmds command not found, singe image cannot be build"
	    echo "please install package for $tmp_cmds"
	    exit
	fi
    done


    if [ ! -e $FIT_BUILD_DIR ]
    then
	echo "$FIT_BUILD_DIR not found cannot continue"
	exit
    fi

    cd $FIT_BUILD_DIR
    mkdir -p ipq
    mkdir -p bin

    if [ ! -e $NSS_IMAGE0 ]
    then
	echo "$NSS_IMAGE0 not found"
	exit
    fi

    if [ ! -e $NSS_IMAGE1 ]
    then
	echo "$NSS_IMAGE1 not found"
	exit
    fi

    cp -v $NSS_IMAGE0 ipq/.
    cp -v $NSS_IMAGE1 ipq/.

    for tmp_file in $QSDK_BUILD_OUT
    do
    	if [ ! -e $QSDK_BIN_DIR/$tmp_file ]
	then
	    echo "ERROR: $tmp_file not found in $QSDK_BIN_DIR, cannnot continue"
	    echo "QSDK build could have been failed"
	    exit	
	fi
		cp -v $QSDK_BIN_DIR/$tmp_file ipq/.
    done
	
    if [ ! -e $FIT_BUILD_SCR ]
    then
	echo "$FIT_BUILD_SCR not found in $PWD"
	exit
    fi

    python update_common_info.py

    echo "##### Building single FIT images for board DONE"
}

function usage()
{
    echo "usage"
    echo -e "    $1 [setup | build | fit |all]"
    echo -e "         all  : For all in one [setup->build->fit]"
    echo -e "         setup: For dev env setup"
    echo -e "         build: For qsdk build "
    echo -e "         fit  : To create single fit image"
}

##### SCRIPT EXECUTION STARTS HERE ##########################

#check if CHIPCODE_PACKAGE_NAME defined
if [ ! -n "$CHIPCODE_PACKAGE_NAME" ] 
then
    echo "ERROR: CHIPCODE_PACKAGE_NAME not defined, cannnot continue"
    exit
fi

#Check the input options
if [ "$1" = "all" ]
    then
	dev_setup
	qsdk_build
	build_fit_image
elif [ "$1" = "setup" ]
    then
	dev_setup
elif [ "$1" = "build" ]
    then
	qsdk_build
elif [ "$1" = "fit" ]
    then
	build_fit_image
else
	echo "Invalid option $1"
	usage $0
	exit
fi
