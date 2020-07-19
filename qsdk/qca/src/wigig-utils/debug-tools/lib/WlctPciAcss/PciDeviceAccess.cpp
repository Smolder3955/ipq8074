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

#include "wlct_os.h"

#include "PciDeviceAccess.h"
#include "WlctPciAcssHelper.h"
#include "DeviceAccess.h"
#include <ctime>
#include "Util.h"


//////////////////////////////////////////////////////////////////////////
// PCI interface

CPciDeviceAccess::CPciDeviceAccess(const TCHAR* tchDeviceName, DType devType)
    : IoctlDev(tchDeviceName)/*, SamplingThread(SamplingThreadProc, this),
                               PciPluginThread(this)*/
{
    LOG_MESSAGE_INFO(_T("Create PCI device access for: %s"), tchDeviceName);
    _tcscpy_s(szInterfaceName, MAX_DEVICE_NAME_LEN, tchDeviceName);
    deviceType = devType;
    m_close_state = 2;
    m_open_state = 0;
    //LOG_MESSAGE_INFO(_T("[close %d] [open %d]"), m_close_state, m_open_state);
    bInitialized = false;
    Open();
    if (bInitialized)
    {
//        PciPluginThread.Start();
    }
    return;
}

void CPciDeviceAccess::Open()
{
    if (IoctlDev.IsOpened())
    {
        LOG_MESSAGE_INFO(_T("Already open. changing m_close_state = 0 and m_open_state = 2 [close %d] [open %d]"), m_close_state, m_open_state);
        bInitialized = true;
        m_close_state = 0;
        m_open_state = 2;
        return;
    }

    m_sampling_arr = NULL;
    m_get_sampling_arr = NULL;
//    m_hMonitorwPciPluginThread = NULL;
    lastError = WLCT_OS_ERROR_SUCCESS;
    dwBaseAddress = 0x880000;

    bIsLocal = true;
    TCHAR LorR = szInterfaceName[_tcslen(szInterfaceName) - 1];
    if (LorR == _T('R') || LorR == _T('r'))
    {
        bIsLocal = false;
        dwBaseAddress += 0x100000;
    }

    lastError = IoctlDev.Open();
    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        //LOG_MESSAGE_ERROR(_T("PCIdeviceAccess: failed to open driver wPci. ERROR_FILE_NOT_FOUND = %d"), lastError);
        return;
    }

//    ReOpen();

    bInitialized = true;
//    LOG_MESSAGE_INFO(_T("changing m_close_state to 0 and m_open_state to 2 [close %d] [open %d]"), m_close_state, m_open_state);
    m_close_state = 0;
    m_open_state = 2;

    m_flag_busy = false;
}

CPciDeviceAccess::~CPciDeviceAccess()
{
    CloseDevice();
}

int CPciDeviceAccess::ReOpen()
{
    LOG_MESSAGE_INFO(_T("Sending ioctl IOCTL_FILTER_OPEN_DEVICE"));

    FILTER_OPEN_DEVICE openDevice;

    lastError = IoctlDev.Ioctl(bIsLocal?IOCTL_FILTER_OPEN_LOCAL_DEVICE:IOCTL_FILTER_OPEN_REMOTE_DEVICE,
                               NULL, 0, &openDevice, sizeof(openDevice));
    if ( lastError != WLCT_OS_ERROR_SUCCESS )
    {
        LOG_MESSAGE_ERROR(_T("Error in DeviceIoControl: IOCTL_FILTER_OPEN_DEVICE [err %d]"), lastError);
        return -1;
    }

    LOG_MESSAGE_INFO(_T("Successfully sent ioctl IOCTL_FILTER_OPEN_DEVICE"));

    return 0;
}

int CPciDeviceAccess::GetType()
{
    return IDeviceAccess::ETypePCI;
}

int CPciDeviceAccess::InternalCloseDevice()
{
//    DWORD bytesReturned = 0;

//    sleep_ms(100);

//     if (bIsLocal)
//     {
//         bRc = DeviceIoControl (hPciCtrlDevice, IOCTL_FILTER_CLOSE_LOCAL_DEVICE, NULL, 0, NULL, 0, &bytesReturned, NULL);
//     }
//     else
//     {
//         bRc = DeviceIoControl (hPciCtrlDevice, IOCTL_FILTER_CLOSE_REMOTE_DEVICE, NULL, 0, NULL, 0, &bytesReturned, NULL);
//     }
//     lastError = ::GetLastError();
//     if ( !bRc || bytesReturned != 0)
//     {
//         LOG_MESSAGE_WARN(_T("Error in DeviceIoControl : %d"), lastError);
//     }
//     else
//     {
//         LOG_MESSAGE_INFO(_T("Successfully sent ioctl IOCTL_FILTER_CLOSE_DEVICE"))
//     }

    IoctlDev.Close();
//    LOG_MESSAGE_INFO(_T("changing m_close_state to 2 [close %d] [open %d]"), m_close_state, m_open_state);
    m_close_state = 2;

    return 0;
}

int CPciDeviceAccess::CloseDevice()
{
    //PciPluginThread.Stop();
    if (InternalCloseDevice() == 0)
    {
        bInitialized = false;
        //LOG_MESSAGE_INFO(_T("changing m_open_state to 0 and bInitialized to false [close %d] [open %d]"), m_close_state, m_open_state);
        m_open_state = 0;
    }
    return 0;
}

