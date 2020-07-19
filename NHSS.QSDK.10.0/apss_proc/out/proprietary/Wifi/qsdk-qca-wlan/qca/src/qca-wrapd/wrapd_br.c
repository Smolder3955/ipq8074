/*
 * Copyright (c) 2015,2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2015 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <sys/un.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <net/if.h>
#include <linux/in6.h>
#include <unistd.h>
#include <linux/if_bridge.h>
#include "bridge.h"
#include "common.h"

static void wrap_copy_fdb(struct new_fdb_table *to, struct __fdb_entry *from)
{
    os_memcpy(to->mac_addr, from->mac_addr, IEEE80211_ADDR_LEN);
    to->flag=0;
    to->ageing_timer_value=from->ageing_timer_value;
}

static int br_dev_ioctl(struct input *data, unsigned long arg0,
        unsigned long arg1, unsigned long arg2,
        unsigned long arg3)
{
    unsigned long args[4];
    struct ifreq ifr;

    args[0] = arg0;
    args[1] = arg1;
    args[2] = arg2;
    args[3] = arg3;

    os_memcpy(ifr.ifr_name, data->brname, IFNAMSIZ);
    ifr.ifr_data = (char *)args;

    return ioctl(data->ioctl_sock, SIOCDEVPRIVATE, &ifr);
}
int wrap_read_forwarding_database(struct new_fdb_table *fdbs, unsigned long offset, int chunk)
{
    FILE *fp;
    int i,j=0,k,n=0;
    struct __fdb_entry entry[chunk];
    char path[SYSFS_PATH_MAX];
    unsigned long num_entries = MAX_READ_LENGTH/sizeof(struct __fdb_entry);
    int read_complete = 0;
    int read_progress = 0;
    snprintf(path, SYSFS_PATH_MAX, SYS_CLASS_NET "%s/" SYSFS_BRIDGE_FDB, data->brname);
    fp=fopen(path,"r");
    if(fp)
    {
        fclose(fp);
        do{
            if((chunk-read_complete) > num_entries)
	    {
                read_progress += num_entries;
            }
            else
            {
                read_progress += chunk - read_complete;
            }
            fp=fopen(path,"r");
            if(fp)
            {
                fseek(fp, (read_complete + offset) * sizeof(struct __fdb_entry), SEEK_SET);
                n=fread(entry, sizeof(struct __fdb_entry), num_entries, fp);
		read_complete += n;
                fclose(fp);
            }
	} while(read_complete != chunk && n!=0);
    }
    else if(data->ioctl_sock >= 0)
    {
	int retry = 0;
retry:
	    read_complete = br_dev_ioctl(data, BRCTL_GET_FDB_ENTRIES, (unsigned long)entry, chunk, offset);
            if (read_complete < 0 && errno==EAGAIN && ++retry<10)
            {
                goto retry;
            }
    }
    for(i=0;i<read_complete;i++)
    {
        struct __fdb_entry *from=entry + i;
        for(k=0; k < data->no_of_interfaces;k++)
        {
            if( (from->port_no==data->port[k]) && (from->is_local==0))
            {
                wrap_copy_fdb(fdbs+j, from);
                j++;
            }
        }
    }
    data->new_table_no+=j;
    return read_complete;
}


int wrap_interface_to_port(char *ifname)
{
    int port = DEFAULT_PORT;
    int n;
    int i;
    int index = if_nametoindex(ifname);
    int indices[MAX_PORTS];
    if(data->ioctl_sock >= 0)
    {
        memset(indices, 0, sizeof(indices));
        n=br_dev_ioctl(data, BRCTL_GET_PORT_LIST, (unsigned long)indices, MAX_PORTS, 0);
        if (n < 0)
        {
            printf("Error retreiving port number");
	    goto out;
        }
        for (i = 0; i < MAX_PORTS; i++)
        {
            if (indices[i]==index)
            {
                port=i;
                break;
            }
        }
    }
out:
    return port;
}
