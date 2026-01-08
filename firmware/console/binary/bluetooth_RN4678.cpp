/**
 * @file	bluetooth_RN4678.cpp
 *
 * RN4678 Bluetooth Dual Mode module setup.
 * CHIP DOCS: https://ww1.microchip.com/downloads/en/DeviceDoc/50002506B.pdf
 *
 * Command mode entry: $$$
 * Responses: AOK on success, ERR<code> on failure. CMD> prompt may follow.
 * Commands end with <CR> ('\r', 0x0d).
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

static volatile bool btSetupIsRequested = false;

bluetooth_module_e btModuleType;
static int setBaudIdx = -1;
static char btName[16 + 1];
static char btPinCode[6 + 1];
static uint8_t workingBaudIndex;

static const struct {
	uint32_t rate;
	uint8_t code;
} baudRates[] = {
	// common rates first
	{115200, 0x03},
	{9600, 0x09},
	{38400, 0x05},
	{57600, 0x04},
	{19200, 0x07},
	{4800, 0x0A},
	{2400, 0x0B},
	{230400, 0x02},
	{460800, 0x01},
	{921600, 0x00},
	{307200, 0x10},
	{1843200, 0x0F},
	{3000000, 0x0C},
	{3250000, 0x0E},
	{4000000, 0x0D},
	{28800, 0x06},
	{14400, 0x08},
};

static const int btModuleTimeout = TIME_MS2I(2500);
static const int btFlushTimeout = TIME_MS2I(50);

static void btWrite(TsChannelBase* tsChannel, const char *str, size_t len)
{
	/* Just a wrapper for debug purposes */
#if EFI_BLUETOOTH_SETUP_DEBUG
	efiPrintf("sending %.*s", (int)len, str);
#endif
	tsChannel->write((uint8_t *)str, len);
}

// There might be some stuff still pending; read until timeout.
static void btReadIntoVoid(TsChannelBase* tsChannel)
{
	uint8_t buf = 0x0;
	while (true) {
		if (tsChannel->readTimeout(&buf, 1, btFlushTimeout) != 1) {
			return;
		}
	}
}

static bool btReadUntilToken(TsChannelBase* tsChannel, const char *token, const char *context)
{
	char response[64];
	size_t read = 0;

	while (read < (sizeof(response) - 1)) {
		if (tsChannel->readTimeout((uint8_t *)&response[read], 1, btModuleTimeout) != 1) {
			efiPrintf("Timeout waiting for %s after %d byte(s)", context, read);
			return false;
		}
		read++;
		response[read] = 0;
		if (strstr(response, token) != nullptr) {
			return true;
		}
	}

	efiPrintf("Unexpected response while waiting for %s: %s", context, response);
	return false;
}

static bool btWaitAok(SerialTsChannelBase* tsChannel, const char *context)
{
	char response[64];
	size_t read = 0;

	while (read < (sizeof(response) - 1)) {
		if (tsChannel->readTimeout((uint8_t *)&response[read], 1, btModuleTimeout) != 1) {
			efiPrintf("Timeout waiting for response to %s after %d byte(s)", context, read);
			return false;
		}
		read++;
		response[read] = 0;
		if (strstr(response, "AOK") != nullptr) {
			return true;
		}
		if (strstr(response, "ERR") != nullptr) {
			efiPrintf("RN4678 error on %s: %s", context, response);
			return false;
		}
	}

	efiPrintf("Unexpected response to %s: %s", context, response);
	return false;
}

static bool btWaitAokOrReboot(SerialTsChannelBase* tsChannel, const char *context)
{
	char response[64];
	size_t read = 0;

	while (read < (sizeof(response) - 1)) {
		if (tsChannel->readTimeout((uint8_t *)&response[read], 1, btModuleTimeout) != 1) {
			if (read > 0) {
				efiPrintf("Partial response to %s: %s", context, response);
			}
			// Module likely rebooted before sending a full response.
			return true;
		}
		read++;
		response[read] = 0;
		if (strstr(response, "AOK") != nullptr || strstr(response, "Reboot") != nullptr) {
			return true;
		}
		if (strstr(response, "ERR") != nullptr) {
			efiPrintf("RN4678 error on %s: %s", context, response);
			return false;
		}
	}

	efiPrintf("Unexpected response to %s: %s", context, response);
	return false;
}

static bool btRN4678EnterCmdMode(SerialTsChannelBase* tsChannel)
{
	const char cmdRequest[] = { '$', '$', '$' };
	btReadIntoVoid(tsChannel);
	btWrite(tsChannel, cmdRequest, sizeof(cmdRequest));
	return btReadUntilToken(tsChannel, "CMD", "CMD");
}

static bool btRN4678ExitCmdMode(SerialTsChannelBase* tsChannel)
{
	const char cmdRequest[] = { '-', '-', '-', '\r' };
	btReadIntoVoid(tsChannel);
	btWrite(tsChannel, cmdRequest, sizeof(cmdRequest));
	return btReadUntilToken(tsChannel, "END", "END");
}

static bool btRN4678SetName(SerialTsChannelBase* tsChannel)
{
	char cmd[32];
	chsnprintf(cmd, sizeof(cmd), "SN,%s\r", btName);
	btReadIntoVoid(tsChannel);
	btWrite(tsChannel, cmd, strlen(cmd));
	return btWaitAok(tsChannel, "SN");
}