#ifdef _WINDOWS
int CPciDeviceAccess::SetDriverMode(int newState, int &oldState, int &payloadResult)
{
    if(!isInitialized()){
        return -1;
    }
    if (m_close_state == 2)
        Open();
    if (!IoctlDev.IsOpened())
        return -1;

    SET_MODE_CMD_INPUT inBuffer;
    SET_MODE_CMD_OUTPUT outBuffer;

    memset(&inBuffer, 0, sizeof(SET_MODE_CMD_INPUT));
    memset(&outBuffer, 0, sizeof(SET_MODE_CMD_OUTPUT));

    inBuffer.mode = (DRIVER_MODE)newState;

    lastError = IoctlDev.Ioctl(WILOCITY_IOCTL_SET_MODE_NEW, &inBuffer, sizeof(inBuffer), &outBuffer, sizeof(outBuffer));

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("WILOCITY_IOCTL_SET_MODE_NEW failed. %X"), lastError);
        oldState = -1;
        return -1;
        payloadResult = -1;
    }
    if(!outBuffer.result){
        LOG_MESSAGE_WARN(_T("WILOCITY_IOCTL_SET_MODE_NEW did not change the mode"));
    }
    payloadResult = outBuffer.result;
    oldState = outBuffer.previousMode;
    return 0;
}


int CPciDeviceAccess::GetDriverMode(int &currentState){
    if(!isInitialized()){
        return false;
    }
    if (m_close_state == 2)
        Open();
    if (!IoctlDev.IsOpened())
        return -1;

    GET_MODE_CMD_OUTPUT currentModeBuffer;
    memset(&currentModeBuffer, 0, sizeof(GET_MODE_CMD_OUTPUT));


    lastError = IoctlDev.Ioctl(WILOCITY_IOCTL_GET_MODE_NEW, NULL, 0, &currentModeBuffer, sizeof(currentModeBuffer));

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("WILOCITY_IOCTL_GET_MODE_NEW failed. %X"), lastError);
        return -1;
    }

    currentState = currentModeBuffer.currentMode;
    return 0;
}
#endif //_WINDOWS

// MARLON
int CPciDeviceAccess::r32(DWORD addr, DWORD & val)
{
    if (!isInitialized())
        return -1;
    if (m_close_state == 2)
        Open();
    if (!IoctlDev.IsOpened())
        return -1;

    FILTER_1_ULONG_PARAM    inParams;
    FILTER_1_ULONG_PARAM    outParams;

    inParams.param = addr;

    if (!bIsLocal)
    {
        inParams.param += 0x100000;
    }

    if (IoctlDev.bOldIoctls)
        lastError = IoctlDev.Ioctl(IOCTL_INDIRECT_READ_OLD, &inParams, sizeof(inParams), &outParams, sizeof(outParams));
    else
        lastError = IoctlDev.Ioctl(WILOCITY_IOCTL_INDIRECT_READ, &inParams, sizeof(inParams), &outParams, sizeof(outParams));

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("WILOCITY_IOCTL_INDIRECT_READ failed. %X"), lastError);
        return -1;
    }

    val = outParams.param;
    LOG_MESSAGE_DEBUG(_T("ADDR(in bytes): 0x%X Value: 0x%X"), addr, val);
    return 0;
}

bool CPciDeviceAccess::try2open(int timeout)
{
    do
    {
        Open();
        if (IoctlDev.IsOpened())
            break;
        sleep_ms(100);
        timeout -= 100;
    } while (timeout > 0);

    if (!IoctlDev.IsOpened())
        return false;
    return true;
}


int CPciDeviceAccess::do_interface_reset()
{
    if (!isInitialized())
        return -1;
    if (m_close_state == 2)
        Open();
    if (!IoctlDev.IsOpened())
        return -1;

    FILTER_1_ULONG_PARAM    inParams;
    FILTER_1_ULONG_PARAM    outParams;

    /*inParams.param = addr;

      if (!bIsLocal)
      {
      inParams.param += 0x100000;
      }*/

    lastError = IoctlDev.Ioctl(WILOCITY_IOCTL_DEVICE_SW_RESET, &inParams, sizeof(inParams), &outParams, sizeof(outParams));


    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("WILOCITY_IOCTL_DEVICE_SW_RESET failed. %X"), lastError);
        InternalCloseDevice();
        return -1;
    }

    //val = outParams.param;
    LOG_MESSAGE_INFO(_T("Pci Interface Reset using IOCTL"));
    return 0;
}

int CPciDeviceAccess::do_sw_reset()
{
    LOG_MESSAGE_ERROR(_T("NOT IMPLEMENTED"));
    return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
}


