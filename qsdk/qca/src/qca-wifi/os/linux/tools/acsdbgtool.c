/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Description:
 * Sends user-defined beacon and channel events from an acsreport-like
 * file and sends it to the driver to the ACS debug framework which injects
 * these parameters into the algorithm to give you the ability to analyze
 * in-depth
 *
 * Usage:
 * acsdbgtool athX --file|-f <filename>
 * acsdbgtool athX --help|-h
 */

#include <qcatools_lib.h>

#define ACSDBG_SUCCESS           0
#define ACSDBG_ERROR             -1

#define MAX_INTERFACE_LEN        20
#define MAX_FILENAME_LEN         255
#define MAX_BEACON_COUNT         100
#define MAX_SSID_LEN             32
#define MAX_REPORT_LINE_LEN      200
#define MAC_ADDR_SIZE            6

#define BCN_PARAM_NUM            13
#define CHAN_EVENT_PARAM_NUM      4

#define BCN_CAPTURE_START         "BEACONS"
#define CHAN_EVENT_CAPTURE_START  "FREQUENCY"

#define CHAN_BAND_5G              5
#define CHAN_BAND_2G              2
#define CHAN_NUM_5G              25
#define CHAN_NUM_2G              11

#define ACS_DBG_NL80211_CMD_SOCK_ID DEFAULT_NL80211_CMD_SOCK_ID
#define ACS_DBG_NL80211_EVENT_SOCK_ID DEFAULT_NL80211_EVENT_SOCK_ID

#define strchk(a,b) ((strncasecmp(a,b,sizeof(b)-1) == 0))

/* Application Parameters */
struct raw_bcn_event {
    int8_t  nbss;
    uint32_t channel_number;
    int32_t  rssi;
    uint8_t  bssid[MAC_ADDR_SIZE];
    uint8_t  ssid[MAX_SSID_LEN];
    uint32_t phymode;
    uint8_t  sec_chan_seg1;
    uint8_t  sec_chan_seg2;
    uint8_t  srpen;
};

struct raw_chan_event {
    int8_t  nchan;
    uint8_t  channel_number;
    uint8_t  channel_load;
    int16_t  noise_floor;
    uint32_t txpower;
};

struct socket_context sock_ctx = {0};

/* Basic Prototype */
static int  validate_command(int argc, char **argv, char *ifname);
static void print_usage(void);
static int  validate_file(char *file);
static int  parse_report(FILE *fp, char *ifname);

/* Beacon Specific Prototype */
static int    capture_bcn_report(char *line, int8_t *nbss, FILE *fp);
static struct raw_bcn_event * create_bcn_buf(int8_t nbss);
static void   cleanup_bcn(struct raw_bcn_event *bcn);
static int    record_bcn(struct raw_bcn_event *bcn, int8_t nbss, char *line,FILE *fp);

/* Channel Event Specific Prototype */
static int    capture_chan_report(char *line, int8_t *nchan, FILE *fp);
static struct raw_chan_event * create_chan_event_buf(int8_t nchan);
static void   cleanup_chan_event(struct raw_chan_event *chan);
static int    record_chan_event(struct raw_chan_event *chan, int8_t nchan, char *line, FILE *fp);

int main(int argc, char *argv[])
{
    FILE *fp = NULL;
    char ifname[MAX_INTERFACE_LEN]  = {0};
    int8_t ret = 0;
    uint8_t is_sock_init = 0;

    sock_ctx.cfg80211 = get_config_mode_type();


    if (validate_command(argc, argv, ifname))
        ret = ACSDBG_ERROR;

    if (!ret && (strchk(argv[2], "--help") || strchk(argv[2], "-h"))) {
        /* Help Command */
        print_usage();
        ret = ACSDBG_SUCCESS;

    } else if (!ret && (strchk(argv[2], "--file") || strchk(argv[2], "-f"))) {
        /* Actionable Parsing Command */
        if (validate_file(argv[3])) {
            fprintf(stderr, "File information is invalid\n");
            ret = ACSDBG_ERROR;
        }

        if (!ret)
            fp = fopen(argv[3], "r");

        if (!ret && !fp) {
            fprintf(stderr, "File cannot be opened\n");
            ret = ACSDBG_ERROR;
        }

        if (!ret && init_socket_context(&sock_ctx, DEFAULT_NL80211_CMD_SOCK_ID,
                DEFAULT_NL80211_EVENT_SOCK_ID) && (is_sock_init = 1)) {
            fprintf(stderr, "Socket could not be initialized\n");
            ret = ACSDBG_ERROR;
        }

        /* Parsing the report */
        if (!ret && parse_report(fp, ifname)) {
            fprintf(stderr, "Report cannot be parsed\n");
            ret =  ACSDBG_ERROR;
        }

        /*
         * Post-Application Clean-Up:
         */
        if (is_sock_init) {
            destroy_socket_context(&sock_ctx);
        }

        if (fp) {
            fclose(fp);
        }

    } else {
        fprintf(stderr, "Invalid command entered\n");
        print_usage();
        ret = ACSDBG_ERROR;
    }

    return ret;
}

