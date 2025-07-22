/**
 * @file	bluetooth_RNBD451.cpp
 *
 * Original bluetooth.cpp felt like a mess seperating the logic between different module logics
 * I think it is best to switch the implementations using different files for different modules
 * CHIP DOCS: https://www.microchip.com/en-us/product/rnbd451pe#Documentation
 * 
 * TLDR:
 * When in Command mode, valid ASCII commands are issued to control or configure the RNBD451
  module. All commands end with a carriage return <cr>('\r', \x0d). Do not issue any
  subsequent command until a response is received for the previous command.
  For commands, AOK indicates a positive or successful response, whereas, Err indicates an error or
  negative response. By default, when the RNBD451 module is ready to receive the next command,
  the command prompt CMD> is sent to UART.

  Get commands return the value requested by the corresponding command to be retrieved. Most
  of the other commands return either AOK (<AOK><CR><LF>), which indicates a positive response,
  or Err (<Err<CR><LF>) as a negative response.
 */

#include "pch.h"

#include "tunerstudio.h"

#include "tunerstudio_io.h"
#include "bluetooth.h"

#include <stdio.h>
#include <ctype.h>

#if EFI_BLUETOOTH_SETUP

#ifndef EFI_BLUETOOTH_SETUP_DEBUG
#define EFI_BLUETOOTH_SETUP_DEBUG TRUE
#endif

#ifndef EFI_BLUETOOTH_RNBD451_BAUD
#define EFI_BLUETOOTH_RNBD451_BAUD 115200
#endif

static volatile bool btSetupIsRequested = false;

bluetooth_module_e btModuleType;
static char btName[20 + 1];
static char btPinCode[4 + 1];

static const int btModuleTimeout = TIME_MS2I(2500);

// There might be some stuff still pending
// Let's just read until there is nothing left = timeout
static void btReadIntoVoid(TsChannelBase* tsChannel)
{
  uint8_t buf = 0x0;
  while (true) {
    if (tsChannel->readTimeout(&buf, 1, btModuleTimeout) != 1) {
			return;
		}
  }
}

static void btWrite(TsChannelBase* tsChannel, const char *cmd, uint8_t cmdLen)
{
	/* Just a wrapper for debug purposes */
#if EFI_BLUETOOTH_SETUP_DEBUG
	efiPrintf("sending %s", cmd);
#endif
	tsChannel->write((uint8_t *)cmd, cmdLen);
}

// https://github.com/MicrochipTech/RNBD451_BLE_ARDUINO_LIBRARY/blob/main/src/rnbd.cpp
static bool btRNBDSendCommandReceiveResponse(TsChannelBase* tsChannel, const char *cmdMsg, uint8_t cmdLen, const char *responseMsg, uint8_t responseLen) {
  unsigned int responseRead = 0, responseCheck = 0;
	char response[16];
  
  // Clear anything unread
  btReadIntoVoid(tsChannel);

  // Sending Command to UART
  btWrite(tsChannel, cmdMsg, cmdLen);

  // Read until timeout or full message received
  while(responseRead < responseLen) {
		if (tsChannel->readTimeout((uint8_t *)&response[responseRead], 1, btModuleTimeout) != 1) {
			efiPrintf("Timeout waiting for BT reply after %d byte(s), expected %d byte(s)", responseRead, responseLen);
			return false;
		}
    responseRead++;
  }
  // Comparing the Response with expected result
  for (responseCheck = 0; responseCheck < responseRead; responseCheck++) {
    if (response[responseCheck] != responseMsg[responseCheck]) {
      // Termination for printing
      response[responseRead] = 0x0;
			efiPrintf("Unexpected response: %s", response);
      return false;
    }
  }
  return true;
}

static bool btRNBDEnterCmdMode(void) {
  const char cmdRequest[] = { '$', '$', '$' };
  const char cmdModeResponse[] = { 'C', 'M', 'D', '>', ' ' };
  return btRNBDSendCommandReceiveResponse(cmdRequest, 3U, cmdModeResponse, 5U);
}

static bool btRNBDSetName(void) {
	char cmd[64];
  const char response[] = { 'A', 'O', 'K', '\r', '\n', 'C', 'M', 'D', '>', ' ' };
  chsnprintf(cmd, sizeof(cmd), "S-,%s\r\n", btName);

  return btRNBDSendCommandReceiveResponse(cmd, strlen(btName) + 5U, response, 10U);
}