#define USING_RESET_FUNCTION 0
int CPciDeviceAccess::do_reset(BOOL bFirstTime)
{
    if (USING_RESET_FUNCTION == 0)
    {
        LOG_MESSAGE_WARN(_T("reset was not done because USING_RESET_FUNCTION == 0"));
        return -1;
    }

    if (!isInitialized())
        return -1;
    if (m_close_state == 2)
        Open();
    if (!IoctlDev.IsOpened())
        return -1;

    DWORD addr;
    if (deviceStep == STEP_NONE)
        addr = 0x880AE0;
    else if (deviceStep == MARLON_STEP_B0 || deviceStep == SPARROW_STEP_A0 || deviceStep == SPARROW_STEP_B0)
        addr = 0x880B04;
    else // default
    {
        addr = 0x880B04;
    }

    DWORD val;
    bool bres;

    // check if there is an access to the board
    int ret = r32(BAUD_RATE_REGISTER, val);
    if (ret != 0 || val == 0 || val == 0xFFFFFFFF)
    {
        // access failed.
        if (bFirstTime == FALSE)
            return -1;

        // do disable enable
        InternalCloseDevice();
        sleep_ms(500);
        bres = Util::ICHDisableEnable();
        if (!bres)
            sleep_ms(1000);
        Open();
        do_reset(FALSE);
    }

    w32(addr, 1);
    InternalCloseDevice();
    sleep_ms(500);
    bres = Util::ICHDisableEnable();

    bres = try2open(2000);
    if (!bres)
    {
        LOG_MESSAGE_INFO(_T("do_reset: failed to open the device after reset"));
        return -1;
    }


    int timeout = 2000;
    bres = false;
    do
    {
        ret = r32(BAUD_RATE_REGISTER, val);
        if (ret == 0 && val > 0 && val < 0xFFFFFFFF)
        {
            bres = true;
            break;
        }
        sleep_ms(100);
        timeout -= 100;
    } while (timeout > 0);
    if (!bres)
    {
        LOG_MESSAGE_ERROR(_T("ERROR: failed to read the baudrate after reset."));
        return -1;
    }
    return 0;
}

int CPciDeviceAccess::reset_complete()
{
    DWORD val;
    bool bres;
    int ret;

    InternalCloseDevice();
    sleep_ms(500);
    Util::ICHDisableEnable();

    bres = try2open(2000);
    if (!bres)
    {
        LOG_MESSAGE_INFO(_T("failed to open the device after reset"));
        return -1;
    }

    int timeout = 2000;
    bres = false;
    do
    {
        ret = r32(BAUD_RATE_REGISTER, val);
        if (ret == 0 && val > 0 && val < 0xFFFFFFFF)
        {
            bres = true;
            break;
        }
        sleep_ms(100);
        timeout -= 100;
    } while (timeout > 0);
    if (!bres)
    {
        LOG_MESSAGE_ERROR(_T("ERROR: failed to read the baudrate after reset."));
        return -1;
    }
    return 0;
}

// marlon
int CPciDeviceAccess::w32(DWORD addr, DWORD val)
{
    if (!isInitialized())
        return -1;
    if (m_close_state == 2)
        Open();
    if (!IoctlDev.IsOpened())
        return -1;


    FILTER_2_ULONG_PARAM    inParams;

    inParams.param1 = addr;

    if (!bIsLocal)
    {
        inParams.param1 += 0x100000;
    }

    inParams.param2 = val;


    if (IoctlDev.bOldIoctls)
        lastError = IoctlDev.Ioctl(IOCTL_INDIRECT_WRITE_OLD, &inParams, sizeof(inParams), NULL, 0);
    else
        lastError = IoctlDev.Ioctl(WILOCITY_IOCTL_INDIRECT_WRITE, &inParams, sizeof(inParams), NULL, 0);


    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("IOCTL_INDIRECT_WRITE failed. %X"), lastError);
        InternalCloseDevice();
        return -1;
    }

    if (USING_RESET_FUNCTION == 0)
    {
        if (((deviceStep == STEP_NONE) && (addr == 0x880AE0)) || ((deviceStep == MARLON_STEP_B0 || deviceStep == SPARROW_STEP_A0 || deviceStep == SPARROW_STEP_B0) && (addr == 0x880B04)))
        {
            if (val & 0x1)
            {
                LOG_MESSAGE_WARN(_T("Calling reset from write command instead of calling reset directly!!!!!!!!!!!!!!!!!!!!!!!!!!"));
                reset_complete();
            }
        }
    }

    LOG_MESSAGE_DEBUG(_T("ADDR: 0x%X Value: 0x%X"),addr, val);
    return 0;
}

// addr is in bytes.
// block size for SWITH is number of 16 bits and MARLON is number of bytes
// In MARLON the block size should be align to 32 bits (otherwise it will be
// truncated by the hardware)
//
int CPciDeviceAccess::rb(DWORD addr, DWORD blockSize, char *arrBlock)
{
    if (!isInitialized())
        return -1;
//     if (m_close_state == 1)
//         InternalCloseDevice();
//     if (m_open_state == 1)
//         Open();
//     if (m_close_state == 2)
//         return -1;
    if (m_close_state == 2)
        Open();
    if (!IoctlDev.IsOpened())
        return -1;


    //if (GetDeviceType() == MST_MARLON)
    //{
    FILTER_1_ULONG_PARAM    inParams;

    inParams.param = addr;

    if (!bIsLocal)
    {
        inParams.param += 0x100000;
    }


    if (IoctlDev.bOldIoctls)
        lastError = IoctlDev.Ioctl(IOCTL_INDIRECT_READ_BLOCK, &inParams, sizeof(inParams), arrBlock, blockSize);
    else
        lastError = IoctlDev.Ioctl(WILOCITY_IOCTL_INDIRECT_READ_BLOCK, &inParams, sizeof(inParams), arrBlock, blockSize);

    //    DWORD le = GetLastError();
    //    LOG_MESSAGE_WARN(_T("IOCTL_INDIRECT_READ return %d [lastError 0x%X]"), bRc, le);
    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("IOCTL_INDIRECT_READ failed. %X"), lastError);
        InternalCloseDevice();
        return -1;
    }
    //}

    return 0;
}

