#ifndef ACFG_WSUPP_EVENT_H
#define ACFG_WSUPP_EVENT_H

/**
 * @brief open_event_socket - open an async event socket
 * @param[in] mctx opaque handle to the module
 * @param[in] name - interface name
 * 
 * @return  status of operation
 */
acfg_status_t acfg_open_event_socket(acfg_wsupp_hdl_t *mctx, char *name);

/**
 * @brief close_event_socket - close an async event socket
 * @param[in] mctx opaque handle to the module
 * 
 */
void acfg_close_event_socket(acfg_wsupp_hdl_t *mctx);

/**
 * @brief For socket implementations, get the socket fd which
 *        can be used in the caller's poll/select loop. Once the
 *        fd shows data is available, the user calls
 *        acfg_wsupp_event_read_socket to read the event.
 * @param[in] mctx module context
 * 
 * @return fd handle or 0 if acfg_open_event_socket failed.
 */
int acfg_wsupp_event_get_socket_fd(acfg_wsupp_hdl_t *mctx);

/**
 * @brief Reads pending events from a socket. The caller should
 *        have made sure the socket has data available to read.
 *        This function calls one of the functions in the event
 *        table.
 * @param[in] mctx module context
 * @param[in] socket_fd to read from.
 * @return  structure ptr with results of operation, 
 *           NULL if nothing to read or on failure
 */
acfg_eventdata_t * acfg_wsupp_event_read_socket(acfg_wsupp_hdl_t *mctx,
        int socket_fd);
#endif
