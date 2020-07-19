/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "WlctPciAcss.h"
#include "WlctPciAcssHelper.h"
#include "wlct_os.h"
#include "DeviceAccess.h"
#include "PciDeviceAccess.h"
#ifdef _WINDOWS
#include "JtagDeviceAccess.h"
#include "SerialDeviceAccess.h"
#endif
#include "MemoryAccess.h"
#include "Util.h"

#ifdef _WINDOWS
#define WIGIG_REGISTRY_PATH _T("Software\\QualcommAtheros\\WIGIG")
#endif

using namespace std;

#define WLCT_IFACE_NAME_LEN 256
#define BL_READY_TIMEOUT_MS 1000
#define BL_READY_WAIT_INTERVAL_MS 20

WLCTPCIACSS_API int GetMyVersion(WLCT_DLL_VERSION *pVer)
{
    return GetPciAcssVersion(pVer);
}

WLCTPCIACSS_API bool wPciDisableEnable()
{
    return Util::ICHDisableEnable();
}

// return 0 for success
WLCTPCIACSS_API int GetInterfaces(INTERFACE_LIST* ifList, int* ReturnItems)
{
    *ReturnItems = 0;
//    int ret = 0;
#ifdef _WINDOWS
#ifndef NO_JTAG
    if (!jtagLoaded) {
        if (!load_jtag_dll()) {
            printf("jTag DLL were not found, skipping jTag interfaces...\n");
        }
    }
#endif
#endif

    for (int i = 0; i < MAX_INTERFACES; i++)
    {
        memset(ifList->list[i].ifName, 0, MAX_IF_NAME_LENGTH);
    }

#ifdef _WINDOWS
    LOG_MESSAGE_INFO(_T("Checking registry for SkipSerialInterfaces register"));
    DWORD dwSkip = Util::ReadRegistrySettings(WIGIG_REGISTRY_PATH, _T("SkipSerialInterfaces"));
    if (dwSkip == (DWORD)-1)
    {
        LOG_MESSAGE_INFO(_T("SkipSerialInterfaces not found, adding registry key with default set to skip serial interfaces scan"));
        dwSkip = 0;
        Util::WriteRegistrySettings(WIGIG_REGISTRY_PATH, _T("SkipSerialInterfaces"), dwSkip);
    }
    else if (dwSkip == 0)
    {
        LOG_MESSAGE_INFO(_T("Get Interfaces for Serial"));
        GetSerialInterfaces(ifList, ReturnItems);
    }
    else
    {
        LOG_MESSAGE_INFO(_T("SkipSerialInterfaces registry key found, skipping serial interface scan"));
    }
#endif

    LOG_MESSAGE_INFO(_T("Get Interfaces for PCI"));
    GetPciInterfaces(ifList, ReturnItems);

#ifdef _WINDOWS
#ifndef NO_JTAG
    if (jtagLoaded)
    {
        LOG_MESSAGE_INFO(_T("Get Interfaces for jTag"));
        GetJtagInterfaces(ifList, ReturnItems);
    }
#endif

#ifndef NO_SERIAL
    //GetSerialInterfaces(ifList, ReturnItems);
#endif

#endif

    printf("Found %d interfaces \n",*ReturnItems);

    return 0;
}

WLCTPCIACSS_API int StrGetInterfaces(char* ifListStr, int capacity)
{
    INTERFACE_LIST ifList;
    int ReturnItems;
    int ret = GetInterfaces(&ifList, &ReturnItems);
    WLCT_UNREFERENCED_PARAM(ret);

    std::ostringstream ifListBuilder;

    for (int item = 0; item < ReturnItems; item++)
    {
        ifListBuilder << ifList.list[item].ifName << ' ';
    }

    int ifListActualLength = _snprintf(ifListStr, capacity, "%s", ifListBuilder.str().c_str());
    if (ifListActualLength >= capacity)
    {
        LOG_MESSAGE_ERROR(
            _T("Error filling interface list: %S, insufficient buffer capacity: %d\n"),
            ifListBuilder.str().c_str(), capacity);
        return 1;
    }

    LOG_MESSAGE_INFO(_T("Interfaces returned: %S"), ifListStr);
    return 0;
}

//    NAME OF FUNCTION: CreateDeviceAccessHandler
//    CREDIT:
//    PURPOSE:
//        The function will create and will initialize an access of read and write
//        to Wilocity hardware. According to tchDeviceName it will interpret the
//        access type (COM port, PCIe driver or Jtag)
//
//    PARAMETERS:
//    name                type    value/reference        description
// ---------------------------------------------------------------------
//    tchDeviceName    TCHAR*  reference            the name of the access interface
//    devType            int        value                the hardware type (MARLON)
//
//    RETURN VALUE:
//    name         type    description
//    handle / pointer to the access object.
//
//    NOTE:
//        After calling this
// ---------------------------------------------------------------------
//    func return    void*    the overall total enrollment for the university
//
//    CALLS TO: This is a factory method. According to the device name it interprets
//        the right interface and create a new object.
//
// CALLED FROM: main
//
// METHOD: The following is a factory.