int CPciDeviceAccess::wb(DWORD addr, DWORD blockSize, const char *arrBlock)
{
    if (!isInitialized())
        return -1;
    if (m_close_state == 2)
        Open();
    if (!IoctlDev.IsOpened())
        return -1;

    PFILTER_WRITE_BLOCK    pinParams;
    char* p = new char[2*sizeof(ULONG) + blockSize];
    if (!p) return -1;

    pinParams = (PFILTER_WRITE_BLOCK)p;
    pinParams->address = addr;

    if (!bIsLocal)
    {
        pinParams->address += 0x100000;
    }

    pinParams->size = blockSize;
    memcpy(pinParams->buffer, arrBlock, blockSize);


    if (IoctlDev.bOldIoctls)
        lastError = IoctlDev.Ioctl(IOCTL_INDIRECT_WRITE_BLOCK, pinParams, 2*sizeof(ULONG) + blockSize, NULL, 0);
    else
        lastError = IoctlDev.Ioctl(WILOCITY_IOCTL_INDIRECT_WRITE_BLOCK, pinParams, 2*sizeof(ULONG) + blockSize, NULL, 0);

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("WILOCITY_IOCTL_INDIRECT_WRITE_BLOCK failed. %X"), lastError);
        InternalCloseDevice();
        delete [] p;
        return -1;
    }

    delete [] p;
    return 0;
}

void CPciDeviceAccess::rr32(DWORD addr, DWORD num_repeat, DWORD *arrBlock)
{
    WLCT_UNREFERENCED_PARAM(addr);
    WLCT_UNREFERENCED_PARAM(num_repeat);
    WLCT_UNREFERENCED_PARAM(arrBlock);
    WLCT_ASSERT(0);
}

int CPciDeviceAccess::rr(DWORD addr, DWORD num_repeat, DWORD *arrBlock)
{
    if (!isInitialized())
        return -1;
    if (m_close_state == 2)
        Open();
    if (!IoctlDev.IsOpened())
        return -1;

    //if (GetDeviceType() == MST_MARLON)
    //{
    FILTER_1_ULONG_PARAM    inParams;
    inParams.param = addr;
    if (!bIsLocal)
    {
        inParams.param += 0x100000;
    }

    lastError = IoctlDev.Ioctl(IOCTL_INDIRECT_READ_REPEAT, &inParams, sizeof(inParams), arrBlock, num_repeat*sizeof(DWORD));
    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("IOCTL_INDIRECT_READ failed. %X"), lastError);
        InternalCloseDevice();
        return -1;
    }


//         actualAddr = addr - dwBaseAddress;
//         if (actualAddr >= CRSsize)
//             return -1;
//
//         rr32(actualAddr>>2, num_repeat, arrBlock);
    //}
    return 0;
}

int CPciDeviceAccess::getFwDbgMsg(FW_DBG_MSG** pMsg)
{
    (void)pMsg;
    //    LOG_MESSAGE_DEBUG(_T("NOT IMPLEMENTED"));
    return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
}

int CPciDeviceAccess::clearAllFwDbgMsg()
{
    LOG_MESSAGE_ERROR(_T("NOT IMPLEMENTED"));
    return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
}

