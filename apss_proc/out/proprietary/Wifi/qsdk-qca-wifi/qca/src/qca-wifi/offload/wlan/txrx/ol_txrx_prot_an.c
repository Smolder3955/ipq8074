/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @details protocol analyzer debug feature
 * @details
 * Protocol analyzer library to examine contents of tx and rx
 * frames passing through the txrx layer.
 */

#include <qdf_mem.h>       /* qdf_mem_malloc, etc. */
#include <qdf_types.h>     /* qdf_print */
#include <qdf_time.h>      /* qdf_time */
#include <qdf_nbuf.h>         /* qdf_nbuf_t, etc. */

#include <ol_txrx_types.h>    /* ol_txrx_pdev_t */
#include <ol_txrx_prot_an.h>  /* ol_txrx_prot_an_action, etc. */

/*--- data type definitions ---*/

struct ol_txrx_prot_an_field_spec_t {
    struct ol_txrx_prot_an_field_spec_t *next;
    int offset;
    const char *name;
    void (*print)(int prefix_len, const char *name, const u_int8_t *data);
};

struct ol_txrx_prot_an_t {
    struct ol_txrx_prot_an_t *next;
    struct ol_txrx_prot_an_t *parent;

    int depth;

    u_int32_t id;
    const char *name;
    u_int32_t count;   /* how many packets of this protocol have been seen */

    enum ol_txrx_prot_an_action action; /* count, print, or parse sub prots? */

    struct { /* info used for parsing encapsulated sub protocols */
        int header_bytes;  /* how large is this encapsulation header */
        u_int32_t (*classifier)(const u_int8_t *data, int offset);
        int classifier_offset;
        struct ol_txrx_prot_an_t *sub_prots;
        struct ol_txrx_prot_an_t *catch_all;
    } parse;

    struct { /* info used for deciding whether and what to print */
        struct ol_txrx_prot_an_field_spec_t *field_specs;
        enum ol_txrx_prot_an_period_type period_type;
        u_int32_t period_mask;
        u_int32_t period_ticks;
        u_int32_t timestamp;
    } print;
};

#if defined(ENABLE_TXRX_PROT_ANALYZE)

/*--- internal functions ---*/

ol_txrx_prot_an_handle
ol_txrx_prot_an_create(
    struct ol_txrx_pdev_t *pdev,
    const char *name)
{
    struct ol_txrx_prot_an_t *prot_an;

    prot_an = qdf_mem_malloc(sizeof(*prot_an));
    if (prot_an == NULL) {
        return NULL;
    }

    prot_an->name = name;

    prot_an->depth = 0;

    prot_an->next = NULL;
    prot_an->parent = NULL;

    prot_an->action = TXRX_PROT_ANALYZE_ACTION_COUNT; /* default */

    prot_an->parse.classifier = NULL;
    prot_an->parse.sub_prots = NULL;
    prot_an->parse.catch_all = NULL;

    prot_an->print.field_specs = NULL;

    return prot_an;
}

void
ol_txrx_prot_an_sub_prot_add(
    ol_txrx_prot_an_handle prot_an,
    ol_txrx_prot_an_handle sub_prot)
{
    sub_prot->next = prot_an->parse.sub_prots;
    sub_prot->parent = prot_an;
    sub_prot->depth = prot_an->depth + 1;
    prot_an->parse.sub_prots = sub_prot;
}

int
ol_txrx_prot_an_field_add(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_prot_an_t *prot_an,
    const char *field_name,
    int field_offset,
    void (*field_print)(int prefix_len, const char *name, const u_int8_t *data))
{
    struct ol_txrx_prot_an_field_spec_t *field_spec;
    field_spec = qdf_mem_malloc(sizeof(*field_spec));
    if (!field_spec) {
        return 1; /* failure */
    }
    field_spec->name = field_name;
    field_spec->offset = field_offset;
    field_spec->print = field_print;
    field_spec->next = prot_an->print.field_specs;
    prot_an->print.field_specs = field_spec;

    return 0;
}