WLCTPCIACSS_API int CreateDeviceAccessHandler(const char* deviceName, DType devType, void** pDeviceAccess)
{
    LOG_STACK_ENTER;
    IDeviceAccess* pIDeviceAccss = NULL;
    const TCHAR* const delimit = _T("!");

    TCHAR *tchDeviceName;

    TCHAR *next_token = NULL;
    DType devTypeFromInterface = devType;
    TCHAR tchInterfaceName[WLCT_IFACE_NAME_LEN];
    TCHAR tchOriginalInterfaceName[WLCT_IFACE_NAME_LEN];

    // tchInterfaceName may be not null-terminated after calling _tcscpy_s()
    _tcscpy_s(tchInterfaceName, WLCT_IFACE_NAME_LEN, TSTR_ARG(deviceName));
    tchInterfaceName[WLCT_IFACE_NAME_LEN - 1] = '\0';

    _tcscpy_s(tchOriginalInterfaceName, WLCT_IFACE_NAME_LEN, TSTR_ARG(deviceName));
    //tchOriginalInterfaceName[WLCT_IFACE_NAME_LEN - 1] = '\0';

    LOG_MESSAGE_INFO(_T("CreateDeviceAccessHandler for interface named: %s"), tchInterfaceName);

    tchDeviceName = _tcstok_s( tchInterfaceName, delimit, &next_token);

    if (!tchDeviceName)
    {
        LOG_MESSAGE_ERROR(_T("No device name found"));
        return WLCT_OS_ERROR_NOT_SUPPORTED;
    }

    if (_tcsstr(tchDeviceName, _T("wPci")) == tchDeviceName ||
            _tcsstr(tchDeviceName, _T("wpci")) == tchDeviceName)
    {
            if (devTypeFromInterface != devType)
            {
                return WLCT_OS_ERROR_NOT_SUPPORTED;
            }
            LOG_MESSAGE_INFO(_T("wPci Interface detected"));
            pIDeviceAccss = new CPciDeviceAccess(tchDeviceName, devType);
    }
    else if (_tcsstr(tchDeviceName, _T("wMp")) == tchDeviceName ||
            _tcsstr(tchDeviceName, _T("wmp")) == tchDeviceName)
    {
            if (devTypeFromInterface != devType)
            {
                return WLCT_OS_ERROR_NOT_SUPPORTED;
            }
            LOG_MESSAGE_INFO(_T("wMp Interface detected"));
            pIDeviceAccss = new CPciDeviceAccess(tchDeviceName, devType);
    }
    else if (_tcsstr(tchDeviceName, _T("wEP")) == tchDeviceName ||
            _tcsstr(tchDeviceName, _T("wep")) == tchDeviceName)
    {
            LOG_MESSAGE_INFO(_T("wEP Interface detected"));
            pIDeviceAccss = new CPciDeviceAccess(tchOriginalInterfaceName, devType);
    }
    else if (_tcsstr(tchDeviceName, _T("wController")) == tchDeviceName ||
            _tcsstr(tchDeviceName, _T("wcontroller")) == tchDeviceName)
    {
            if (devTypeFromInterface != devType)
            {
                return WLCT_OS_ERROR_NOT_SUPPORTED;
            }
            LOG_MESSAGE_INFO(_T("wController Interface detected"));
            pIDeviceAccss = new CPciDeviceAccess(tchDeviceName, devType);
    }    // acceptable format is: com3, COM3
    else if (_tcsstr(tchDeviceName, _T("COM")) == tchDeviceName ||
            _tcsstr(tchDeviceName, _T("com")) == tchDeviceName)
    {
        if (devTypeFromInterface != devType)
        {
            return WLCT_OS_ERROR_NOT_SUPPORTED;
        }

#ifdef _WINDOWS
        pIDeviceAccss = new CSerialDeviceAccess(tchDeviceName, devType);
        pIDeviceAccss->SetLastError(0);
#else
                return WLCT_OS_ERROR_NOT_SUPPORTED;
#endif
    }
    else /*if (_tcsicmp(tchDeviceName, _T("wjtag")) == 0)*/
    {
            if (devTypeFromInterface != devType)
            {
                return WLCT_OS_ERROR_NOT_SUPPORTED;
            }
#ifdef _WINDOWS
#ifndef NO_JTAG
            pIDeviceAccss = new CJtagDeviceAccess(tchDeviceName, devType);
            if (pIDeviceAccss->lastError == WLCT_OS_ERROR_OPEN_FAILED)
            {
                printf("JTAG CJtagDeviceAccess creation failed!\n");
                return WLCT_OS_ERROR_NOT_SUPPORTED;
            }
#endif
#else
            return WLCT_OS_ERROR_NOT_SUPPORTED;
#endif
    }

        // Sanity check - pIDeviceAccss should be set according to the access type in the above block
        if (!pIDeviceAccss)
        {
            LOG_MESSAGE_ERROR(_T("No device access handler is set"));
            return WLCT_OS_ERROR_NOT_SUPPORTED;
        }

    // assume device type = unknown and search for correct device type
    pIDeviceAccss->SetDeviceStep(STEP_NONE);

    DWORD rev_id_address = 0x880B34;
    DWORD dft_signature_address = 0x800C;
    DWORD val = 0;

    int ret = WlctAccssRead(pIDeviceAccss, rev_id_address, val);
    LOG_MESSAGE_INFO(_T("RevID on [%S] is [0x%X]"),deviceName,val);
    if (ret == 0 && val != 0xFFFFFFFF)
    {
        if (devType == MST_MARLON)
        {
            if (val == 0x612072F)
            {
                pIDeviceAccss->SetDeviceStep(MARLON_STEP_B0);
                LOG_MESSAGE_INFO(_T("MARLON STEP B0"));
            }

            /* This is not really Marlon, it is a DFT device and it should have
                been a different gateway (not a step inside Marlon) */
            else if (val == 0x0) // DFT
            {
                int ret = WlctAccssRead(pIDeviceAccss, dft_signature_address, val);
                if (ret == 0 && val == 0xCE4)
                {
                    pIDeviceAccss->SetDeviceStep(DFT_STEP);
                    LOG_MESSAGE_INFO(_T("DFT STEP"));
                }
            }
            else
            {

                //for now always use marlon as B0

                // Device doesn't match the device type MST_MARLON
                LOG_MESSAGE_INFO(_T("NOT Marlon, RevID = [0x%X], closing handler.."),val);
                pIDeviceAccss->CloseDevice();
                return WLCT_OS_ERROR_NOT_SUPPORTED;
            }
        }
        else if (devType == MST_SPARROW)
        {
            if (val == 0x0632072F)
            {
                pIDeviceAccss->SetDeviceStep(SPARROW_STEP_A0);
                LOG_MESSAGE_INFO(_T("SPARROW STEP A0"));
            }
            else if(val == 0x1632072F)
            {
                pIDeviceAccss->SetDeviceStep(SPARROW_STEP_A1);
                LOG_MESSAGE_INFO(_T("SPARROW STEP A1"));
            }
            else if (val == 0x2632072F)
            {
                pIDeviceAccss->SetDeviceStep(SPARROW_STEP_B0);
                LOG_MESSAGE_INFO(_T("SPARROW STEP B0"));
            }
            else
            {
                LOG_MESSAGE_INFO(_T("Unknown RevID = [0x%X] "),val);
                // This case is for future unrecognized devices, for example, MST_NONE
                pIDeviceAccss->SetDeviceStep(STEP_NONE);
                LOG_MESSAGE_INFO(_T("UNKNOWN DEVICE found, assuming it is Sparrow's next step"));
                //pIDeviceAccss->CloseDevice();
                //return WLCT_OS_ERROR_NOT_SUPPORTED;
            }
        }
    }
    else
    {
        pIDeviceAccss->SetLastError(-1);
    }

    *pDeviceAccess = (void*)pIDeviceAccss;
    return pIDeviceAccss->GetLastError();
}

