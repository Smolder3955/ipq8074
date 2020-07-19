/*
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include "common.h"
#include "ven_cmd_tool.h"
#include <libxml/xmlreader.h>

const char* const short_options = "h:i:f:";

/* These are node type definitions from xmlreader.h library */
#define NODE_TYPE_ELEMENT 1
#define NODE_TYPE_COMMENT 8
#define NODE_TYPE_END_ELEMENT 15

#if __WIN__
/* This command is at index 63(?), getlongoption returns 63
 * if command is not available.
 * whatever command is there at index 63 can not work.
 *
 */
#define RESERVE_ASCII_VALUE 63
#define ARGV_COMMAND_OFFSET 6
char reservecmd[FILE_NAME_LEN];
int display_all_commands = 0; /* switch to display all avaialble commands */
#endif

static enum attr_type getAttrTypeenum(char *value)
{
    enum attr_type type = INVALID_ATTR;
    if(!strcasecmp(value, "flag")) {
        type = FLAG;
    } else if(!strcasecmp(value, "u8")) {
        type = U8;
    } else if(!strcasecmp(value, "u16")) {
        type = U16;
    } else if(!strcasecmp(value, "u32")) {
        type = U32;
    } else if(!strcasecmp(value, "u64")) {
        type = U64;
    } else if(!strcasecmp(value, "s8")) {
        type = S8;
    } else if(!strcasecmp(value, "s16")) {
        type = S16;
    } else if(!strcasecmp(value, "s32")) {
        type = S32;
    } else if(!strcasecmp(value, "s64")) {
        type = S64;
    } else if(!strcasecmp(value, "string")) {
        type = STRING;
    } else if(!strcasecmp(value, "mac_addr")) {
        type = MAC_ADDR;
    } else if(!strcasecmp(value, "blob")) {
        type = BLOB;
    } else if(!strcasecmp(value, "nested")) {
        type = NESTED;
    } else {
        printf("unknown attr type : %s\n", value);
    }

    return type;
}


static enum cmd_type getCmdType(char *value)
{
    enum cmd_type type = INVALID_CMD;

    if(!strcasecmp(value, "VendorCmd")) {
        type = VEN_CMD;
    } else if(!strcasecmp(value, "VendorRsp")) {
        type = RESPONSE;
    } else if(!strcasecmp(value, "VendorEvent")) {
        type = VEN_EVENT;
    } else if(!strcasecmp(value, "NLEvent")) {
        type = NL_EVENT;
    } else if(!strcasecmp(value, "switch")) {
        type = SWITCH;
    } else if(!strcasecmp(value, "case")) {
        type = CASE;
    } else if(!strcasecmp(value, "attribute")) {
        type = ATTRIBUTE;
    } else {
        printf("unknown cmd type : %s\n", value);
    }

    return type;
}


static void parseDefaultAttrList(union default_values *default_val,
    const char *data)
{
    struct default_list_t *node=NULL, *list = NULL;
    char *values, *id, delim[2] = {',', 0};
    char *saveptr;

    values = (char *)malloc(strlen(data) + 1);
    if (!values) {
        printf("Failed to allocate memory for values\n");
        return;
    }
    memcpy(values, data, strlen(data)+1);

#ifndef __WIN__
    printf("Default attributes: %s\n", values);
#endif /* __WIN__ */
    id = strtok_r(values, delim, &saveptr);
    while (id) {
        node = (struct default_list_t *)malloc(sizeof(struct default_list_t));
        if (!node) {
            printf("Failed to allocate memory for node\n");
            return;
        }
        node->default_id = atoi(id);
        node->next = list;
        list = node;
        id = strtok_r(NULL, delim, &saveptr);
    }

    default_val->default_list = list;

    free(values);
}

