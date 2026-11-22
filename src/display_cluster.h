#pragma once
#include <zboss_api.h>

/* Custom cluster 0xFC10 "remote display" (server), all RW:
 *   0x0000..0x0002 octet string  tile 1..3 "icon|text"            (UTF-8, <=48 B)
 *   0x0003         uint8         wake: screen on for N seconds
 *   0x0004         octet string  alert "icon|l1|l2|l3", empty = clear
 *   0x0005         uint8         flip: 1 = rotate 180°
 *   0x0006         int8          x offset (px), 0x0007 int8 y offset (px); shows a frame for 20 s
 *   0x0009         octet string  title: header text in the yellow band (e.g. room), empty = battery %
 */
#define ZB_ZCL_CLUSTER_ID_REMOTE_DISPLAY 0xFC10
#define DISPLAY_SPEC_BYTES 48

void display_cluster_init(void);
#define ZB_ZCL_CLUSTER_ID_REMOTE_DISPLAY_SERVER_ROLE_INIT display_cluster_init
#define ZB_ZCL_CLUSTER_ID_REMOTE_DISPLAY_CLIENT_ROLE_INIT NULL

extern zb_zcl_attr_t display_attr_list[];
#define DISPLAY_ATTR_COUNT 11
