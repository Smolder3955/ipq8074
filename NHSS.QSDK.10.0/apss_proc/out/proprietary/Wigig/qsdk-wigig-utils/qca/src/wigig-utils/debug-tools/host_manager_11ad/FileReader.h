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

#ifndef _FILE_READER_H_
#define _FILE_READER_H_

#include <stdio.h>
#include <string>

// *************************************************************************************************

// Reads a file from the file system and exports its content to a buffer provided by the caller.
// If the file is larger than a provide bugffer, it is delivered by chunks of the buffer size.
// The last chunk may occupy less than the whole buffer. It's a caller's responsibility to allocate
// the buffer and call the FileReader API untill the file is fully exported.

class FileReader
{
public:

    explicit FileReader(const char* pFileName);
    ~FileReader();

    size_t ReadChunk(char* pBuf, size_t availableSpace);

    bool IsCompleted() const { return m_ReadTillNow == m_FileSize; }
    bool IsError() const { return m_IsError; }

    size_t ReadTillNow() const { return m_ReadTillNow; }
    size_t GetFileSize() const { return m_FileSize; }

private:

    bool CanReadFromFile(char* pBuf, size_t availableSpace);

    const std::string m_FileName;        // File name - cached for tracing
    FILE* m_pFile;                       // File Handler - open for read
    size_t m_FileSize;                   // File Size in bytes
    size_t m_ReadTillNow;                // Bytes read till now
    bool m_IsCompleted;                  // Set to true when OEF is reached
    bool m_IsError;                      // Error flag

};


#endif    // _FILE_READER_H_