WLCTPCIACSS_API int CloseDeviceAccessHandler(void* pDeviceAccss)
{
    int res = 0;
    IDeviceAccess* pIDevice = (IDeviceAccess*)pDeviceAccss;
    if (pIDevice != NULL)
    {
        __TRY
        {
            pIDevice->CloseDevice();
            #ifdef _WINDOWS
            delete pIDevice;
            #else
            pIDevice = NULL;
            #endif
        }
        __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
        {
            LOG_MESSAGE_ERROR(_T("got exception"));
            res = -1;
        }
    }
    return res;
}

#ifdef _WINDOWS
WLCTPCIACSS_API int WlctAccssSetDriverMode(void* pDeviceAccss, int newState, int & oldState, int &payloadResult){
    if(pDeviceAccss == NULL){
        return -1;
    }
    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        LOG_MESSAGE_ERROR(_T("Could not get lock for PCI access"));
        return -1;
    }

    __TRY
    {
        if (((IDeviceAccess*)pDeviceAccss)->GetType() == IDeviceAccess::ETypePCI)
            res = ((CPciDeviceAccess*)pDeviceAccss)->SetDriverMode(newState, oldState, payloadResult);
        else
            res = -1;

    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception on Mode Change"));
        res = -1;
        oldState = -1;
    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }
    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
        return res;
}

WLCTPCIACSS_API int WlctAccssGetDriverMode(void* pDeviceAccss, int & oldState){
    if(pDeviceAccss == NULL){
        return -1;
    }
    int res = 0;
    int got_resource;
    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        LOG_MESSAGE_ERROR(_T("Could not get lock for PCI access"));
        return -1;
    }
    __TRY
    {
        if (((IDeviceAccess*)pDeviceAccss)->GetType() == IDeviceAccess::ETypePCI)
            res = ((CPciDeviceAccess*)pDeviceAccss)->GetDriverMode(oldState);
        else
            res = -1;

    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception on Mode Change"));
        res = -1;
        oldState = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }
    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;
}
#endif //_WINDOWS

