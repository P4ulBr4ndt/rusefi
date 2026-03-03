// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/algo/engine_state.txt
// by class com.rusefi.output.CHeaderConsumer
// begin
#pragma once
#include "rusefi_types.h"
// start of LuaAdjustments
struct LuaAdjustments {
	/**
	 * Lua: Fuel add
	 * units: g
	 * offset 0
	 */
	float fuelAdd = (float)0;
	/**
	 * Lua: Fuel mult
	 * offset 4
	 */
	float fuelMult = (float)0;
	/**
	 * Lua: torque
	 * offset 8
	 */
	float engineTorque = (float)0;
	/**
	offset 12 bit 0 */
	bool clutchUpState : 1 {};
	/**
	offset 12 bit 1 */
	bool brakePedalState : 1 {};
	/**
	offset 12 bit 2 */
	bool acRequestState : 1 {};
	/**
	offset 12 bit 3 */
	bool luaDisableEtb : 1 {};
	/**
	offset 12 bit 4 */
	bool luaIgnCut : 1 {};
	/**
	offset 12 bit 5 */
	bool luaFuelCut : 1 {};
	/**
	offset 12 bit 6 */
	bool clutchDownState : 1 {};
	/**
	offset 12 bit 7 */
	bool disableDecelerationFuelCutOff : 1 {};
	/**
	offset 12 bit 8 */
	bool torqueReductionState : 1 {};
	/**
	offset 12 bit 9 */
	bool jssState : 1 {};
	/**
	offset 12 bit 10 */
	bool opsState : 1 {};
	/**
	offset 12 bit 11 */
	bool unusedBit_14_11 : 1 {};
	/**
	offset 12 bit 12 */
	bool unusedBit_14_12 : 1 {};
	/**
	offset 12 bit 13 */
	bool unusedBit_14_13 : 1 {};
	/**
	offset 12 bit 14 */
	bool unusedBit_14_14 : 1 {};
	/**
	offset 12 bit 15 */
	bool unusedBit_14_15 : 1 {};
	/**
	offset 12 bit 16 */
	bool unusedBit_14_16 : 1 {};
	/**
	offset 12 bit 17 */
	bool unusedBit_14_17 : 1 {};
	/**
	offset 12 bit 18 */
	bool unusedBit_14_18 : 1 {};
	/**
	offset 12 bit 19 */
	bool unusedBit_14_19 : 1 {};
	/**
	offset 12 bit 20 */
	bool unusedBit_14_20 : 1 {};
	/**
	offset 12 bit 21 */
	bool unusedBit_14_21 : 1 {};
	/**
	offset 12 bit 22 */
	bool unusedBit_14_22 : 1 {};
	/**
	offset 12 bit 23 */
	bool unusedBit_14_23 : 1 {};
	/**
	offset 12 bit 24 */
	bool unusedBit_14_24 : 1 {};
	/**
	offset 12 bit 25 */
	bool unusedBit_14_25 : 1 {};
	/**
	offset 12 bit 26 */
	bool unusedBit_14_26 : 1 {};
	/**
	offset 12 bit 27 */
	bool unusedBit_14_27 : 1 {};
	/**
	offset 12 bit 28 */
	bool unusedBit_14_28 : 1 {};
	/**
	offset 12 bit 29 */
	bool unusedBit_14_29 : 1 {};
	/**
	offset 12 bit 30 */
	bool unusedBit_14_30 : 1 {};
	/**
	offset 12 bit 31 */
	bool unusedBit_14_31 : 1 {};
};
static_assert(sizeof(LuaAdjustments) == 16);

// start of speed_density_s
struct speed_density_s {
	/**
	 * Air: Charge temperature estimate
	 * units: deg C
	 * offset 0
	 */
	scaled_channel<int16_t, 100, 1> tCharge = (int16_t)0;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 2
	 */
	uint8_t alignmentFill_at_2[2] = {};
	/**
	 * Air: Charge temperature estimate K
	 * offset 4
	 */
	float tChargeK = (float)0;
};
static_assert(sizeof(speed_density_s) == 8);

