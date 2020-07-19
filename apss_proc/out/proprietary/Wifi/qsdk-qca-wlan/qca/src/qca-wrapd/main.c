/*
 * Copyright (c) 2012,2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2012 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <dirent.h>
#include <sys/un.h>
#include <signal.h>
#include "includes.h"
#include "os.h"
#include "eloop.h"
#include "common.h"
#include "wpa_ctrl.h"
#include "wrapd_api.h"
#include "bridge.h"

#define CONFIG_CTRL_IFACE_CLIENT_DIR "/tmp"
#define CONFIG_CTRL_IFACE_CLIENT_PREFIX "wrap_ctrl_"

const char *hostapd_ctrl_iface_dir = "/var/run/hostapd";
const char *wpa_s_ctrl_iface_dir = "/var/run/wpa_supplicant";
const char *wrapd_ctrl_iface_dir = "/var/run/wrapd";

const char *global_wrapd_ctrl_iface_path = "/var/run/wrapdglobal";
const char *global_wpa_s_ctrl_iface_path = "/var/run/wpa_supplicantglobal";
const char *global_hostapd_ctrl_iface_path = "/var/run/hostapd/global";

struct wrapd_global *wrapg;

struct wrapd_ctrl {
	int sock;
	struct sockaddr_un local;
	struct sockaddr_un dest;
};

static void usage(void)
{
    printf("wrapd [-g<wrapd ctrl intf>]"
        "[-w<global wpa_s ctrl intf>] [-H<>global_hostapd_ctrl_iface]\\\n"
        "      [-A<psta oma>] [-R<psta oma>]"
        "[-b<bridge name> ] [-t<poll time> ] [-i<bridge ifname> ] [-l<wired client limit> ] \\\n"
        "[-e<enable auto wired addtion> ] [-r<enable auto wired deletion> ]\\\n"
        "      [command..]\n"
        "        -h = help (show this usage text)\n"
		"        -B = run as daemon in the background\n"
        "        -M = do MAT while adding PSTA\n"
        "        -L = list oma->vma for active PSTAs\n"
        "        -S = run in slave mode, send msg to wrapd\n"
        "        -T = use timer to connect detected PSTAs\n"
        "      default global wrapd ctrl intf: /var/run/wrapdglobal\n"
        "      default global hostapd ctrl path: /var/run/hostapd/global\n"
        "      default global wpa_s ctrl intf: /var/run/wpa_supplicantglobal\n");
}

struct wrapd_ctrl *
wrapd_ctrl_open(const char *ctrl_iface, wrapd_hdl_t *handle)
{
	struct wrapd_ctrl *priv;
    struct sockaddr_un addr;

	if (ctrl_iface == NULL)
		return NULL;

	priv = os_zalloc(sizeof(*priv));
	if (priv == NULL)
		return NULL;

	priv->sock = -1;
	priv->sock = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (priv->sock < 0) {
		wrapd_printf("Fail to create socket");
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	os_strlcpy(addr.sun_path, ctrl_iface, sizeof(addr.sun_path));

    if (bind(priv->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        wrapd_printf("1st, fail to bind socket");

        if (connect(priv->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {

            if (unlink(ctrl_iface) < 0) {
                wrapd_printf("Intf exists but does not allow to connect");
                goto fail;
            }

            if (bind(priv->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
                wrapd_printf("2st, fail to bind socket");
                goto fail;
            }
            wrapd_printf("2st, success to bind socket");
        } else {
            wrapd_printf("Intf exists");
            goto fail;
        }
    }

    //eloop_register_read_sock(priv->sock, wrapd_ctrl_iface_receive, handle, NULL);
    return priv;

fail:
	if (priv->sock >= 0)
		close(priv->sock);
	os_free(priv);
	return NULL;

}

int wrapd_send_msg(const char *msg, int len, const char *dest_path)
{
    int sock;
    struct sockaddr_un local;
    struct sockaddr_un dest;
    socklen_t destlen = sizeof(dest);
    static int counter = 0;
    int ret;
    size_t res;
    int tries = 0;
    int connect_tries = 0;
    struct timeval tv;
    fd_set rfds;
    char reply[256];
    size_t reply_len = sizeof(reply) - 1;

    sock = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        wrapd_printf("%s:%d:Error:socket creation failed\n", __func__,__LINE__);
        return -1;
    }

    os_memset(reply, 0, sizeof(reply));

    local.sun_family = AF_UNIX;
    counter++;
try_again:
    ret = os_snprintf(local.sun_path, sizeof(local.sun_path),
            CONFIG_CTRL_IFACE_CLIENT_DIR "/"
            CONFIG_CTRL_IFACE_CLIENT_PREFIX "%d-%d",
            (int) getpid(), counter);
    if (ret < 0 || (size_t) ret >= sizeof(local.sun_path)) {
        close(sock);
        wrapd_printf("%s:%d:Error:os_snprintf failed\n", __func__,__LINE__);
        return -1;
    }
    tries++;
    if (bind(sock, (struct sockaddr *) &local, sizeof(local)) < 0) {
        if (errno == EADDRINUSE && tries < 2) {
            /*
             * getpid() returns unique identifier for this instance
             * of wpa_ctrl, so the existing socket file must have
             * been left by unclean termination of an earlier run.
             * Remove the file and try again.
             */
            unlink(local.sun_path);
            goto try_again;
        }
        close(sock);
        wrapd_printf("%s:%d:Error:bind failed\n", __func__,__LINE__);
        return -1;
    }

    dest.sun_family = AF_UNIX;
    res = os_strlcpy(dest.sun_path, dest_path, sizeof(dest.sun_path));
    if (res >= sizeof(dest.sun_path)) {
        close(sock);
        unlink(local.sun_path);
        wrapd_printf("%s:%d:Error:os_strlcpy failed\n", __func__,__LINE__);
        return -1;
    }