WLCTPCIACSS_API int WlctAccssRead(void* pDeviceAccss, DWORD addr, DWORD & val)
{
    if (pDeviceAccss == NULL)
        return -1;

    if (addr == BAUD_RATE_REGISTER)
        if (((IDeviceAccess*)pDeviceAccss)->GetDeviceStep() == DFT_STEP)
        {
            DWORD baud_rate = 350; // expected baud rate value from DFT
            val = baud_rate;
            return 0;
        }

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        res = accssRead(pDeviceAccss, addr, val);
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    LOG_MESSAGE_DEBUG(_T("%S [addr 0x%X] [val %d]"), __FUNCTION__, addr, val);
    return res;
}

WLCTPCIACSS_API int accssRead(void* pDeviceAccss, DWORD addr, DWORD & val) {

    return ((IDeviceAccess*)pDeviceAccss)->r32(addr, val);
}


WLCTPCIACSS_API int WlctAccssAllocPmc(void* pDeviceAccss, DWORD size, DWORD numOfDescriptors)
{
    if (pDeviceAccss == NULL)
        {
            LOG_MESSAGE_ERROR(_T("Cannot allocate PMC descriptors - No device access handler"));
            return -1;
        }

    // Get Resource
    int got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
            LOG_MESSAGE_ERROR(_T("Cannot allocated PMC descriptors - Cannot obtain a lock"));
            return -1;
    }

        int res = 0;
    __TRY
    {
        res = ((IDeviceAccess*)pDeviceAccss)->alloc_pmc(size, numOfDescriptors);
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
            LOG_MESSAGE_ERROR(_T("Cannot allocated PMC descriptors - Got exception"));
            res = -1;
            ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    LOG_MESSAGE_DEBUG(_T("Allocated PMC descriptors. status: %d"), res);
    return res;
}