void
ol_txrx_prot_an_print(
    u_int8_t *data,
    ol_txrx_prot_an_handle prot_an,
    int name_only,
    int recurse, /* 1 -> pre-recurse up, 0 -> none, -1 -> post-recurse down */
    int report /* 1 -> print report, 0 -> print packet event */)
{
    struct ol_txrx_prot_an_field_spec_t *field;

    if (recurse == 1 && prot_an->parent) {
        ol_txrx_prot_an_print(data, prot_an->parent, 1, recurse, report);
    }
    if (name_only) {
        qdf_print("%s ", prot_an->name);
        return;
    }
    if (report) {
        qdf_print("%*s%s: cnt = %d",
            prot_an->depth * 4, "", prot_an->name, prot_an->count);
    } else {
        qdf_print("%s: cnt = %d", prot_an->name, prot_an->count);
        field = prot_an->print.field_specs;
        while (field) {
            field->print(
                prot_an->depth * 4 + 2, field->name, data + field->offset);
            field = field->next;
        }
    }
    if (recurse == -1) {
        struct ol_txrx_prot_an_t *sub_prot;
        sub_prot = prot_an->parse.sub_prots;
        while (sub_prot) {
            ol_txrx_prot_an_print(data, sub_prot, name_only, recurse, report);
            sub_prot = sub_prot->next;
        }
        if (prot_an->parse.catch_all) {
            ol_txrx_prot_an_print(
                data, prot_an->parse.catch_all, name_only, recurse, report);
        }
    }
}

static void
ol_txrx_prot_an_log_recurse(ol_txrx_prot_an_handle prot_an, u_int8_t *data)
{
    prot_an->count++;
    if (prot_an->action == TXRX_PROT_ANALYZE_ACTION_PARSE) {
        struct ol_txrx_prot_an_t *sub_prot;
        u_int32_t classifier;

        /* look at what's encapsulated within this layer */
        qdf_assert(prot_an->parse.classifier);
        classifier =
            prot_an->parse.classifier(data, prot_an->parse.classifier_offset);
        sub_prot = prot_an->parse.sub_prots;
        while (sub_prot) {
            if (classifier == sub_prot->id) {
                break;
            }
            sub_prot = sub_prot->next;
        }
        if (!sub_prot) {
            sub_prot = prot_an->parse.catch_all;
        }
        if (sub_prot) {
            ol_txrx_prot_an_log_recurse(
                sub_prot, data + prot_an->parse.header_bytes);
        }
    } else if (prot_an->action == TXRX_PROT_ANALYZE_ACTION_PRINT) {
        int do_print = 0;
        /* check whether to actually do the print, based on periodicity specs */
        if (prot_an->print.period_type == TXRX_PROT_ANALYZE_PERIOD_COUNT) {
            if ((prot_an->count & prot_an->print.period_mask) == 0) {
                do_print = 1;
            }
        } else { /* TXRX_PROT_ANALYZE_PERIOD_TIME */
            u_int32_t time_now;
            u_int32_t delta_ticks;
            time_now = qdf_system_ticks();
            delta_ticks = time_now - prot_an->print.timestamp;
            if (prot_an->print.timestamp == 0 ||
                delta_ticks > prot_an->print.period_ticks)
            {
                prot_an->print.timestamp = time_now;
                do_print = 1;
            }
        }
        if (do_print) {
            ol_txrx_prot_an_print(
                data, prot_an,
                0 /* name only */, 1 /* recurse */, 0/* indent */);
        }
    }
}

static void
ol_txrx_field_print_u8(int prefix_len, const char *name, const u_int8_t *data)
{
    qdf_print("%*s%s: %u", prefix_len, "", name, data[0]);
}

static void
ol_txrx_field_print_u32(int prefix_len, const char *name, const u_int8_t *data)
{
    u_int32_t value;

    value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    qdf_print("%*s%s: %u", prefix_len, "", name, value);
}

#if 0
static void
ol_txrx_field_print_hex32(
    int prefix_len,
    const char *name,
    const u_int8_t *data)
{
    u_int32_t value;

    value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    qdf_print("%*s%s: %#x", prefix_len, "", name, value);
}
#endif

static void
ol_txrx_field_print_tcp_ack(
    int prefix_len,
    const char *name,
    const u_int8_t *data)
{
    u_int32_t value;

    value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];

    /* ACK flag: 5th bit of 5th byte after ack number field */
    if (data[5] & 0x10) {
        qdf_print("%*s%s: %u", prefix_len, "", name, value);
    }
}

