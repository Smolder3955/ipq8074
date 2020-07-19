#ifndef __LOWI_SSID_H__
#define __LOWI_SSID_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI SSID Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI SSID

Copyright (c) 2012 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/

#include <inc/lowi_defines.h>
#include <inc/lowi_const.h>


namespace qc_loc_fw
{

/**
 * Class for SSID
 */
class LOWISsid
{
public:
  LOWISsid ();
  /**
   * Checks if the SSID is valid
   * @return true for valid, false otherwise
   */
  bool isSSIDValid() const;

  /**
   * Sets SSID
   * @param unsigned char* SSID to be set
   * @param int   Length of the SSID
   * @return 0 for success, non zero for error
   */
  int setSSID(const unsigned char * const ssid, const int length);

  /**
   * Gets SSID
   * @param [out] unsigned char* SSID to be retrieved
   * @param [out] int*   Length of the SSID
   * @return 0 for success, non zero for error
   */
  int getSSID(unsigned char * const pSsid, int * const pLength) const;

  /**
   * Compares the ssid with the current
   * @param LOWISsid& LOWISsid to compare with
   * @return 0 for equal, non zero otherwise
   */
  int compareTo (const LOWISsid & ssid);

  /**
   * Log Tag
   */
  static const char * const TAG;

private:
  bool m_isSsidValid;
  int m_ssid_length;
  unsigned char m_ssid[32];
  bool m_isSsidSet;
};

} // namespace qc_loc_fw

#endif //#ifndef __LOWI_SSID_H__