static char *enumToString(enum option_type type)
{
    char *cmdtype;
    switch (type)
    {
        case O_HELP:
            cmdtype = "HELP";
            break;
        case O_COMMAND:
            cmdtype = "START_CMD";
            break;
        case O_END_CMD:
            cmdtype = "END_CMD";
            break;
        case O_RESPONSE:
            cmdtype = "RESPONSE";
            break;
        case O_END_RSP:
            cmdtype = "END_RSP";
            break;
        case O_EVENT:
            cmdtype = "EVENT";
            break;
        case O_END_EVENT:
            cmdtype = "END_EVENT";
            break;
        case O_NESTED:
            cmdtype = "NESTED";
            break;
        case O_SWITCH:
            cmdtype = "SWITCH";
            break;
        case O_CASE:
            cmdtype = "CASE";
            break;
        case O_NESTED_AUTO:
            cmdtype = "NESTED_AUTO";
            break;
        case O_END_ATTR:
            cmdtype = "END_ATTR";
            break;
        case O_END_NESTED_AUTO:
            cmdtype = "END_NESTED_AUTO";
            break;
        default:
            printf("invalid enum value : %d\n", type);
            cmdtype = NULL;
    }
    return cmdtype;
}


static void populateCLIOptions(struct cmd_params *cmd)
{
    enum option_type j;
    char *cmdname;

    for (j=O_HELP; j< O_CMD_LAST; j++, cmd->num_entries++) {
        cmd->entry[cmd->num_entries].ctrl_option = j;

        cmdname = enumToString(j);
        if (!cmdname) {
            printf("command name not found option_type : %d\n", j);
            return;
        }
        cmd->long_options[cmd->num_entries].name =
                                            (char *)malloc(strlen(cmdname)+1);
        memcpy((char *)cmd->long_options[cmd->num_entries].name,
               cmdname,
               strlen(cmdname)+1);
        cmd->long_options[cmd->num_entries].val = cmd->num_entries;
        /* No need to populate other elements for now */
    }
}

/*
 * This function handles collecting all supported commands and
 * also attributes of the command given by user if there's a match.
 */

