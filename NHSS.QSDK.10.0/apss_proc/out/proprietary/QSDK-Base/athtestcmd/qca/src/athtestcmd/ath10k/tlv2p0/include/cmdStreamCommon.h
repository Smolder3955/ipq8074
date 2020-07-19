#if !defined(__CMDSTREAM_COMMON_H)
#define __CMDSTREAM_COMMON_H

// test cmd sequence stream: version(4B) header(4B) length(4B) checksum(4B) headerDependentValue(4B) headerExtended(4B) cmdPayload(variable, 4xnB) 

// header
#define REPEAT_TEST_SHIFT 0x0
#define REPEAT_TEST_MASK  0x00FF
#define FAIL_RETRY_SHIFT  8
#ifndef FAIL_RETRY_MASK 
#define FAIL_RETRY_MASK   0x0F00
#endif
#define TIMEOUT_RETRY_SHIFT  12
#define TIMEOUT_RETRY_MASK   0xF000
#define CMD_STREAM_HEADER_REPEAT_TEST           0x00000001  //  headerDepValue lower B: value=0-> infinite, non-zero val is the repeated count
#define CMD_STREAM_HEADER_FAIL_RETRY            0x00000002  // headerDepValue higher B lower nible: value=0->beginning    , non-zero, value is the back steps
#define CMD_STREAM_HEADER_TIMEOUT_RETRY         0x00000004  // headerDepValue higher B higher nible: value=0->beginning    , non-zero, value is the back steps
#define CMD_STREAM_HEADER_CAL_W_TGTPWR          0x00000008  // header bit 0 -> tx gain CAL, 1 -> tgt power CAL  
#define CMD_STERAM_HEADER_EXTENDED_RESERVED     0x00000080

// headerDepValue

// header extended value
#define CMD_STREAM_HEADER_EXTENDED_BYTESTREAM_BYPASS     0x00000001 

// others
#define TEST_FAIL_RETRY_STEPS                 2

// version
#define CMD_STREAM_VER1                       1     
#define CMD_STREAM_VER2                       2     
#define CMD_STREAM_VER3                       3     
#if defined(CMD_STREAM_VER_LATEST)
#undef  CMD_STREAM_VER_LATEST
#endif
#define CMD_STREAM_VER_LATEST                 CMD_STREAM_VER3                     


// test flow cmd stream ------------ 
// test cmd sequence stream: version(1B) header(1B) length(2B) checksum(2B) headerDependentValue(2B) headerExtended(4B) cmdPayload(variable, 4xnB) 
// TBD?? intended to have a shorter version, but decided to keep the V1's
#if !defined(_TESTFLOW_DEFINED) 
typedef struct testFlowCmdStreamHeader {
    A_UINT32    cmdId;  // reserve this for compatibility with TCMD..
    A_UINT32    version;
    A_UINT32    header;
    A_UINT32    length; // payload length
    A_UINT32    checkSum;
    A_UINT32    headerDepValue;
    A_UINT32    headerExtended;
} __ATTRIB_PACK  _TESTFLOW_CMD_STREAM_HEADER; 

typedef struct testFlowCmdStream {
    _TESTFLOW_CMD_STREAM_HEADER cmdStreamHeader;
    A_UINT8                     payload[CMD_PAYLOAD_LEN_MAX];
} __ATTRIB_PACK  _TESTFLOW_CMD_STREAM;

#endif //#if !defined(_TESTFLOW_DEFINED) 

#endif //if !defined(__CMDSTREAM_COMMON_H)

