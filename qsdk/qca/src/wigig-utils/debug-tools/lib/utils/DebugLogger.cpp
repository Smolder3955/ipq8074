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
#include "DebugLogger.h"
#include "LoggerSupport.h"
#include "Thread.h"

#ifdef _WINDOWS
#define TRACING_REGISTRY_PATH _T("Software\\QualcommAtheros\\WIGIG\\Tracing")
#endif

using namespace std;

CDebugLogger g_Logger(_T("WLCT"));

CDebugLogger *g_pLogger = &g_Logger;

static const int WLCT_MAX_DEBUG_MSG_LEN = 512;

const TCHAR *g_szMessageType[] =
{
   _T("ERROR"),
   _T("WARN"),
   _T("INFO"),
   _T("DEBUG"),
   _T("TRACE"),
   _T("VERBOSE")
};

#ifdef _WINDOWS
void ReadSettingsThread(void *pLogger)
{
    LOG_MESSAGE_DEBUG(_T("start running"));
    CDebugLogger *pDebugLogger = (CDebugLogger*)pLogger;
    pDebugLogger->m_bExit = false;

    CRegKey regCofig;
    if ( ERROR_SUCCESS == regCofig.Open(HKEY_LOCAL_MACHINE, TRACING_REGISTRY_PATH, KEY_READ))
    {
        // key found, reset settings
//        pDebugLogger->SetDefaultSettings();

        DWORD dwTraceLevel;
        if ( ERROR_SUCCESS == regCofig.QueryDWORDValue( _T("TraceLevel"), dwTraceLevel ) )
        {
            pDebugLogger->m_eTraceLevel = (COsDebugLogger::E_LOG_LEVEL)dwTraceLevel;
        }

        DWORD dwLogToFile;
        if ( ERROR_SUCCESS == regCofig.QueryDWORDValue( _T("LogToFile"), dwLogToFile ) )
        {
            TCHAR szPath[MAX_PATH] = { 0 };
            ULONG ulChars = MAX_PATH;
            if ( ERROR_SUCCESS == regCofig.QueryStringValue( _T("LogFilePath"), szPath, &ulChars ) )
            {
                pDebugLogger->m_sLogFilePath = szPath;
                //m_bLogToFile   = TRUE;
            }
        }
    }

    // Create an event.
    pDebugLogger->hNotifyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (pDebugLogger->hNotifyEvent == NULL)
    {
        _tprintf(TEXT("Error in CreateEvent (%d).\n"), GetLastError());
        return;
    }

    LONG lErrorCode = 0;// regCofig.NotifyChangeKeyValue(FALSE, REG_NOTIFY_CHANGE_LAST_SET, pDebugLogger->hNotifyEvent, TRUE);
    if (lErrorCode != ERROR_SUCCESS)
    {
        //_tprintf(TEXT("Error in RegNotifyChangeKeyValue (%d).\n"), lErrorCode);
        return;
    }

    do
    {
        if (WaitForSingleObject(pDebugLogger->hNotifyEvent, INFINITE) == WAIT_FAILED)
        {
            _tprintf(TEXT("Error in WaitForSingleObject (%d).\n"), GetLastError());
        }

        DWORD dwTraceLevel;
        if ( ERROR_SUCCESS == regCofig.QueryDWORDValue( _T("TraceLevel"), dwTraceLevel ) )
        {
            pDebugLogger->m_eTraceLevel = (COsDebugLogger::E_LOG_LEVEL)dwTraceLevel;
        }

    } while (!pDebugLogger->m_bExit);

    CloseHandle(pDebugLogger->hNotifyEvent);
    regCofig.Close();
}
#endif

CDebugLogger::CDebugLogger(const TCHAR *tag, bool logToConsole) :
  COsDebugLogger(logToConsole)
{
    m_sTag = tag;
    SetDefaultSettings();

//     ReadTraceSettings( TRACING_REGISTRY_PATH );
//
//     // override settings
//     CString sRegKeyPath;
//     sRegKeyPath.Format(_T("%s\\%s"), TRACING_REGISTRY_PATH, lpszTag );
//
//     ReadTraceSettings(lpszTag);

    // start the read tread
#ifdef _WINDOWS
    m_hThread = (HANDLE)_beginthread(ReadSettingsThread, 0, this);
#endif
}

