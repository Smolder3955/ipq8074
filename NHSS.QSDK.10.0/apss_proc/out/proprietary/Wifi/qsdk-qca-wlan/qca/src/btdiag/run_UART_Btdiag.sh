#!/bin/bash
#TOPDIR=/home/ubuntu/Desktop/Rome_USB_x86_BIT_PKG_01.00.064/Btdiag
#
# enable radio
#
sudo rfkill unblock all
sudo service network-manager stop

echo "=============Uninstall Driver..."
for dev in `ls /sys/module/mac80211/holders/`; do
sudo rmmod $dev
done
for dev in `ls /sys/module/cfg80211/holders/`; do
sudo rmmod $dev
done
sudo rmmod cfg80211 2> /dev/null
echo "=============Done!"

#
# configure ethernet
#
#
# configure ethernet
#
WLANDEV=
for dev in `ls /sys/class/net/`; do
  if [ "$dev" != "lo" ]; then
    if [ -e /sys/class/net/$dev/device/idProduct ]; then
       USB_PID=`cat /sys/class/net/$dev/device/idProduct`
       if [ "$USB_PID" = "9378" ]; then
           WLANDEV=$dev
       fi
    fi
    if [ -e /sys/class/net/$dev/device/device ]; then
       PCI_DID=`cat /sys/class/net/$dev/device/device`
       if [ "$PCI_DID" = "0x003e" ]; then
            WLANDEV=$dev
       fi
    fi
    if [ -e /sys/class/net/$dev/device/device ]; then
       SDIO_DID=`cat /sys/class/net/$dev/device/device`
       if [  "$SDIO_DID" = "0x0509" ] || [ "$SDIO_DID" = "0x0504" ] || [ "$SDIO_DID" = "0x0700" ]; then
            WLANDEV=$dev
       fi
     fi
     if [ "$WLANDEV" = "" ]; then
         if [ "$dev" != "wlan0" ]; then
           echo "Configure ethernet $dev 192.168.250.40"
           sudo ifconfig $dev 192.168.250.40
         fi
     fi
  fi
done

#
# configure bluetooth UART interface
#
FDTIstr="403/6014"
for x in 0 1 2 3 4 5 6 7 8 9 10
do
	sudo grep $FDTIstr /sys/bus/usb-serial/devices/ttyUSB$x/../uevent
	if [ $? == 0 ]; then
		echo "found FDTI device on $x"
		#cd ${TOPDIR}
		sudo ./Btdiag UDT=yes PORT=2390 IOType=SERIAL DEVICE=/dev/ttyUSB$x BAUDRATE=115200
		break
	fi
done
read -p "Press Enter key to exit..."
