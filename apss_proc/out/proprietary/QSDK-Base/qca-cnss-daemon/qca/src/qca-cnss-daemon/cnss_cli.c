/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "cnss_cli.h"

#define MAX_CMD_LEN 256
#define MAX_NUM_OF_PARAMS 2
#define MAX_CNSS_CMD_LEN 32

#define ARRAY_SIZE(x)   (sizeof(x)/sizeof(x[0]))

static char *cnss_cmd[][2] = {
	{"qdss_trace_start", ""},
	{"qdss_trace_stop", "<Hex base number: e.g. 0x3f>"},
	{"qdss_trace_load_config", ""},
	{"quit", ""}
};

void help(void)
{
	int i = 0;

	printf("Supported Command:\n");
	for (i = 0; i < (int)ARRAY_SIZE(cnss_cmd); i++)
		printf("%s %s\n", cnss_cmd[i][0], cnss_cmd[i][1]);
}

int main(int argc, char **argv)
{
	char *tmp = NULL;
	char cmd_str[MAX_CMD_LEN];
	char token[MAX_NUM_OF_PARAMS][MAX_CNSS_CMD_LEN];
	int token_num = 0;
	int i = 0;
	int sockfd = 0;
	char buffer[CNSS_CLI_MAX_PAYLOAD] = {0};
	struct cnss_cli_msg_hdr *hdr = NULL;
	void *cnss_cli_data = NULL;
	enum cnss_cli_cmd_type type = CNSS_CLI_CMD_NONE;
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		printf("Failed to connect to cnss-daemon\n");
		return -1;
	}

	while (1) {
		printf("cnss_cli_cmd >> ");
		fgets(cmd_str, MAX_CMD_LEN, stdin);
		if (!strcmp(cmd_str, "\n"))
			continue;

		tmp = &cmd_str[0];
		i = 0;
		int len = 0;
		char *tmp1;

		while (*tmp != '\0') {
			if (i >= MAX_NUM_OF_PARAMS) {
				printf("Invalid input, max number of params is %d\n",
				       MAX_NUM_OF_PARAMS);
				break;
			}
			tmp1 = tmp;
			len = 0;

			while (*tmp1 == ' ') {
				tmp1++;
				tmp++;
			}
			while (*tmp1 != ' ' && *tmp1 != '\n') {
				len++;
				tmp1++;
			}
			if (*tmp1 != '\n')
				*tmp1 = '\0';

			strlcpy(token[i], tmp, sizeof(token[i]));
			token[i][len] = '\0';
			tmp = tmp1;
			if (*tmp == '\n')
				break;
			tmp++;
			i++;
		}

		if (i >= MAX_NUM_OF_PARAMS)
			continue;

		token_num = i + 1;

		if (!strcmp(token[0], "qdss_trace_start")) {
			type = QDSS_TRACE_START;
		} else if (!strcmp(token[0], "qdss_trace_stop")) {
			if (token_num != 2) {
				printf("qdss_trace_stop <option>\n");
				continue;
			}
			type = QDSS_TRACE_STOP;
		} else if (!strcmp(token[0], "qdss_trace_load_config")) {
			type = QDSS_TRACE_CONFIG_DOWNLOAD;
		} else if (!strcmp(token[0], "help")) {
			help();
			type = CNSS_CLI_CMD_NONE;
		} else if (!strcmp(token[0], "quit")) {
			goto out;
		} else {
			printf("Invalid command %s\n", token[0]);
			type = CNSS_CLI_CMD_NONE;
		}

		if (type == CNSS_CLI_CMD_NONE)
			continue;

		memset(buffer, 0, sizeof(buffer));

		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(PORT);
		inet_pton(AF_INET, NAME, &serv_addr.sin_addr);

		hdr = (struct cnss_cli_msg_hdr *)buffer;
		hdr->type = type;

		switch (type) {
		case QDSS_TRACE_START:
		case QDSS_TRACE_CONFIG_DOWNLOAD:
			hdr->len = 0;
			break;
		case QDSS_TRACE_STOP:
			{
				struct cnss_cli_qdss_trace_stop_data data;
				char *pend;

				hdr->len = sizeof(data);
				data.option = strtoull(token[1], &pend, 16);
				cnss_cli_data = (char *)hdr +
					sizeof(struct cnss_cli_msg_hdr);
				memcpy(cnss_cli_data, &data, sizeof(data));
			}
			break;
		default:
			goto out;
		}

		sendto(sockfd, buffer, sizeof(buffer), 0,
		       (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	}
out:
	close(sockfd);
	return 0;
}