CDebugLogger::~CDebugLogger(void)
{
#ifdef _WINDOWS
    m_bExit = true;
    SetEvent(hNotifyEvent);
    if (WaitForSingleObject(m_hThread, 1000) == WAIT_TIMEOUT)
    {
        TerminateThread(m_hThread, 1);
    }
#endif
}

void CDebugLogger::SetDefaultSettings(void)
{
    m_bLogToFile   = false;
    m_eTraceLevel = E_LOG_LEVEL_DEBUG;
    m_sLogFilePath = _T("");
}

#ifdef _WINDOWS
void CDebugLogger::ReadTraceSettings( LPCTSTR lpszRegistryPath )
{
    CRegKey regCofig;
    if ( ERROR_SUCCESS == regCofig.Open(HKEY_LOCAL_MACHINE,    lpszRegistryPath, KEY_READ))
    {
        // key found, reset settings
        SetDefaultSettings();

        DWORD dwTraceLevel;
        if ( ERROR_SUCCESS == regCofig.QueryDWORDValue( _T("TraceLevel"), dwTraceLevel ) )
        {
            m_eTraceLevel = (COsDebugLogger::E_LOG_LEVEL)dwTraceLevel;
        }

        DWORD dwLogToFile;
        if ( ERROR_SUCCESS == regCofig.QueryDWORDValue( _T("LogToFile"), dwLogToFile ) )
        {
            TCHAR szPath[MAX_PATH] = { 0 };
            ULONG ulChars = MAX_PATH;
            if ( ERROR_SUCCESS == regCofig.QueryStringValue( _T("LogFilePath"), szPath, &ulChars ) )
            {
                m_sLogFilePath = szPath;
//                m_bLogToFile   = TRUE;
            }
        }
    }
}
#endif

void CDebugLogger::LogGlobalDebugMessage(CDebugLogger::E_LOG_LEVEL eTraceLevel,
  S_LOG_CONTEXT *pCtxt, const TCHAR *fmt, ...)
{
  if (!g_pLogger)
  {
    return;
  }

  va_list args;
  va_start(args, fmt);
  g_pLogger->LogDebugMessageV(eTraceLevel, pCtxt, fmt, args);
  va_end(args);
}

DWORD CDebugLogger::GetGlobalTraceLevel()
{
    if (!g_pLogger)
    {
        return 0;
    }

    return g_pLogger->GetGlobalTraceLevel();
}

void CDebugLogger::LogDebugMessage( CDebugLogger::E_LOG_LEVEL eTraceLevel,
  S_LOG_CONTEXT *pCtxt, const TCHAR *fmt, ...)
{
  va_list args;

  if (eTraceLevel > m_eTraceLevel)
  {
    return;
  }

  va_start(args, fmt);
  LogDebugMessageV(eTraceLevel, pCtxt, fmt, args);
  va_end(args);
}

void CDebugLogger::LogDebugMessageV( CDebugLogger::E_LOG_LEVEL eTraceLevel,
  S_LOG_CONTEXT *pCtxt, const TCHAR *fmt, va_list args)
{
  TCHAR msg[WLCT_MAX_DEBUG_MSG_LEN] = {0};
  int bytes_written = 0;

    if (eTraceLevel > m_eTraceLevel)
    {
        return;
    }

  bytes_written = _stprintf_s(msg, WLCT_MAX_DEBUG_MSG_LEN,
    _T("%s(%d): %s: [%s]; pid:%d; tid:%d; err=%d; f=[%s]; "),
     (pCtxt ? pCtxt->funcName : _T("unknown")),
    (pCtxt ? pCtxt->line : 0), m_sTag.c_str(), g_szMessageType[eTraceLevel],
    CWlctThread::getCurrentProcessID(), CWlctThread::getCurrentThreadID(),
    WlctGetLastError(), (pCtxt ? pCtxt->funcName : _T("unknown")));

  bytes_written += _vsntprintf(msg + bytes_written,
    WLCT_MAX_DEBUG_MSG_LEN - bytes_written, fmt, args);

  _stprintf_s(msg + bytes_written, WLCT_MAX_DEBUG_MSG_LEN - bytes_written, _T("\n"));

  print(msg);


  if (m_bLogToFile)
  {
    FILE *fp = 0;
    fp = _tfopen(m_sLogFilePath.c_str(), _T("a"));
    if (NULL != fp)
    {
      fprintf(fp, "%s", msg);
      fclose(fp);
    }
  }
}

