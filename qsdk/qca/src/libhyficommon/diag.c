// vim: set et sw=4 sts=4 cindent:
/*
 * @File: diag.c
 *
 * @Abstract: LBD and HYD diagnostic logging
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * @@-COPYRIGHT-END-@@
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>

#ifdef GMOCK_UNIT_TESTS
#include "strlcpy.h"
#endif

#include <dbg.h>
#include <cmd.h>
#include <diag.h>

#define LOG_IP_SIZE 32

struct diagConfig {
    DIAG_BOOL enableLog;
    char logServerIP[LOG_IP_SIZE];
    u_int32_t logServerPort;
};

struct diagState {
    struct diagConfig config;

    int socketFd;
    int lastErrno;
    struct sockaddr_in serverAddr;

    u_int8_t msgBuf[LOG_BUFFER_SIZE];
    size_t msgBufOffset;

    struct dbgModule *dbgModule;
};

/*
 * diagS -- the instance of global information
 */
/*private*/ struct diagState diagS;

#define DIAGSERVERIP "192.168.1.2"
#define DIAGSERVERPORT 5555

// Forward decls of private functions
static DIAG_STATUS diagOpenSocket(void);
static DIAG_STATUS diagInitServerAddr(struct sockaddr_in *serverAddr);
static void diagResetBuffer(void);
static void diagCloseSocket(void);

// ====================================================================
// Public API
// ====================================================================

DIAG_STATUS diag_init(void) {
    diagS.dbgModule = dbgModuleFind("diag");
    diagS.socketFd = -1;
    diagS.lastErrno = 0;
    diagS.msgBufOffset = 0;

    diagS.config.enableLog = 0;
    strlcpy(diagS.config.logServerIP, DIAGSERVERIP,
            sizeof(diagS.config.logServerIP));
    diagS.config.logServerPort = DIAGSERVERPORT;

    dbgf(diagS.dbgModule, DBGDEBUG,
                 "%s: %d", __func__, diagS.config.enableLog);
    dbgf(diagS.dbgModule, DBGDEBUG,
                 "%s: %s", __func__, diagS.config.logServerIP);
    dbgf(diagS.dbgModule, DBGDEBUG,
                 "%s: %d", __func__, diagS.config.logServerPort);
    return DIAG_OK;
}

void diag_enableLog(DIAG_BOOL value) {
    if (value && !diagS.config.enableLog) {
        if (DIAG_NOK == diagOpenSocket()) {
            return;
        }
    } else if (!value && diagS.config.enableLog) {
        diagCloseSocket();
    }
    diagS.config.enableLog = value ? DIAG_TRUE : DIAG_FALSE;
    dbgf(diagS.dbgModule, DBGDEBUG,
                 "%s: %d", __func__, diagS.config.enableLog);
}

void diag_setServerIP(const char *serverip) {
    strlcpy(diagS.config.logServerIP, serverip,
            sizeof(diagS.config.logServerIP));
    dbgf(diagS.dbgModule, DBGDEBUG,
                 "%s: %s", __func__, diagS.config.logServerIP);
}

void diag_setServerPort(int serverport) {
    diagS.config.logServerPort = serverport;
    dbgf(diagS.dbgModule, DBGDEBUG,
                 "%s: %d", __func__, diagS.config.logServerPort);
}

void diag_write(const void* buffer, size_t buflen) {
    if (diagS.config.enableLog) {
        if (diagS.msgBufOffset + buflen >= LOG_BUFFER_SIZE) {
            diagResetBuffer();
            return;
        }

        memcpy(diagS.msgBuf + diagS.msgBufOffset, buffer, buflen);
        diagS.msgBufOffset += buflen;
    }
}

void diag_finishEntry(void) {
    if (diagS.config.enableLog) {
        if (diagS.msgBufOffset == 0) {
            return;
        }

        if (diagS.socketFd < 0) {
            diagCloseSocket();
            return;
        }

        if (sendto(diagS.socketFd, diagS.msgBuf,
                   diagS.msgBufOffset, MSG_DONTWAIT,
                   (const struct sockaddr *) &diagS.serverAddr,
                   sizeof(diagS.serverAddr)) < 0) {
            if (errno != diagS.lastErrno) {
                diagS.lastErrno = errno;
            }
        }

        diagResetBuffer();
    }
}