// start of cranking_fuel_s
struct cranking_fuel_s {
	/**
	 * Fuel: cranking CLT mult
	 * offset 0
	 */
	float coolantTemperatureCoefficient = (float)0;
	/**
	 * Fuel: cranking TPS mult
	 * offset 4
	 */
	float tpsCoefficient = (float)0;
	/**
	 * Fuel: Cranking cycle base mass
	 * units: mg
	 * offset 8
	 */
	scaled_channel<uint16_t, 100, 1> baseFuel = (uint16_t)0;
	/**
	 * Fuel: Cranking cycle mass
	 * units: mg
	 * offset 10
	 */
	scaled_channel<uint16_t, 100, 1> fuel = (uint16_t)0;
};
static_assert(sizeof(cranking_fuel_s) == 12);

// start of engine_state_s
struct engine_state_s {
	/**
	 * offset 0
	 */
	float injectionMass[MAX_CYLINDER_COUNT] = {};
	/**
	 * offset 48
	 */
	LuaAdjustments lua;
	/**
	 * offset 64
	 */
	speed_density_s sd;
	/**
	 * offset 72
	 */
	cranking_fuel_s crankingFuel;
	/**
	 * @@GAUGE_NAME_FUEL_BARO_CORR@@
	 * offset 84
	 */
	float baroCorrection = (float)0;
	/**
	 * Detected Board ID
	 * units: id
	 * offset 88
	 */
	int16_t hellenBoardId = (int16_t)0;
	/**
	 * @@INDICATOR_NAME_CLUTCH_UP@@
	 * offset 90
	 */
	int8_t clutchUpState = (int8_t)0;
	/**
	 * @@INDICATOR_NAME_BRAKE_DOWN@@
	 * offset 91
	 */
	int8_t brakePedalState = (int8_t)0;
	/**
	 * JSS State
	 * offset 92
	 */
	int8_t jssState = (int8_t)0;
	/**
	 * OPS State
	 * offset 93
	 */
	int8_t opsState = (int8_t)0;
	/**
	 * offset 94
	 */
	int8_t startStopState = (int8_t)0;
	/**
	 * offset 95
	 */
	int8_t smartChipState = (int8_t)0;
	/**
	 * offset 96
	 */
	int8_t smartChipRestartCounter = (int8_t)0;
	/**
	 * offset 97
	 */
	int8_t smartChipAliveCounter = (int8_t)0;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 98
	 */
	uint8_t alignmentFill_at_98[2] = {};
	/**
	offset 100 bit 0 */
	bool startStopPhysicalState : 1 {};
	/**
	 * Harley ACR Active
	offset 100 bit 1 */
	bool acrActive : 1 {};
	/**
	offset 100 bit 2 */
	bool acrEngineMovedRecently : 1 {};
	/**
	offset 100 bit 3 */
	bool heaterControlEnabled : 1 {};
	/**
	offset 100 bit 4 */
	bool luaDigitalState0 : 1 {};
	/**
	offset 100 bit 5 */
	bool luaDigitalState1 : 1 {};
	/**
	offset 100 bit 6 */
	bool luaDigitalState2 : 1 {};
	/**
	offset 100 bit 7 */
	bool luaDigitalState3 : 1 {};
	/**
	 * @@INDICATOR_NAME_CLUTCH_DOWN@@
	offset 100 bit 8 */
	bool clutchDownState : 1 {};
	/**
	offset 100 bit 9 */
	bool unusedBit_24_9 : 1 {};
	/**
	offset 100 bit 10 */
	bool unusedBit_24_10 : 1 {};
	/**
	offset 100 bit 11 */
	bool unusedBit_24_11 : 1 {};
	/**
	offset 100 bit 12 */
	bool unusedBit_24_12 : 1 {};
	/**
	offset 100 bit 13 */
	bool unusedBit_24_13 : 1 {};
	/**
	offset 100 bit 14 */
	bool unusedBit_24_14 : 1 {};
	/**
	offset 100 bit 15 */
	bool unusedBit_24_15 : 1 {};
	/**
	offset 100 bit 16 */
	bool unusedBit_24_16 : 1 {};
	/**
	offset 100 bit 17 */
	bool unusedBit_24_17 : 1 {};
	/**
	offset 100 bit 18 */
	bool unusedBit_24_18 : 1 {};
	/**
	offset 100 bit 19 */
	bool unusedBit_24_19 : 1 {};
	/**
	offset 100 bit 20 */
	bool unusedBit_24_20 : 1 {};
	/**
	offset 100 bit 21 */
	bool unusedBit_24_21 : 1 {};
	/**
	offset 100 bit 22 */
	bool unusedBit_24_22 : 1 {};
	/**
	offset 100 bit 23 */
	bool unusedBit_24_23 : 1 {};
	/**
	offset 100 bit 24 */
	bool unusedBit_24_24 : 1 {};
	/**
	offset 100 bit 25 */
	bool unusedBit_24_25 : 1 {};
	/**
	offset 100 bit 26 */
	bool unusedBit_24_26 : 1 {};
	/**
	offset 100 bit 27 */
	bool unusedBit_24_27 : 1 {};
	/**
	offset 100 bit 28 */
	bool unusedBit_24_28 : 1 {};
	/**
	offset 100 bit 29 */
	bool unusedBit_24_29 : 1 {};
	/**
	offset 100 bit 30 */
	bool unusedBit_24_30 : 1 {};
	/**
	offset 100 bit 31 */
	bool unusedBit_24_31 : 1 {};
	/**
	 * offset 104
	 */
	uint32_t startStopStateToggleCounter = (uint32_t)0;
	/**
	 * offset 108
	 */
	float currentVe = (float)0;
	/**
	 * offset 112
	 */
	float luaSoftSparkSkip = (float)0;
	/**
	 * offset 116
	 */
	float luaHardSparkSkip = (float)0;
	/**
	 * offset 120
	 */
	float tractionControlSparkSkip = (float)0;
	/**
	 * Fuel: Injection counter
	 * offset 124
	 */
	uint32_t fuelInjectionCounter = (uint32_t)0;
	/**
	 * Ign: Spark counter
	 * offset 128
	 */
	uint32_t globalSparkCounter = (uint32_t)0;
	/**
	 * @@GAUGE_NAME_FUEL_LOAD@@
	 * offset 132
	 */
	float fuelingLoad = (float)0;
	/**
	 * @@GAUGE_NAME_IGNITION_LOAD@@
	 * offset 136
	 */
	float ignitionLoad = (float)0;
	/**
	 * units: %
	 * offset 140
	 */
	scaled_channel<uint16_t, 100, 1> veTableYAxis = (uint16_t)0;
	/**
	 * offset 142
	 */
	scaled_channel<int16_t, 10, 1> veTableIdleYAxis = (int16_t)0;
	/**
	 * "Ignition: overcharge canceled"
	 * offset 144
	 */
	uint8_t overDwellCanceledCounter = (uint8_t)0;
	/**
	 * "Ignition: overDwellNotScheduled"
	 * offset 145
	 */
	uint8_t overDwellNotScheduledCounter = (uint8_t)0;
	/**
	 * "Ignition: sparkOutOfOrder"
	 * offset 146
	 */
	uint8_t sparkOutOfOrderCounter = (uint8_t)0;
	/**
	 * "Ignition: undecharge warnings"
	 * offset 147
	 */
	uint8_t dwellUnderChargeCounter = (uint8_t)0;
	/**
	 * "Ignition: overcharge warnings"
	 * offset 148
	 */
	uint8_t dwellOverChargeCounter = (uint8_t)0;
	/**
	 * need 4 byte alignment
	 * units: units
	 * offset 149
	 */
	uint8_t alignmentFill_at_149[3] = {};
	/**
	 * "Ignition: Dwell deviation"
	 * units: %
	 * offset 152
	 */
	float dwellActualRatio = (float)0;
	/**
	 * STFT: Bank
	 * units: %
	 * offset 156
	 */
	float stftCorrection[FT_BANK_COUNT] = {};
};
static_assert(sizeof(engine_state_s) == 164);

// end
// this section was generated automatically by rusEFI tool config_definition_base-all.jar based on (unknown script) controllers/algo/engine_state.txt