int parseXMLCommands(xmlTextReaderPtr reader, struct cmd_params *cmds[],
                     char* user_command, int print_commands)
{
    const xmlChar *name = NULL, *value = NULL;
    int ret, i = 0;
    struct cmd_params *cmd;

    /* Advance reader to first element */
    ret = xmlTextReaderRead(reader);
    name = xmlTextReaderConstName(reader);

    /* Loop over entire document until EOF */
    while ( ret == 1 ) {
        /* Start with reading only elements (commands/response)
         * instead of all lines.
         */
        if (xmlTextReaderNodeType(reader) == NODE_TYPE_ELEMENT)  {
            name = xmlTextReaderConstName(reader);
            /* Proceed only if it is a vendor command */
            if (xmlStrcmp(name, (const xmlChar *) "VendorCmd") == 0) {
                /* This will be executed in second iteration,
                 * when we are searching for user command.
                 */
                if (user_command) {
                    /* Checking for command match */
                    if (xmlStrcmp((const xmlChar *) user_command,
                                  xmlTextReaderGetAttribute(reader,
                                         (const xmlChar *)"name")) == 0) {
                        /* Match found for given command.
                         * So store all attributes/responses.
                         */
                        /* Varible decides where to store attributes.
                         * 0 for command, 1 for response
                         */
                        int cmd_resp = 0;
                        /*Loop over until all attribute/responses are stored*/
                        do {
                            if (xmlStrcmp(name,
                                (const xmlChar *) "VendorCmd") == 0) {
                                /* break condition when we enconter next
                                 * vendor command after a response
                                 */
                                if (cmd_resp)
                                    break;
                            } else if (xmlStrcmp(name,
                                        (const xmlChar *) "VendorRsp") == 0) {
                                cmd_resp = 1;
                                if (xmlTextReaderNodeType(reader) ==
                                        NODE_TYPE_END_ELEMENT) {
                                    /* Skip copying the empty closing tag
                                     * of response
                                     */
                                    ret = xmlTextReaderRead(reader);
                                    name = xmlTextReaderConstName(reader);
                                    continue;
                                }
                            } else if (xmlStrcmp(name,
                                        (const xmlChar *) "WCN_VenTool") == 0) {
                                    break;
                            }
                            /* cmds has structures for commands, responses and
                             * events in it. cmd_resp decides where to
                             * store the attributes.
                             */
                            cmd = cmds[cmd_resp];
                            if(!cmd) {
                                printf("cmd is NULL\n");
                                return -1;
                            }
                            i = cmd->num_entries;
                            cmd->entry[i].xml_entry = getCmdType((char *)name);

                            /* Storing attribute's name */
                            value = xmlTextReaderGetAttribute(reader,
                                        (const xmlChar *)"name");
                            if(value) {
                                cmd->long_options[i].name =
                                    (char *)malloc(strlen((const char *)value)
                                                   + 1);
                                if (!cmd->long_options[i].name) {
                                    printf("Failed to allocate memory"
                                            " for name\n");
                                }
                                memcpy((char *)cmd->long_options[i].name,
                                       value, strlen((const char *)value)+1);
                                cmd->long_options[i].val = i;
                            }

                            /* Storing attribute's ID */
                            value = xmlTextReaderGetAttribute(reader,
                                        (const xmlChar *) "ID");
                            if(value){
                                cmd->attr_id[i] = atoi((const char *)value);
                            }

                            /* Storing attribute's type */
                            value = xmlTextReaderGetAttribute(reader,
                                        (const xmlChar *)"TYPE");
                            if(value) {
                                cmd->data_type[i] =
                                    getAttrTypeenum((char *)value);
                                if (cmd->data_type[i] >= U8 &&
                                    cmd->data_type[i] <= BLOB)
                                    cmd->long_options[i].has_arg = 1;
                            }

                            /* Storing vendor command's default value */
                            value = xmlTextReaderGetAttribute(reader,
                                        (const xmlChar *) "DEFAULTS");
                            if(value) {
                                parseDefaultAttrList(&cmd->default_val[i],
                                                     (const char *)value);
                            }

                            /* Storing attribute's default value */
                            value = xmlTextReaderGetAttribute(reader,
                                        (const xmlChar *)"DEFAULT");
                            if(value) {
                                cmd->default_val[i].val =
                                    (char *)malloc(strlen((const char *)value)
                                                          + 1);
                                if (!cmd->default_val[i].val) {
                                    printf("Failed to allocate memory"
                                            " for default val\n");
                                }
                                memcpy((char *)cmd->default_val[i].val, value,
                                       strlen((const char *)value)+1);
                            }

                            /* Storing attribute's maximum attribute count */
                            value = xmlTextReaderGetAttribute(reader,
                                        (const xmlChar *) "ATTR_MAX");
                            if(value) {
                                cmd->attr_max[i] = atoi((const char *)value);
                            }

                            /* Read next line */
                            xmlTextReaderRead(reader);
                            if (!reader)
                                break;
                            name = xmlTextReaderConstName(reader);
                            /* Entry has been read but the content/name of the
                             * entry is NULL. Exit reading the file
                             */
                            if (!name)
                                break;
                            cmd->num_entries++;

                        } while (1);
                        /* Break out of outer while loop as well
                         * we've read everything required
                         */
                        ret = 0;
                        break;
                    }
                } else {
                    /* Excecuted in first iteration where we read
                     * vendor commands only.
                     */
                    cmd = cmds[0];
                    i = cmd->num_entries;
                    cmd->entry[i].xml_entry = getCmdType((char *)name);

                    /* Storing attribute's name */
                    value = xmlTextReaderGetAttribute(reader,
                                (const xmlChar *) "name");
                    if(value) {
                        cmd->long_options[i].name =
                                (char *)malloc(strlen((const char *)value)+1);
                        if (!cmd->long_options[i].name) {
                            printf("Failed to allocate memory for name\n");
                        }
                        memcpy((char *)cmd->long_options[i].name, value,
                                strlen((const char *)value)+1);
                        cmd->long_options[i].val = i;
                    }

                    /* Storing attribute's ID */
                    value = xmlTextReaderGetAttribute(reader,
                                (const xmlChar *) "ID");
                    if(value) {
                        cmd->attr_id[i] = atoi((const char *)value);
                    }

                    /* Storing attribute's type */
                    value = xmlTextReaderGetAttribute(reader,
                                (const xmlChar *) "TYPE");
                    if(value) {
                        cmd->data_type[i] = getAttrTypeenum((char *)value);
                        if (cmd->data_type[i] >= U8 &&
                            cmd->data_type[i] <= BLOB)
                            cmd->long_options[i].has_arg = 1;
                    }

                    /* Storing vendor command's default value */
                    value = xmlTextReaderGetAttribute(reader,
                                (const xmlChar *) "DEFAULTS");
                    if(value) {
                        parseDefaultAttrList(&cmd->default_val[i],
                                (const char *)value);
                    }

                    /* Storing attribute's default value */
                    value = xmlTextReaderGetAttribute(reader,
                                (const xmlChar *) "DEFAULT");
                    if(value) {
                        cmd->default_val[i].val =
                                (char *)malloc(strlen((const char *)value)+1);
                        if (!cmd->default_val[i].val) {
                            printf("Failed to allocate memory"
                                   " for default val\n");
                        }
                        memcpy((char *)cmd->default_val[i].val, value,
                               strlen((const char *)value)+1);
                    }

                    /* Storing attribute's maximum attribute count */
                    value = xmlTextReaderGetAttribute(reader,
                                (const xmlChar *) "ATTR_MAX");
                    if(value) {
                        cmd->attr_max[i] = atoi((const char *)value);
                    }
#ifdef __WIN__
                    if (print_commands && display_all_commands) {
                        if (!i) {
                            printf(" Available vendor commands: \n");
                            printf("\t %-30s \n",
                                    cmd->long_options[i].name);
                        } else {
                            printf("\t %-30s \n",
                                    cmd->long_options[i].name);
                        }
                    }

                    if ((print_commands &&
                                (cmd->num_entries == RESERVE_ASCII_VALUE))) {
                        memcpy(reservecmd, cmd->long_options[i].name,
                                strlen(cmd->long_options[i].name));
                    }
#endif
                    cmd->num_entries++;
                }
            } else if(xmlStrcmp(name, (const xmlChar *) "VendorEvent") == 0) {
                /* Yet to handle VendorEvent */
            } else if(xmlStrcmp(name, (const xmlChar *) "VendorRsp") == 0) {
                /* Yet to handle VendorRsp */
            } else if(xmlStrcmp(name, (const xmlChar *) "Attribute") == 0) {
                /* Attribute tags are handled above, no need to handle here */
            } else
                printf("Invalid Entry %s , in funtion %s:line: %d\n", name, __func__, __LINE__);
        }
        ret = xmlTextReaderRead(reader); /* move to next node */
    }
    return 0;
}

