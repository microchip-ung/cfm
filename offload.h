// Copyright (c) 2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: (GPL-2.0)

#ifndef OFFLOAD_H
#define OFFLOAD_H

int cfm_offload_create(uint32_t br_ifindex, uint32_t instance, uint32_t domain, uint32_t direction, uint16_t vid, uint32_t ifindex);
int cfm_offload_delete(uint32_t br_ifindex, uint32_t instance);
int cfm_offload_init(void);

#endif
