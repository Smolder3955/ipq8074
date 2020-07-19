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
#include <pthread.h>
#include "includes.h"
#include "common.h"
#include "linux_ioctl.h"
#include "bridge.h"
#include "wrapd_api.h"
#include<linux/if_bridge.h>
#include <limits.h>

/* INT_MAX doesnt seem to be available on 64-bit compilation.
 * Define it for now unitl fixed.
 */
#ifndef INT_MAX
#define INT_MAX 2147483647
#endif
/*
Fetches the entry with the smallest timer value among the
available entries for creation of vap
*/
static struct new_fdb_table *wrap_find_minimum_ageing_time
		(struct new_fdb_table *new_table, int number)
{
    int i;
    int min=INT_MAX;
    struct new_fdb_table *entry=NULL;
    for(i=0;i<number;i++)
    {
	struct new_fdb_table *new_entry = new_table + i;
        if((new_entry->ageing_timer_value>0) && (new_entry->flag==0)
				&& (new_entry->ageing_timer_value<min))
        {
            min=new_entry->ageing_timer_value;
            entry=new_entry;
        }
    }
    if (entry==NULL)
    {
	return NULL;
    }
    entry->flag=1;
    return entry;
}

/*
Flag common entries
*/
static int wrap_mac_check(struct fdb_table *old, struct new_fdb_table *new)
{
    if(os_memcmp(old->mac_addr, new->mac_addr, IEEE80211_ADDR_LEN)==0)
    {
        old->flag=1;
        new->flag=1;
	return 1;
    }
    return 0;
}

/*
Compares the bridge table with the Vap table
*/
static void wrap_compare_mac_entries(struct new_fdb_table *new_table)
{
    int i,j,flag;
    for(i=0; i<data->table_no; i++)
    {
	for(j=0; j < data->new_table_no; j++)
	{
	    flag=wrap_mac_check(table+i, new_table+j);
	    if(flag==1)
	    {
	        break;
	    }
	}
    }
}


/*
Issues a socket message to wrapd global listener for creation of vap
and adds the entry at the last of the Vap table.
*/
static void wrap_create_vap(struct new_fdb_table *entry)
{
    char msg[1024],mac[100];
    snprintf(mac,100,"%.2x-%.2x-%.2x-%.2x-%.2x-%.2x",entry->mac_addr[0],
		entry->mac_addr[1], entry->mac_addr[2],entry->mac_addr[3],
		entry->mac_addr[4], entry->mac_addr[5]);
    snprintf(msg, sizeof(msg),"ETH_PSTA_ADD %s", mac);

    /* Donot create vap in multicast address */
    if(entry->mac_addr[0] & 0x01) {
        return;
    }

    struct fdb_table *new_entry=table + data->table_no;
    os_memcpy(new_entry->mac_addr, entry->mac_addr, IEEE80211_ADDR_LEN);
    new_entry->flag=0;
    new_entry->success_flag=1;
    data->table_no++;
    wrapd_send_msg(msg, 128, wrapg->wrapd_ctrl_intf);
}

/*
Issues a socket message to wrapd global listener for deletion of vap
*/
static void wrap_delete_vap(struct fdb_table *entry)
{
    char msg[1024],mac[100];
    snprintf(mac,100,"%.2x-%.2x-%.2x-%.2x-%.2x-%.2x",entry->mac_addr[0],
		entry->mac_addr[1], entry->mac_addr[2],entry->mac_addr[3],
		entry->mac_addr[4], entry->mac_addr[5]);
    snprintf(msg, sizeof(msg),"ETH_PSTA_REMOVE %s", mac);
    entry->success_flag=1;
    wrapd_send_msg(msg, (16 + 17), wrapg->wrapd_ctrl_intf);
}

static void wrap_check_success_flag()
{
    int i;
    for(i=0;i<data->table_no;i++)
    {
	struct fdb_table *entry=table + i;
	if(entry->success_flag==2)
	{
	    /*
	    Replace the entry to be deleted with the last entry in
	    the table so that subsequent addition in the table will
	    take O(1) time instead of O(n) time. Always addition is
	    done at the end of the table.
	    */
	    struct fdb_table *last_entry=table + data->table_no - 1;
	    os_memcpy(entry->mac_addr, last_entry->mac_addr, IEEE80211_ADDR_LEN);
            entry->flag=last_entry->flag;
	    entry->success_flag=last_entry->success_flag;
            data->table_no--;
	    i--;
	}
    }
}

