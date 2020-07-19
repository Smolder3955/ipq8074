/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                  LOWI Version Interface Header File

GENERAL DESCRIPTION
  This file contains the current Version for
  the LOWI-SERVER software.

Copyright (c) 2013-2015 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================*/

#ifndef _LOWI_VERSION_H_
#define _LOWI_VERSION_H_

//The version will be updated with every change

//After 5.0.0 - Moving to a slightly different version schema - 5.a.x.y
//Every major architecture change will bump the first byte of version i.e. 1.a.x.y to 2.0.0.0
//  This is arbitrary and might change based on the weight of the change or architecture change etc
//Every interface change will bump the second byte of version i.e. b.8.x.y to b.9.0.0
//  This also indicates to the clients that the interface they have been using is now changed.
//Same as before - Every major update (feature addition) will bump the third byte of version i.e. b.x.8.y to b.x.9.0
//Same as before - Every minor update (Bug fix, config value change) will bump the last byte of version i.e. b.x.y.2 to b.x.y.3

//Upto 5.0.0
//Every interface change will bump the first byte of version i.e. 1.x.y to 2.0.0
//Every major update (feature addition) will bump the middle byte of version i.e. x.8.y to x.9.0
//Every minor update (Bug fix, config value change) will bump the last byte of version i.e. x.y.2 to x.y.3

//After 5.2.11.0 (included) - moving to the following version scheme:
// a.b.x.y
// a => Arbitrary. Will be changed when we have major architecture changes in LOWI-AP or when we have significant changes that we feel needs attention from the clients.
// b => Major updates and architecture changes to LOWI Controller Layers, and interface changes to the clients.
// x => Major updates and architecture changes to LOWI Wi-Fi Driver Layers, and interface changes to Wi-Fi Driver.
// y => Minor changes and bug fixes.
namespace qc_loc_fw
{
  const char* const LOWI_VERSION = "LOWI-7.2.1.0.w2";
} // namespace qc_loc_fw
#endif /* _LOWI_VERSION_H_ */