/*
 * validate_command:
 * Checks if the command is valid to continue further into the application
 * In addition to the above, it puts the interface name in the "ifname".
 *
 * Parameters:
 * argc: The number of arguments in the command (including the application name)
 * argv: Character double pointer pointing to each argument as a string array
 *
 * Return:
 *  0 - Success
 * -1 - Failure
 */
static int validate_command(int argc, char **argv, char *ifname)
{
    int ret = 0;

    if (argc < 3) {
        ret = ACSDBG_ERROR;
    }

    if (!ret && strlcpy(ifname, argv[1], strlen(argv[1]) + 1) >= MAX_INTERFACE_LEN) {
        fprintf(stderr, "Interface name is too long\n");
        ret = ACSDBG_ERROR;
    }

    return ret;
}

/*
 * print_usage:
 * Prints the general usage of the application. This function gets called if
 * the user sends in a help command or if the user has incorrectly sent in his
 * command
 *
 * Parameters:
 * None
 *
 * Return:
 * None
 */
static void print_usage(void)
{
    fprintf(stdout,
            "Usage: \n"
            "acsdbgtool athX --file|-f <filename>\n"
            "acsdbgtool athX --help|-h\n");
}

/*
 * validate_file:
 * Checks if the file path is valid and if it's valid, checks to see if it's
 * length is within limits set by the application.
 *
 * Parameters:
 * file: Pointer to the filepath
 *
 * Return:
 *  0 - Success
 * -1 - Failure
 */
static int validate_file(char *file)
{
    int ret = 0;

    if (!file) {
        /* File information is invalid or not present */
        ret = ACSDBG_ERROR;
    }

    if (!ret && (strlen(file) > MAX_FILENAME_LEN)) {
        /* File path is too long */
        ret = ACSDBG_ERROR;
    }

    return ret;
}

/*
 * parse_report:
 * Parses the report CSV file from the userspace and places it in a buffer
 * to be sent into the driver.
 *
 * Parameters:
 * fp    : Pointer to the CSV report file
 * ifname: Name of the interface sent when the user calls the application from
 *         the command line
 *
 * Returns:
 *  0 - Success
 * -1 - Failure
 */
static int parse_report(FILE *fp, char *ifname)
{
    char line[MAX_REPORT_LINE_LEN];
    int8_t nbss = -1, nchan = -1;
    int8_t ret = 0;
    struct raw_bcn_event *bcn = NULL;
    struct raw_chan_event *chan = NULL;

    if (!capture_bcn_report(line, &nbss, fp)) {
        /* Creating beacon buffer */
        bcn = create_bcn_buf(nbss);

        if (!bcn)
            ret = ACSDBG_ERROR;

        /* Recording the beacon line */
        if (!ret && record_bcn(bcn, nbss, line, fp)) {
            ret = ACSDBG_ERROR;
        }

        /* Sending all the beacons to the driver */
#if ATH_ACS_DEBUG_SUPPORT
        if (!ret) {
            send_command(&sock_ctx, ifname, bcn, nbss*sizeof(struct raw_bcn_event), NULL, QCA_NL80211_VENDOR_SUBCMD_ACSDBGTOOL_ADD_BCN, 0);
        }

        cleanup_bcn(bcn);
    } else {
        fprintf(stdout, "No beacon events found\n");
        send_command(&sock_ctx, ifname, bcn, 0, NULL, QCA_NL80211_VENDOR_SUBCMD_ACSDBGTOOL_ADD_BCN, 0);
        ret = ACSDBG_SUCCESS;
    }
#endif

    if (!capture_chan_report(line, &nchan, fp)) {
        /* Creating channel events buffer */
        chan = create_chan_event_buf(nchan);

        if (!chan)
            ret = ACSDBG_ERROR;
        /* Recording the channel events per line */

        if (!ret && record_chan_event(chan, nchan, line, fp)) {
            ret = ACSDBG_ERROR;
        }

#if ATH_ACS_DEBUG_SUPPORT
        /* Sending all the channel events to the driver */
        if (!ret) {
            send_command(&sock_ctx, ifname, chan, nchan*sizeof(struct raw_chan_event), NULL, QCA_NL80211_VENDOR_SUBCMD_ACSDBGTOOL_ADD_CHAN, 0);
        }

        cleanup_chan_event(chan);
    } else {
        fprintf(stdout, "No channel events found\n");
        send_command(&sock_ctx, ifname, chan, 0, NULL, QCA_NL80211_VENDOR_SUBCMD_ACSDBGTOOL_ADD_CHAN, 0);
        ret = ACSDBG_SUCCESS;
    }
#endif

    return ret;
}