static void
ol_txrx_field_print_icmp_ident(
    int prefix_len,
    const char *name,
    const u_int8_t *data)
{
    if (data[0] == 0x0 /* echo reply */ ||
        data[0] == 0x8 /* echo request */) {
        u_int16_t value;
        value = (data[4] << 8) | data[5];
        qdf_print("%*s%s: %u", prefix_len, "", name, value);
    }
}

static void
ol_txrx_field_print_icmp_seq_num(
    int prefix_len,
    const char *name,
    const u_int8_t *data)
{
    if (data[0] == 0x0 /* echo reply */ ||
        data[0] == 0x8 /* echo request */) {
        u_int16_t value;
        value = (data[6] << 8) | data[7];
        qdf_print("%*s%s: %u", prefix_len, "", name, value);
    }
}

static void
ol_txrx_field_print_arp_oper(
    int prefix_len,
    const char *name,
    const u_int8_t *data)
{
    u_int16_t value;

    value = (data[0] << 8) | data[1];

    qdf_print("%*s%s: %s", prefix_len, "", name,
        value == 1 ? "req" : value == 2 ? "reply" : "unknown");
}

u_int32_t
ol_txrx_prot_an_u8(const u_int8_t *data, int offset)
{
    return data[offset];
}

u_int32_t
ol_txrx_prot_an_u16(const u_int8_t *data, int offset)
{
    data += offset;
    return (data[0] << 8) | data[1];
}

/*--- API functions ---*/

ol_txrx_prot_an_handle
ol_txrx_prot_an_create_802_3(struct ol_txrx_pdev_t *pdev, const char *name)
{
    struct ol_txrx_prot_an_t *prot_an;

    prot_an = ol_txrx_prot_an_create(pdev, name);
    if (! prot_an) return NULL;

    prot_an->parse.classifier = ol_txrx_prot_an_u16;
    prot_an->parse.classifier_offset = 12;
    prot_an->parse.header_bytes = 14; /* assume no 802.1Q or SNAP/LLC */

    return prot_an;
}

ol_txrx_prot_an_handle
ol_txrx_prot_an_add_ipv4(
    struct ol_txrx_pdev_t *pdev,
    ol_txrx_prot_an_handle l2_prot)
{
    struct ol_txrx_prot_an_t *ipv4;
    ipv4 = ol_txrx_prot_an_create(pdev, "IPv4");
    if (!ipv4) {
        return NULL;
    }

    ipv4->id = 0x800;

    /* attach the IPv4 object to the parent */
    ol_txrx_prot_an_sub_prot_add(l2_prot, ipv4);

    /* change the parent object's action to "parse" */
    l2_prot->action = TXRX_PROT_ANALYZE_ACTION_PARSE;

    /*
     * Leave the IPv4 object's action as "count".
     * It will be changed to "parse" if sub-protocols (e.g. TCP)
     * are added under it.
     * Go ahead and specify the classifier function, in case this
     * gets promoted from "count" to "parse".
     */
    ipv4->parse.classifier = ol_txrx_prot_an_u8;
    ipv4->parse.classifier_offset = 9; /* "protocol" field in octet 9 */
    ipv4->parse.header_bytes = 20; /* assumes no options */

    /* could print out IP src and/or dest address fields */
#if 0
    ol_txrx_prot_an_field_add(
        pdev, ipv4, "src IP addr"
        12, /* src IP addr offset = 12 octets */
        ol_txrx_field_print_hex32);
    ol_txrx_prot_an_field_add(
        pdev, ipv4, "dest IP addr"
        16, /* dest IP addr offset = 16 octets */
        ol_txrx_field_print_hex32);
#endif

    return ipv4;
}