/*
  void CPciDeviceAccess::SamplingThreadProc(void *pDeviceAccss)
  {
  LOG_MESSAGE_DEBUG(_T("start running"));
  CPciDeviceAccess *pDevice = (CPciDeviceAccess*)pDeviceAccss;
  #ifdef WCLT_SAMPLING_TO_FILE_IS_SUPPORTED
  HANDLE            hSamplingFile = INVALID_HANDLE_VALUE;
  #endif
  uint64_t          startTS;

  pDevice->m_sampling_head = 0;
  pDevice->m_sampling_tail = 0;
  pDevice->m_loops = 0;

  startTS = GetTimeStampMs();

  WLCT_UNREFERENCED_PARAM(startTS);

  SAMPLING_REGS* currEntry;

  #ifdef WCLT_SAMPLING_TO_FILE_IS_SUPPORTED
  DWORD written = 0;
  if (pDevice->m_transferMethod == 1)
  {    // saving to file
  TCHAR file_name[256];
  _stprintf (file_name, _T("c:\\sampling_%X.bin"), GetTickCount());
  hSamplingFile = CreateFile(file_name, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hSamplingFile == INVALID_HANDLE_VALUE)
  {
  return;
  }
  // first data in the file is the number of registers in each sampling
  WriteFile(hSamplingFile, &pDevice->m_no_sampling_regs, sizeof(pDevice->m_no_sampling_regs), &written, NULL);
  // then write the registers address
  WriteFile(hSamplingFile, pDevice->m_pRegsArr, sizeof(DWORD)* pDevice->m_no_sampling_regs, &written, NULL);
  }
  #endif

  uint64_t last_timeStamp = 0;
  do
  {
  //sleep_ms(pDevice->m_interval);
  do
  {
  sleep_ms(0);
  } while ( (GetTimeStampMs() - last_timeStamp) <  pDevice->m_interval);
  last_timeStamp = GetTimeStampMs();

  currEntry = (SAMPLING_REGS*)(pDevice->m_sampling_arr + pDevice->m_sampling_head * (pDevice->m_no_sampling_regs + 1));
  currEntry->timeStamp = (DWORD)last_timeStamp;
  for (DWORD i = 0; i < pDevice->m_no_sampling_regs; i++)
  {
  DWORD val;
  if (pDevice->GetDeviceType() == MST_SWIFT)
  {
  pDevice->r2(pDevice->m_pRegsArr[i], val);
  currEntry->regArr[i] = val;
  }
  else if (pDevice->GetDeviceType() == MST_MARLON)
  {
  pDevice->r32(pDevice->m_pRegsArr[i], currEntry->regArr[i]);
  }
  }

  pDevice->m_sampling_head++;
  if (pDevice->m_sampling_head >= pDevice->m_maxSampling)
  {
  pDevice->m_sampling_head = 0;
  #ifdef WCLT_SAMPLING_TO_FILE_IS_SUPPORTED
  if (pDevice->m_transferMethod == 1)
  {
  // save the buffer to the disk;
  WriteFile(hSamplingFile, pDevice->m_sampling_arr, pDevice->oneSamplingSize * pDevice->m_maxSampling, &written, NULL);
  }
  #endif
  if (pDevice->m_transferMethod == 2)
  {
  pDevice->m_loops++;
  }

  }

  } while (!pDevice->SamplingThread.ShouldStop());
  #ifdef WCLT_SAMPLING_TO_FILE_IS_SUPPORTED
  if (pDevice->m_transferMethod == 1)
  {
  CloseHandle(hSamplingFile);
  }
  #endif
  LOG_MESSAGE_DEBUG(_T("Leaving"));
  }

  int CPciDeviceAccess::startSampling(
  DWORD* pRegsArr,
  DWORD regArrSize,
  DWORD interval,
  DWORD maxSampling,
  DWORD transferMethod)
  {
  m_no_sampling_regs = regArrSize;
  m_pRegsArr = new DWORD[regArrSize];
  memcpy(m_pRegsArr, pRegsArr, sizeof(DWORD)*regArrSize);
  m_interval = interval;
  m_maxSampling = maxSampling;
  m_transferMethod = transferMethod;

  // allocate the sampling buffer
  // each sampling required:
  // m_no_sampling_regs * sizeof(DWORD) + timestamp
  oneSamplingSize = m_no_sampling_regs * sizeof(DWORD) + sizeof(DWORD);
  m_sampling_arr = new DWORD[oneSamplingSize * m_maxSampling];
  m_get_sampling_arr = new DWORD[oneSamplingSize * m_maxSampling];

  // start the read tread
  SamplingThread.Start();
  sleep_ms(100);

  return 0;
  }

  int CPciDeviceAccess::stopSampling()
  {
  SamplingThread.Stop();

  // free the sampling buffers
  delete [] m_pRegsArr;
  delete [] m_sampling_arr;
  delete [] m_get_sampling_arr;
  return 0;
  }

  int CPciDeviceAccess::getSamplingData(DWORD** pDataSamples)
  {
  DWORD total_copy = 0;

  // get the head and tail position
  DWORD head = m_sampling_head;
  DWORD tail = m_sampling_tail;

  LOG_MESSAGE_DEBUG(_T("[head %X] [tail %X] [loops %d]"), head, tail, m_loops);

  memset(m_get_sampling_arr, 0, oneSamplingSize * m_maxSampling);

  // 0 - no new data. nothing to copy
  // -1 - error
  // 1 - copy from tail to head
  // 2 - copy from tail to end buffer and then from start buffer to head
  // 3 - overlapped. copy from head to head.
  //
  DWORD copy_type = 0;
  if (m_loops == 0)
  {
  if (head == tail)
  {
  // no new data
  copy_type = 0;
  }
  else if (head > tail)
  {
  // new data. copy from tail to head
  copy_type = 1;
  }
  else if (head < tail)
  {
  // error
  LOG_MESSAGE_ERROR(_T("ERROR: !!!!!!!! loop is ZERO [head %X] [tail %X]"), head, tail);
  copy_type = -1;
  }
  }
  else if (m_loops == 1)
  {
  if (head == tail)
  {
  // overlapped. copy head to head
  copy_type = 3;
  }
  else if (head > tail)
  {
  // overlapped. copy head to head
  copy_type = 3;
  }
  else if (head < tail)
  {
  // copy from tail to end buffer and then from start buffer to head
  copy_type = 2;
  }
  }
  else if (m_loops > 1)
  {
  // overlapped. copy head to head
  copy_type = 3;
  }

  DWORD srcPos;
  switch (copy_type)
  {
  case 1:
  {
  srcPos = oneSamplingSize * tail / sizeof(DWORD);
  memcpy(m_get_sampling_arr, &(m_sampling_arr[srcPos]), oneSamplingSize * (head - tail));
  total_copy = oneSamplingSize * (head - tail);
  }
  break;
  case 2:
  {
  // copy from tail to the end of the buffer
  srcPos = oneSamplingSize * tail / sizeof(DWORD);
  memcpy(m_get_sampling_arr, &(m_sampling_arr[srcPos]), oneSamplingSize * (m_maxSampling - tail));
  //
  DWORD nextIndex = (oneSamplingSize * (m_maxSampling - tail))/sizeof(DWORD);
  memcpy(&(m_get_sampling_arr[nextIndex]), m_sampling_arr, oneSamplingSize * head);
  total_copy = oneSamplingSize * (m_maxSampling - tail) + oneSamplingSize * head;
  }
  break;
  case 3:
  {
  // copy all the cyclic buffer, starting from the head
  // the head is pointing to the next place to put there new sample.
  // there is a racing condition because it is possible that just before
  // we will start coping a context switch will happened and the read
  // sampling thread will overwrite this area.
  // we are not locking this sharing resource for now
  memcpy(m_get_sampling_arr, &(m_sampling_arr[head]), oneSamplingSize * (m_maxSampling - head));
  // copy from the beginning till the head
  DWORD nextIndex = (oneSamplingSize * (m_maxSampling - head))/sizeof(DWORD);
  memcpy(&(m_get_sampling_arr[nextIndex]), &(m_sampling_arr[0]), oneSamplingSize * head);
  total_copy = oneSamplingSize * m_maxSampling;

  LOG_MESSAGE_WARN(_T("detect a loop [%d]. samples lost."), m_loops);
  }
  break;
  }

  m_loops = 0;
  m_sampling_tail = head;
  LOG_MESSAGE_DEBUG(_T("[m_sampling_tail %X] [total_copy %d]"), m_sampling_tail, total_copy);

  *pDataSamples = m_get_sampling_arr;
  return total_copy;
  }
*/
int CPciDeviceAccess::alloc_pmc(int num_of_descriptors, int size_of_descriptor){
    if (!isInitialized())
    {
        LOG_MESSAGE_ERROR(_T("alloc_pmc - Not initialized"));
        return -1;
    }
    if (m_close_state == 2)
    {
        Open();
    }
    if (!IoctlDev.IsOpened())
    {
        LOG_MESSAGE_ERROR(_T("alloc_pmc - Not opened"));
        return -1;
    }

    int data[2] = {num_of_descriptors, size_of_descriptor};
    int res = IoctlDev.DebugFS((char*)"pmccfg", data, 8, 0);

    if (res != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_ERROR(_T("Cannot open DebugFS for pmccfg: 0x%X"), res);
    }

    return res;
}
#ifdef _WINDOWS
int CPciDeviceAccess::send_wmi_cmd(SEND_RECEIVE_WMI* wmi)
{
    LOG_MESSAGE_INFO(_T("send_wmi_cmd: 0x%X"),  wmi->uCmdId);

    if (!isInitialized())
        return -1;


    lastError = IoctlDev.Ioctl(WILOCITY_IOCTL_SEND_WMI, wmi, sizeof(SEND_RECEIVE_WMI), NULL, 0);

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("WILOCITY_IOCTL_SEND_WMI failed. %X"), lastError);
        return lastError;
    }


    return 0;
}

