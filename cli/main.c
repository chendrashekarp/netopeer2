/**
 * @file main.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief netopeer2-cli tool
 *
 * @copyright
 * Copyright (c) 2019 - 2021 Deutsche Telekom AG.
 * Copyright (c) 2017 - 2021 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>

#include <libyang/libyang.h>
#include <nc_client.h>

#include "commands.h"
#include "compat.h"
#include "completion.h"
#include "configuration.h"
#include "linenoise/linenoise.h"

int done;

extern char *config_editor;
extern struct nc_session *session;

void
lnc2_print_clb(NC_VERB_LEVEL level, const char *msg)
{
    int was_rawmode = 0;

    if (ls.rawmode) {
        was_rawmode = 1;
        linenoiseDisableRawMode(ls.ifd);
        printf("\n");
    }

    switch (level) {
    case NC_VERB_ERROR:
        fprintf(stderr, "nc ERROR: %s\n", msg);
        break;
    case NC_VERB_WARNING:
        fprintf(stderr, "nc WARNING: %s\n", msg);
        break;
    case NC_VERB_VERBOSE:
        fprintf(stderr, "nc VERBOSE: %s\n", msg);
        break;
    case NC_VERB_DEBUG:
    case NC_VERB_DEBUG_LOWLVL:
        fprintf(stderr, "nc DEBUG: %s\n", msg);
        break;
    }

    if (was_rawmode) {
        linenoiseEnableRawMode(ls.ifd);
        linenoiseRefreshLine();
    }
}

void
ly_print_clb(LY_LOG_LEVEL level, const char *msg, const char *path)
{
    int was_rawmode = 0;

    if (ls.rawmode) {
        was_rawmode = 1;
        linenoiseDisableRawMode(ls.ifd);
        printf("\n");
    }

    switch (level) {
    case LY_LLERR:
        if (path) {
            fprintf(stderr, "ly ERROR: %s (%s)\n", msg, path);
        } else {
            fprintf(stderr, "ly ERROR: %s\n", msg);
        }
        break;
    case LY_LLWRN:
        if (path) {
            fprintf(stderr, "ly WARNING: %s (%s)\n", msg, path);
        } else {
            fprintf(stderr, "ly WARNING: %s\n", msg);
        }
        break;
    case LY_LLVRB:
        if (path) {
            fprintf(stderr, "ly VERBOSE: %s (%s)\n", msg, path);
        } else {
            fprintf(stderr, "ly VERBOSE: %s\n", msg);
        }
        break;
    case LY_LLDBG:
        if (path) {
            fprintf(stderr, "ly DEBUG: %s (%s)\n", msg, path);
        } else {
            fprintf(stderr, "ly DEBUG: %s\n", msg);
        }
        break;
    default:
        /* silent, just to cover enum, shouldn't be here in real world */
        return;
    }

    if (was_rawmode) {
        linenoiseEnableRawMode(ls.ifd);
        linenoiseRefreshLine();
    }
}

int
main(void)
{
    char *cmd, *cmdline, *cmdstart, *tmp_config_file = NULL;
    int i, j;
    struct sigaction action;

    nc_client_init();

    /* ignore SIGPIPE */
    memset(&action, 0, sizeof action);
    action.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &action, NULL);

    nc_set_print_clb(lnc2_print_clb);
    ly_set_log_clb(ly_print_clb, 1);
    linenoiseSetCompletionCallback(complete_cmd);
    linenoiseHistoryDataFree(free);

    load_config();

    if (!config_editor) {
        config_editor = getenv("EDITOR");
        if (config_editor) {
            config_editor = strdup(config_editor);
        }
    }
    if (!config_editor) {
        config_editor = strdup("vi");
    }
	
	static int is_FirstTime=0;
    while (!done) {
        /* get the command from user */
	if(is_FirstTime==0)
	{
	cmdline=strdup("listen");
	is_FirstTime++;	
	}

	else if(is_FirstTime==1)
	{	
	cmdline=strdup("subscribe");
	is_FirstTime++;
	}

	else if(is_FirstTime==2)
	{	
	cmdline=strdup("get --filter-xpath /o-ran-delay-management:* ");
	is_FirstTime++;
	}
	
	else if(is_FirstTime==3)
	{	
	printf("saving the data to the file /home/user/PureSoftware/MP_3.0/config_data_xml/del_mgmt.xml edit the file (remove 1st and last line-mandatory )\n");
	cmdline=strdup("get --filter-xpath /o-ran-delay-management:* --out /home/user/PureSoftware/MP_3.0/config_data_xml/del_mgmt.xml");

	is_FirstTime++;
	}
	else if(is_FirstTime==4)
	{
		getchar();
		
		system("vi /home/user/PureSoftware/MP_3.0/config_data_xml/del_mgmt.xml");		

		cmdline=strdup("edit-config --target running --config=/home/user/PureSoftware/MP_3.0/config_data_xml/del_mgmt.xml --defop replace");
		is_FirstTime++;
	}

	else if(is_FirstTime==5)
	{
		printf("Updated Data after edit-config opeartion \n");
		
		cmdline=strdup("get --filter-xpath /o-ran-delay-management:* ");
		is_FirstTime++;	
	}

	else
        cmdline = linenoise(PROMPT);

        /* EOF -> exit */
        if (cmdline == NULL) {
            done = 1;
            cmdline = strdup("quit");
        }

        /* empty line -> wait for another command */
        if (*cmdline == '\0') {
            free(cmdline);
            continue;
        }

        /* isolate the command word. */
        for (i = 0; cmdline[i] && (cmdline[i] == ' '); i++) {}
        cmdstart = cmdline + i;
        for (j = 0; cmdline[i] && (cmdline[i] != ' '); i++, j++) {}
        cmd = strndup(cmdstart, j);

        /* parse the command line */
        for (i = 0; commands[i].name; i++) {
            if (strcmp(cmd, commands[i].name) == 0) {
                break;
            }
        }

        /* execute the command if any valid specified */
        if (commands[i].name) {
            /* display help */
            if ((strchr(cmdstart, ' ') != NULL) && ((strncmp(strchr(cmdstart, ' ') + 1, "-h", 2) == 0) ||
                    (strncmp(strchr(cmdstart, ' ') + 1, "--help", 6) == 0))) {
                if (commands[i].help_func != NULL) {
                    commands[i].help_func();
                } else {
                    printf("%s\n", commands[i].helpstring);
                }
            } else {
                if (ls.history_index) {
                    tmp_config_file = (char *)ls.history[ls.history_len - ls.history_index].data;
                }
                commands[i].func((const char *)cmdstart, &tmp_config_file);
            }
        } else {
            /* if unknown command specified, tell it to user */
            fprintf(stderr, "%s: No such command, type 'help' for more information.\n", cmd);
        }
        if (!done) {
            linenoiseHistoryAdd(cmdline, tmp_config_file);
        }

        tmp_config_file = NULL;
        free(cmd);
        free(cmdline);
    }

    store_config();

    free(config_editor);

    if (session) {
        nc_session_free(session, NULL);
    }

    nc_client_destroy();

    return 0;
}