void *wrap_main_function()
{
    int i,n,offset=0;
    struct new_fdb_table *new_table=NULL, *temp = NULL;
    table=(struct fdb_table*)os_malloc(MAX_VAP_LIMIT * sizeof(struct fdb_table));
    data->ioctl_sock=0;
    if((data->ioctl_sock=socket(AF_LOCAL, SOCK_STREAM, 0))<0)
    {
    	wrapd_printf("error in ioctl socket opening");
        goto out;
    }
    data->table_no=0;
    while(1)
    {
	wrap_check_success_flag();
	offset=0;
	data->new_table_no=0;
	for(i=0;i<data->no_of_interfaces;i++)
	{
            data->port[i]=wrap_interface_to_port(data->ifname[i]);
	}
	while(1)
	{
	    temp = realloc(new_table, (offset+CHUNK) * sizeof
						(struct new_fdb_table));
	    if(!temp)
            {
                wrapd_printf("out of memory");
                break;
            }
            new_table = temp;
            n=wrap_read_forwarding_database(new_table+offset, offset, CHUNK);
            if(n==0)
	    {
                break;
	    }
            offset+=n;
	}
	if(data->table_no==0 && data->new_table_no!=0)
	{
	    for(i=0; i < data->new_table_no; i++)
	    {
		struct new_fdb_table *entry=NULL;
		if(data->table_no<data->vap_limit && global_vap_limit_flag==0)
		{
		    entry=wrap_find_minimum_ageing_time
					(new_table, data->new_table_no);
		    if(entry!=NULL)
		    {
			wrap_create_vap(entry);
		    }
		}
	    }
	}
	else if(data->table_no!=0 && data->new_table_no==0
					&& data->delete_enable==1)
	{
	    for(i=0;i<data->table_no;i++)
	    {
		struct fdb_table *entry=table+i;
		if(entry->success_flag==0)
		{
		    wrap_delete_vap(entry);
		}
	    }
	}
	else if(data->table_no != 0 && data->new_table_no != 0)
	{
	    wrap_compare_mac_entries(new_table);
	    if(data->delete_enable==1)
	    {
		for(i=0;i<data->table_no;i++)
		{
		    struct fdb_table *entry=table+i;
		    if(entry->flag==0 && entry->success_flag==0)
		    {
			wrap_delete_vap(entry);
		    }
		}
	    }
	    for(i=0;i<data->new_table_no;i++)
            {
		struct new_fdb_table *entry=NULL;
		if(data->table_no<data->vap_limit && global_vap_limit_flag==0)
		{
                    entry=wrap_find_minimum_ageing_time
					(new_table, data->new_table_no);
                    if(entry!=NULL)
		    {
                        wrap_create_vap(entry);
		    }
		}
            }
	    for(i=0;i<data->table_no;i++)
	    {
		(table+i)->flag=0;
	    }
	}
	sleep(data->sleep_timer);
    }

out:
    wrapd_printf("ERROR: Exiting thread %s\n", __func__);
    pthread_exit(NULL);
}

static void wrap_add_static_entry(unsigned char *mac_addr)
{
    struct fdb_table *entry=table+data->table_no;
    os_memcpy(entry->mac_addr,mac_addr,IEEE80211_ADDR_LEN);
    entry->success_flag=3;
    data->table_no++;

}

static void wrap_set_flag(unsigned char *mac_addr, int flag)
{
    int i;
    for(i=0; i<data->table_no;i++)
    {
	struct fdb_table *entry=table + i;
	if(os_memcmp(mac_addr, entry->mac_addr, IEEE80211_ADDR_LEN) == 0)
	{
            entry->success_flag=flag;
	    return;
	}
    }
    if(flag == 0)
    {
	wrap_add_static_entry(mac_addr);
    }
}


void *wrap_check_socket()
{
    char message;
    char type;
    char value;
    u_int8_t mac_addr[IEEE80211_ADDR_LEN];
    char buffer[1024];
    int i, s, res;
    struct sockaddr_un dest;
    socklen_t addr_size;
    s=socket(AF_UNIX, SOCK_DGRAM , 0);
    if(s < 0)
    {
        wrapd_printf("socket creation error");
        goto out;
    }
    dest.sun_family=AF_UNIX;
    if (strlcpy(dest.sun_path, ADDRESS,SOCKET_ADDR_LEN) >= SOCKET_ADDR_LEN) {
        wrapd_printf("source too long\n");
        goto out;
    }
    addr_size= sizeof dest;
    unlink(ADDRESS);
    i=bind(s, (struct sockaddr *)&dest, addr_size);
    if(i<0)
    {
        wrapd_printf("binding error");
        goto out;
    }
    os_memset(buffer, 0, sizeof(buffer));
    while(1)
    {
        res = recvfrom(s,buffer,1024,0,(struct sockaddr *)&dest, &addr_size);
        if (res < 0) {
            wrapd_printf("recvfrom err");
            goto out;
        }
        message=buffer[0];
	type=buffer[1];
	value=buffer[2];
	if(message==0)
	{
            os_memcpy(mac_addr, &buffer[3],IEEE80211_ADDR_LEN);
	    if(type==1)
	    {
		if(value==1)
		{
		    wrap_set_flag(mac_addr,0);
		}
		else if(value==0)
		{
		    global_vap_limit_flag=0;
		    wrap_set_flag(mac_addr,2);
		}
	    }
	    else if(type==0)
	    {
		if(value==1)
		{
		    global_vap_limit_flag=0;
	            wrap_set_flag(mac_addr,2);
		}
		else if (value==0)
		{
		    wrap_set_flag(mac_addr,0);
		}
	    }
	}
	else if (message==1)
	{
	    if(type==0)
	    {
	        if(value==0)
		{
		    data->delete_enable=0;
		}
		else
		{
		    data->delete_enable=1;
		}
	    }
	    else
	    {
		data->sleep_timer=buffer[2];
	    }
	}
    }
out:
    wrapd_printf("ERROR: Exiting thread %s\n", __func__);
    pthread_exit(NULL);
}