WLCTPCIACSS_API int SwReset(void* pDeviceAccss)
{
    UINT32 bootLoadReady;
    UINT i, maxIterations;
    USER_RGF_BL_INFO_VER_0 blInfo;

    DWORD readVal;
    int res = 0;

    LOG_MESSAGE_INFO(_T("SwReset Initiated..."));

    /* Clear MAC link up */
    accssSet32(pDeviceAccss, USER_RGF_HP_CTRL, BIT(15));
    accssSet32(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_MASK_0, BIT(6));    /* hpal_perst_from_pad_src_n_mask */
    accssSet32(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_MASK_0, BIT(7));    /* car_perst_rst_src_n_mask */

    /* Halt CPU */
    accssWrite(pDeviceAccss, USER_RGF_USER_CPU_0, BIT(1));
    accssWrite(pDeviceAccss, USER_RGF_MAC_CPU_0, BIT(1));


    /* clear all boot loader "ready" bits */
    accssWrite(pDeviceAccss, USER_RGF_BL_READY, 0); //USER_RGF_BL_READY

    /* Clear Fw Download notification */
    accssClear32(pDeviceAccss, USER_RGF_USAGE_6, BIT(0)); //USER_RGF_USAGE_6)

    // Sparrow only register configuration
    accssSet32(pDeviceAccss, USER_RGF_CAF_OSC_CONTROL, BIT_CAF_OSC_XTAL_EN); //USER_RGF_CAF_OSC_CONTROL



    LOG_MESSAGE_INFO(_T("Sleep: waiting for XTAL stabilization..."));
    /* XTAL stabilization should take about 3ms */
    sleep_ms(XTAL_STABLE_WAIT_MS);
    LOG_MESSAGE_INFO(_T("Sleep done: Checking XTAL stabilization..."));
    res = accssRead(pDeviceAccss, USER_RGF_CAF_PLL_LOCK_STATUS, readVal);    //USER_RGF_CAF_PLL_LOCK_STATUS

    if (!(readVal & BIT_CAF_OSC_DIG_XTAL_STABLE)) {    //BIT_CAF_OSC_DIG_XTAL_STABLE
            LOG_MESSAGE_ERROR(_T("%s: BIT_CAF_OSC_DIG_XTAL_STABLE=0x%x\n"), __FUNCTION__, BIT_CAF_OSC_DIG_XTAL_STABLE);
            LOG_MESSAGE_ERROR(_T("%s: Xtal stabilization timeout USER_RGF_CAF_PLL_LOCK_STATUS val=0x%x\n"), __FUNCTION__, readVal);
            return -1;
    }

    /* switch 10k to XTAL*/
    accssClear32(pDeviceAccss, USER_RGF_SPARROW_M_4, BIT_SPARROW_M_4_SEL_SLEEP_OR_REF); //USER_RGF_SPARROW_M_4

    /* 40 MHz */
    accssClear32(pDeviceAccss, USER_RGF_CLKS_CTL_0, BIT_USER_CLKS_CAR_AHB_SW_SEL);  //USER_RGF_CLKS_CTL_0


    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_EXT_SW_RST_VEC_0, 0x3ff81f); //USER_RGF_CLKS_CTL_EXT_SW_RST_VEC_0
    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_EXT_SW_RST_VEC_1, 0xf);    //USER_RGF_CLKS_CTL_EXT_SW_RST_VEC_1

    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_VEC_2, 0xFE000000);    //USER_RGF_CLKS_CTL_SW_RST_VEC_2
    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_VEC_1, 0x0000003F); //USER_RGF_CLKS_CTL_SW_RST_VEC_1
    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_VEC_3, 0x000000f0); //USER_RGF_CLKS_CTL_SW_RST_VEC_3
    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_VEC_0, 0xFFE7FC00);    //USER_RGF_CLKS_CTL_SW_RST_VEC_0

    /* Sparrow Exit SW Reset */
    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_EXT_SW_RST_VEC_0, 0x0);    //USER_RGF_CLKS_CTL_EXT_SW_RST_VEC_0
    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_EXT_SW_RST_VEC_1, 0x0);    //USER_RGF_CLKS_CTL_EXT_SW_RST_VEC_1


    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_VEC_2, 0);    //USER_RGF_CLKS_CTL_SW_RST_VEC_2
    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_VEC_1, 0); //USER_RGF_CLKS_CTL_SW_RST_VEC_1
    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_VEC_3, 0); //USER_RGF_CLKS_CTL_SW_RST_VEC_3
    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_VEC_0, 0);    //USER_RGF_CLKS_CTL_SW_RST_VEC_0

    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_VEC_3, 0x00000003); //USER_RGF_CLKS_CTL_SW_RST_VEC_3
    /* reset A2 PCIE AHB */
    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_VEC_2, 0x00008000);    //USER_RGF_CLKS_CTL_SW_RST_VEC_2

    /* TODO: check order here!!! Erez code is different */
    accssWrite(pDeviceAccss, USER_RGF_CLKS_CTL_SW_RST_VEC_0, 0x0);    //USER_RGF_CLKS_CTL_SW_RST_VEC_0

    // Now wait for device ready, use HW_MACHINE register for this
    maxIterations = BL_READY_TIMEOUT_MS / BL_READY_WAIT_INTERVAL_MS;


    LOG_MESSAGE_INFO(_T("Checking bootloader..."));

    for (i = maxIterations; i; --i)
    {

        //First wait to let the register have the stabilization time it needs
        sleep_ms(BL_READY_WAIT_INTERVAL_MS);

        bootLoadReady = accssRead(pDeviceAccss, USER_RGF_BL_READY, readVal);    //USER_RGF_BL_READY
        (void)bootLoadReady;

        if (readVal == BL_BIT_READY)//BL_BIT_READY
        {
            break;
        }
    }

    LOG_MESSAGE_INFO(_T("Wait on register USER_RGF_BL_READY for %d msec\n"), (maxIterations - i)*BL_READY_WAIT_INTERVAL_MS);

    // check for long wait
    if (0 == i)
    {
            LOG_MESSAGE_ERROR(_T("%s: timeout on register USER_RGF_BL_READY\n"), __FUNCTION__);
            return -1;
    }

    // read info from BL
    void* pBlInfo = &blInfo; // keep the address in temporary variable to prevent false positive warning
    hwCopyFromIO(pBlInfo, pDeviceAccss, USER_RGF_BL_START_OFFSET, sizeof(blInfo));


    LOG_MESSAGE_INFO(_T("Boot loader information: Ready=0x%x, StructVersion=0x%x, RfType=0x%x BaseBandType=0x%x\n"),
        blInfo.Ready, blInfo.StructVersion, blInfo.RfType, blInfo.BaseBandType);

    LOG_MESSAGE_INFO(_T("Boot loader macaddr=%02x:%02x:%02x:%02x:%02x:%02x\n"),
        blInfo.MacAddr[0], blInfo.MacAddr[1], blInfo.MacAddr[2],
        blInfo.MacAddr[3], blInfo.MacAddr[4], blInfo.MacAddr[5]);

    accssClear32(pDeviceAccss, USER_RGF_CLKS_CTL_0, BIT_USER_CLKS_RST_PWGD); //USER_RGF_CLKS_CTL_0


    if (blInfo.StructVersion > 0)
    {
        USER_RGF_BL_INFO_VER_1 blInfoVer1;
        // print the BL version
        void* pBlInfo1 = &blInfoVer1; // keep the address in temporary variable to prevent false positive warning
        hwCopyFromIO(pBlInfo1, pDeviceAccss, USER_RGF_BL_START_OFFSET, sizeof(blInfoVer1));

        LOG_MESSAGE_INFO(_T("Boot Loader build %d.%d.%d.%d\n"),
            blInfoVer1.VersionMajor, blInfoVer1.VersionMinor,
            blInfoVer1.VersionSubminor, blInfoVer1.VersionBuild);
    }

    LOG_MESSAGE_INFO(_T("SwReset Done..."));

    return res;
}

WLCTPCIACSS_API int InterfaceReset(void* pDeviceAccss)
{
    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        res = ((IDeviceAccess*)pDeviceAccss)->do_interface_reset();
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    LOG_MESSAGE_DEBUG(_T("Interface Reset flow end"));
    return res;
}

/* helper function copied and modified from the driver
    we can use this to read info from the boot loader
*/
WLCTPCIACSS_API void hwCopyFromIO( void* dst, void* pDeviceAccss, DWORD src, UINT32 len)
{
    const size_t regSizeInBytes = 4;
    UINT32 regCount = (len + regSizeInBytes - 1) / regSizeInBytes;
    std::vector<DWORD> readBuffer(regCount);

    for (size_t i = 0U; i < readBuffer.size(); ++i)
    {
        ((IDeviceAccess*)pDeviceAccss)->r32(src + i, readBuffer[i]);
    }

    memcpy(dst, &readBuffer[0], len);
}

