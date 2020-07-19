#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <string.h>
#include <net/if_arp.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/stat.h>
#include <acfg_types.h>
#include <acfg_api_pvt.h>
#include <linux/un.h>
#include <acfg_security.h>

int
acfg_ctrl_iface_open(uint8_t *ifname, char *ctrl_iface_dir,
        struct wsupp_ev_handle *wsupp_h)
{
    int sock;
    static int counter = 0;

    sock = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }
    wsupp_h->sock = sock;
    memset(&wsupp_h->local, 0, sizeof(wsupp_h->local));
    memset(&wsupp_h->dest, 0, sizeof(wsupp_h->dest));
    wsupp_h->local.sun_family = AF_UNIX;
    snprintf(wsupp_h->local.sun_path, sizeof (wsupp_h->local.sun_path),
            "/tmp/acfg_ctrl-%d-%d", getpid(), counter++);
    if (bind(sock, (struct sockaddr *)&wsupp_h->local,
                sizeof (wsupp_h->local)) < 0)
    {
        acfg_log_errstr("acfg_ctrl_iface_open: bind failed: %s\n", strerror(errno));
        if (errno == EADDRINUSE) {
            unlink(wsupp_h->local.sun_path);
            if (bind(sock, (struct sockaddr *)&wsupp_h->local,
                        sizeof (wsupp_h->local)) < 0)
            {
                acfg_log_errstr("acfg_ctrl_iface_open: bind failed: %s\n", strerror(errno));
                goto fail;
            }
        }
    }
    wsupp_h->dest.sun_family = AF_UNIX;
    snprintf(wsupp_h->dest.sun_path, sizeof (wsupp_h->dest.sun_path),
            "%s/%s", ctrl_iface_dir, ifname);
    if (connect(sock, (struct sockaddr *)&wsupp_h->dest,
                sizeof (wsupp_h->dest)) < 0)
    {
        goto fail;
    }
    return sock;
fail:
    unlink(wsupp_h->local.sun_path);
    close(sock);
    return -1;
}

int
acfg_ctrl_iface_present (uint8_t *ifname, acfg_opmode_t opmode)
{
    int sock;
    struct wsupp_ev_handle wsupp_h;

    sock = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (opmode == ACFG_OPMODE_STA) {
        snprintf(wsupp_h.dest.sun_path,
                sizeof (wsupp_h.dest.sun_path), "%s/%s",
                ctrl_wpasupp, ifname);
    } else if (opmode == ACFG_OPMODE_HOSTAP)  {
        snprintf(wsupp_h.dest.sun_path,
                sizeof (wsupp_h.dest.sun_path), "%s/%s",
                ctrl_hapd, ifname);
    }
    wsupp_h.dest.sun_family = AF_UNIX;
    if (connect(sock, (struct sockaddr *)&wsupp_h.dest,
                sizeof (wsupp_h.dest)) < 0)
    {
        close(sock);
        return -1;
    }
    close(sock);
    return 1;
}

void
acfg_ctrl_iface_close (int32_t sock)
{
    close(sock);
}

int
acfg_poll_read_sock (int32_t sock, char *buf, uint32_t *len)
{
    fd_set rdfs;
    int32_t  res = 0;

    FD_ZERO(&rdfs);
    FD_SET(sock, &rdfs);

    res = select(sock + 1, &rdfs, NULL, NULL, 0);
    if (res < 0) {
        acfg_log_errstr("acfg_poll_read_sock: select failed: %s\n", strerror(errno));
        return -1;
    }
    if (FD_ISSET(sock, &rdfs)) {
        res = recv(sock, buf, *len, 0);
        if (res < 0) {
            acfg_log_errstr("acfg_poll_read_sock: recv fail: %s\n", strerror(errno));
            return -1;
        }
        *len = res;
        return 0;
    }
    return 0;
}

void
acfg_close_ctrl_sock (struct wsupp_ev_handle wsupp_h)
{
    if (wsupp_h.sock >= 0) {
        unlink(wsupp_h.local.sun_path);
        close (wsupp_h.sock);
    }
}