static bool btRNBDAppOptions(void) {
  const char cmdRequest[] = "SR,1001\r\n";
  const char response[] = { 'A', 'O', 'K', '\r', '\n', 'C', 'M', 'D', '>', ' ' };
  return btRNBDSendCommandReceiveResponse(cmdRequest, 9, response, 10U);
}

static bool btRNBDReboot(void) {
  bool rebootStatus = false;
  const char cmdRequest[] = "R,1\r\n";
  const char rebootResponse[] = { 'R', 'e', 'b', 'o', 'o', 't', 'i', 'n', 'g', '\r', '\n' };

  rebootStatus = btRNBDSendCommandReceiveResponse(cmdRequest, 5U, rebootResponse, 11U);
  chThdSleepMilliseconds(250);
  return rebootStatus;
}

// Main communication code
// We assume that the user has disconnected the software before starting the code.
static void runCommands(SerialTsChannelBase* tsChannel) {
  if (!btRNBDEnterCmdMode()) {
	  efiPrintf("Reboot failed");
    return;
  }

  if (!btRNBDSetName()) {
	  efiPrintf("Setting name failed");
    return;
  }

  if (!btRNBDAppOptions()) {
	  efiPrintf("Setting Application Options failed");
    return;
  }

  if (!btRNBDReboot()) {
	  efiPrintf("BT Rebooting failed");
    return;
  }

	efiPrintf("SUCCESS! All commands passed to the Bluetooth module!");
	return;
}

void bluetoothStart(bluetooth_module_e moduleType, const char *baudRate, const char *name, const char *pinCode) {
	static const char *usage = "Usage: bluetooth_rnbd451 <baud> <name> <pincode>";

	if ((baudRate == nullptr) || (name == nullptr) || (pinCode == nullptr)) {
		efiPrintf("%s", usage);
		return;
	}

	if (getBluetoothChannel() == nullptr) {
		efiPrintf("This firmware does not support bluetooth [%s]", getTsSignature());
		return;
	}

	if (btSetupIsRequested) {
		efiPrintf("The Bluetooth module init procedure is already started!");
		return;
	}

	// now check the arguments and add other commands:
	// 1) baud rate
  // WITH RNBD451 we have a fixed baudrate

	// 2) check name
	if ((strlen(name) < 1) || (strlen(name) > 20)) {
		efiPrintf("Wrong <name> parameter! Up to 20 characters expected! %s", usage);
		return;
	}

	// 3) check pin code
	if (strlen(pinCode) != 4) {
		efiPrintf("Wrong <pincode> parameter! 4 digits expected! %s", usage);
		return;
	}
	for (int i = 0; i < 4; i++) {
		if (!isdigit(pinCode[i])) {
			efiPrintf("<pincode> should contain digits only %s", usage);
			return;
		}
	}

	/* copy settings */
	strncpy(btName, name, 20);
	strncpy(btPinCode, pinCode, 4);

	btModuleType = moduleType;
	btSetupIsRequested = true;
}

// Called after 1S of silence on BT UART...
void bluetoothSoftwareDisconnectNotify(SerialTsChannelBase* tsChannel) {
	if (btSetupIsRequested) {
		efiPrintf("*** Bluetooth module setup procedure ***");

		/* JDY33 & JDY31 supports disconnect on request */
		if ((btModuleType != BLUETOOTH_JDY_3x) &&
			(btModuleType != BLUETOOTH_JDY_31)) {
			efiPrintf("!Warning! Please make sure you're not currently using the BT module for communication (not paired)!");
			efiPrintf("TO START THE PROCEDURE: PLEASE DISCONNECT YOUR PC COM-PORT FROM THE BOARD NOW!");
			efiPrintf("After that please don't turn off the board power and wait for ~15 seconds to complete. Then reconnect to the board!");
		}

		uint8_t tmp[1];
		if (tsChannel->readTimeout(tmp, 1, BLUETOOTH_SILENT_TIMEOUT) != 0) {
			efiPrintf("The Bluetooth module init procedure is cancelled (wait for silent timeout)!");
			btSetupIsRequested = false;
			return;
		}

		runCommands(tsChannel);
		btSetupIsRequested = false;
	}
}

#endif /* EFI_BLUETOOTH_SETUP */