/* ParseXMLDocReader is called twice in this file.
 * First time is to store all the commands and print
 * if no command argument is given. Second itearation
 * is to compare with user input to find a match.
 */

static status parseXMLDocReader(char *config_file, struct cmd_params *cmds[],
        int num, char *user_command)
{
    struct cmd_params *cmd;
    xmlTextReaderPtr reader;
    const xmlChar *name = NULL;

    if (num < 2)
        return INVALID_ARG;
    cmd = cmds[0];

    /* Stream the file with no blanks flag to discard null nodes
     * in xml automatically.
     */
    reader = xmlReaderForFile(config_file, "UTF-8",
                              XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
    if (reader == NULL)
        return INVALID_ARG;

    /* Skip comments and read until first node */
    do {
        xmlTextReaderRead(reader);
    } while(xmlTextReaderNodeType(reader) == NODE_TYPE_COMMENT);

    name = xmlTextReaderConstName(reader);
    if (name == NULL)
        return INVALID_ARG;

    if(xmlStrcmp(name, (const xmlChar *) "WCN_VenTool")) {
        printf("%s not a Ven xml\n", config_file);
        xmlFreeTextReader(reader);
        return INVALID_ARG;
    }

    populateCLIOptions(cmd);

    /* Populate all commands & elments to internal data structure */
    parseXMLCommands(reader, cmds, user_command, 1);

    cmd->long_options[cmd->num_entries].name = NULL;
    cmd->long_options[cmd->num_entries].has_arg = 0;
    cmd->long_options[cmd->num_entries].flag = 0;
    cmd->long_options[cmd->num_entries].val = 0;

    xmlFreeTextReader(reader);
    return SUCCESS;
}

/* Parse command inputs;
 * This API expects only long options as described in the
 * xml file. Any inputs out of xml file will be discarded.
 */
static void parseCmdinputs(struct nlIfaceInfo *info, int argc, char **argv,
                    struct cmd_params *cmd, int *option_index,
                    common_data data)
{
    int c, ret;
    struct nl_msg *nlmsg;
    struct nlattr *nlVenData = NULL;
    struct cmd_params command, response, event;
    struct cmd_params *cur_cmds[NUM_NLMSG_TYPES] =
                                    {&command, &response, &event};
    struct resp_event_info resp_info;

    memset(&command, 0, sizeof(struct cmd_params));
    memset(&response, 0, sizeof(struct cmd_params));
    memset(&event, 0, sizeof(struct cmd_params));
    memset(&resp_info, 0, sizeof(struct resp_event_info));

    /* Use this response attributes while parsing the response */
    resp_info.rsp = &response;

    /* Parse for command id; This is the first input in the command params */
    c = getopt_long(argc, argv, short_options,
            cmd->long_options, option_index);
    if (c == -1 || c > MAX_OPTIONS-1)
        return;
    /*
     * for ASCII(?) i.e 63rd entry also we will hit below condition
     * we need to differentiate between actual command not find.
     */
    if (c == '?') {
#ifdef __WIN__
        if (strncmp(reservecmd,  &(argv[ARGV_COMMAND_OFFSET][2]),
                    strlen(&(argv[ARGV_COMMAND_OFFSET][2]))) != 0) {
            printf("%s:%d Unsupported Command \n",__func__,__LINE__);
            return;
        }
#endif
    }

    if (c > cmd->num_entries || cmd->entry[c].xml_entry != VEN_CMD) {
        printf("Command not present: c = %d, entry = %d\n",
                c, cmd->entry[c].xml_entry);
        return;
    }
#if __DEBUG__
    printf ("command cli: %s %s\n", cmd->long_options[c].name, __func__);
#endif

    if (parseXMLDocReader(data.config_file, cur_cmds, NUM_NLMSG_TYPES,
                          (char *)cmd->long_options[c].name) != SUCCESS) {
        printf ("Failed to parse the file : %s\n", argv[1]);
        return;
    }

    c = O_CMD_LAST-1;

    /* command.attr_id[c] carries the vendor command number */
    nlmsg = prepareNLMsg(info, command.attr_id[c], data.iface);
    if (!nlmsg) {
        printf("Failed to prepare nlmsg\n");
        return;
    }

    nlVenData = startVendorData(nlmsg);
    if (!nlVenData)
        printf("failed to start vendor data\n");

    populateDefaultAttrs(argc, argv, &command, option_index, &c, nlmsg, 0);
    /* Parse for each input and populate it accordingly */
    while (1) {
        c = getopt_long(argc, argv, short_options,
                command.long_options, option_index);
        if (c == -1 || c > MAX_OPTIONS-1)
            break;
        if (command.entry[c].xml_entry == ATTRIBUTE) {
            if (populateAttribute(argc, argv, &command, option_index,
                                  &c, nlmsg, 0) != 0) {
                printf("Failed to fill Attributes\n");
                break;
            }
        } else if (command.entry[c].ctrl_option == O_END_CMD) {
            if (nlVenData)
                endVendorData(nlmsg, nlVenData);
            ret = sendNLMsg(info, nlmsg, &resp_info);
            if (ret != 0) {
                printf("Failed to send message to driver Error:%d\n", ret);
                break;
            }
        } else if (command.entry[c].ctrl_option == O_RESPONSE) {
            c = getopt_long(argc, argv, short_options,
                    command.long_options, option_index);
            if (c == -1 || c > MAX_OPTIONS-1)
                break;
            if (command.attr_id[c]) {
#ifndef __WIN__
                printf("Monitor for Response : %d\n", command.attr_id[c]);
#endif
                startmonitorResponse(&resp_info, command.attr_id[c]);
            }
        } else {
            printf("Unknown CLI option\n");
        }
    }
    free(resp_info.response);
}


/* General tendency is to provide common data first and then
 * command specific data. This API expects iface and file inputs in
 * any order and then command specific data
 */
static status get_common_data(common_data *data,
                       int argc,
                       char** argv,
                       const char* short_options,
                       struct option *long_options)
{
    int data_read = 0; //No.of elements to be read as common data
    int status = 0;
    unsigned int arg_len;
#ifdef __WIN__
    char help_command[FILE_NAME_LEN];
    char xml_filename[FILE_NAME_LEN];
    char sysctl_path[FILE_PATH_LEN];
    char xml_filepath[FILE_PATH_LEN];
    FILE *fp = NULL;
#endif
    while (1) {
        int option_index = 0, c;

        c = getopt_long(argc, argv, short_options,
                long_options, &option_index);
        if (c == -1)
            break;

        arg_len = optarg ? strlen(optarg) : 0;
        switch(c) {
            case 'i':
                if (!arg_len || arg_len >= IFACE_LEN) {
                    printf("iface name too long or empty\n");
                    return INVALID_ARG;
                }

                memcpy(&data->iface[0], optarg, arg_len);
                data->iface[arg_len] = '\0';
                data_read++;
            break;
            case 'f':
                if (!arg_len || arg_len >= FILE_NAME_LEN) {
                    printf("file name too long or empty\n");
                    return INVALID_ARG;
                }

                memcpy(&data->config_file[0], optarg, arg_len);
                data->config_file[arg_len] = '\0';
                data_read++;
            break;
            case 'h':
#ifdef __WIN__
                if (arg_len >= FILE_NAME_LEN)
                    arg_len = FILE_NAME_LEN - 1;

                /*
                 * Copy dest to src with NULL terminator,
                 * or start with terminator if dest invalid.
                 */
                if (arg_len)
                    memcpy(help_command, optarg, arg_len);
                help_command[arg_len] = '\0';

                if (strncmp(help_command, "list", 4) == 0 ) {
                    display_all_commands = 1;
                }
                data_read++;
#endif
            break;
        }
        if (data_read == NO_OF_ELEMENTS_IN_COMMON_DATA) {
            break;
        }
    }
    if (strlen(data->iface) == 0) {
        printf ("Failed to get iface\n");
        return INVALID_ARG;
    }

#ifdef __WIN__
    /*
     * We have seperate XMLs for Radio and VAP interfaces.
     * so based on inerface, xml file that need to be used by
     * cfg80211tool is defined in /sys/class/net/athX(wifiX)/cfg80211_xmlfile.
     *
     * Read sysctl file for given interface and copy xmlfile name for tool.
     */
    status = snprintf(sysctl_path, FILE_PATH_LEN,
                      "/sys/class/net/%s/cfg80211_xmlfile", data->iface);
    if(status >= FILE_PATH_LEN || status < 0) {
        printf("%s:[%d] Invalid file name\n", __func__, __LINE__);
        return INVALID_ARG;
    }
    fp = fopen(sysctl_path, "r");
    if (fp == NULL) {
        printf("File not preset \n");
        return INVALID_ARG;
    }

    /* read file name in sysctl */
    fscanf(fp,"%s", xml_filename);
    if (strlen(xml_filename) == 0) {
        printf("Invalid XML file name.. \n");
        fclose(fp);
        return INVALID_ARG;
    } else {
        status = snprintf(xml_filepath, FILE_PATH_LEN, "/lib/wifi/%s",
                          xml_filename);
        if(status >= FILE_PATH_LEN || status < 0) {
           printf("%s:[%d] Invalid file name\n", __func__, __LINE__);
           return INVALID_ARG;
        }
        /* dest length is FILE_NAME_LEN, hence check src here */
        arg_len = strlen(xml_filepath);
        if (arg_len >= FILE_NAME_LEN)
            arg_len = FILE_NAME_LEN - 1;

        memcpy(data->config_file, xml_filepath, arg_len);
        data->config_file[arg_len] = '\0';

        fclose(fp);
    }
#endif
    if (strlen(data->config_file) == 0) {
        printf ("Failed to get input file\n");
        return INVALID_ARG;
    }

    return SUCCESS;
}


static status dissolveLongOptions(struct cmd_params *cmds[], int num)
{
    int i, j;
    struct cmd_params *cmd;

    for (i=0; i<num; i++) {
        cmd = cmds[i];
        for (j=0; j<cmd->num_entries;j++) {
            free((void *)cmd->long_options[j].name);
        }
    }

    return SUCCESS;
}


int main(int argc, char **argv)
{
    struct nlIfaceInfo *info;
    struct cmd_params command, response, event;
    struct cmd_params *cmds[NUM_NLMSG_TYPES] = {&command, &response, &event};
    common_data data;

    memset(&command, 0, sizeof(struct cmd_params));
    memset(&response, 0, sizeof(struct cmd_params));
    memset(&event, 0, sizeof(struct cmd_params));

    memset(&data, 0, sizeof(common_data));

    if (get_common_data(&data, argc, argv, short_options,
                        cmds[NLMSG_TYPE_COMMAND]->long_options) != SUCCESS) {
        printf ("Failed to get common data\n");
        return INVALID_ARG;
    }
    printf("%s\t", data.iface);

    if (parseXMLDocReader(data.config_file, cmds, NUM_NLMSG_TYPES, NULL)
            != SUCCESS) {
        printf ("Failed to parse the file : %s\n", argv[1]);
        return INVALID_ARG;
    }

    info = NCT_initialize();
    if (info == NULL) {
        printf ("Failed to initialize sockets\n");
        return INVALID_ARG;
    }
    while (1) {
        int option_index = 0, c;

        /* Parse for command/event params from CLI */
        c = getopt_long(argc, argv, short_options,
                cmds[NLMSG_TYPE_COMMAND]->long_options, &option_index);
        if (c == -1 || c > MAX_OPTIONS-1)
            break;

        if (c < cmds[NLMSG_TYPE_COMMAND]->num_entries) {
            if (cmds[NLMSG_TYPE_COMMAND]->entry[c].ctrl_option == O_COMMAND) {
                parseCmdinputs(info, argc, argv, cmds[NLMSG_TYPE_COMMAND],
                               &option_index, data);
            } else
                printf("Unknown Command : %d\n",
                       cmds[NLMSG_TYPE_COMMAND]->entry[c].ctrl_option);
        } else
            printf("getopt returned character code %d \n", c);
    }
    dissolveLongOptions(cmds, 3);
    cleanupNLinfo(info);
    return 0;
}
