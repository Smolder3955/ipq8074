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

#include<stdio.h>
#include<sys/socket.h>
#include <dirent.h>
#include <sys/un.h>
#include<unistd.h>
#define ADDRESS "/tmp/wrapd_cli_socket"
#define SOCKET_ADDR_LEN 64


int main(int argc, char * argv[])
{
    char c,buffer[1024];
    int i,s,nBytes;
    struct sockaddr_un dest;
    socklen_t addr_size;
    int time, delete_enable;
    s = socket(AF_UNIX, SOCK_DGRAM, 0);
    if(s<0)
    {
        printf("socket creation error");
	exit(0);
    }
    dest.sun_family=AF_UNIX;
    os_strlcpy(dest.sun_path, ADDRESS,SOCKET_ADDR_LEN);
    addr_size= sizeof dest;
    while(1)
    {
        c=getopt(argc,argv,"t:d: ");
        if(c<0)
            break;
        switch(c)
        {
            case 't':
		buffer[0]=1;
	        buffer[1]=1;
		time = atoi(optarg);
		if(time<10 && time>0)
		{
	            buffer[2]=time;
		}
		else
		{
		    goto out;
		}
	        break;
	    case 'd':
		buffer[0]=1;
		buffer[1]=0;
		delete_enable=atoi(optarg);
		if(delete_enable==0 || delete_enable==1)
		{
			buffer[2]=delete_enable;
		}
		else
		{
		    goto out;
		}
	        break;
	    default:
		goto out;
        }
        nBytes = 4;
        sendto(s,buffer,nBytes,0,(struct sockaddr *)&dest, addr_size);
    }
out:
     return 0;
}
