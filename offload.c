// Copyright (c) 2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: (GPL-2.0)

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/types.h>
#include <linux/if_bridge.h>
#include <net/if.h>
#include <errno.h>

#include "libnetlink.h"
#include "list.h"

struct rtnl_handle rth = { .fd = -1 };

static LIST_HEAD(mrp_rings);

struct mrp_ring {
	struct list_head list;
	uint32_t ifindex;
	uint32_t ring_id;
	uint32_t p_ifindex;
	uint32_t s_ifindex;
};

struct request {
	struct nlmsghdr		n;
	struct ifinfomsg	ifm;
	char			buf[1024];
};

static void cfm_nl_bridge_prepare(uint32_t ifindex, int cmd, struct request *req,
				  struct rtattr **afspec, struct rtattr **af,
				  struct rtattr **af_sub, int attr)
{
	req->n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req->n.nlmsg_flags = NLM_F_REQUEST;
	req->n.nlmsg_type = cmd;
	req->ifm.ifi_family = PF_BRIDGE;

	req->ifm.ifi_index = ifindex;

	*afspec = addattr_nest(&req->n, sizeof(*req), IFLA_AF_SPEC);
	addattr16(&req->n, sizeof(*req), IFLA_BRIDGE_FLAGS, BRIDGE_FLAGS_SELF);

	*af = addattr_nest(&req->n, sizeof(*req),
			   IFLA_BRIDGE_CFM | NLA_F_NESTED);
	*af_sub = addattr_nest(&req->n, sizeof(*req),
			       attr | NLA_F_NESTED);
}

static int cfm_nl_terminate(struct request *req, struct rtattr *afspec,
			    struct rtattr *af, struct rtattr *af_sub)
{
	int err;

	printf("cfm_nl_terminate\n");

	addattr_nest_end(&req->n, af_sub);
	addattr_nest_end(&req->n, af);
	addattr_nest_end(&req->n, afspec);

	err = rtnl_talk(&rth, &req->n, NULL);
	if (err) {
		printf("cfm_nl_terminate: rtnl_talk failed\n");
		return err;
	}

	return 0;
}

int cfm_offload_init(void)
{
	if (rtnl_open(&rth, 0) < 0) {
		fprintf(stderr, "Cannot open rtnetlink\n");
		return EXIT_FAILURE;
	}

	return 0;
}

void cfm_offload_uninit(void)
{
	rtnl_close(&rth);
}

int cfm_offload_create(uint32_t br_ifindex, uint32_t instance, uint32_t domain, uint32_t direction, uint16_t vid, uint32_t ifindex)
{
	struct rtattr *afspec, *af, *af_sub;
	struct request req = { 0 };

	printf("cfm_offload_create\n");

	cfm_nl_bridge_prepare(br_ifindex, RTM_SETLINK, &req, &afspec,
			      &af, &af_sub, IFLA_BRIDGE_CFM_MEP_CREATE);

	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_CFM_MEP_CREATE_INSTANCE,
		  instance);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_CFM_MEP_CREATE_DOMAIN,
		  domain);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_CFM_MEP_CREATE_DIRECTION,
		  direction);
	addattr16(&req.n, sizeof(req), IFLA_BRIDGE_CFM_MEP_CREATE_VID,
		  vid);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_CFM_MEP_CREATE_IFINDEX,
		  ifindex);

	return cfm_nl_terminate(&req, afspec, af, af_sub);
}

int cfm_offload_delete(uint32_t br_ifindex, uint32_t instance)
{
	struct rtattr *afspec, *af, *af_sub;
	struct request req = { 0 };

	cfm_nl_bridge_prepare(br_ifindex, RTM_SETLINK, &req, &afspec, &af,
			      &af_sub, IFLA_BRIDGE_CFM_MEP_DELETE);

	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_CFM_MEP_CREATE_INSTANCE,
		  instance);

	return cfm_nl_terminate(&req, afspec, af, af_sub);
}
