/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/*
 * @file netlink socket handler
 */

#include <nss_nlbase.h>
#include <nss_nlsock_api.h>

/*
 * nss_nlsock_init_sock()
 * 	initialize the socket and callback
 */
static int nss_nlsock_init_sock(struct nss_nlsock_ctx *sock, nl_recvmsg_msg_cb_t cb)
{
	pthread_spin_init(&sock->lock, PTHREAD_PROCESS_PRIVATE);

	/*
	 * Create netlink socket
	 */
	sock->nl_sk = nl_socket_alloc();
	if (!sock->nl_sk) {
		nss_nlsock_log_error("%d:failed to alloc socket for family(%s)\n", sock->pid, sock->family_name);
		return ENOMEM;
	}

	/*
	 * create callback
	 */
	sock->nl_cb = nl_cb_alloc(NL_CB_CUSTOM);
	if (!sock->nl_cb) {
		nss_nlsock_log_error("%d:failed to alloc callback for family(%s)\n",sock->pid, sock->family_name);
		nl_socket_free(sock->nl_sk);
		return ENOMEM;
	}

	nl_cb_set(sock->nl_cb, NL_CB_VALID, NL_CB_CUSTOM, cb, sock);
	sock->ref_cnt = 1;

	return 0;
}

/*
 * nss_nlsock_get_sock()
 * 	get the NL socket and increment the reference count
 */
static inline struct nl_sock *nss_nlsock_get_sock(struct nss_nlsock_ctx *sock, int *ref_cnt)
{
	pthread_spin_lock(&sock->lock);

	/*
	 * if, there are no references it means that the socket
	 * is freed
	 */
	if (*ref_cnt == 0) {
		pthread_spin_unlock(&sock->lock);
		return NULL;
	}

	(*ref_cnt)++;

	pthread_spin_unlock(&sock->lock);

	return sock->nl_sk;
}

/*
 * nss_nlsock_put_sock()
 * 	decrement the reference count and free socket resources if '0'
 */
static inline void nss_nlsock_put_sock(struct nss_nlsock_ctx *sock, int *ref_cnt)
{
	pthread_spin_lock(&sock->lock);

	assert(*ref_cnt);

	(*ref_cnt)--;

	if ((*ref_cnt)) {
		pthread_spin_unlock(&sock->lock);
		return;
	}

	nl_cb_put(sock->nl_cb);

	nl_socket_free(sock->nl_sk);

	sock->nl_sk = NULL;

	pthread_spin_unlock(&sock->lock);
}

/*
 * nss_nlsock_sync()
 *	drain out pending responses from the netlink socket
 *
 * Note: this thread is woken up whenever someone sends a NL message.
 * Thread blocks on the socket for data and comes out when
 * 1. Data is available on the socket
 * 2. or a callback has returned NL_STOP
 * 3. or the socket is configured for non-blocking
 */
void *nss_nlsock_sync(void *arg)
{
	struct nss_nlsock_ctx *sock = (struct nss_nlsock_ctx *)arg;
	struct nl_sock *nl_sk;
	assert(sock);

	/*
	 * drain responses on the socket
	 */
	for (;;) {

		/*
		 * if, socket is freed then break out
		 */
		nl_sk = nss_nlsock_get_sock(sock, &sock->ref_cnt);
		if (!nl_sk) {
			break;
		}

		/*
		 * get or block for pending messages
		 */
	        nl_recvmsgs(nl_sk, sock->nl_cb);
		nss_nlsock_put_sock(sock, &sock->ref_cnt);

	}

	return NULL;
}

/*
 * nss_nlsock_listen()
 * 	listen for async events on the socket
 */
int nss_nlsock_listen(struct nss_nlsock_ctx *sock, char *grp_name)
{
	struct nl_sock *nl_sk;
	pid_t pid = getpid();
	int error = -EINVAL;

	nl_sk = nss_nlsock_get_sock(sock, &sock->ref_cnt);
	if (!nl_sk) {
		nss_nlsock_log_error("%d:failed to get NL socket\n", pid);
		return -ENOMEM;
	}

	sock->grp_id = genl_ctrl_resolve_grp(nl_sk, sock->family_name, grp_name);
	if (!sock->grp_id) {
		nss_nlsock_log_error("%d:failed to resolve family(%s)\n", pid, grp_name);
		goto done;
	}

	error = nl_socket_add_memberships(nl_sk, sock->grp_id, 0);
	if (error < 0) {
		nss_nlsock_log_error("%d:failed to register grp(%s)\n", pid, grp_name);
		goto done;
	}
done:
	nss_nlsock_put_sock(sock, &sock->ref_cnt);
	return error;
}