int CPciDeviceAccess::recieve_wmi_event(SEND_RECEIVE_WMI* evt)
{
    LOG_MESSAGE_INFO(_T("recieve_wmi_event"));

    if (!isInitialized())
        return -1;


    lastError = IoctlDev.Ioctl( WILOCITY_IOCTL_RECEIVE_WMI, NULL, 0, evt, sizeof(SEND_RECEIVE_WMI));

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("WILOCITY_IOCTL_RECEIVE_WMI failed. %X"), lastError);
        return lastError;
    }

    return 0;
}
#endif //_WINDOWS

int CPciDeviceAccess::register_driver_mailbox_event(HANDLE* pMailboxEventHandle )
{
    LOG_MESSAGE_INFO(_T("register_driver_mailbox_event"));

    if (!isInitialized())
        return -1;
    if (!IoctlDev.IsOpened())
        return -1;

    REGISTER_EVENT req;
    memset(&req, 0, sizeof(REGISTER_EVENT));

    req.hEvent = *pMailboxEventHandle;

    lastError = IoctlDev.Ioctl(WILOCITY_IOCTL_REGISTER_WMI_RX_EVENT, &req, sizeof(REGISTER_EVENT), NULL, 0);

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("WILOCITY_IOCTL_REGISTER_WMI_RX_EVENT failed. 0x%X"), lastError);
        InternalCloseDevice();
        return -1;
    }

    return ERROR_SUCCESS;
}

