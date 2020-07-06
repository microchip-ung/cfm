// Copyright (c) 2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: (GPL-2.0)


#include <stdio.h>
#include <stdbool.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <ev.h>
#include <fcntl.h>

#include "offload.h"
#include "libnetlink.h"

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

int main (void)
{
	if (netlink_init()) {
		printf("netlink init failed!\n");
		return -1;
	}

	mrp_offload_init();

	return 0;
}