/*
 * capture_bcn_report:
 * Finds the beacon section in the report to begin the parsing of beacon
 * events. Also, it finds & reads the number of beacons which are being
 * described in the report.
 *
 * Parameters:
 * line: Pointer to the string that contains a specific line from the report
 * nbss: Pointer to the value for the number of beacons to be sent to the driver
 * fp  : Pointer to the CSV report file.
 *
 * Return:
 *  0 - Success
 * -1 - Failure
 */
static int capture_bcn_report(char *line, int8_t *nbss, FILE *fp)
{
    int8_t ret = 0;

    while (fgets(line, MAX_REPORT_LINE_LEN, fp)) {
        if (strchk(line, BCN_CAPTURE_START)) {
            if (sscanf(line, "BEACONS = %hhu\n", nbss) != 1) {
                ret = ACSDBG_ERROR;
            }
            break;
        }
    }

    if (*nbss <= 0)
        ret = ACSDBG_ERROR;

    return ret;
}

/*
 * capture_chan_report:
 * Finds the channel events section in the report to begin the parsing of
 * channel events. Also, it finds & reads the number of channel events which
 * are being described in the report.
 *
 * Parameters:
 * line : Pointer to the string that contains a specific line from the report
 * nchan: Pointer to the value for the nubmer of beacons to be sent to the
 *        driver
 * fp   : Pointer to the CSV report file.
 *
 * Returns:
 *  0 - Success
 * -1 - Failure
 */
static int capture_chan_report(char *line, int8_t *nchan, FILE *fp)
{
    int8_t ret = 0;

    while (fgets(line, MAX_REPORT_LINE_LEN, fp)) {
        if (strchk(line, CHAN_EVENT_CAPTURE_START)) {
            if (!sscanf(line, "FREQUENCY = %hhu\n", nchan)) {
                ret = ACSDBG_ERROR;
            }

            if (!ret && *nchan == CHAN_BAND_5G)
                *nchan = CHAN_NUM_5G;
            else if(!ret && *nchan == CHAN_BAND_2G)
                *nchan = CHAN_NUM_2G;
            else
                ret = ACSDBG_ERROR;

            break;
        }
    }

    if (*nchan <= 0)
        ret = ACSDBG_ERROR;

    return ret;
}

/*
 * create_bcn_buf:
 * Creates the buffer that gets sent to the driver to the ACS debug framework
 * for injection into the ACS algorithm
 *
 * Parameters:
 * nbss: Value for the number of beacons to send into the driver.
 *
 * Return:
 *  Pointer to allocated memory
 */
static struct raw_bcn_event * create_bcn_buf(int8_t nbss)
{
    struct raw_bcn_event *ptr = NULL;

    if (nbss > 0) {
        ptr = malloc(nbss *
                        sizeof(struct raw_bcn_event) +
                        sizeof(int32_t));
    }

    return ptr;
}

/*
 * create_chan_event_buf:
 * Creates the buffer that gets sent to the driver to the ACS debug framework
 * for injection into the ACS algorithm
 *
 * Parameters:
 * nchan: Value for the number of channel events to send into the driver.
 *
 * Return:
 *  Pointer to allocated memory
 */
static struct raw_chan_event * create_chan_event_buf(int8_t nchan)
{
    struct raw_chan_event *ptr = NULL;