ol_txrx_prot_an_handle
ol_txrx_prot_an_add_ipv6(
    struct ol_txrx_pdev_t *pdev,
    ol_txrx_prot_an_handle l2_prot)
{
    struct ol_txrx_prot_an_t *ipv6;
    ipv6 = ol_txrx_prot_an_create(pdev, "IPv6");
    if (!ipv6) {
        return NULL;
    }

    ipv6->id = 0x86DD; /* 0x8600 - 0x86FF? */

    /* attach the IPv4 object to the parent */
    ol_txrx_prot_an_sub_prot_add(l2_prot, ipv6);

    /* change the parent object's action to "parse" */
    l2_prot->action = TXRX_PROT_ANALYZE_ACTION_PARSE;

    /*
     * Leave the IPv6 object's action as "count".
     * It will be changed to "parse" if sub-protocols (e.g. TCP)
     * are added under it.
     * Go ahead and specify the classifier function, in case this
     * gets promoted from "count" to "parse".
     */
/* Actually, set the action to "print every" */
ipv6->action = TXRX_PROT_ANALYZE_ACTION_PRINT;
ipv6->print.period_type = TXRX_PROT_ANALYZE_PERIOD_COUNT;
ipv6->print.period_mask = 0x0;

    ipv6->parse.classifier = ol_txrx_prot_an_u8;
    ipv6->parse.classifier_offset = 6; /* octet 6 ("next header") */
    ipv6->parse.header_bytes = 40;

    return ipv6;
}

ol_txrx_prot_an_handle
ol_txrx_prot_an_add_arp(
    struct ol_txrx_pdev_t *pdev,
    ol_txrx_prot_an_handle l2_prot)
{
    struct ol_txrx_prot_an_t *arp;
    arp = ol_txrx_prot_an_create(pdev, "ARP");
    if (!arp) {
        return NULL;
    }

    arp->id = 0x806;

    /* attach the IPv4 object to the parent */
    ol_txrx_prot_an_sub_prot_add(l2_prot, arp);

    /* change the parent object's action to "parse" */
    l2_prot->action = TXRX_PROT_ANALYZE_ACTION_PARSE;

    /* print every */
    arp->action = TXRX_PROT_ANALYZE_ACTION_PRINT;
    arp->print.period_type = TXRX_PROT_ANALYZE_PERIOD_COUNT;
    arp->print.period_mask = 0x0;

    ol_txrx_prot_an_field_add(
        pdev, arp, "operation", 6, ol_txrx_field_print_arp_oper);

    return arp;
}


ol_txrx_prot_an_handle
ol_txrx_prot_an_add_tcp(
    struct ol_txrx_pdev_t *pdev,
    ol_txrx_prot_an_handle l3_prot,
    enum ol_txrx_prot_an_period_type period_type,
    u_int32_t period_mask,
    u_int32_t period_msec)
{
    struct ol_txrx_prot_an_t *tcp;
    tcp = ol_txrx_prot_an_create(pdev, "TCP");
    if (!tcp) {
        return NULL; /* failure */
    }

    tcp->id = 0x6;

    /* attach the TCP object to the parent */
    ol_txrx_prot_an_sub_prot_add(l3_prot, tcp);

    /* change the parent object's action to "parse" */
    l3_prot->action = TXRX_PROT_ANALYZE_ACTION_PARSE;

    /* print certain fields of the TCP header */
    /* no need to specify a classifier or header_bytes */

    tcp->action = TXRX_PROT_ANALYZE_ACTION_PRINT;
    tcp->print.period_type = period_type;
    tcp->print.period_mask = period_mask;
    tcp->print.period_ticks = qdf_msecs_to_ticks(period_msec);
    tcp->print.timestamp = 0;

    /* allow the sequence number field to be printed */
    ol_txrx_prot_an_field_add(
        pdev, tcp, "seq num",
        4, /* sequence number offset = 4 octets */
        ol_txrx_field_print_u32);

    /* print the ack field only if the ACK flag bit is set */
    ol_txrx_prot_an_field_add(
        pdev, tcp, "ack",
        8, /* ack offset = 8 octets */
        ol_txrx_field_print_tcp_ack);

    return tcp;
}

