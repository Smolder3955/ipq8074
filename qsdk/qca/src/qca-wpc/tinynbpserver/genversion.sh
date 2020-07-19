if [ -f ../../../linux/kernels/mips-linux-2.6.31/ath_version.mk ]; 
then
	echo "Version File Exists"
	awk -F "-" '{print "#define NBPSERVER_VERSION \"" $3 "\""}' ../../../linux/kernels/mips-linux-2.6.31/ath_version.mk > version.h
	cat version.h
else
	echo "Version File Doesn't Exists\n"
	echo "#define NBPSERVER_VERSION" \"`date +%F_%R`\" > version.h;
fi