/*
 * nss_nlsock_open()
 *	open a socket to communicate with the generic netlink framework
 */
int nss_nlsock_open(struct nss_nlsock_ctx *sock, nl_recvmsg_msg_cb_t cb)
{
	int pid = getpid();
	int error = 0;

	if (!sock) {
		nss_nlsock_log_error("%d: invalid NSS Socket context\n", pid);
		return EINVAL;
	}

	sock->pid = pid;

	error = nss_nlsock_init_sock(sock, cb);
	if (error) {
		return error;
	}

	/*
	 * Connect the socket with the netlink bus
	 */
	if (genl_connect(sock->nl_sk)) {
		nss_nlsock_log_error("%d:failed to connect socket for family(%s)\n", pid, sock->family_name);
		error = EBUSY;
		goto free_sock;
	}

	/*
	 * resolve the family
	 */
	sock->family_id = genl_ctrl_resolve(sock->nl_sk, sock->family_name);
	if (sock->family_id <= 0) {
		nss_nlsock_log_error("%d:failed to resolve family(%s)\n", pid, sock->family_name);
		error = EINVAL;
		goto free_sock;
	}

	/*
	 * create the sync thread for clearing the pending resp on the socket
	 */
	error = pthread_create(&sock->thread, NULL, nss_nlsock_sync, sock);
	if (error) {
		nss_nlsock_log_error("%d:failed to create sync thread for family(%s)\n", pid, sock->family_name);
		goto free_sock;
	}

	return 0;

free_sock:

	nss_nlsock_put_sock(sock, &sock->ref_cnt);
	return error;
}

/*
 * nss_nlsock_close()
 * 	close the allocated socket and all associated memory
 */
void nss_nlsock_close(struct nss_nlsock_ctx *sock)
{
	struct nl_sock *nl_sk;

	assert(sock);
	assert(sock->nl_sk);

	nl_socket_drop_memberships(sock->nl_sk, sock->grp_id, 0);

	/*
	 * put the reference down for the socket
	 */
	nss_nlsock_put_sock(sock, &sock->ref_cnt);

	/*
	 * check if the sync thread is blocking on the socket;
	 * this will wake it and not allow further blocking
	 */
	nl_sk = nss_nlsock_get_sock(sock, &sock->ref_cnt);
	if (nl_sk) {
		nl_socket_set_nonblocking(nl_sk);
		nss_nlsock_put_sock(sock, &sock->ref_cnt);
	}

	/*
	 * wait for the sync thread to complete
	 */
	pthread_cancel(sock->thread);
	pthread_join(sock->thread, NULL);
}

/*
 * nss_nlsock_send_msg()
 *	send a message through the socket
 */
int nss_nlsock_send(struct nss_nlsock_ctx *sock, struct nss_nlcmn *cm, void *data)
{
	struct nl_sock *nl_sk;
	int error = ENOMEM;
	struct nl_msg *msg;
	int pid = sock->pid;
	void *user_hdr;
	uint32_t ver;
	uint8_t cmd;
	int len;

	/*
	 * allocate new message buffer
	 */
	msg = nlmsg_alloc();
	if (!msg) {
		nss_nlsock_log_error("%d:failed to allocate message buffer\n", pid);
		return -ENOMEM;
	}

	ver = nss_nlcmn_get_ver(cm);
	len = nss_nlcmn_get_len(cm);
	cmd = nss_nlcmn_get_cmd(cm);

	/*
	 * create space for user header
	 */
	user_hdr = genlmsg_put(msg, pid, NL_AUTO_SEQ, sock->family_id, len, 0, cmd, ver);
	if (!user_hdr) {
		nss_nlsock_log_error("%d:failed to put message header of len(%d)\n", pid, len);
		goto free_msg;
	}

	memcpy(user_hdr, data, len);

	nl_sk = nss_nlsock_get_sock(sock, &sock->ref_cnt);
	if (!nl_sk) {
		nss_nlsock_log_error("%d:failed to get NL socket\n", pid);
		goto free_msg;
	}

	/*increment the msg reference count */
	nlmsg_get(msg);

	/*
	 * send message and wait for ACK/NACK, this will free message upon success
	 */
	error = nl_send_auto(nl_sk, msg);
	if (error < 0) {
		nss_nlsock_log_error("%d:failed to send message, family(%s) error:%d\n", pid, sock->family_name, error);
		goto free_sock;
	}

	nss_nlsock_put_sock(sock, &sock->ref_cnt);

	return 0;

free_sock:
	nss_nlsock_put_sock(sock, &sock->ref_cnt);

free_msg:
	nlmsg_free(msg);
	return error;
}
