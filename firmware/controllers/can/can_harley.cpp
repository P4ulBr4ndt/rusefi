#include "pch.h"

#if EFI_CAN_SUPPORT 

#include "can_harley.h"
#include "can_msg_tx.h" // CanTxMessage
#include "crc.h" // crc8
#include "engine_configuration.h" // engineConfiguration->vinNumber

uint8_t frameCounter142 = 0x0;
uint8_t frameCounter144 = 0x0;
uint8_t frameCounter146_342 = 0x0;
uint8_t frameCounter148 = 0x40;

void boardUpdateDash(CanCycle cycle) {
  handleHarleyCAN(cycle);
}

/*
TODO: Actual Value Mapping
N: 0.872V => 0xA0
1: 0.484V => 0x10
2: 1.262V => 0x20
3: 2.098V => 0x30
4: 2.874V => 0x40
5: 3.643V => 0x50
6: 4.439V => 0x60
*/
uint8_t calculateHarleyGearValue() {
  float sensorValue = Sensor::getOrZero(SensorType::AuxLinear1);

  return 0xA0;
}

static void handleHarleyCAN(CanCycle cycle) {
  if (cycle.isInterval(CI::_10ms)) {
    {
      CanTxMessage msg(CanCategory::NBC, CAN_HD_RPM_ID);
      msg.setShortValue(Sensor::getOrZero(SensorType::Rpm), CAN_HD_RPM_OFFSET);
      msg.setShortValue(Sensor::getOrZero(SensorType::VehicleSpeed), CAN_HD_VSS_OFFSET);
      msg[CAN_HD_GEAR_OFFSET] = calculateHarleyGearValue();
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

  if (cycle.isInterval(CI::_50ms)) {
    {
      CanTxMessage msg(CanCategory::NBC, 0x146);
      msg[0] = 0x11;
      msg[1] = 0x00;
      msg[2] = 0x00;
      msg[3] = 0x04;
      msg[4] = 0x00;
      msg[5] = 0x00;
      msg[6] = frameCounter146_342;
      msg[7] = crc8(msg, 7);
    }
    
    {
      CanTxMessage msg(CanCategory::NBC, 0x342);
      msg[0] = 0x54;
      msg[1] = 0x0A;
      msg[2] = 0x54;
      msg[3] = 0x00;
      msg[4] = 0x00;
      msg[5] = 0x00;
      msg[6] = frameCounter146_342;
      msg[7] = crc8(msg, 7);
    }

    {
      CanTxMessage msg(CanCategory::NBC, 0x344);
      msg[0] = 0x00;
      msg[1] = 0x28;
      msg[2] = 0x36;
      msg[3] = 0xFF;
      msg[4] = 0xCD;
      msg[5] = 0x21;
      msg[6] = 0x00;
      msg[7] = 0x00;
    }
    frameCounter146_342 = (frameCounter146_342 + 1) % 64;
  }

  if (cycle.isInterval(CI::_200ms)) {
    {
      CanTxMessage msg(CanCategory::NBC, 0x540);
      msg[0] = 0x00;
      msg[1] = 0x22;
      msg[2] = 0x91;
      msg[3] = 0x29;
      msg[4] = 0x00;
      msg[5] = 0x00;
      msg[6] = 0x00;
      msg[7] = 0x00;
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
      CanTxMessage msg(CanCategory::NBC, 0x148);
      msg[0] = 0x31;
      msg[1] = 0x13;
      msg[2] = 0x00;
      msg[3] = 0x00;
      msg[4] = 0x00;
      msg[5] = 0x00;
      msg[6] = frameCounter148;
      msg[7] = crc8(msg, 7);
    }

    {
      CanTxMessage msg(CanCategory::NBC, 0x346);
      msg[0] = 0x00;
      msg[1] = 0x1A;
      msg[2] = 0x2B;
      msg[3] = 0x55;
      msg[4] = 0x00;
      msg[5] = 0x4F;
      msg[6] = 0x80;
      msg[7] = 0x00;
    }

    {
      CanTxMessage msg(CanCategory::NBC, 0x348);
      msg[0] = 0x00;
      msg[1] = 0x00;
      msg[2] = 0x00;
      msg[3] = 0x0D;
      msg[4] = 0xAC;
      msg[5] = 0x00;
      msg[6] = 0x00;
      msg[7] = 0x00;
    }

    {
      CanTxMessage msg(CanCategory::NBC, CAN_HD_ECM_PING_ID, 0x1/* DLC */);
      msg[0] = 0x1;
    }
  }
}

#endif // EFI_CAN_SUPPORT