// vim: set et sw=4 sts=4 cindent:
/*
 * @File: diag.h
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
 */

#ifndef diag__h
#define diag__h

#if defined(__cplusplus)
extern "C" {
#endif

#define LOG_BUFFER_SIZE 1024

/*
 * DIAG_STATUS - Diagnostics API return values:
 *
 * DIAG_OK: Function succeeded
 * DIAG_NOK: Function failed
 *
 */
typedef enum
{
    DIAG_OK = 0,
    DIAG_NOK = -1

} DIAG_STATUS;

/*
 * DIAG_BOOL - Diagnostics boolean return values: FALSE & TRUE
 */
typedef enum
{
    DIAG_FALSE = 0,
    DIAG_TRUE = !DIAG_FALSE

} DIAG_BOOL;

/**
 * @brief Initialize the diagnostic logging module, including obtaining the
 *        default configuration.
 */
DIAG_STATUS diag_init(void);

/**
 * @brief Write an arbitrary buffer into the log message currently being
 *        constructed.
 *
 * @param [in] buffer  the data to log
 * @param [in] buflen  the number of bytes to log
 */
void diag_write(const void *buffer, size_t buflen);

/**
 * @brief Complete the in progress log entry, sending it over the network.
 */
void diag_finishEntry(void);

/**
 * @brief Parse / Format Commands and apply to diag or dbg or cmd modules.
 */
void diag_parseCmd(char *Cmd);

#if defined(__cplusplus)
}
#endif

#endif
