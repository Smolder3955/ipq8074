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

#ifndef _PMC_DATA_H_
#define _PMC_DATA_H_

#include "OperationStatus.h"

#include <string>
#include <iostream>

// *************************************************************************************************

// Locates a PMC data file path according to its ID.
class PmcFileLocator
{
public:

    explicit PmcFileLocator(int fileId);

    int GetFileId() const { return m_FileId; }
    const char* GetDataFileName() const { return m_DataFileName.c_str(); }
    const char* GetRingFileName() const { return m_RingFileName.c_str(); }
    bool DataFileExists() const { return FileExists(m_DataFileName); }
    bool RingFileExists() const { return FileExists(m_RingFileName); }

    static const char* GetDirectory() { return s_pDirectory; }

private:

    bool FileExists(const std::string& fileName) const;

    static const char* const s_pDirectory;

    const int m_FileId;        // File ID (expected to be unique)
    std::string m_DataFileName;
    std::string m_RingFileName;

};

std::ostream& operator<<(std::ostream& os, const PmcFileLocator& pmcFilesLocator);

// *************************************************************************************************

// Creates a PMC data file according to a provided ID.
class PmcFileWriter
{
public:

    PmcFileWriter(int fileId, const std::string& debugFsPath);
    OperationStatus WriteFiles() const;

private:

    OperationStatus MeetWritePrerequisites() const;
    OperationStatus WriteFile(const char* srcFileName, const char* dstFileName) const;

    const PmcFileLocator m_PmcFilesLocator;
    std::string m_SrcPmcDataFilePath;
    std::string m_SrcPmcRingFilePath;
    bool m_driverSupportsRingFlush;
};


#endif    // _PMC_DATA_H_