WLCTPCIACSS_API int WlctAccssWrite(void* pDeviceAccss, DWORD addr, DWORD val)
{
    int res = 0;
    int got_resource;

    if (pDeviceAccss == NULL)
        return -1;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return -1;
    }

    __TRY
    {
        res = accssWrite(pDeviceAccss,addr,val);
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    LOG_MESSAGE_DEBUG(_T("%S [addr 0x%X] [val %d]"), __FUNCTION__, addr, val);
    return res;
}

WLCTPCIACSS_API int accssWrite(void* pDeviceAccss, DWORD addr, DWORD val)
{

    return ((IDeviceAccess*)pDeviceAccss)->w32((addr), val);

}

/* accssSet32 is a modifiled write function based on logic from the driver
    First read from the addr
    Set  the readVal to the desired val with |= val
    Write readVal back into addr
*/
WLCTPCIACSS_API int accssSet32(void* pDeviceAccss, DWORD addr, DWORD val) {
    int res = 0;
    DWORD readVal;

    res = accssRead(pDeviceAccss, addr, readVal);
    readVal |= val;
    res = accssWrite(pDeviceAccss, addr, readVal);

    return res;
}

/* accssClear32 is a modifiled write function based on logic from the driver
    First read from the addr
    Clear the readVal to the desired val with &= ~val
    Write readVal back into addr
*/
WLCTPCIACSS_API int accssClear32(void* pDeviceAccss, DWORD addr, DWORD val) {
    int res = 0;
    DWORD readVal;

    res = accssRead(pDeviceAccss, addr, readVal);
    readVal &= ~val;
    res = accssWrite(pDeviceAccss, addr, readVal);

    return res;
}

WLCTPCIACSS_API bool isInit(void* pDeviceAccss)
{
    if (pDeviceAccss == NULL)
        return false;

    bool res = 0;
    __TRY
    {
        res = ((IDeviceAccess*)pDeviceAccss)->isInitialized();
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = false;
    }
    return res;
}

WLCTPCIACSS_API int writeBlock(void* pDeviceAccss, DWORD addr, DWORD blockSize, const char *arrBlock)
{
    int got_resource;

    if (pDeviceAccss == NULL)
        return -1;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return -1;
    }


    int res = 0;
    __TRY
    {
        LOG_MESSAGE_DEBUG(_T("start running"));
        res = ((IDeviceAccess*)pDeviceAccss)->wb(addr, blockSize, arrBlock);
        LOG_MESSAGE_DEBUG(_T("Leaving"));
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;
}

WLCTPCIACSS_API int readBlock(void* pDeviceAccss, DWORD addr, DWORD blockSize, char *arrBlock)
{
    int got_resource;

    if (pDeviceAccss == NULL)
        return -1;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return -1;
    }


    int res = 0;
    __TRY
    {

        LOG_MESSAGE_DEBUG(_T("start running"));

        res = ((IDeviceAccess*)pDeviceAccss)->rb(addr, blockSize, arrBlock);
        LOG_MESSAGE_DEBUG(_T("Leaving"));
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;
}

//
// read repeat - will read from the same address num_repeat times
// the result will store in arrBlock. arrBlock is an array of shorts with at least num_repeat entries.
//
WLCTPCIACSS_API int readRepeat(void* pDeviceAccss, DWORD addr, DWORD num_repeat, DWORD *arrBlock)
{
    int got_resource;

    if (pDeviceAccss == NULL)
        return -1;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return -1;
    }


    int res = 0;
    __TRY
    {
        LOG_MESSAGE_DEBUG(_T("start running"));
        res = ((IDeviceAccess*)pDeviceAccss)->rr(addr, USHORT(num_repeat), arrBlock);
        LOG_MESSAGE_DEBUG(_T("Leaving"));
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;
}

WLCTPCIACSS_API DType getDeviceType(void* pDeviceAccss)
{
    return ((IDeviceAccess*)pDeviceAccss)->GetDeviceType();
}

WLCTPCIACSS_API int getDbgMsg(void* pDeviceAccss, FW_DBG_MSG** pMsg)
{
    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    __TRY
    {
        res = ((IDeviceAccess*)pDeviceAccss)->getFwDbgMsg(pMsg);
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
    }
    return res;
}

WLCTPCIACSS_API int clearAllFwDbgMsg(void* pDeviceAccss)
{
    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    __TRY
    {
        ((IDeviceAccess*)pDeviceAccss)->clearAllFwDbgMsg();
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
    }
    return res;
}

WLCTPCIACSS_API int card_reset(void* pDeviceAccss)
{
    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    __TRY
    {
        ((IDeviceAccess*)pDeviceAccss)->do_reset();
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
    }
    return res;
}