ol_txrx_prot_an_handle
ol_txrx_prot_an_add_udp(
    struct ol_txrx_pdev_t *pdev,
    ol_txrx_prot_an_handle l3_prot,
    enum ol_txrx_prot_an_period_type period_type,
    u_int32_t period_mask,
    u_int32_t period_msec)
{
    struct ol_txrx_prot_an_t *udp;
    udp = ol_txrx_prot_an_create(pdev, "UDP");
    if (!udp) {
        return NULL; /* failure */
    }

    udp->id = 0x11;

    /* attach the TCP object to the parent */
    ol_txrx_prot_an_sub_prot_add(l3_prot, udp);

    /* change the parent object's action to "parse" */
    l3_prot->action = TXRX_PROT_ANALYZE_ACTION_PARSE;

    /* print certain fields of the TCP header */
    /* no need to specify a classifier or header_bytes */

    udp->action = TXRX_PROT_ANALYZE_ACTION_PRINT;
    udp->print.period_type = period_type;
    udp->print.period_mask = period_mask;
    udp->print.period_ticks = qdf_msecs_to_ticks(period_msec);
    udp->print.timestamp = 0;

    return udp;
}

ol_txrx_prot_an_handle
ol_txrx_prot_an_add_icmp(
    struct ol_txrx_pdev_t *pdev,
    ol_txrx_prot_an_handle l3_prot,
    enum ol_txrx_prot_an_period_type period_type,
    u_int32_t period_mask,
    u_int32_t period_msec)
{
    struct ol_txrx_prot_an_t *icmp;
    icmp = ol_txrx_prot_an_create(pdev, "ICMP");
    if (!icmp) {
        return NULL; /* failure */
    }

    icmp->id = 0x1;

    /* attach the TCP object to the parent */
    ol_txrx_prot_an_sub_prot_add(l3_prot, icmp);

    /* change the parent object's action to "parse" */
    l3_prot->action = TXRX_PROT_ANALYZE_ACTION_PARSE;

    /* print certain fields of the TCP header */
    /* no need to specify a classifier or header_bytes */

    icmp->action = TXRX_PROT_ANALYZE_ACTION_PRINT;
    icmp->print.period_type = period_type;
    icmp->print.period_mask = period_mask;
    icmp->print.period_ticks = qdf_msecs_to_ticks(period_msec);
    icmp->print.timestamp = 0;

    ol_txrx_prot_an_field_add(
        pdev, icmp, "type", 0, ol_txrx_field_print_u8);
    ol_txrx_prot_an_field_add(
        pdev, icmp, "ident", 0, ol_txrx_field_print_icmp_ident);
    ol_txrx_prot_an_field_add(
        pdev, icmp, "seq num", 0, ol_txrx_field_print_icmp_seq_num);

    return icmp;
}

void
ol_txrx_prot_an_log(ol_txrx_prot_an_handle prot_an, qdf_nbuf_t netbuf)
{
    u_int8_t *data;
    data = qdf_nbuf_data(netbuf);
    ol_txrx_prot_an_log_recurse(prot_an, data);
}

void
ol_txrx_prot_an_display(ol_txrx_prot_an_handle prot_an)
{
    int name_only = 0;
    int recurse = -1; /* recurse down */
    int report = 1;

    /* first print our own display */
    qdf_print("protocol analyzer summary report:");
    ol_txrx_prot_an_print(NULL, prot_an, name_only, recurse, report);
}

void
ol_txrx_prot_an_free(ol_txrx_prot_an_handle prot_an)
{
    struct ol_txrx_prot_an_t *sub_prot;
    struct ol_txrx_prot_an_field_spec_t *field_spec;

    qdf_assert(prot_an);

    /* first recurse to free any child protocol analyzers */
    if (prot_an->parse.catch_all) {
        ol_txrx_prot_an_free(prot_an->parse.catch_all);
    }
    sub_prot = prot_an->parse.sub_prots;
    while (sub_prot) {
        struct ol_txrx_prot_an_t *next;
        next = sub_prot->next;
        ol_txrx_prot_an_free(sub_prot);
        sub_prot = next;
    }

    /* free any field specs */
    field_spec = prot_an->print.field_specs;
    while (field_spec) {
        struct ol_txrx_prot_an_field_spec_t *next;
        next = field_spec->next;
        qdf_mem_free(field_spec);
        field_spec = next;
    }

    /* free the prot_an object itself */
    qdf_mem_free(prot_an);
}

#endif /* ENABLE_TXRX_PROT_ANALYZE */
