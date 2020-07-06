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

static void mrp_nl_bridge_prepare(uint32_t ifindex, int cmd, struct request *req,
				  struct rtattr **afspec, struct rtattr **afmrp,
				  struct rtattr **af_submrp, int mrp_attr)
{
	req->n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req->n.nlmsg_flags = NLM_F_REQUEST;
	req->n.nlmsg_type = cmd;
	req->ifm.ifi_family = PF_BRIDGE;

	req->ifm.ifi_index = ifindex;

	*afspec = addattr_nest(&req->n, sizeof(*req), IFLA_AF_SPEC);
	addattr16(&req->n, sizeof(*req), IFLA_BRIDGE_FLAGS, BRIDGE_FLAGS_SELF);

	*afmrp = addattr_nest(&req->n, sizeof(*req),
			      IFLA_BRIDGE_MRP | NLA_F_NESTED);
	*af_submrp = addattr_nest(&req->n, sizeof(*req),
				  mrp_attr | NLA_F_NESTED);
}

static int mrp_nl_terminate(struct request *req, struct rtattr *afspec,
			    struct rtattr *afmrp, struct rtattr *af_submrp)
{
	int err;

	addattr_nest_end(&req->n, af_submrp);
	addattr_nest_end(&req->n, afmrp);
	addattr_nest_end(&req->n, afspec);

	err = rtnl_talk(&rth, &req->n, NULL);
	if (err)
		return err;

	return 0;
}

int mrp_offload_init(void)
{
	if (rtnl_open(&rth, 0) < 0) {
		fprintf(stderr, "Cannot open rtnetlink\n");
		return EXIT_FAILURE;
	}

	return 0;
}

void mrp_offload_uninit(void)
{
	rtnl_close(&rth);
}

int mrp_offload_add(struct mrp *mrp, struct mrp_port *p, struct mrp_port *s,
		    uint16_t prio)
{
	struct rtattr *afspec, *afmrp, *af_submrp;
	struct request req = { 0 };

	mrp_nl_bridge_prepare(mrp->ifindex, RTM_SETLINK, &req, &afspec,
			      &afmrp, &af_submrp, IFLA_BRIDGE_MRP_INSTANCE);

	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_RING_ID,
		  mrp->ring_nr);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_P_IFINDEX,
		  p->ifindex);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_S_IFINDEX,
		  s->ifindex);
	addattr16(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_PRIO, prio);

	return mrp_nl_terminate(&req, afspec, afmrp, af_submrp);
}

int mrp_offload_del(struct mrp *mrp)
{
	struct rtattr *afspec, *afmrp, *af_submrp;
	struct request req = { 0 };

	mrp_nl_bridge_prepare(mrp->ifindex, RTM_DELLINK, &req, &afspec, &afmrp,
			      &af_submrp, IFLA_BRIDGE_MRP_INSTANCE);

	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_RING_ID,
		  mrp->ring_nr);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_P_IFINDEX,
		  mrp->p_port->ifindex);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_S_IFINDEX,
		  mrp->s_port->ifindex);

	return mrp_nl_terminate(&req, afspec, afmrp, af_submrp);
}
