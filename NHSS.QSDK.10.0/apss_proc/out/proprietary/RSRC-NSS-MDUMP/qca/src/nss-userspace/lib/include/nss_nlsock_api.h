/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __NSS_NLSOCK_API_H__
#define __NSS_NLSOCK_API_H__

/**
 * @addtogroup libnss_nl
 * @{
 */

/**
 * @file nss_nlsock_api.h
 * 	This file declares the NSS NL Socket API(s) for direct use.
 *
 * Note: Use these API(s) only if there are no helpers available for the
 * 	 specific family
 */

/**
 * @brief NSS NL socket context
 */
struct nss_nlsock_ctx {
	/* Public, caller must populate using helpers */
	const char *family_name;		/**< family name */
	void *user_ctx;				/**< socket user's context */

	/* Private, maintained by the library */
	pthread_t thread;			/**< response sync */
	pthread_spinlock_t lock;		/**< context lock */
	int ref_cnt;				/**< references to the socket */

	struct nl_sock *nl_sk;			/**< Linux NL socket */
	struct nl_cb *nl_cb;			/**< NSS NL callback context */

	pid_t pid;				/**< pid associated with the socket */
	int family_id;				/**< family identifier */
	int grp_id;				/**< group indentifier */
};

#define nss_nlsock_log_error(arg, ...) printf("NSS_NLERROR:"arg, ##__VA_ARGS__)
#define nss_nlsock_log_info(arg, ...) printf("NSS_NLINFO:"arg, ##__VA_ARGS__)

/**
 * @brief helper for setting the family name
 *
 * @param ctx[IN] socket context
 * @param name[IN] family name
 */
static inline void nss_nlsock_set_family(struct nss_nlsock_ctx *sock, const char *name)
{
	sock->family_name = name;
}

/**
 * @brief helper for setting the user context
 *
 * @param ctx[IN] socket context
 * @param user[IN] user context
 */
static inline void nss_nlsock_set_user_ctx(struct nss_nlsock_ctx *sock, void *user)
{
	sock->user_ctx = user;
}

/**
 * @brief extract the NSS NL message data
 *
 * @param msg[IN] NL message
 *
 * @return start of NSS NL message
 */
static inline void *nss_nlsock_get_data(struct nl_msg *msg)
{
	struct genlmsghdr *genl_hdr = nlmsg_data((nlmsg_hdr(msg)));

	return genlmsg_data(genl_hdr);
}

/**
 * @brief open the NSS NL family socket
 *
 * @param ctx[IN] socket context, to be allocated by the caller
 * @param cb[IN] callback function for response
 *
 * @return status of the operation
 */
int nss_nlsock_open(struct nss_nlsock_ctx *sock, nl_recvmsg_msg_cb_t cb);

/**
 * @brief close the NSS NL family socket
 *
 * @param ctx[IN] socket context
 */
void nss_nlsock_close(struct nss_nlsock_ctx *sock);

/**
 * @brief send a NSS NL message asynchronously
 *
 * @param ctx[IN] socket context
 * @param cm[IN] common message header
 * @param data[IN] message data
 *
 * @return status of the send operation
 *
 * @note If, the underlying entity wants to send responses then
 * 	 it will be delivered asynchronously
 */
int nss_nlsock_send(struct nss_nlsock_ctx *sock, struct nss_nlcmn *cm, void *data);

/**
 * @brief subsribe for listening to NSS NL event data, when a event arrives
 * 	  it will be delivered using through the callback function
 *
 * @param ctx[IN] socket context
 * @param grp_name[IN] NSS NL group name
 *
 * @return status of the subscription
 */
int nss_nlsock_listen(struct nss_nlsock_ctx *sock, char *grp_name);

/**}@*/
#endif /* !__NSS_NLSOCK_API_H__*/