int CPciDeviceAccess::register_device_unplug2(INT_PTR hEvent){
    LOG_MESSAGE_INFO(_T("register_device_unplug2"));

    WLCT_UNREFERENCED_PARAM(hEvent);

    if (!isInitialized())
        return -1;
    if (!IoctlDev.IsOpened())
        return -1;


#ifdef _WINDOWS
    DRIVER_REGISTER_EVENT req;
    memset(&req, 0, sizeof(DRIVER_REGISTER_EVENT));
    LOG_MESSAGE_INFO(_T("register_device_unplug2: <Event Pointer=0x%p>"), hEvent);
    req.UserEventHandle = hEvent;

    lastError = IoctlDev.Ioctl( WILOCITY_IOCTL_REGISTER_DEVICE, &req, sizeof(req), NULL, 0);


#endif //_WINDOWS

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("register_device_unplug2 failed. 0x%X"), lastError);
        InternalCloseDevice();
        return -1;
    }

    return ERROR_SUCCESS;
}

int CPciDeviceAccess::unregister_device_unplug2(){
    LOG_MESSAGE_INFO(_T("unregister_device_unplug2"));

    if (!isInitialized())
        return -1;
    if (!IoctlDev.IsOpened())
        return -1;


#ifdef _WINDOWS
    DRIVER_REGISTER_EVENT req;
    memset(&req, 0, sizeof(DRIVER_REGISTER_EVENT));
    LONG invalidPointer = -1;
    LOG_MESSAGE_INFO(_T("register_device_unplug2: <Event Pointer=0x%X>"), invalidPointer);
    req.UserEventHandle = invalidPointer;

    lastError = IoctlDev.Ioctl( WILOCITY_IOCTL_REGISTER_DEVICE, &req, sizeof(req), NULL, 0);


#endif //_WINDOWS

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("register_device_unplug2 failed. 0x%X"), lastError);
        InternalCloseDevice();
        return -1;
    }

    return ERROR_SUCCESS;
}



int CPciDeviceAccess::register_driver_device_event(INT_PTR hEvent,int deviceId, int eventId)
{
    LOG_MESSAGE_INFO(_T("register_driver_device_event"));
    WLCT_UNREFERENCED_PARAM(hEvent);
    WLCT_UNREFERENCED_PARAM(deviceId);
    WLCT_UNREFERENCED_PARAM(eventId);

    if (!isInitialized())
        return -1;
    if (!IoctlDev.IsOpened())
        return -1;


#ifdef _WINDOWS

    LOG_MESSAGE_INFO(_T("register_driver_device_event: <Event Pointer=0x%p> <deviceId=0x%x> <EventId=0x%x>"), hEvent, deviceId, eventId);

    // Our DLL assumed to be always compiled for 32bit application.
    // Since the driver excpects to recieve HANDLEs of size that IS platform speciefic,
    // we must dynamically find out if we are running on 64bit OS or now.
    // for more information, read remarks on WlcyPciAcssWmi.h file
    if (Util::Is64BitWindows())
    {
        LOG_MESSAGE_INFO(_T("register_driver_device_event Driver HANDLE problem WA, using 64bit struct"));
        EVENT_HANDLE_64BIT req;
        memset(&req, 0, sizeof(EVENT_HANDLE_64BIT));
        req.DeviceId = deviceId;
        req.EventId = eventId;
        req.hEvent_l = hEvent;
        req.hEvent_h = 0;

        lastError = IoctlDev.Ioctl( WILOCITY_IOCTL_REGISTER_EVENT, &req, sizeof(req), NULL, 0);


    }
    else
    {
        LOG_MESSAGE_INFO(_T("register_driver_device_event Driver HANDLE problem WA, using 32bit struct"));
        EVENT_HANDLE_32BIT req;
        memset(&req, 0, sizeof(EVENT_HANDLE_32BIT));

        req.DeviceId = deviceId;
        req.EventId = eventId;
        req.hEvent = hEvent;

        lastError = IoctlDev.Ioctl( WILOCITY_IOCTL_REGISTER_EVENT, &req, sizeof(req), NULL, 0);
    }

#endif //_WINDOWS

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("WILOCITY_IOCTL_REGISTER_EVENT failed. 0x%X"), lastError);
        InternalCloseDevice();
        return -1;
    }
    //do something with params
    (void)deviceId;
    (void)eventId;
    return ERROR_SUCCESS;
}
int CPciDeviceAccess::register_driver_device_events(INT_PTR hDnEvent, INT_PTR hUpEvent, INT_PTR hUnplugEvent, INT_PTR hSysAssertEvent)
{
    LOG_MESSAGE_INFO(_T("register_driver_device_events"));
    WLCT_UNREFERENCED_PARAM(hDnEvent);
    WLCT_UNREFERENCED_PARAM(hUpEvent);
    WLCT_UNREFERENCED_PARAM(hUnplugEvent);
    WLCT_UNREFERENCED_PARAM(hSysAssertEvent);

    if (!isInitialized())
        return -1;
    if (!IoctlDev.IsOpened())
        return -1;


#ifdef _WINDOWS

    LOG_MESSAGE_INFO(_T("register_driver_device_events: <hDnEvent=0x%p> <hUpEvent=0x%p> <hUnplugEvent=0x%p> <hSysAssertEvent=0x%p>"), hDnEvent, hUpEvent, hUnplugEvent, hSysAssertEvent );

    // Our DLL assumed to be always compiled for 32bit application.
    // Since the driver excpects to recieve HANDLEs of size that IS platform speciefic,
    // we must dynamically find out if we are running on 64bit OS or now.
    // for more information, read remarks on WlcyPciAcssWmi.h file
    if (Util::Is64BitWindows())
    {
        LOG_MESSAGE_INFO(_T("register_driver_device_events Driver HANDLES problem WA, using 64bit struct"));
        SHARED_EVENT_64BIT req;
        memset(&req, 0, sizeof(SHARED_EVENT_64BIT));

        req.hDnEvent_l = hDnEvent;
        req.hDnEvent_h = 0;
        req.hUpEvent_l = hUpEvent;
        req.hUpEvent_h = 0;
        req.unplugEvent_l = hUnplugEvent;
        req.hUpEvent_h = 0;
        req.sysAssertEvent_l = hSysAssertEvent;
        req.sysAssertEvent_h = 0;

        lastError = IoctlDev.Ioctl( IOCTL_OPEN_USER_MODE_EVENT, &req, sizeof(req), NULL, 0);


    }
    else
    {
        LOG_MESSAGE_INFO(_T("register_driver_device_events Driver HANDLES problem WA, using 32bit struct"));
        SHARED_EVENT_32BIT req;
        memset(&req, 0, sizeof(SHARED_EVENT_32BIT));

        req.hDnEvent = hDnEvent;
        req.hUpEvent = hUpEvent;
        req.unplugEvent = hUnplugEvent;
        req.sysAssertEvent = hSysAssertEvent;

        lastError = IoctlDev.Ioctl( IOCTL_OPEN_USER_MODE_EVENT, &req, sizeof(req), NULL, 0);
    }

#endif //_WINDOWS

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("IOCTL_OPEN_USER_MODE_EVENT failed. 0x%X"), lastError);
        InternalCloseDevice();
        return -1;
    }

    return ERROR_SUCCESS;
}