WLCTPCIACSS_API int startSampling(void* pDeviceAccss, DWORD* pRegsArr, DWORD regArrSize, DWORD interval, DWORD maxSampling, DWORD transferMethod)
{
    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    __TRY
    {
        ((IDeviceAccess*)pDeviceAccss)->startSampling(pRegsArr, regArrSize, interval, maxSampling, transferMethod);
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
    }
    return res;
}

WLCTPCIACSS_API int stopSampling(void* pDeviceAccss)
{
    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    __TRY
    {
        ((IDeviceAccess*)pDeviceAccss)->stopSampling();
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
    }
    return res;
}

WLCTPCIACSS_API int getSamplingData(void* pDeviceAccss, DWORD** pDataSamples)
{
    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    __TRY
    {
        res = ((IDeviceAccess*)pDeviceAccss)->getSamplingData(pDataSamples);
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
    }
    return res;
}

WLCTPCIACSS_API TCHAR* getInterfaceName(void* pDeviceAccss)
{
    if (pDeviceAccss == NULL)
        return NULL;

    TCHAR* szInfName = NULL;
    __TRY
    {
        szInfName = ((IDeviceAccess*)pDeviceAccss)->GetInterfaceName();
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
    }
    return szInfName;
}

#ifdef _WINDOWS
#ifndef NO_JTAG
WLCTPCIACSS_API int WlctAccssJtagSendDR(void* pDeviceAccss, DWORD num_bits, BYTE *arr_in, BYTE *arr_out)
{
    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        if (isInit(pDeviceAccss))
        {
            if (((IDeviceAccess*)pDeviceAccss)->GetType() == IDeviceAccess::ETypeJTAG)
            {
                if (((IDeviceAccess*)pDeviceAccss)->GetDeviceType() == MST_MARLON)
                {
                    res = ((CJtagDeviceAccess*)pDeviceAccss)->jtag_set_dr(arr_in, arr_out, num_bits);
                }
            }
        }
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    //    LOG_MESSAGE_DEBUG(_T("%S [addr 0x%X] [val %d]"), __FUNCTION__, addr, val);
    return res;
}

WLCTPCIACSS_API int WlctAccssJtagSendIR(void* pDeviceAccss, DWORD num_bits, BYTE *arr_in)
{
    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        if (isInit(pDeviceAccss))
        {
            if (((IDeviceAccess*)pDeviceAccss)->GetType() == IDeviceAccess::ETypeJTAG)
            {
                if (((IDeviceAccess*)pDeviceAccss)->GetDeviceType() == MST_MARLON)
                {
                    res = ((CJtagDeviceAccess*)pDeviceAccss)->jtag_set_ir(num_bits, arr_in);
                }
            }
        }
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
//    LOG_MESSAGE_DEBUG(_T("%S [addr 0x%X] [val %d]"), __FUNCTION__, addr, val);
    return res;
}

WLCTPCIACSS_API int JtagSetClockSpeed(void* pDeviceAccss, DWORD requestedFreq, DWORD* frqSet)
{
    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    __TRY
    {
        ((CJtagDeviceAccess*)pDeviceAccss)->set_speed(requestedFreq, frqSet);
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
    }
    return res;
}

WLCTPCIACSS_API int JtagPutTmsTdiBits(void* pDeviceAccss, BYTE *tms, BYTE *tdi, BYTE* tdo, UINT length)
{
    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    __TRY
    {
        ((CJtagDeviceAccess*)pDeviceAccss)->direct_control(tms, tdi, tdo, length);
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
    }
    return res;
}

WLCTPCIACSS_API int JtagTmsShiftToReset(void* pDeviceAccss)
{
    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        if (isInit(pDeviceAccss))
        {
            if (((IDeviceAccess*)pDeviceAccss)->GetType() == IDeviceAccess::ETypeJTAG)
            {
                if (((IDeviceAccess*)pDeviceAccss)->GetDeviceType() == MST_MARLON)
                {
                    res = ((CJtagDeviceAccess*)pDeviceAccss)->jtag_reset();
                }

                if (((IDeviceAccess*)pDeviceAccss)->GetDeviceType() == MST_SPARROW)
                {
                    res = ((CJtagDeviceAccess*)pDeviceAccss)->jtag_reset();
                }
            }
        }
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    //    LOG_MESSAGE_DEBUG(_T("%S [addr 0x%X] [val %d]"), __FUNCTION__, addr, val);
    return res;
}
#else // JTAG

WLCTPCIACSS_API int JtagSetClockSpeed(void* pDeviceAccss, DWORD requestedFreq, DWORD* frqSet)
{
    return -1;
}

WLCTPCIACSS_API int JtagPutTmsTdiBits(void* pDeviceAccss, BYTE *tms, BYTE *tdi, BYTE* tdo, UINT length)
{
    return -1;
}

WLCTPCIACSS_API int JtagTmsShiftToReset(void* pDeviceAccss)
{
    return -1;
}

#endif // ifndef NO_JTAG
#endif /* _WINDOWS */

