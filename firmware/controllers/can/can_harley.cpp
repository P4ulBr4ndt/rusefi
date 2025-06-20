#include "pch.h"

#if EFI_CAN_SUPPORT 

#include "can_harley.h"
#include "can_msg_tx.h" // CanTxMessage
#include "engine_configuration.h" // engineConfiguration->vinNumber

uint8_t frameCounter142 = 0x0;
uint8_t frameCounter144 = 0x0;
uint8_t frameCounter146_342 = 0x0;
uint8_t frameCounter148 = 0x40;

/*

TODO CLUTCH looks like 0xD0

N: 0.872V => 17.44% => 0xA0
1: 0.484V => 09.86% => 0x10
2: 1.262V => 25,24% => 0x20
3: 2.098V => 41,96% => 0x30
4: 2.874V => 57,48% => 0x40
5: 3.643V => 72,96% => 0x50
6: 4.439V => 88,78% => 0x60
*/
float harleyGearValues[] = { 17.44f, 9.86f, 25.24f, 41.96f, 57.48f, 72.96f, 88.78f };
uint8_t calculateHarleyGearValue() {
  float sensorValue = Sensor::getOrZero(SensorType::AuxLinear1);
  float bestMatch = 0.0f;
  uint8_t bestOffs = 0;
  for (uint8_t i = 0; i < sizeof(harleyGearValues)/sizeof(harleyGearValues[0]); i++) {
    float i_delta = abs(harleyGearValues[i] - sensorValue);
    float x_delta = abs(bestMatch - sensorValue);
    if (i_delta < x_delta) {
      bestMatch = harleyGearValues[i];
      bestOffs = i;
    }
  }

  switch (bestOffs)
  {
    case 0:
      return 0xA0; // N
      break;
    case 1:
      return 0x10; // 1
      break;
    case 2:
      return 0x20; // 2
      break;
    case 3:
      return 0x30; // 3
      break;
    case 4:
      return 0x40; // 4
      break;
    case 5:
      return 0x50; // 5
      break;
    case 6:
      return 0x60; // 6
      break;
    
    default:
    return 0x0;
      break;
  }
}

static void handleHarleyCAN(CanCycle cycle) {
  if (cycle.isInterval(CI::_10ms)) {
    {
      CanTxMessage msg(CanCategory::NBC, CAN_HD_RPM_ID);
      msg.setShortValueMsb(Sensor::getOrZero(SensorType::Rpm), CAN_HD_RPM_OFFSET);
      msg.setShortValueMsb(Sensor::getOrZero(SensorType::VehicleSpeed), CAN_HD_VSS_OFFSET);
      msg[CAN_HD_GEAR_OFFSET] = calculateHarleyGearValue();
      msg[6] = frameCounter142;
      msg[7] = crc8(msg.getFrame()->data8, 7);
      frameCounter142 = (frameCounter142 + 1) % 64;
    }
  }
  
  /**
  uint floatToShortForTPS(float tps)
  {
    float buf;
    
    buf = FLOOR((float)(int)((tps + 0.1) * 5.0));
    return ((int)buf + 5000) * (uint)(-5001 < SUB42(buf,0)) & 0xffff;
  }
  */
  if (cycle.isInterval(CI::_20ms)) {
    {
      CanTxMessage msg(CanCategory::NBC, CAN_HD_THROTTLE_ID);
      msg.setShortValueMsb(Sensor::getOrZero(SensorType::Tps1Primary), 0); // TARGET TPS?
      msg.setShortValueMsb(Sensor::getOrZero(SensorType::Tps1Secondary), 2); // ACTUAL TPS?
      msg[4] = Sensor::getOrZero(SensorType::AcceleratorPedal) / 0.4545; // As OEM does
      msg[5] = 0x00; // TODO: What is this? It rarely moves up from 0 to 0x20 or 0x40
      msg[6] = frameCounter144;
      msg[7] = crc8(msg.getFrame()->data8, 7);
      frameCounter144 = (frameCounter144 + 1) % 64;
    }
  }

  if (cycle.isInterval(CI::_50ms)) {
    bool running = engine->rpmCalculator.isRunning();
    {
      CanTxMessage msg(CanCategory::NBC, 0x146);
      msg[0] = 0x11; // JIFFY STAND SENSOR in here 0x11 = UP, 0x12 = DOWN
      msg[1] = 0x00;
      msg[2] = 0x00;
      msg[3] = running ? 0x44 : 0x04;
      msg[4] = 0x00;
      msg[5] = 0x00;
      msg[6] = frameCounter146_342;
      msg[7] = crc8(msg.getFrame()->data8, 7);
    }
    
    {
      CanTxMessage msg(CanCategory::NBC, 0x342);
      msg[0] = 0x54;
      msg[1] = running ? 0x2A : 0x04;
      msg[2] = 0x54;
      msg[3] = 0x00;
      msg[4] = 21.0f * Sensor::getOrZero(SensorType::AuxLinear2) * (100.0f / 5.0f); // Range left in KM. 5.0f = l/100km 21.0f = tank volume TODO: Make tank volume and fuel usage somewhat dynamic
      msg[5] = Sensor::getOrZero(SensorType::AuxLinear2); //TODO Fuel Level Sensor in %
      msg[6] = frameCounter146_342;
      msg[7] = crc8(msg.getFrame()->data8, 7);
    }

    {
      CanTxMessage msg(CanCategory::NBC, 0x344);
      msg[0] = 0x00;
      msg[1] = Sensor::getOrZero(SensorType::OilTemperature); // ENGINE TEMPERATURE
      msg[2] = Sensor::getOrZero(SensorType::Clt); // CLT WHEN AVAILABLE
      msg[3] = 0xFF;
      msg[4] = 0xCC; // Mostly 0xCC, in the end of log 0xCB, Ambient Pressure * 2 ?? 204 / 2 = 102 = 1020hPa?
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
      msg[0] = 0x31; // Sometimes swichtes between 0x31 and 0x11
      msg[1] = 0x13;
      msg[2] = 0x00;
      msg[3] = 0x00;
      msg[4] = 0x00;
      msg[5] = 0x00;
      msg[6] = frameCounter148;
      msg[7] = crc8(msg.getFrame()->data8, 7);
    }
    frameCounter148++;
    if (frameCounter148 > 0x7F) {
      frameCounter148 = 0x40;
    }

    {
      CanTxMessage msg(CanCategory::NBC, 0x346);
      msg[0] = 0x00; // TRIP B OR ODOMETER 0x000A8658 = 689752 Displayed as 689.8KM on PAM AMERICA ST
      msg[1] = 0x1A; // TRIP B OR ODOMETER
      msg[2] = 0x2B; // TRIP B OR ODOMETER
      msg[3] = 0x55; // TRIP B OR ODOMETER
      msg[4] = 0x00;
      msg[5] = 0x4F;
      msg[6] = 0x80;
      msg[7] = 0x00;
    }

    {
      CanTxMessage msg(CanCategory::NBC, 0x348);
      msg[0] = 0x00;
      msg[1] = 0x00; // ALWAYS COUNTING UP
      msg[2] = 0x00; // ALWAYS COUNTING UP
      msg[3] = 0x0D; // STANDING STILL: 0x0D, MOVING: 0x17
      msg[4] = 0xAC; // IDLE: 0xAC, DRIVING: Moving down a lot from base value 0xD4
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

void boardUpdateDash(CanCycle cycle) {
  handleHarleyCAN(cycle);
}

#endif // EFI_CAN_SUPPORT