int CPciDeviceAccess::unregister_driver_device_events()
{
    LOG_MESSAGE_INFO(_T("unregister_driver_device_events"));

    if (!isInitialized())
        return -1;
    if (!IoctlDev.IsOpened())
        return -1;


#ifdef _WINDOWS

    LOG_MESSAGE_INFO(_T("unregister_driver_device_events with invalid handles"));

    // Our DLL assumed to be always compiled for 32bit application.
    // Since the driver excpects to recieve HANDLEs of size that IS platform speciefic,
    // we must dynamically find out if we are running on 64bit OS or now.
    // for more information, read remarks on WlcyPciAcssWmi.h file
    if (Util::Is64BitWindows())
    {
        LOG_MESSAGE_INFO(_T("register_driver_device_events Driver HANDLES problem WA, using 64bit struct"));
        SHARED_EVENT_64BIT req;
        memset(&req, 0, sizeof(SHARED_EVENT_64BIT));

        req.hDnEvent_l = -1;
        req.hDnEvent_h = -1;
        req.hUpEvent_l = -1;
        req.hUpEvent_h = -1;
        req.unplugEvent_l = -1;
        req.hUpEvent_h = -1;
        req.sysAssertEvent_l = -1;
        req.sysAssertEvent_h = -1;

        lastError = IoctlDev.Ioctl( IOCTL_OPEN_USER_MODE_EVENT, &req, sizeof(req), NULL, 0);


    }
    else
    {
        LOG_MESSAGE_INFO(_T("register_driver_device_events Driver HANDLES problem WA, using 32bit struct"));
        SHARED_EVENT_32BIT req;
        memset(&req, 0, sizeof(SHARED_EVENT_32BIT));

        req.hDnEvent = -1;
        req.hUpEvent = -1;
        req.unplugEvent = -1;
        req.sysAssertEvent = -1;

        lastError = IoctlDev.Ioctl( IOCTL_OPEN_USER_MODE_EVENT, &req, sizeof(req), NULL, 0);
    }

#endif //_WINDOWS

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("IOCTL_OPEN_USER_MODE_EVENT failed. 0x%X"), lastError);
        InternalCloseDevice();
        return -1;
    }

    return ERROR_SUCCESS;
}

int CPciDeviceAccess::setMpDriverLogLevels(ULONG logModulesEnableMsk, ULONG logModuleLevelsMsk[WILO_DRV_NUM_LOG_COMPS])
{
    LOG_MESSAGE_INFO(_T("setMpDriverLogLevels"));

    if (!isInitialized())
        return -1;
    if (!IoctlDev.IsOpened())
        return -1;

    DRIVER_LOGS_CFG req;
    memset(&req, 0, sizeof(DRIVER_LOGS_CFG));
    req.logModulesEnableMsk = logModulesEnableMsk;
    memcpy( req.logModuleLevelsMsk, logModuleLevelsMsk, (WILO_DRV_NUM_LOG_COMPS*4)) ;

    lastError = IoctlDev.Ioctl( WILOCITY_IOCTL_DRIVER_LOG_LEVELS_CFG, &req, sizeof(req), NULL, 0);

    if (lastError != WLCT_OS_ERROR_SUCCESS)
    {
        LOG_MESSAGE_WARN(_T("WILOCITY_IOCTL_DRIVER_LOG_LEVELS_CFG failed. 0x%X"), lastError);
        InternalCloseDevice();
        return -1;
    }

    return ERROR_SUCCESS;
}
