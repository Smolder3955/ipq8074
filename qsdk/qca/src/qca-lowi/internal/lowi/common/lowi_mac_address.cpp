/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Mac Address

GENERAL DESCRIPTION
  This file contains the implementation of LOWIMacAddress

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <base_util/log.h>

#include <inc/lowi_mac_address.h>

using namespace qc_loc_fw;
const char* const LOWIMacAddress::TAG = "LOWIMacAddress";
////////////////////
// LOWIMacAddress
////////////////////
LOWIMacAddress::LOWIMacAddress()
{
  setMac(0, 0);
}

LOWIMacAddress::LOWIMacAddress(const unsigned char * const pAddr)
{
  setMac(pAddr);
}

LOWIMacAddress::LOWIMacAddress(const LOWIMacAddress& rhs)
{
  setMac(rhs);
}

LOWIMacAddress::LOWIMacAddress (uint8 a0, uint8 a1, uint8 a2,
    uint8 a3, uint8 a4, uint8 a5)
{
  m_hi24 =
    (unsigned int) (a0) << 16 | (unsigned int) (a1) << 8 | (unsigned int) (a2);

  m_lo24 =
    (unsigned int) (a3) << 16 | (unsigned int) (a4) << 8 | (unsigned int) (a5);
}

unsigned int LOWIMacAddress::getLo24() const
{
  return m_lo24;
}

unsigned int LOWIMacAddress::getHi24() const
{
  return m_hi24;
}

unsigned long long LOWIMacAddress::getFull48() const
{
  unsigned long long temp = (unsigned) m_hi24;
  temp <<= 24;
  temp |= (unsigned long long) m_lo24;
  return temp;
}

unsigned long long LOWIMacAddress::getReversed48() const
{
  unsigned long long temp = (unsigned) m_lo24;
  temp <<= 24;
  temp |= (unsigned long long) m_hi24;
  return temp;
}

int LOWIMacAddress::setMac(const LOWIMacAddress& rhs)
{
  return setMac(rhs.m_hi24, rhs.m_lo24);
}

int LOWIMacAddress::setMac(const unsigned char * const pAddr)
{
  if(0 != pAddr)
  {
    m_hi24 = (unsigned int) (pAddr[0]) << 16 | (unsigned int) (pAddr[1]) << 8 | (unsigned int) (pAddr[2]);

    m_lo24 = (unsigned int) (pAddr[3]) << 16 | (unsigned int) (pAddr[4]) << 8 | (unsigned int) (pAddr[5]);
    return 0;
  }
  log_error(TAG, "setMac 2: null pointer");
  return 1;
}

int LOWIMacAddress::setMac(const int addr_hi24, const int addr_lo24)
{
  int result = 1;
  do
  {
    m_lo24 = 0;
    m_hi24 = 0;

    if((addr_lo24 < 0) || (addr_lo24 > 0xFFFFFF))
    {
      result = 2;
      break;
    }

    if((addr_hi24 < 0) || (addr_hi24 > 0xFFFFFF))
    {
      result = 3;
      break;
    }

    m_lo24 = addr_lo24;
    m_hi24 = addr_hi24;

    result = 0;
  } while (0);

  if(0 != result)
  {
    log_error(TAG, "setMac 3: result %d", result);
  }
  return result;
}

int LOWIMacAddress::compareTo(const void * pLhs, const void * pRhs)
{
  const int diff_hi24 = ((const LOWIMacAddress *) pLhs)->m_hi24 -
      ((const LOWIMacAddress *) pRhs)->m_hi24;
  if(0 != diff_hi24)
  {
    return diff_hi24;
  }
  else
  {
    return ((const LOWIMacAddress *) pLhs)->m_lo24 - ((const LOWIMacAddress *)
        pRhs)->m_lo24;
  }
}
int LOWIMacAddress::compareTo (const LOWIMacAddress & mac)
{
  const int diff_hi24 = m_hi24 - mac.m_hi24;
  if(0 != diff_hi24)
  {
    return diff_hi24;
  }
  else
  {
    return (m_lo24 - mac.m_lo24);
  }
}

unsigned char LOWIMacAddress::operator [](const int index) const
{
  int result = 1;
  unsigned char b = 0;
  if(index >= 0)
  {
    if(index <= 2)
    {
      b = (unsigned char) ((m_hi24 >> (8 * (2 - index))) & 0xFF);
      result = 0;
    }
    else if(index <= 5)
    {
      b = (unsigned char) ((m_lo24 >> (8 * (5 - index))) & 0xFF);
      result = 0;
    }
    else
    {
      result = 2;
    }
  }
  else
  {
    result = 3;
  }

  if(0 != result)
  {
    log_error(TAG, "LOWIMacAddress::operator []: result %d", result);
  }

  return b;
}

LOWIMacAddress & LOWIMacAddress::operator =(const LOWIMacAddress & rhs)
{
  if(&rhs != this)
  {
    m_lo24 = rhs.m_lo24;
    m_hi24 = rhs.m_hi24;
  }
  return *this;
}

void LOWIMacAddress::print()
{
  log_verbose (TAG, "BSSID = %02x:%02x:%02x:%02x:%02x:%02x", (*this)[0]
                                                        , (*this)[1]
                                                        , (*this)[2]
                                                        , (*this)[3]
                                                        , (*this)[4]
                                                        , (*this)[5]);
}

bool LOWIMacAddress::isLocallyAdministeredMac ()
{
  return (((*this)[0] & 0x02) != 0);
}
