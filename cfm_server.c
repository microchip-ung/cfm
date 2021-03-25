// Copyright (c) 2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: (GPL-2.0)


#include <stdint.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/if_bridge.h>
#include <errno.h>

#include "list.h"



#include <stdio.h>
#include <stdbool.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <ev.h>
#include <fcntl.h>
#include <getopt.h>
#include <net/if.h>

#include "cfm_netlink.h"
#include "libnetlink.h"

volatile bool quit = false;

static void handle_signal(int sig)
{
    ev_break(EV_DEFAULT, EVBREAK_ALL);;
}

int signal_init(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    return 0;
}

static struct rtnl_handle rth;
static ev_io netlink_watcher;

char *rta_getattr_mac(const struct rtattr *rta)
{
	static char buf_ret[100];
	unsigned char mac[6];

	memcpy(&mac, RTA_DATA(rta), 6);
	snprintf(buf_ret, sizeof(buf_ret), "%02X-%02X-%02X-%02X-%02X-%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	return buf_ret;
}

static int netlink_listen(struct rtnl_ctrl_data *who, struct nlmsghdr *n,
			  void *arg)
{
	struct rtattr *aftb[IFLA_BRIDGE_MAX + 1];
	struct rtattr *info_peer[IFLA_BRIDGE_CFM_CC_PEER_EVENT_MAX + 1];
	struct rtattr *info_mip[IFLA_BRIDGE_CFM_MIP_EVENT_MAX + 1];
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct rtattr *tb[IFLA_MAX + 1];
	int len = n->nlmsg_len;
	struct rtattr *i, *list;
	int rem;
	uint32_t instance, request, sub_code, status;

	if (n->nlmsg_type == NLMSG_DONE)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0) {
		fprintf(stderr, "Message too short!\n");
		return -1;
	}

	if (ifi->ifi_family != AF_BRIDGE)
		return 0;

	if (n->nlmsg_type != RTM_NEWLINK)
		return 0;

	parse_rtattr_flags(tb, IFLA_MAX, IFLA_RTA(ifi), len, NLA_F_NESTED);

	if (tb[IFLA_IFNAME] == NULL) {
		printf("No IFLA_IFNAME\n");
		return -1;
	}

	if (!tb[IFLA_AF_SPEC])
		return 0;

	parse_rtattr_flags(aftb, IFLA_BRIDGE_MAX, RTA_DATA(tb[IFLA_AF_SPEC]), RTA_PAYLOAD(tb[IFLA_AF_SPEC]), NLA_F_NESTED);
	if (!aftb[IFLA_BRIDGE_CFM])
		return 0;

	list = aftb[IFLA_BRIDGE_CFM];
	rem = RTA_PAYLOAD(list);

	printf("EVENT CFM CC peer status:\n");
	instance = 0xFFFFFFFF;
	for (i = RTA_DATA(list); RTA_OK(i, rem); i = RTA_NEXT(i, rem)) {
		if (i->rta_type != (IFLA_BRIDGE_CFM_CC_PEER_EVENT_INFO | NLA_F_NESTED))
			continue;

		parse_rtattr_flags(info_peer, IFLA_BRIDGE_CFM_CC_PEER_EVENT_MAX, RTA_DATA(i), RTA_PAYLOAD(i), NLA_F_NESTED);
		if (!info_peer[IFLA_BRIDGE_CFM_CC_PEER_EVENT_INSTANCE])
			continue;

		if (instance != rta_getattr_u32(info_peer[IFLA_BRIDGE_CFM_CC_PEER_EVENT_INSTANCE])) {
			instance = rta_getattr_u32(info_peer[IFLA_BRIDGE_CFM_CC_PEER_EVENT_INSTANCE]);
			printf("Instance %u\n", rta_getattr_u32(info_peer[IFLA_BRIDGE_CFM_CC_PEER_EVENT_INSTANCE]));
		}
		printf("    Peer-mep %u\n", rta_getattr_u32(info_peer[IFLA_BRIDGE_CFM_CC_PEER_EVENT_PEER_MEPID]));
		printf("        CCM defect %u\n", rta_getattr_u32(info_peer[IFLA_BRIDGE_CFM_CC_PEER_EVENT_CCM_DEFECT]));
		printf("\n");
	}

	printf("EVENT CFM MIP RAPS info:\n");
	instance = 0xFFFFFFFF;
	for (i = RTA_DATA(list); RTA_OK(i, rem); i = RTA_NEXT(i, rem)) {
		if (i->rta_type != (IFLA_BRIDGE_CFM_MIP_EVENT_INFO | NLA_F_NESTED))
			continue;

		parse_rtattr_flags(info_mip, IFLA_BRIDGE_CFM_MIP_EVENT_MAX, RTA_DATA(i), RTA_PAYLOAD(i), NLA_F_NESTED);
		if (!info_mip[IFLA_BRIDGE_CFM_MIP_EVENT_INSTANCE])
			continue;

		if (instance != rta_getattr_u32(info_mip[IFLA_BRIDGE_CFM_MIP_EVENT_INSTANCE])) {
			instance = rta_getattr_u32(info_mip[IFLA_BRIDGE_CFM_MIP_EVENT_INSTANCE]);
			printf("Instance %u\n", rta_getattr_u32(info_mip[IFLA_BRIDGE_CFM_MIP_EVENT_INSTANCE]));
		}
		request = (rta_getattr_u32(info_mip[IFLA_BRIDGE_CFM_MIP_EVENT_RAPS_REQUEST_SUBCODE]) & 0xF0) >> 4;
		sub_code = rta_getattr_u32(info_mip[IFLA_BRIDGE_CFM_MIP_EVENT_RAPS_REQUEST_SUBCODE]) & 0x0F;
		status = rta_getattr_u32(info_mip[IFLA_BRIDGE_CFM_MIP_EVENT_RAPS_STATUS]);
		printf("    request %u\n", request);
		printf("    sub_code %u\n", sub_code);
		printf("    status %u\n", status);
		printf("    Node-id %s\n", rta_getattr_mac(info_mip[IFLA_BRIDGE_CFM_MIP_EVENT_RAPS_NODE_ID]));
		printf("\n");
	}

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

int main (void)
{
	if (netlink_init()) {
		printf("netlink init failed!\n");
		return -1;
	}

	ev_run(EV_DEFAULT, 0);

	netlink_uninit();

	return 0;
}

