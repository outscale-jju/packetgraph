/* Copyright 2015 Outscale SAS
 *
 * This file is part of Butterfly.
 *
 * Butterfly is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as published
 * by the Free Software Foundation.
 *
 * Butterfly is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Butterfly.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PG_VHOST_H
#define _PG_VHOST_H

#include <packetgraph/errors.h>
#ifndef __cplusplus
#include <linux/virtio_net.h>
#else
#define VIRTIO_NET_F_CSUM	0	/* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM	1	/* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS 2 /* Dynamic offload configuration. */
#define VIRTIO_NET_F_MAC	5	/* Host has given MAC address. */
#define VIRTIO_NET_F_GUEST_TSO4	7	/* Guest can handle TSOv4 in. */
#define VIRTIO_NET_F_GUEST_TSO6	8	/* Guest can handle TSOv6 in. */
#define VIRTIO_NET_F_GUEST_ECN	9	/* Guest can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_GUEST_UFO	10	/* Guest can handle UFO in. */
#define VIRTIO_NET_F_HOST_TSO4	11	/* Host can handle TSOv4 in. */
#define VIRTIO_NET_F_HOST_TSO6	12	/* Host can handle TSOv6 in. */
#define VIRTIO_NET_F_HOST_ECN	13	/* Host can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_HOST_UFO	14	/* Host can handle UFO in. */
#define VIRTIO_NET_F_MRG_RXBUF	15	/* Host can merge receive buffers. */
#define VIRTIO_NET_F_STATUS	16	/* virtio_net_config.status available */
#define VIRTIO_NET_F_CTRL_VQ	17	/* Control channel available */
#define VIRTIO_NET_F_CTRL_RX	18	/* Control channel RX mode support */
#define VIRTIO_NET_F_CTRL_VLAN	19	/* Control channel VLAN filtering */
#define VIRTIO_NET_F_CTRL_RX_EXTRA 20	/* Extra RX mode control support */
#define VIRTIO_NET_F_GUEST_ANNOUNCE 21	/* Guest can announce device on the
					 * network */
#define VIRTIO_NET_F_MQ	22	/* Device supports Receive Flow
					 * Steering */
#define VIRTIO_NET_F_CTRL_MAC_ADDR 23	/* Set MAC address */
#ifndef VIRTIO_NET_NO_LEGACY
#define VIRTIO_NET_F_GSO	6	/* Host handles pkts w/ any GSO type */
#endif /* VIRTIO_NET_NO_LEGACY */
#endif /*__cplusplus */

struct pg_brick;

/**
 * Create a new vhost brick.
 * vhost interface is configured with offload features disabled.
 * Note: this brick support pg_brick_rx_bytes and pg_brick_tx_bytes.
 *
 * @name:	name of the brick
 * @west_max:	maximum of links you can connect on the west side
 * @east_max:	maximum of links you can connect on the east side
 * @output:	The side of the output (so the side of the "VM")
 * @errp:	set in case of an error
 * @return:	a pointer to a brick structure on success, NULL on error
 */
PG_WARN_UNUSED
struct pg_brick *pg_vhost_new(const char *name, uint32_t west_max,
			      uint32_t east_max, enum pg_side output,
			      struct pg_error **errp);

/**
 * Initialize vhost-user at program startup
 *
 * Does not take care of cleaning up everything on failure since
 * the program will abort in this case.
 *
 * @param: the base directory where the vhost-user socket will be
 * @param: an error pointer
 * @return: 0 on success, -1 on error
 */
int pg_vhost_start(const char *base_dir, struct pg_error **errp);

/**
 * Get path to the unix socket path.
 * @param: brick pointer to the vhost brick
 * @param: an error pointer
 * @param: errp is set in case of an error
 * @return: string containing the socket path or NULL in case of error
 */
const char *pg_vhost_socket_path(struct pg_brick *brick,
				 struct pg_error **errp);
/**
 * Un-initialize vhost-user.
 * You should have call pg_vhost_start in the past before calling this.
 */
void pg_vhost_stop(void);

/**
 * Enable virtio features.
 * @feature_mask virtio feature (see VIRTIO_NET_F_* in linux/virtio_net.h)
 * @return 0 on success, -1 on error
 */
int pg_vhost_enable(uint64_t feature_mask);

/**
 * Disable virtio features.
 * @feature_mask virtio feature (see VIRTIO_NET_F_* in linux/virtio_net.h)
 * @return: 0 on success, -1 on error
 *
 * example: pg_vhost_disable(1ULL << VIRTIO_NET_F_HOST_TSO4)
 */
int pg_vhost_disable(uint64_t feature_mask);

#endif  /* _PG_VHOST_H */
