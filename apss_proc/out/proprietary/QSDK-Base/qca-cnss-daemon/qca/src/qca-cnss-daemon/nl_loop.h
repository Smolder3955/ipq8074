/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */

#ifndef __NL_LOOP_H_
#define __NL_LOOP_H_

#include <sys/socket.h>
#include "wlan_msg.h"

/* type: indication type
 * user_data: a pointer to the data that the handler will use
 * len: the length of the user_data
 */
typedef int (*nl_loop_ind_handler)(int type, void *user_data, int len);
#ifdef CONFIG_WLAN_MSG_SVC
int nl_loop_init(void);
int nl_loop_deinit(void);
int nl_loop_register(int ind, nl_loop_ind_handler ind_handler, void *user_data);
int nl_loop_unregister(int ind);

int nl_loop_terminate(void);
int nl_loop_run(void);
#else
extern int terminate_application;
static int nl_loop_register(int ind, nl_loop_ind_handler ind_handler,
							void *user_data)
{ return 0; }
static int nl_loop_unregister(int ind) { return 0; }
static int nl_loop_terminate(void) { return 0; }
static int nl_loop_init(void) { return 0; }
static int nl_loop_deinit(void) { return 0; }
static int nl_loop_run(void) { return 0; }
#endif /* CONFIG_WLAN_MSG_SVC */
#endif  /* __NL_LOOP_H_ */