try_connect:
    if (connect(sock, (struct sockaddr *) &dest, sizeof(dest)) < 0) {
        if(connect_tries < 60)
        {
            os_sleep(1, 0);
            connect_tries++;
            goto try_connect;
        }
        close(sock);
        unlink(local.sun_path);
        wrapd_printf("%s:%d:Error:connect failed\n", __func__,__LINE__);
        return -1;
    }
    sendto(sock, msg, len, 0, (struct sockaddr *) &dest, destlen);
    for (;;) {
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        res = select(sock + 1, &rfds, NULL, NULL, &tv);
        if (res < 0)
        {
            wrapd_printf("%s:%d:Error:select failed\n", __func__,__LINE__);
            goto error;
        }
        if (FD_ISSET(sock, &rfds)) {
            res = recv(sock, reply, reply_len, 0);
            if (res < 0)
            {
                wrapd_printf("%s:%d:Error:recv failed\n", __func__,__LINE__);
                goto error;
            }
            if (res > 0 && reply[0] == '<') {
                continue;
            }
            reply_len = res;
            close(sock);
            unlink(local.sun_path);
            return 0;
        } else {
            wrapd_printf("%s:%d:Error:recv exception\n", __func__,__LINE__);
            goto error;
        }
    }

error:
    close(sock);
    unlink(local.sun_path);
    return -1;
}

int store_pid(const char *pid_file) {
    if (pid_file) {
        FILE *f = fopen(pid_file, "w");
        if (f) {
            fprintf(f, "%u\n", getpid());
            fclose(f);
            return 0;
        }
    }
    return -1;
}





int wrap_create_request(char *msg, size_t msglen, int argc, char *argv[])
{
    int i, res;
    char *pos, *end;

    pos = msg;
    end = msg + msglen;

    for (i = 0; i < argc; i++) {
        res = os_snprintf(pos, end - pos, "%s\t", argv[i]);
        if (os_snprintf_error(end - pos, res))
            goto fail;
        pos += res;
    }

    msg[msglen - 1] = '\0';
    return 0;

fail:
    wrapd_printf("Too long command\n");
    return -1;
}
void start_wired_client_addition()
{
    printf("Started wired client addition\n");
	pthread_create(&wrapg->wrap_main_func, NULL, wrap_main_function,NULL);
	pthread_create(&wrapg->wrap_cli_sock, NULL, wrap_check_socket,NULL);
}

extern struct init_func_pointers ioctl_initialize;
#if CFG80211_ENABLE
extern struct init_func_pointers cfg80211_initialize;
#endif

const struct init_func_pointers *const mode[] =
{
    &ioctl_initialize,
#if CFG80211_ENABLE
    &cfg80211_initialize,
#endif
};

void initialize_function_pointers(int driver_mode)
{
    driver = mode[driver_mode];
}