void diag_parseCmd(char *Cmd)
{
#define CMD_SIZE 50
    char str[CMD_SIZE], *B, *str_tmp;
    int len = 0;
    if (!Cmd)
        return;

    dbgf(diagS.dbgModule, DBGDEBUG,
                 "%s: Command: %s", __func__, Cmd);

    memset(str, 0, CMD_SIZE);
    if( NULL != (B = (void *) strtok_r(Cmd, " ", &str_tmp)) )
    {
        if (!strncmp(B,"DiagServerIP", 12)) {
            if( NULL != (B = (void *) strtok_r(NULL, " ", &str_tmp)) ) {
                diag_setServerIP(B);
            }
        } else if (!strncmp(B, "DiagServerPort", 14)) {
            if( NULL != (B = (void *) strtok_r(NULL, " ", &str_tmp)) ) {
                diag_setServerPort(atoi(B));
            }
        } else if (!strncmp(B, "DiagEnable", 10)) {
            if( NULL != (B = (void *) strtok_r(NULL, " ", &str_tmp))) {
                diag_enableLog(atoi(B));
            }
        } else if (diagS.config.enableLog) {
            if (!strncmp(B, "dbg", 3)) {
                if( NULL != (B = (void *) strtok_r(NULL, " ", &str_tmp)) ) {
                    if (!strncmp(B,"level", 5)) {
                        B = (void *) strtok_r(NULL, " ", &str_tmp);
                        while (B) {
                            snprintf(str + len, CMD_SIZE - len, "%s ", B);
                            len = strlen(str);
                            B = (void *) strtok_r(NULL, " ", &str_tmp);
                        }
                        dbgModuleLevelFromBuf(str);
                    }
                }
            }  else {
                while (B) {
                    snprintf(str + len, CMD_SIZE - len, "%s ", B);
                    len = strlen(str);
                    B = (void *) strtok_r(NULL, " ", &str_tmp);
                }
                cmdMenuDiag(str);
            }
        }
    }
#undef CMD_SIZE
}

// ====================================================================
// Private API
// ====================================================================

/**
 * @brief Open the socket that will be used to send diagnostic logging
 *        records.
 */
static DIAG_STATUS diagOpenSocket(void) {
    struct sockaddr_in serverAddr;
    if (diagInitServerAddr(&serverAddr) == DIAG_NOK) {
        return DIAG_NOK;
    }
    diagS.serverAddr = serverAddr;

    diagS.socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (diagS.socketFd == -1) {
        return DIAG_NOK;
    }

    return DIAG_OK;
}

/**
 * @brief Initialize the address to use as the destination for diagnostic
 *        logging messages.
 *
 * @pre the logServerIP and logServerPort members of the configuration have
 *      already been populated
 *
 * @return DIAG_OK if the address was resolved successfully; otherwise DIAG_NOK
 */
static DIAG_STATUS diagInitServerAddr(struct sockaddr_in *serverAddr) {
    memset(serverAddr, 0, sizeof(*serverAddr));
    if (inet_aton(diagS.config.logServerIP,
                  &serverAddr->sin_addr) == 0) {
        return DIAG_NOK;
    }
    serverAddr->sin_family = AF_INET;
    serverAddr->sin_port = htons(diagS.config.logServerPort);

    return DIAG_OK;
}

/**
 * @brief Reset the internal buffer to prepare it for the next log.
 */
static void diagResetBuffer(void) {
    diagS.msgBufOffset = 0;
}

/**
 * @brief Close the socket that was being used for diagnostic logging.
 */
static void diagCloseSocket(void) {
    if (diagS.socketFd < 0) {
        // nothing to do as socket is invalid
        return;
    }

    close(diagS.socketFd);
    diagS.socketFd = -1;
}
