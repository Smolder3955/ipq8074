#ifndef __IEEE80211_HLSMGR_H
#define __IEEE80211_HLSMGR_H
#include<ieee80211_wpc.h>

struct req {
   struct nsp_header nsphdr;
   union {
        struct nsp_mrqst mreq;
        struct nsp_wakeup_req wakereq;
        struct nsp_tsf_req tsfreq;
        struct nsp_cap_req capreq;
        struct nsp_tsf_req sreq;
        struct nsp_sleep_req sleepreq;
        struct nsp_wpc_dbg_req wpcdbgreq;
   };
}__attribute__((packed));

struct resp {
    struct nsp_header nsphdr;
    struct nsp_mresp mresp;
	union {
        struct nsp_type1_resp type1resp;
        struct nsp_wakeup_resp wakeresp;
        struct nsp_tsf_resp tsfresp;
        struct nsp_sleep_resp sleepresp;
    };
}__attribute__((packed));

struct type1_resp {
        struct nsp_mresp mresp;
        struct nsp_type1_resp type1resp;
}__attribute__((packed));

#define CDUMP_SIZE_PER_CHAIN 130

#define WIFIPOS_TYPE1_RES_SIZE ( sizeof(struct nsp_header) + sizeof(struct nsp_mresp)  + sizeof (struct nsp_type1_resp) )

#endif

