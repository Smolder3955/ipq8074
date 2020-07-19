#!/bin/busybox sh
# Copyright (c) 2016 Qualcomm Atheros, Inc.
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.

. ../lib/repacd-ethmon.sh

# use stub programs here
PATH=$PWD:$PATH

###########################################
echo "test __ethmon_get_port_state"
###########################################

# stub out swconfig
swconfig() {
echo "        link: port:0 link:up speed:1000baseT full-duplex txflow rxflow"
echo "        link: port:1 link:down"
echo "        link: port:2 link:down"
echo "        link: port:3 link:down"
echo "        link: port:4 link:up speed:1000baseT full-duplex"
echo "        link: port:5 link:up speed:1000baseT full-duplex"
}

test_state=""
test_port_state=$(__ethmon_get_port_state "$test_state" 0)
[ $test_port_state == "unknown" ] || { echo "__ethmon_get_port_state test failed"; exit 1; }

test_state="$(swconfig)"
test_port_state=$(__ethmon_get_port_state "$test_state" 0)
[ $test_port_state == "up" ] || { echo "__ethmon_get_port_state test failed"; exit 1; }
test_port_state=$(__ethmon_get_port_state "$test_state" 1)
[ $test_port_state == "down" ] || { echo "__ethmon_get_port_state test failed"; exit 1; }
test_port_state=$(__ethmon_get_port_state "$test_state" 5)
[ $test_port_state == "up" ] || { echo "__ethmon_get_port_state test failed"; exit 1; }


###########################################
echo "test link down"
###########################################

# stub out swconfig
swconfig() {
echo "        link: port:0 link:up speed:1000baseT full-duplex txflow rxflow"
echo "        link: port:1 link:down"
echo "        link: port:2 link:down"
echo "        link: port:3 link:down"
echo "        link: port:4 link:up speed:1000baseT full-duplex"
echo "        link: port:5 link:up speed:1000baseT full-duplex"
}

# run repacd_ethmon_check
rm -f /tmp/hotplug-call-env*.sh
repacd_ethmon_check

# make sure hotplug-call was called for every port with the right PORT, EVENT and STATE
. /tmp/hotplug-call-env-1.sh
( [ "$PORT" == "0" ] && [ "$EVENT" == "link" ] && [ "$STATE" == "up" ] ) || { echo "ERROR: link down test failed(step 1)"; exit 1; }
. /tmp/hotplug-call-env-2.sh
( [ "$PORT" == "1" ] && [ "$EVENT" == "link" ] && [ "$STATE" == "down" ] ) || { echo "ERROR: link down test failed(step 1)"; exit 1; }
. /tmp/hotplug-call-env-3.sh
( [ "$PORT" == "2" ] && [ "$EVENT" == "link" ] && [ "$STATE" == "down" ] ) || { echo "ERROR: link down test failed(step 1)"; exit 1; }
. /tmp/hotplug-call-env-4.sh
( [ "$PORT" == "3" ] && [ "$EVENT" == "link" ] && [ "$STATE" == "down" ] ) || { echo "ERROR: link down test failed(step 1)"; exit 1; }
. /tmp/hotplug-call-env-5.sh
( [ "$PORT" == "4" ] && [ "$EVENT" == "link" ] && [ "$STATE" == "up" ] ) || { echo "ERROR: link down test failed(step 1)"; exit 1; }
. /tmp/hotplug-call-env-6.sh
( [ "$PORT" == "5" ] && [ "$EVENT" == "link" ] && [ "$STATE" == "up" ] ) || { echo "ERROR: link down test failed(step 1)"; exit 1; }

# change swconfig stub to bring port 4 down
swconfig() {
    if [ "$3" == "show" ]; then
        echo "        link: port:0 link:up speed:1000baseT full-duplex txflow rxflow"
        echo "        link: port:1 link:down"
        echo "        link: port:2 link:down"
        echo "        link: port:3 link:down"
        echo "        link: port:4 link:down"
        echo "        link: port:5 link:up speed:1000baseT full-duplex"
    elif [ "$3" ==  "port" ]; then
        local status
        if [ "$4" == "0" ] || [ "$4" == "5" ]; then
            status=up
        else
            status=down
        fi
        echo "port:$4 link:$status"
    fi
}

# run repacd_ethmon_check
rm -f /tmp/hotplug-call-env*.sh
repacd_ethmon_check

# make sure hotplug-call was called with the right PORT, EVENT and STATE
. /tmp/hotplug-call-env-1.sh
( [ "$PORT" == "4" ] && [ "$EVENT" == "link" ] && [ "$STATE" == "down" ] ) || { echo "ERROR: link down test failed(step 2)"; exit 1; }


###########################################
echo "test link up"
###########################################

# change swconfig stub to bring port 4 back up
swconfig() {
    if [ "$3" == "show" ]; then
        echo "        link: port:0 link:up speed:1000baseT full-duplex txflow rxflow"
        echo "        link: port:1 link:down"
        echo "        link: port:2 link:down"
        echo "        link: port:3 link:down"
        echo "        link: port:4 link:up speed:1000baseT full-duplex"
        echo "        link: port:5 link:up speed:1000baseT full-duplex"
    elif [ "$3" ==  "port" ]; then
        local status
        if [ "$4" == "0" ] || [ "$4" == "4" ] || [ "$4" == "5" ]; then
            status=up
        else
            status=down
        fi
        echo "port:$4 link:$status"
    fi
}

# run repacd_ethmon_check
rm -f /tmp/hotplug-call-env*.sh
repacd_ethmon_check

# make sure hotplug-call was called with the right PORT, EVENT and STATE
. /tmp/hotplug-call-env-1.sh
( [ "$PORT" == "4" ] && [ "$EVENT" == "link" ] && [ "$STATE" == "up" ] ) || { echo "ERROR: link up test failed"; exit 1; }