static bool btRN4678SetPin(SerialTsChannelBase* tsChannel)
{
	char cmd[16];
	chsnprintf(cmd, sizeof(cmd), "SP,%s\r", btPinCode);
	btReadIntoVoid(tsChannel);
	btWrite(tsChannel, cmd, strlen(cmd));
	return btWaitAok(tsChannel, "SP");
}

static bool btRN4678SetBaud(SerialTsChannelBase* tsChannel)
{
	char cmd[16];
	chsnprintf(cmd, sizeof(cmd), "SU,%02X\r", baudRates[setBaudIdx].code);
	btReadIntoVoid(tsChannel);
	btWrite(tsChannel, cmd, strlen(cmd));
	return btWaitAok(tsChannel, "SU");
}

static bool btRN4678Reboot(SerialTsChannelBase* tsChannel)
{
	const char cmdRequest[] = "R,1\r";
	btReadIntoVoid(tsChannel);
	btWrite(tsChannel, cmdRequest, sizeof(cmdRequest) - 1);
	if (!btWaitAokOrReboot(tsChannel, "R,1")) {
		return false;
	}
	chThdSleepMilliseconds(250);
	return true;
}

uint8_t findBaudIndex(SerialTsChannelBase* tsChannel)
{
	for (uint8_t baudIdx = 0; baudIdx < efi::size(baudRates); baudIdx++) {
		tsChannel->stop();
		chThdSleepMilliseconds(10);

		efiPrintf("Restarting at %lu", baudRates[baudIdx].rate);
		tsChannel->start(baudRates[baudIdx].rate);
		chThdSleepMilliseconds(10);

		if (btRN4678EnterCmdMode(tsChannel)) {
			btRN4678ExitCmdMode(tsChannel);
			return baudIdx;
		}
	}

	efiPrintf("Failed to find current RN4678 baud rate");
	tsChannel->start(engineConfiguration->tunerStudioSerialSpeed);
	return 255;
}

// Main communication code
// We assume that the user has disconnected the software before starting the code.
static void runCommands(SerialTsChannelBase* tsChannel) {
	workingBaudIndex = findBaudIndex(tsChannel);
	if (workingBaudIndex == 255) {
		return;
	}

	tsChannel->stop();
	chThdSleepMilliseconds(10);
	tsChannel->start(baudRates[workingBaudIndex].rate);

	if (!btRN4678EnterCmdMode(tsChannel)) {
		efiPrintf("Entering CMD mode failed");
		return;
	}

	if (!btRN4678SetName(tsChannel)) {
		efiPrintf("Setting name failed");
		return;
	}

	if (!btRN4678SetPin(tsChannel)) {
		efiPrintf("Setting pin failed");
		return;
	}

	if (!btRN4678SetBaud(tsChannel)) {
		efiPrintf("Setting baud failed");
		return;
	}

	if (!btRN4678Reboot(tsChannel)) {
		efiPrintf("Rebooting failed");
		return;
	}

	efiPrintf("SUCCESS! All commands passed to the Bluetooth module!");
}

void bluetoothStart(bluetooth_module_e moduleType, const char *baudRate, const char *name, const char *pinCode) {
	static const char *usage = "Usage: bluetooth_rn4678 <baud> <name> <pincode>";

	if (moduleType != BLUETOOTH_RN4678) {
		efiPrintf("This build supports only RN4678 setup");
		return;
	}

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

	// 1) baud rate
	int baud = atoi(baudRate);
	setBaudIdx = -1;
	for (size_t i = 0; i < efi::size(baudRates); i++) {
		if ((int)baudRates[i].rate == baud) {
			setBaudIdx = i;
			break;
		}
	}
	if (setBaudIdx < 0) {
		efiPrintf("Wrong <baud> parameter '%s'! %s", baudRate, usage);
		return;
	}

	// 2) check name
	if ((strlen(name) < 1) || (strlen(name) > 16)) {
		efiPrintf("Wrong <name> parameter! Up to 16 characters expected! %s", usage);
		return;
	}

	// 3) check pin code
	size_t pinLen = strlen(pinCode);
	if ((pinLen != 4) && (pinLen != 6)) {
		efiPrintf("Wrong <pincode> parameter! 4 or 6 digits expected! %s", usage);
		return;
	}
	for (size_t i = 0; i < pinLen; i++) {
		if (!isdigit(pinCode[i])) {
			efiPrintf("<pincode> should contain digits only %s", usage);
			return;
		}
	}

	/* copy settings */
	strncpy(btName, name, sizeof(btName) - 1);
	btName[sizeof(btName) - 1] = 0;
	strncpy(btPinCode, pinCode, sizeof(btPinCode) - 1);
	btPinCode[sizeof(btPinCode) - 1] = 0;

	btModuleType = moduleType;
	btSetupIsRequested = true;
}

// Called after 1S of silence on BT UART...
void bluetoothSoftwareDisconnectNotify(SerialTsChannelBase* tsChannel) {
	if (btSetupIsRequested) {
		efiPrintf("*** Bluetooth module setup procedure ***");

		efiPrintf("!Warning! Please make sure you're not currently using the BT module for communication (not paired)!");
		efiPrintf("TO START THE PROCEDURE: PLEASE DISCONNECT YOUR PC COM-PORT FROM THE BOARD NOW!");
		efiPrintf("After that please don't turn off the board power and wait for ~15 seconds to complete. Then reconnect to the board!");

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