int main(int argc, char *argv[])
{
    int m;
    int c, res, ret = -1;
    const char *global_wpa_s_ctrl_intf = NULL;
    const char *global_hostapd_ctrl_intf = NULL;
    const char *wrapd_ctrl_intf = NULL;
    const char *add_psta_addr = NULL;
    const char *remove_psta_addr = NULL;
    char *pid_file = NULL;
    int daemonize = 0;
    int list_psta_addr = 0;
    int do_mat = 0;
    int do_timer = 0;
    int slave_mode = 0;
    char msg[128] = {0};
    char bridge_name[IFNAMSIZ];
    char bridge_ifname[8][IFNAMSIZ];
    os_strlcpy(bridge_name,DEFAULT_BRIDGE_NAME,IFNAMSIZ);
    os_strlcpy(bridge_ifname[0],DEFAULT_INTERFACE,IFNAMSIZ);
    int sleep_timer = DEFAULT_SLEEP_TIMER;
    int delete_enable = DEFAULT_DELETE_ENABLE;
    int vap_limit = DEFAULT_VAP_LIMIT;
    int no_of_interfaces=0;
    int enable_proxysta_addition = 1;
    int arg_count = 0;
    int wrapd_psta_config = 0;
    struct sigaction action;
    int driver_mode=0;
    char dbglvl[IFNAMSIZ] = {0};
    char dbglvl_high[IFNAMSIZ] = {0};

    if (os_program_init())
        return -1;

    for (;;) {
        c = getopt(argc, argv, "g:p:w:A:R:a:BLMSITc:v:d:D:P:hb:t:i:l:r:e:H:mF:");
        if (c < 0) {
            if(arg_count == 0)
               goto out;
            else
               break;
        }
        arg_count++;

        switch (c) {
            case 'a':
                wrapd_psta_config = atoi(optarg);
                wrapd_printf("wrapd_psta_config:%d\n",wrapd_psta_config);
                if (wrapd_psta_config < 0 || wrapd_psta_config > 2) {
                   wrapd_psta_config = 0;
                }
                break;
            case 'g':
                wrapd_ctrl_intf = optarg;
                break;
            case 'w':
                global_wpa_s_ctrl_intf = optarg;
                break;
            case 'H':
                global_hostapd_ctrl_intf = optarg;
                break;
		    case 'B':
			    daemonize++;
			    break;
            case 'A':
                add_psta_addr = optarg;
                break;
            case 'R':
                remove_psta_addr = optarg;
                break;
            case 'L':
                list_psta_addr = 1;
                break;
            case 'M':
                do_mat = 1;
                break;
            case 'S':
                slave_mode = 1;
                break;
            case 'T':
                do_timer = 1;
                break;
            case 'h':
                usage();
                ret = 0;
                goto out;
            case 'P':
                pid_file = os_rel2abs_path(optarg);
                break;
            case 'm':
                driver_mode = 1;
                break;
	    case 'b':
		os_strlcpy(bridge_name,optarg,IFNAMSIZ);
		break;
	    case 't':
		sleep_timer=atoi(optarg);
		if(sleep_timer>MAX_SLEEP_TIMER && sleep_timer<MIN_SLEEP_TIMER)
		{
		    sleep_timer=DEFAULT_SLEEP_TIMER;
		}
		break;
	    case 'i':
                if(no_of_interfaces < MAX_PORT_LIMIT)
                {
                    os_strlcpy(bridge_ifname[no_of_interfaces],optarg,IFNAMSIZ);
                    no_of_interfaces++;
                }
		break;
	    case 'l':
		vap_limit=atoi(optarg);
		if(vap_limit>MAX_VAP_LIMIT)
		{
		    vap_limit=MAX_VAP_LIMIT;
		}
		break;
	    case 'r':
		delete_enable = atoi(optarg);
		break;
            case 'e':
                enable_proxysta_addition = atoi(optarg);
                break;
            case 'F':
                    if((optarg[0] == 'l') && (strlen(optarg)>2))
                        os_strlcpy(dbglvl,&optarg[2],IFNAMSIZ);
                    if((optarg[0] == 'h') && (strlen(optarg)>2))
                        os_strlcpy(dbglvl_high,&optarg[2],IFNAMSIZ);
                break;
            default:
                usage();
                goto out;
        }
    }

    initialize_function_pointers(driver_mode);

    if (slave_mode) {
        if(wrapd_ctrl_intf == NULL)
            goto out;
        if(add_psta_addr) {
            if (do_mat) {
                res = os_snprintf(msg, sizeof(msg),"ETH_PSTA_ADD MAT %s", add_psta_addr);
            } else
                res = os_snprintf(msg, sizeof(msg),"ETH_PSTA_ADD %s", add_psta_addr);

            if (res < 0 || res >= sizeof(msg)){
                wrapd_printf("Fail to build ETH_PSTA_ADD msg");
                goto out;
            }
            wrapd_send_msg(msg, 128, wrapd_ctrl_intf);
            ret = 0;
            goto out;

        } else if (remove_psta_addr) {
            res = os_snprintf(msg, sizeof(msg),"ETH_PSTA_REMOVE %s", remove_psta_addr);
            if (res < 0 || res >= sizeof(msg)){
                wrapd_printf("Fail to build ETH_PSTA_REMOVE msg");
                goto out;
            }
            wrapd_send_msg(msg, (16 + 17), wrapd_ctrl_intf);
            ret = 0;
            goto out;

        } else if (list_psta_addr) {
            wrapd_send_msg("PSTA_LIST", 9, wrapd_ctrl_intf);
            ret = 0;
            goto out;
        } else if (optind < argc) {
            /* Parse remaining non-option arguments */
            res = wrap_create_request(msg, sizeof(msg), argc - optind, &argv[optind]);
            if (res < 0 || res >= sizeof(msg)){
                wrapd_printf("Fail to build wrapd slave msg\n");
                goto out;
            }
            wrapd_send_msg(msg, 128, wrapd_ctrl_intf);
            ret = 0;
            goto out;
        }
    }

    os_memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = cleanup_ifaces;
    sigaction(SIGTERM, &action, NULL);

    /* Store PID for killing process during down/up */
    if (daemonize) {
        if (os_daemonize(pid_file)) {
            wrapd_printf("daemon");
            goto out;
        }
    } else  {
        if(store_pid(pid_file)) {
            wrapd_printf("Unable to store PID in file");
            goto out;
        }
    }

    if (eloop_init()) {
        wrapd_printf("Failed to initialize event loop");
        goto out;
    }

    if(NULL == global_wpa_s_ctrl_intf)
        global_wpa_s_ctrl_intf = global_wpa_s_ctrl_iface_path;

    if(NULL == global_hostapd_ctrl_intf)
        global_hostapd_ctrl_intf = global_hostapd_ctrl_iface_path;

    if(NULL == wrapd_ctrl_intf)
        wrapd_ctrl_intf = global_wrapd_ctrl_iface_path;

    wrapg = wrapd_init_global((const char*)global_wpa_s_ctrl_intf, (const char*)global_hostapd_ctrl_intf, (const char*)wrapd_ctrl_intf, bridge_name, do_timer);
    if (wrapg == NULL)
        goto out;

    wrapg->wrapd_psta_config = wrapd_psta_config;
    wrapg->wifi_count = 0;
    wrapg->enable_proxysta_addition = enable_proxysta_addition;
    wrapg->global_wrapd = wrapd_ctrl_open(wrapd_ctrl_intf, wrapg);
    wrapg->max_aptr = NULL;
    os_strlcpy(wrapg->dbglvl, dbglvl, IFNAMSIZ);
    os_strlcpy(wrapg->dbglvl_high, dbglvl_high,IFNAMSIZ);
    if(wrapg->global_wrapd)
        eloop_register_read_sock(wrapg->global_wrapd->sock, wrapd_global_ctrl_iface_receive, (void *)wrapg, NULL);


        if(slave_mode==0 && enable_proxysta_addition == 1)
    {
	data=(struct input *)os_malloc(sizeof(struct input));
    if(data == NULL)
    {
        wrapd_printf("Allocation of data failed\n");
        goto out;
    }
	data->no_of_interfaces = 1;
	if(no_of_interfaces != 0)
	{
		data->no_of_interfaces = no_of_interfaces;
	}
	data->ioctl_sock=0;
	os_strlcpy(data->brname,bridge_name,IFNAMSIZ);

        for(m=0;m<data->no_of_interfaces;m++)
        {
            os_strlcpy(data->ifname[m],bridge_ifname[m],IFNAMSIZ);
        }
	data->delete_enable=delete_enable;
	data->sleep_timer=sleep_timer;
	data->vap_limit=vap_limit;
	global_vap_limit_flag=0;

    }

    eloop_run();

    if(slave_mode == 0 && enable_proxysta_addition == 1)
    {
	pthread_join(wrapg->wrap_main_func, NULL);
        pthread_join(wrapg->wrap_cli_sock, NULL);
    }

out:
    if(slave_mode==0 && enable_proxysta_addition == 1)
    {
        os_free(data);
        os_free(table);
    }
    if(pid_file) {
        if(daemonize) {
            os_daemonize_terminate(pid_file);
        }
        os_free(pid_file);
    }
    os_program_deinit();

    return ret;

}

