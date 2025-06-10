#include "pch.h"

#if EFI_CAN_SUPPORT 

#include "can_harley.h"
#include "can_msg_tx.h" // CanTxMessage
#include "crc.h" // crc8
#include "engine_configuration.h" // engineConfiguration->vinNumber

uint8_t frameCounter142 = 0x0;
uint8_t frameCounter144 = 0x0;
uint8_t frameCounter146_342 = 0x0;
uint8_t frameCounter148 = 0x40

void boardUpdateDash(CanCycle cycle) {
  handleHarleyCAN(cycle);
}

static void handleHarleyCAN(CanCycle cycle) {
  if (cycle.isInterval(CI::_10ms)) {
    {
      CanTxMessage msg(CanCategory::NBC, CAN_HD_RPM_ID);
      msg.setShortValue(Sensor::getOrZero(SensorType::Rpm), CAN_HD_RPM_OFFSET);
      msg.setShortValue(Sensor::getOrZero(SensorType::Rpm), CAN_HD_VSS_OFFSET);
      msg[CAN_HD_GEAR_OFFSET] = Sensor::getOrZero(SensorType::AuxLinear1); // TODO proper sensor and conversion
      msg[6] = frameCounter142;
      msg[7] = crc8(msg, 7);
      frameCounter142 = (frameCounter142 + 1) % 64;
    }
  }
  if (cycle.isInterval(CI::_20ms)) {
    {
      CanTxMessage msg(CanCategory::NBC, CAN_HD_THROTTLE_ID);
      msg.setShortValue(Sensor::getOrZero(SensorType::Tps1Primary), 0);
      msg.setShortValue(Sensor::getOrZero(SensorType::Tps1Secondary), 2);
      msg[4] = Sensor::getOrZero(SensorType::AcceleratorPedal) * 2;
      msg[6] = frameCounter144;
      msg[7] = crc8(msg, 7);
      frameCounter144 = (frameCounter144 + 1) % 64;
    }
  }
  if (cycle.isInterval(CI::_1000ms)) {
    {
      CanTxMessage msg(CanCategory::NBC, CAN_HD_VIN_ID_1);
      msg[0] = engineConfiguration->vinNumber[0];
      msg[1] = engineConfiguration->vinNumber[1];
      msg[2] = engineConfiguration->vinNumber[2];
      msg[3] = engineConfiguration->vinNumber[3];
      msg[4] = engineConfiguration->vinNumber[4];
      msg[5] = engineConfiguration->vinNumber[5];
    }

    {
      CanTxMessage msg(CanCategory::NBC, CAN_HD_VIN_ID_2);
      msg[0] = engineConfiguration->vinNumber[6];
      msg[1] = engineConfiguration->vinNumber[7];
      msg[2] = engineConfiguration->vinNumber[8];
      msg[3] = engineConfiguration->vinNumber[9];
      msg[4] = engineConfiguration->vinNumber[10];
      msg[5] = engineConfiguration->vinNumber[11];
    } 

    {
      CanTxMessage msg(CanCategory::NBC, CAN_HD_VIN_ID_3);
      msg[0] = engineConfiguration->vinNumber[12];
      msg[1] = engineConfiguration->vinNumber[13];
      msg[2] = engineConfiguration->vinNumber[14];
      msg[3] = engineConfiguration->vinNumber[15];
      msg[4] = engineConfiguration->vinNumber[16];
      msg[5] = engineConfiguration->vinNumber[17];
    }

    {
      CanTxMessage msg(CanCategory::NBC, CAN_HD_ECM_PING_ID, 0x1/* DLC */);
      msg[0] = 0x1;
    }
  }
}

#endif // EFI_CAN_SUPPORT