#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

# Wi-Fi related commands
GET_WIFI_FREQUENCY_FMT = 'iwconfig %s | grep Frequency'
SET_ACS_SCANLIST_FMT = 'wifitool %s setchanlist %d'
START_ACS_REPORT_FMT = 'iwpriv %s acsreport 1'
GET_ACS_REPORT_FMT = 'wifitool %s acsreport | grep %d'
GET_ASSOC_STAS_FMT = 'wlanconfig %s list'
GET_APSTATS_VAP_TXRX = 'apstats -v -i %s | grep "^[TR]x Data Payload Bytes"'


def parse_channel(freq_string):
    """Parse frequency output from iwconfig to get Wi-Fi channel

    Args:
        output (str): the output from the iwconfig command
            Sample valid output:
                'Frequency:5.745 GHz'

    Returns:
        the channel number parsed, or 0 for invalid output, and the corresponding frequency
    """
    try:
        # Extract 5765 from 'Frequency:5.765 GHz'
        freq = int(float(freq_string.split(':')[1].split()[0]) * 1000)
        if freq > 5000:
            # 5 GHz channel
            chan = (freq - 5000) / 5
        else:
            # 2.4 GHz channel
            chan = (freq - 2407) / 5
        return chan, freq
    except Exception:
        # Invalid output from iwconfig
        return 0, 0


def parse_util(util_string):
        """Parse output from wifitool acsreport to get utilization

        Args:
            util_string (str): the output from the wifitool acsreport command
            Sample valid output:
                [' 2412(  1)    0        0         0   -105       1           0          0   ']
        Returns:
            the utilization parsed, or -1 for invalid output
        """
        try:
            # This parsing logic is relying on the fact that the noise floor
            # value should always be negative.
            utilization = int((util_string[0].split('-')[1]).split()[1])
            if utilization > 100 or utilization <= 0:
                # invalid utilization value
                return -1
            return utilization
        except Exception:
            # invalid output from acsreport
            return -1