int acfg_ctrl_req(uint8_t *ifname, char *cmd, size_t cmd_len,
        char *replybuf,
        uint32_t *reply_len,
        acfg_opmode_t opmode)
{
    int res;
    struct wsupp_ev_handle wsupp_h;

    acfg_print("%s %s\n", ifname, cmd);
    if (opmode == ACFG_OPMODE_STA) {
        wsupp_h.sock = acfg_ctrl_iface_open (ifname,
                ctrl_wpasupp, &wsupp_h);
    } else if (opmode == ACFG_OPMODE_HOSTAP) {
        wsupp_h.sock = acfg_ctrl_iface_open (ifname,
                ctrl_hapd, &wsupp_h);
    }
    else {
        acfg_log_errstr("acfg_ctrl_req: unsupported opmode\n");
        return -1;
    }
    if (wsupp_h.sock < 0) {
        acfg_log_errstr("acfg_ctrl_req: sock creation failed: %s\n", strerror(errno));
        return -1;
    }
    res = send(wsupp_h.sock, cmd, cmd_len, 0);
    if (res < 0) {
        acfg_log_errstr("acfg_ctrl_req: send failed: %s\n", strerror(errno));
        close(wsupp_h.sock);
        return -1;
    }
    if (replybuf) {
        res = acfg_poll_read_sock(wsupp_h.sock, replybuf, reply_len);
        if (res < 0) {
            acfg_log_errstr("acfg_ctrl_req: read sock failed: %s\n", strerror(errno));
            acfg_close_ctrl_sock (wsupp_h);
            return -1;
        }
    }
    acfg_close_ctrl_sock (wsupp_h);
    return 0;
}

int
acfg_ctrl_iface_send(uint32_t sock, char *cmd, size_t cmd_len,
        char *replybuf, uint32_t *reply_len)
{
    int res;

    res = send(sock, cmd, cmd_len, 0);
    if (res < 0) {
        acfg_log_errstr("acfg_ctrl_iface_send: send failed: %s\n", strerror(errno));
        close(sock);
        return -1;
    }
    if (replybuf) {
        acfg_poll_read_sock(sock, replybuf, reply_len);
    }

    return 0;
}

uint32_t
acfg_app_open_socket(struct wsupp_ev_handle *app_sock_h)
{   
    int sock = 0;
    struct sockaddr_un local;

    app_sock_h->sock = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        acfg_log_errstr("acfg_app_open_socket: socket failed: %s\n", strerror(errno));
        return -1;
    }
    memset(&app_sock_h->local, 0, sizeof(local));
    app_sock_h->local.sun_family = AF_UNIX;
    snprintf(app_sock_h->local.sun_path, 
            sizeof (app_sock_h->local.sun_path), 
            ACFG_APP_CTRL_IFACE);
    if (bind(app_sock_h->sock, (struct sockaddr *)&app_sock_h->local,
                sizeof (app_sock_h->local)) < 0)
    {
        acfg_log_errstr("acfg_app_open_socket: bind failed: %s\n", strerror(errno));
        if (errno == EADDRINUSE) {
            unlink(app_sock_h->local.sun_path);
            if (bind(app_sock_h->sock, (struct sockaddr *)&app_sock_h->local,
                        sizeof (app_sock_h->local)) < 0)
            {
                acfg_log_errstr("acfg_app_open_socket: bind failed: %s\n", strerror(errno));
                goto fail;
            }
        }
    }
    return app_sock_h->sock;
fail:
    unlink(app_sock_h->local.sun_path);
    close(app_sock_h->sock);
    return -1;
}

uint32_t
acfg_open_app_sock(struct sockaddr_un *local)
{
    int sock;
    static int counter = 0;
    struct sockaddr_un dest;

    sock = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        acfg_log_errstr("acfg_open_app_sock: socked failed: %s\n", strerror(errno));
        return -1;
    }
    memset(local, 0, sizeof(*local));
    memset(&dest, 0, sizeof(dest));
    local->sun_family = AF_UNIX;
    snprintf(local->sun_path, sizeof (local->sun_path),
            "/tmp/acfg_app-%d-%d", getpid(), counter++);
    if (bind(sock, (struct sockaddr *)local,
                sizeof (*local)) < 0)
    {
        acfg_log_errstr("acfg_open_app_sock: bind failed: %s\n", strerror(errno));
        if (errno == EADDRINUSE) {
            unlink(local->sun_path);
            if (bind(sock, (struct sockaddr *)local,
                        sizeof (*local)) < 0)
            {
                acfg_log_errstr("acfg_open_app_sock: bind failed: %s\n", strerror(errno));
                goto fail;
            }
        }
    }
    dest.sun_family = AF_UNIX;
    snprintf(dest.sun_path, sizeof (dest.sun_path),
            "%s", ACFG_APP_CTRL_IFACE);
    if (connect(sock, (struct sockaddr *)&dest,
                sizeof (dest)) < 0)
    {
        acfg_log_errstr("acfg_open_app_sock: connect failed: %s\n", strerror(errno));
        goto fail;
    }
    return sock;
fail:
    unlink(local->sun_path);
    close(sock);
    return -1;
}

uint32_t
acfg_send_interface_event(char *event, int len)
{
    int sock, res;
    struct sockaddr_un local;

    sock = acfg_open_app_sock(&local);
    if (sock < 0) {
        return -1;
    }
    res = send(sock, event, len, 0);
    if (res < 0) {
        acfg_log_errstr("acfg_send_interface_event: send failed: %s\n", strerror(errno));
        close(sock);
        return -1;
    }
    if (sock >= 0) {
        unlink(local.sun_path);
        close (sock);
    }
    return 0;
}
