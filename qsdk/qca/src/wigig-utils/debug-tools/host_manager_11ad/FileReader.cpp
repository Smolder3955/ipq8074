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

#include "FileReader.h"
#include "DebugLogger.h"

#include <cstring>
#include <cerrno>
#include <stdio.h>

// *************************************************************************************************

FileReader::FileReader(const char* pFileName)
    : m_FileName(pFileName ? pFileName : "")
    , m_pFile(NULL)
    , m_FileSize(0)
    , m_ReadTillNow(0)
    , m_IsCompleted(false)
    , m_IsError(false)
{
    if (NULL == pFileName)
    {
        LOG_ERROR << "Invalid file reader created - No file name provided" << std::endl;
        return;
    }

    LOG_VERBOSE << "Opening file reader for: " << m_FileName << std::endl;
    m_pFile = fopen(pFileName, "rb");

    if (NULL == m_pFile)
    {
        int lastErrno = errno;
        LOG_ERROR << "Error opening file."
                  << " Name: " << pFileName
                  << " Error: " << lastErrno
                  << " Message: " << strerror(lastErrno)
                  << std::endl;
        return;
    }

    fseek(m_pFile, 0, SEEK_END);
    m_FileSize = ftell(m_pFile);
    rewind(m_pFile);
}

// *************************************************************************************************

FileReader::~FileReader()
{
    if (m_pFile)
    {
        LOG_VERBOSE << "Closing the file: " << m_FileName << std::endl;
        fclose(m_pFile);
        m_pFile = NULL;
    }
}

// *************************************************************************************************

bool FileReader::CanReadFromFile(char* pBuf, size_t availableSpace)
{
    if (!pBuf)
    {
        LOG_ERROR << "Cannot read from file " << m_FileName << ": " << "No buffer is provided" << std::endl;
        return false;
    }

    if (0 == availableSpace)
    {
        LOG_ERROR << "Cannot read from file " << m_FileName << ": " << "No buffer space is provided" << std::endl;
        return false;
    }

    if (NULL == m_pFile)
    {
        LOG_ERROR << "Cannot read from file " << m_FileName << ": " << "No file handle is available" << std::endl;
        return false;
    }

    if (m_IsCompleted)
    {
        LOG_ERROR << "Unexpected read from file " << m_FileName << ": " << "EoF is reached" << std::endl;
        return false;
    }

    if (m_IsError)
    {
        LOG_ERROR << "Unexpected read from file " << m_FileName << ": " << "Error occured" << std::endl;
        return false;
    }

    return true;

}

// *************************************************************************************************

size_t FileReader::ReadChunk(char* pBuf, size_t availableSpace)
{
    LOG_VERBOSE << "Reading a chunk."
        << " File Name: " << m_FileName
        << " File Size: " << m_FileSize << "B"
        << " Read till now: " << m_ReadTillNow << "B"
        << " Buffer: " << availableSpace << "B"
        << " Completed: " << BoolStr(m_IsCompleted)
        << " Error: " << BoolStr(m_IsError)
        << std::endl;

    if (false == CanReadFromFile(pBuf, availableSpace))
    {
        LOG_ERROR << "Cannot read from file: " << m_FileName << " Check previous errors/status" << std::endl;
        m_IsError = true;
        return 0;
    }

    // Read up to availableSpace. Reading less means either EoF is reached or read error occured
    size_t readBytes = fread(pBuf, 1, availableSpace, m_pFile);
    m_ReadTillNow += readBytes;

    if (feof(m_pFile))
    {
        LOG_VERBOSE << "EoF reached" << std::endl;
        m_IsCompleted = true;
    }

    if (ferror(m_pFile))
    {
        int lastErrno = errno;
        m_IsError = true;
        LOG_ERROR << "Cannot read file"
                  << " Name: " << m_FileName
                  << " Error: " << lastErrno
                  << " Message:" << strerror(lastErrno)
                  << std::endl;
    }

    LOG_VERBOSE << "Got a chunk."
                << " File Name: " << m_FileName
                << " File Size: " << m_FileSize << "B"
                << " Read till now: " << m_ReadTillNow << "B"
                << " Buffer: " << availableSpace << "B"
                << " Completed: " << BoolStr(m_IsCompleted)
                << " Error: " << BoolStr(m_IsError)
                << std::endl;

    return readBytes;

}
