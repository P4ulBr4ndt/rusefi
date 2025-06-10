#pragma once
#include "can.h"

#define CAN_HD_RPM_ID 0x142
#define CAN_HD_VSS_ID 0x142
#define CAN_HD_GEAR_ID 0x142
#define CAN_HD_THROTTLE_ID 0x144

#define CAN_HD_RPM_OFFSET 0x0
#define CAN_HD_VSS_OFFSET 0x2
#define CAN_HD_GEAR_OFFSET 0x4

#define CAN_HD_BCM_PING_ID 0x503 // Here we receive a "Ping" kind of thing
#define CAN_HD_ECM_PING_ID 0x502 // Here we send our own

#define CAN_HD_VIN_ID_1 0x34D
#define CAN_HD_VIN_ID_2 0x34E
#define CAN_HD_VIN_ID_3 0x34F
