// Copyright (c) 2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: (GPL-2.0)

#include <stdio.h>
#include <stdbool.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <ev.h>
#include <fcntl.h>
#include <getopt.h>
#include <net/if.h>

#include "offload.h"
#include "libnetlink.h"
#include <linux/cfm_bridge.h>

static void incomplete_command(void)
{
	fprintf(stderr, "Command line is not complete. Try option \"help\"\n");
	exit(-1);
}

#define NEXT_ARG() do { argv++; if (--argc <= 0) incomplete_command(); } while(0)
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

static struct rtnl_handle rth;
static ev_io netlink_watcher;

static int netlink_listen(struct rtnl_ctrl_data *who, struct nlmsghdr *n,
			  void *arg)
{
	return 0;
}

static void netlink_rcv(EV_P_ ev_io *w, int revents)
{
	rtnl_listen(&rth, netlink_listen, stdout);
}

static int netlink_init(void)
{
	int err;

	err = rtnl_open(&rth, RTMGRP_LINK);
	if (err)
		return err;

	fcntl(rth.fd, F_SETFL, O_NONBLOCK);

	ev_io_init(&netlink_watcher, netlink_rcv, rth.fd, EV_READ);
	ev_io_start(EV_DEFAULT, &netlink_watcher);

	return 0;
}

static void netlink_uninit(void)
{
	ev_io_stop(EV_DEFAULT, &netlink_watcher);
	rtnl_close(&rth);
}

static enum br_cfm_domain domain_int(char *arg)
{
	if (strcmp(arg, "port") == 0)
		return BR_CFM_PORT;
	if (strcmp(arg, "vlan") == 0)
		return BR_CFM_VLAN;
	return BR_CFM_PORT;
}

static enum br_cfm_mep_direction direction_int(char *arg)
{
	if (strcmp(arg, "down") == 0)
		return BR_CFM_MEP_DIRECTION_DOWN;
	if (strcmp(arg, "up") == 0)
		return BR_CFM_MEP_DIRECTION_UP;
	return BR_CFM_MEP_DIRECTION_DOWN;
}

static int cmd_createmep(int argc, char *const *argv)
{
	uint32_t br_ifindex = 0, port_ifindex = 0, instance = 0, domain = 0, direction = 0;
	uint16_t vid = 0;

	printf("cmd_createmep\n");

	/* skip the command */
	argv++;
	argc -= 1;

	while (argc > 0) {
		if (strcmp(*argv, "bridge") == 0) {
			NEXT_ARG();
			br_ifindex = if_nametoindex(*argv);
		} else if (strcmp(*argv, "instance") == 0) {
			NEXT_ARG();
			instance = atoi(*argv);
		} else if (strcmp(*argv, "domain") == 0) {
			NEXT_ARG();
			domain = domain_int(*argv);
		} else if (strcmp(*argv, "direction") == 0) {
			NEXT_ARG();
			direction = direction_int(*argv);
		} else if (strcmp(*argv, "vid") == 0) {
			NEXT_ARG();
			vid = atoi(*argv);
		} else if (strcmp(*argv, "port") == 0) {
			NEXT_ARG();
			port_ifindex = if_nametoindex(*argv);
		}

		argc--; argv++;
	}

	if (br_ifindex == 0 || instance == 0 || port_ifindex == 0)
		return -1;

	return cfm_offload_create(br_ifindex, instance, domain, direction, vid, port_ifindex);
}

static int cmd_deletemep(int argc, char *const *argv)
{
	uint32_t br_ifindex = 0, instance = 0;

	printf("cmd_deletemep\n");

	/* skip the command */
	argv++;
	argc -= 1;

	while (argc > 0) {
		if (strcmp(*argv, "bridge") == 0) {
			NEXT_ARG();
			br_ifindex = if_nametoindex(*argv);
		} else if (strcmp(*argv, "instance") == 0) {
			NEXT_ARG();
			instance = atoi(*argv);
		}

		argc--; argv++;
	}

	if (br_ifindex == 0 || instance == 0)
		return -1;

	return cfm_offload_delete(br_ifindex, instance);
}

static int cmd_showmep(int argc, char *const *argv)
{
	printf("cmd_showmep\n");
	return 1;
}

struct command
{
	const char *name;
	int (*func) (int argc, char *const *argv);
	const char *format;
	const char *help;
};

static const struct command commands[] =
{
	{"createmep", cmd_createmep,
	 "bridge <bridge> instance <instance> domain <domain> direction <direction> vid <vid> port <port>", "Create MEP instance"},
	{"deletemep", cmd_deletemep,
	 "bridge <bridge> instance <instance>", "Delete MEP instance"},
	{"showmep", cmd_showmep, "", "Show MEP instances"},
};

static void command_helpall(void)
{
	int i;

	for (i = 0; i < COUNT_OF(commands); ++i) {
		if(strcmp("setportdonttxmt", commands[i].name))
			printf("-%s:\n   %-16s %s\n", commands[i].help,
			       commands[i].name, commands[i].format);
	}
}

static void help(void)
{
	printf("Usage: cfm [options] [commands]\n");
	printf("options:\n");
	printf("  -h | --help              Show this help text\n");
	printf("commands:\n");
	command_helpall();
}

static const struct command *command_lookup(const char *cmd)
{
	int i;

	for(i = 0; i < COUNT_OF(commands); ++i) {
		if(!strcmp(cmd, commands[i].name))
			return &commands[i];
	}

	return NULL;
}

static const struct command *command_lookup_and_validate(int argc,
							 char *const *argv,
							 int line_num)
{
	const struct command *cmd;

	cmd = command_lookup(argv[0]);
	if (!cmd) {
		if (line_num > 0)
			fprintf(stderr, "Error on line %d:\n", line_num);
		fprintf(stderr, "Unknown command [%s]\n", argv[0]);
		if (line_num == 0) {
			help();
			return NULL;
		}
	}

	return cmd;
}

int main (int argc, char *const *argv)
{
	const struct command *cmd;
	int f;
	int ret;

	static const struct option options[] =
	{
		{.name = "help",	.val = 'h'},
		{0}
	};

	if (netlink_init()) {
		printf("netlink init failed!\n");
		return -1;
	}

	cfm_offload_init();

	while (EOF != (f = getopt_long(argc, argv, "h", options, NULL))) {
		switch (f) {
			case 'h':
			help();
			return 0;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		help();
		return 1;
	}

	cmd = command_lookup_and_validate(argc, argv, 0);
	if (!cmd)
		return 1;

	ret = cmd->func(argc, argv);

	netlink_uninit();

	return ret;
}