    if (nchan > 0) {
        ptr = malloc(nchan *
                     sizeof(struct raw_chan_event) +
                     sizeof(int32_t));

    }

    return ptr;
}

/*
 * cleanup_bcn:
 * Cleans up all the allocated memory which was used for the beacon events.
 *
 * Parameters:
 * bcn: Pointer to the buffer location containing all the beacon events
 *
 * Return:
 *  None
 */
static void cleanup_bcn(struct raw_bcn_event *bcn)
{
    if (bcn)
        free(bcn);

}

/*
 * cleanup_chan_event:
 * Cleans up all the allocated memory which was used for the channel events.
 *
 * Parameters:
 * chan: Pointer to the buffer location containing all the channel events
 *
 * Return:
 *  None
 */
static void cleanup_chan_event(struct raw_chan_event *chan)
{
    if (chan)
        free(chan);
}

/*
 * record_bcn:
 * Records each line in the CSV which corresponds to a unique beacon's
 * information. The information includes - BSSID, channel number, phymode,
 * rssi, secondary_channel segments and SRP
 *
 * Parameters:
 * bcn : Pointer to the buffer for beacon events
 * nbss: Value for the number of beacons to send into the driver
 * line: Pointer to the line that contains the unique beacon information
 * fp  : Pointer to the CSV report file
 *
 * Return:
 *  0 - Success
 * -1 - Failure
 */
static int record_bcn(struct raw_bcn_event *bcn, int8_t nbss, char *line, FILE *fp)
{
    uint8_t ix;
    uint8_t ret = 0;

    for (ix = 0; ix < nbss && fgets(line, MAX_REPORT_LINE_LEN, fp); ix++) {
        if (sscanf(line,
                   "%2x:%2x:%2x:%2x:%2x:%2x ,%u ,%u ,%u ,%u ,%u ,%u ,%s",
                   (unsigned int *) &bcn[ix].bssid[0],
                   (unsigned int *) &bcn[ix].bssid[1],
                   (unsigned int *) &bcn[ix].bssid[2],
                   (unsigned int *) &bcn[ix].bssid[3],
                   (unsigned int *) &bcn[ix].bssid[4],
                   (unsigned int *) &bcn[ix].bssid[5],
                   &bcn[ix].channel_number,
                   &bcn[ix].phymode,
                   &bcn[ix].rssi,
                   (unsigned int *) &bcn[ix].sec_chan_seg1,
                   (unsigned int *) &bcn[ix].sec_chan_seg2,
                   (unsigned int *) &bcn[ix].srpen,
                   bcn[ix].ssid) != BCN_PARAM_NUM) {
            ret = ACSDBG_ERROR;
        }

        if (!ret) {
            if (strlen((char *) bcn->ssid) >= MAX_SSID_LEN) {
                fprintf(stdout, "SSID length is too long (max %d chars)\n", MAX_SSID_LEN);
                ret = ACSDBG_ERROR;
            }
        }

        if (ret) {
            break;
        }

        bcn[ix].nbss = nbss;
    }
    return ret;
}

/*
 * record_chan_event:
 * Records each line in the CSV which corresponds to a unique channel's
 * information. The information includes - Channel number, noise_floor value for
 * that channel, max Tx power value and channel load.
 *
 * Parameters:
 * chan : Pointer to the buffer for channel events
 * nchan: Value for the number of channel events to send into the driver
 * line : Pointer to the line that contains the unique channel information
 * fp   : Pointer to the CSV report file
 *
 * Return:
 *  0 - Success
 * -1 - Failure
 */
static int record_chan_event(struct raw_chan_event *chan, int8_t nchan, char *line, FILE *fp)
{
    uint8_t ix;
    uint8_t ret = 0;

    for (ix = 0; ix < nchan && fgets(line, MAX_REPORT_LINE_LEN, fp); ix++) {
        if (sscanf(line,
                    "%hhu ,%hd ,%u ,%hhu",
                    &chan[ix].channel_number,
                    &chan[ix].noise_floor,
                    &chan[ix].txpower,
                    &chan[ix].channel_load) != CHAN_EVENT_PARAM_NUM) {
            ret = ACSDBG_ERROR;
        }

        if (ret)
            break;

        chan[ix].nchan = nchan;
    }

    return ret;
}