#ifdef _WINDOWS
WLCTPCIACSS_API int WlctAccssSendWmiCmd(void* pDeviceAccss, SEND_RECEIVE_WMI* wmi)
{
    LOG_MESSAGE_INFO(_T("WlctAccssSendWmiCmd: 0x%X"),  (*wmi).uCmdId);

    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        if (((IDeviceAccess*)pDeviceAccss)->GetType() == IDeviceAccess::ETypePCI)
            res = ((CPciDeviceAccess*)pDeviceAccss)->send_wmi_cmd(wmi);
        else
            res = -1;
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;
}

WLCTPCIACSS_API int WlctAccssRecieveWmiEvent(void* pDeviceAccss, SEND_RECEIVE_WMI* evt)
{
    LOG_MESSAGE_INFO(_T("WlctAccssRecieveWmiEvent"));

    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        if (((IDeviceAccess*)pDeviceAccss)->GetType() == IDeviceAccess::ETypePCI)
            res = ((CPciDeviceAccess*)pDeviceAccss)->recieve_wmi_event(evt);
        else
            res = -1;
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;
}

WLCTPCIACSS_API int WlctAccssMpSetDriverLogLevels(void* pDeviceAccss, ULONG logModulesEnableMsk, ULONG logModuleLevelsMsk[WILO_DRV_NUM_LOG_COMPS])
{
    LOG_MESSAGE_INFO(_T("WlctAccssMpDriverLogLevels"));

    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        if (((IDeviceAccess*)pDeviceAccss)->GetType() == IDeviceAccess::ETypePCI)
            res = ((CPciDeviceAccess*)pDeviceAccss)->setMpDriverLogLevels(logModulesEnableMsk, logModuleLevelsMsk);
        else
            res = -1;
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;
}

#endif

WLCTPCIACSS_API int WlctAccssRegisterDriverMailboxEvents(void* pDeviceAccss, HANDLE* pMailboxEventHandle )
{
    LOG_MESSAGE_INFO(_T("WlctAccssRegisterDriverMailboxEvents"));

    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        if (((IDeviceAccess*)pDeviceAccss)->GetType() == IDeviceAccess::ETypePCI)
            res = ((CPciDeviceAccess*)pDeviceAccss)->register_driver_mailbox_event(pMailboxEventHandle);
        else
            res = -1;
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;
}



WLCTPCIACSS_API int WlctAccssRegisterDriverDeviceEvents(void* pDeviceAccss, INT_PTR hDnEvent, INT_PTR hUpEvent, INT_PTR hUnplugEvent, INT_PTR hSysAssertEvent )
{
    LOG_MESSAGE_INFO(_T("WlctAccssRegisterDriverDeviceEvents"));

    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        if (((IDeviceAccess*)pDeviceAccss)->GetType() == IDeviceAccess::ETypePCI)
            res = ((CPciDeviceAccess*)pDeviceAccss)->register_driver_device_events (hDnEvent, hUpEvent, hUnplugEvent, hSysAssertEvent);
        else
            res = -1;
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;
}

WLCTPCIACSS_API int WlctAccssUnregisterDriverDeviceEvents(void* pDeviceAccss){
    LOG_MESSAGE_INFO(_T("WlctAccssUnregisterDriverDeviceEvents"));

    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        if (((IDeviceAccess*)pDeviceAccss)->GetType() == IDeviceAccess::ETypePCI)
            res = ((CPciDeviceAccess*)pDeviceAccss)->unregister_driver_device_events();
        else
            res = -1;
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;
}
WLCTPCIACSS_API int WlctAccssRegisterDriverEvent(void* pDeviceAccss, INT_PTR event_ptr, int eventId, int appId)
{
    LOG_MESSAGE_INFO(_T("WlctAccssRegisterDriverDeviceEvent"));

    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        if (((IDeviceAccess*)pDeviceAccss)->GetType() == IDeviceAccess::ETypePCI)
            res = ((CPciDeviceAccess*)pDeviceAccss)->register_driver_device_event (event_ptr, appId, eventId);
        else
            res = -1;
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;

}


WLCTPCIACSS_API int WlctAccssRegisterDeviceForUnplug2Event(void* pDeviceAccss, INT_PTR event_ptr)
{

    LOG_MESSAGE_INFO(_T("WlctAccssRegisterDeviceForUnplug2Event"));

    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
            res = ((CPciDeviceAccess*)pDeviceAccss)->register_device_unplug2 (event_ptr);
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;
}

WLCTPCIACSS_API int WlctAccssUnregisterDeviceForUnplug2Event(void* pDeviceAccss)
{

    LOG_MESSAGE_INFO(_T("WlctAccssUnregisterDeviceForUnplug2Event"));

    if (pDeviceAccss == NULL)
        return -1;

    int res = 0;
    int got_resource;

    // Get Resource
    got_resource = Util::timedResourceInterLockExchange(  &((((IDeviceAccess*)pDeviceAccss)->m_flag_busy)), true, 1000, false);
    if (!got_resource)
    {
        return res;
    }

    __TRY
    {
        res = ((CPciDeviceAccess*)pDeviceAccss)->unregister_device_unplug2 ();
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
        LOG_MESSAGE_ERROR(_T("got exception"));
        res = -1;
        ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    }

    ((IDeviceAccess*)pDeviceAccss)->m_flag_busy = false;
    return res;
}

