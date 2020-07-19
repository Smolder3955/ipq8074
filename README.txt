QSDK supports the premium profile for Linux kernel 4.4.60 support. The QSDK framework is developed using Ubuntu (from version 12.04 to version 16.04) and Debian. However, QSDK
framework regenerates critical tools required to compile firmware at build time. Thus, the framework is independent from the host environment. Although it is developed using the listed distributions, it is expected to work on others such as Red Hat, Mint, or Fedora.

This command is for older/32-bit Debian/Ubuntu releases; customize it for other distributions:
$ sudo apt-get install gcc g++ binutils patch bzip2 flex make gettext pkg-config unzip zlib1g-dev libc6-dev subversion libncurses5-dev gawk sharutils curl libxml-parser-perl ocaml-nox ocaml-nox ocaml ocaml-findlib libpcre3-dev binutils-gold python-yaml libgl1-mesa-dri:i386 ia32-libs-multiarch:i386 libgd2-xpm:i386 ia32-libs-multiarch ia32-libs

This command is for Ubuntu 16.04.3 64-bit build hosts:
$ sudo apt-get install gcc g++ binutils patch bzip2 flex make gettext pkg-config unzip zlib1g-dev libc6-dev subversion libncurses5-dev gawk sharutils curl libxml-parser-perl ocaml-nox ocaml ocaml-findlib libpcre3-dev binutils-gold python-yaml libgl1-mesa-dri:i386 libgd-dev multiarch-support lib32ncurses5 lib32z1 libssl-dev


$sudo apt-get install device-tree-compiler u-boot-tools


cd qsdk
./scripts/feeds update -a
./scripts/feeds install -a -f

1.1 Premium 32-bit
cp -rf qca/configs/qsdk/ipq_premium.config .config
sed -i "s/TARGET_ipq_ipq806x/TARGET_ipq_ipq807x/g" .config
make defconfig
sed -i -e "/CONFIG_PACKAGE_qca-wifi-fw-hw5-10.4-asic/d" .config
make V=s

1.2 Premium 64-bit
cp -rf qca/configs/qsdk/ipq_premium.config .config
sed -i "s/TARGET_ipq_ipq806x/TARGET_ipq_ipq807x_64/g" .config
make defconfig
sed -i -e "/CONFIG_PACKAGE_qca-wifi-fw-hw5-10.4-asic/d" .config
make V=99

2 Generate a complete firmware image
Install mkimage: sudo apt-get install uboot-mkimage
Install DTC: sudo apt-get install device-tree-compiler
Install libfdt: sudo apt-get install libfdt-dev
Install Python 2.7.
Switch to the Qualcomm ChipCode directory: $ cd <ChipCode Directory>
Copy the flash config files to common/build/ipq
Find the single images in the common/build/bin directory. Images are generated for default image
content. Fewer images indicates an issue with one of the partition sizes, or that not all required files
are present.

32-bit
$ cp IPQ8074.ILQ.10.0/common/build/update_common_info.py common/build/update_common_info.py
$ cp qsdk/bin/ipq/openwrt* common/build/ipq
$ cp -rf qsdk/bin/ipq/dtbs/* common/build/ipq/
$ cd common/build
$ sed -i "s/os.chdir(ipq_dir)//" update_common_info.py
$ sed '/debug/d;/packages/d;/"ipq807x_64"/d;/t32/d;/ret_prep_64image/d;/Required/d;/skales/d;/nosmmu/d;/os.system(cmd)/d;/os.chdir(ipq_dir)/d' -i update_common_info.py
$ export BLD_ENV_BUILD_ID=<profile-name>
$ python update_common_info.py
(Where <profile-name> is E, P, LM256, LM512, or S).


64-bit
$ cp IPQ8074.ILQ.10.0/common/build/update_common_info.py common/build/update_common_info.py
$ cp qsdk/bin/ipq/openwrt* common/build/ipq_x64
$ cp -rf qsdk/bin/ipq/dtbs/* common/build/ipq_x64/
$ cd common/build
$ sed -i "s/os.chdir(ipq_dir)//" update_common_info.py
$ sed '/debug/d;/packages/d;/"ipq807x"/d;/t32/d;/ret_prep_32image/d;/Required/d;/nosmmu/d;/os.system(cmd)/d;/skales/d;/os.chdir(ipq_dir)/d' -i update_common_info.py
$ sed -i 's/.\/ipq/.\/ipq_x64/g' update_common_info.py
$ sed -i 's/.\/ipq_x64_x64/.\/ipq_x64/g' update_common_info.py
$ export BLD_ENV_BUILD_ID=<profile-name>
$ python update_common_info.py
(Where <profile-name> is E, P, LM512, or S).




