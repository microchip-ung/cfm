/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */

#ifndef _UAPI_LINUX_CFM_BRIDGE_H_
#define _UAPI_LINUX_CFM_BRIDGE_H_

#define BR_CFM_MAID_LENGTH 48

/* MEP domain */
enum br_cfm_domain {
	BR_CFM_PORT,
	BR_CFM_VLAN,
};

/* MEP direction */
enum br_cfm_mep_direction {
	BR_CFM_MEP_DIRECTION_DOWN,
	BR_CFM_MEP_DIRECTION_UP,
};

/* CCM interval supported. */
enum br_cfm_ccm_interval {
	BR_CFM_CCM_INTERVAL_NONE,
	BR_CFM_CCM_INTERVAL_3_3_MS,
	BR_CFM_CCM_INTERVAL_10_MS,
	BR_CFM_CCM_INTERVAL_100_MS,
	BR_CFM_CCM_INTERVAL_1_SEC,
	BR_CFM_CCM_INTERVAL_10_SEC,
	BR_CFM_CCM_INTERVAL_1_MIN,
	BR_CFM_CCM_INTERVAL_10_MIN
};


#endif
