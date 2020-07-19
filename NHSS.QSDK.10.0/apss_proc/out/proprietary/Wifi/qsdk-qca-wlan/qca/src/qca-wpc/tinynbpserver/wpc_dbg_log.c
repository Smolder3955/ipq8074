#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include<ieee80211_wpc.h>

#define FILEPATH "/var/lib/tftpboot/wpc_dbg_log"
#define WPC_DBG_SIZE 1024*1024

unsigned char *wpc_dbg_buf;
int wpc_dbg_log_len = 0;

int write_dbg_log(int fd)
{
    int result;

    fd = open(FILEPATH, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file for writing");
        exit(EXIT_FAILURE);
    }
//    wpc_dbg_buf = malloc(WPC_DBG_SIZE);
    wpc_dbg_buf = mmap(0, WPC_DBG_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (wpc_dbg_buf == MAP_FAILED) {
        close(fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }
    return fd;
}

void stop_dbg_log ( int fd)
{
    if (munmap(wpc_dbg_buf, WPC_DBG_SIZE) == -1) {
        perror("Error un-mmapping the file");
    }
    close(fd);
}

void print_wpc_dbg_hdr(struct wpc_dbg_header *wpc_dbg_hdr)
{
    printf("Filename: %s ",wpc_dbg_hdr->filename);
    printf("Line No: %d ",ntohl(wpc_dbg_hdr->line_no));
    printf("Frame type: %d\n",wpc_dbg_hdr->frame_type);
}

void print_rtt_mreq(struct nsp_mrqst *mrqst)
{
    unsigned char *s = (char *)mrqst->sta_mac_addr;
    printf("----------------WPC MEASUREMENT_REQUEST-----------------");
    printf("Request ID            :%x\n",mrqst->request_id);
    printf("Mode                  :%x\n",mrqst->mode);
    printf("STA MAC addr          :%x:%x:%x:%x:%x:%x\n", s[0],s[1],s[2],s[3],s[4],s[5]);
    s = (unsigned char *)mrqst->spoof_mac_addr;
    printf("Spoof MAC addr        :%x:%x:%x:%x:%x:%x\n", s[0],s[1],s[2],s[3],s[4],s[5]);
    printf("STA Info              :%x\n",mrqst->sta_info);
    printf("Channel               :%x\n",mrqst->channel);
    printf("Number of measurements:%x\n",mrqst->no_of_measurements);
    printf("Transmit Rate         :%x\n",mrqst->transmit_rate);
    printf("Timeout               :%x\n",mrqst->timeout);
}

int main()
{
    int fd;

    write_dbg_log(fd);
    while(wpc_dbg_log_len + WPCDBGHDR_LEN <= WPC_DBG_SIZE)
    {
        struct wpc_dbg_header *wpc_dbg_hdr;
        struct nsp_mrqst *mrqst;

        wpc_dbg_hdr = (struct wpc_dbg_header *)(wpc_dbg_buf + wpc_dbg_log_len);
        print_wpc_dbg_hdr(wpc_dbg_hdr);
        wpc_dbg_log_len += WPCDBGHDR_LEN;

   //     if (wpc_dbg_hdr->frame_type == NSP_MRQST) {
     //       mrqst = (struct nsp_mrqst *)wpc_dbg_buf + wpc_dbg_log_len;
       //     print_rtt_mreq(mrqst);
      //  }
   } 
            
    stop_dbg_log(fd);
}
