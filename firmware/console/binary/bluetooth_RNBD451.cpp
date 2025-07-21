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

static void btWrite(TsChannelBase* tsChannel, const char *str)
{
	/* Just a wrapper for debug purposes */
#if EFI_BLUETOOTH_SETUP_DEBUG
	efiPrintf("sending %s", str);
#endif
	tsChannel->write((uint8_t *)str, strlen(str));
}

static int btReadLine(TsChannelBase* tsChannel, char *str, size_t max_len) {
	size_t len = 0;

	/* read until end of line */
	do {
		if (len >= max_len) {
			efiPrintf("BT reply is unexpectedly long");
			return -1;
		}
		if (tsChannel->readTimeout((uint8_t *)&str[len], 1, btModuleTimeout) != 1) {
			efiPrintf("Timeout waiting for BT reply after %d byte(s)", len);
			return -1;
		}
	} while (str[len++] != '\n');

	/* termination */
	if (len < max_len)
		str[len] = 0;
	else
		str[max_len - 1] = 0;

#if EFI_BLUETOOTH_SETUP_DEBUG
	if (len) {
		efiPrintf("Received %d %s", len, str);
	}
#endif

	return len;
}

static int btWaitOk(SerialTsChannelBase* tsChannel) {
	int len;
	int ret = -1;
	char tmp[16];

	/* wait for 'AOK\r\n' */
	len = btReadLine(tsChannel, tmp, sizeof(tmp));
	if (len == 5) {
		if (strncmp(tmp, "AOK", 3) == 0)
			ret = 0;
	}

	return ret;
}

static int btWaitReboot(SerialTsChannelBase* tsChannel) {
	int len;
	int ret = -1;
	char tmp[16];

	/* wait for '%REBOOT%\r\n' */
	len = btReadLine(tsChannel, tmp, sizeof(tmp));
	if (len == 10) {
		if (strncmp(tmp, "%REBOOT%", 8) == 0)
			ret = 0;
	}

	return ret;
}

// Main communication code
// We assume that the user has disconnected the software before starting the code.
static void runCommands(SerialTsChannelBase* tsChannel) {
	char tmp[255];

  // In order to run any commands on the RNBD451, we need to enter the command mode by sending $$$
  btWrite(tsChannel, "$$$");
  chThdSleepMilliseconds(5); // TODO: proper wait for "CMD>" without linefeed

#if EFI_BLUETOOTH_SETUP_DEBUG
		/* Debug, gets a lot of information in a single command */
		btWrite(tsChannel, "D\r");
		btReadLine(tsChannel, tmp, sizeof(tmp));
#endif

  /*
  Set Device Name With Address (S-,<text>)
  Format: S-,<text>
  This command sets a serialized Bluetooth name for the device, where <text> is up to 15
  alphanumeric characters. This command automatically appends the last two bytes of the Bluetooth
  MAC address along with _ (underscore) to the name, which is useful for generating a custom
  name with unique numbering. This command does not have a corresponding get command.
  Default: N/A
  Example: S-,MyDevice // Set the device name to MyDevice_XXXX
  Response: AOK // Success
  Err // Syntax error or invalid parameter
  Note: This parameter is stored in PDS and is effective after restarting advertisement.
  */
  chsnprintf(tmp, sizeof(tmp), "S-,%s\r", btName);
	btWrite(tsChannel, tmp);
	if (btWaitOk(tsChannel) != 0) {
		goto cmdFailed;
	}

  /*
  Set Application Options (SR,<hex16>)
  Format: SR,<hex16>
  This command sets the supported feature of the RNBD451 module. The input parameter is a 16-bit
  bitmap that indicates the supported features.
  Note: After changing the features, a reboot is necessary to make the changes effective.
  */
	chsnprintf(tmp, sizeof(tmp), "SR,1001\r"); // 0x1001 = enable all the status LEDS
	btWrite(tsChannel, tmp);
	if (btWaitOk(tsChannel) != 0) {
		goto cmdFailed;
	}

  // Now reset module to apply new settings
  btWrite(tsChannel, "R,1\r");
  if (btWaitOk(tsChannel) != 0) {
    efiPrintf("BT failed to reset");
  }

	efiPrintf("SUCCESS! All commands passed to the Bluetooth module!");
	return;

cmdFailed:
	efiPrintf("FAIL! Command %s failed", tmp);
}

void bluetoothStart(bluetooth_module_e moduleType, const char *baudRate, const char *name, const char *pinCode) {
	static const char *usage = "Usage: bluetooth_<hc05/hc06/bk/jdy/RNBD451> <baud> <name> <pincode>";

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
