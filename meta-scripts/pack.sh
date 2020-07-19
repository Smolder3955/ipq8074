#!/bin/bash

export DEST_DIR="."
export IMAGE_DIR="images"
export PART_DIR="partitions"
export HELP=0

while [ -n "$1" ]; do
	case "$1" in
		-d) export DEST_DIR="$2"; shift;;
		-i) export IMAGE_DIR="$2"; shift;;
		-p) export PART_DIR="$2"; shift;;
		-h|--help) export HELP=1; break;;
		-*)
			echo "Invalid option: $1"
			echo "Use -h to get the list of available options"
			exit 1
		;;
		*) PLATFORMS="$@"; break;;
	esac
	shift;
done

[ -z "${PLATFORMS}" -o "$HELP" -gt 0 ] && {
	cat <<EOF
Usage: $0 <options> file1.xml file2.xml ...

options:
	-d <dest>	Store *.img files into directory <dest> (default: .)
	-i <imagedir>	Image directory location (default: images/)
	-p <partdir>	Partition.xml directory location (default: partitions/)
	-h|--help	Print this help

file.xml:
	List of XML config files to pack
EOF
	exit 1
}

for file in ${PLATFORMS}; do
	./gen-scr.pl -c ${file} -p ${PART_DIR} \
		-o ${IMAGE_DIR}/${file/.xml/-single.scr} || exit 1

	./gen-its.pl -c ${file} -i ${IMAGE_DIR} -s ${IMAGE_DIR}/${file/.xml/-single.scr} \
		-o ${file/.xml/-single.its} || exit 1

	./gen-bootblob.pl -p ${PART_DIR} -i ${IMAGE_DIR} -c ${file} \
                -o ${file/.xml/-bootblob.bin} || exit 1

	mkimage -f ${file/.xml/-single.its} \
		${DEST_DIR}/${file/.xml/-single.img} || exit 1
done
