#include "pch.h"
#include "storage.h"
#include <cmath>

namespace {
struct trip_odometer_persistent_state_s {
	uint32_t consumedGrams;
	float consumedRemainder;
	uint32_t distanceMeters;
	float distanceRemainder;
	uint32_t ignitionOnSeconds;
	uint32_t engineRunningSeconds;
};
}

void TripOdometer::initNoConfiguration() {
	reset();
	m_dirty = false;
	m_stopWriteQueued = false;
	m_seenRunningSinceBoot = false;

#if EFI_CONFIGURATION_STORAGE
	storageReqestReadID(EFI_TRIP_ODOMETER_RECORD_ID);
#endif // EFI_CONFIGURATION_STORAGE
}

void TripOdometer::reset() {
	m_consumedGrams = 0;
	m_consumedRemainder = 0;

	m_distanceMeters = 0;
	m_distanceRemainder = 0;

	m_slowCallbackCounter = 0;
	m_engineRunningSeconds = 0;
	m_ignitionOnSeconds = 0;

	m_rate = 0;
	m_timer.reset();
	m_dirty = true;
	m_stopWriteQueued = false;
	m_seenRunningSinceBoot = false;
}

void TripOdometer::store() {
#if EFI_CONFIGURATION_STORAGE
	trip_odometer_persistent_state_s state = {
		.consumedGrams = m_consumedGrams,
		.consumedRemainder = m_consumedRemainder,
		.distanceMeters = m_distanceMeters,
		.distanceRemainder = m_distanceRemainder,
		.ignitionOnSeconds = m_ignitionOnSeconds,
		.engineRunningSeconds = m_engineRunningSeconds,
	};

	if (storageWrite(EFI_TRIP_ODOMETER_RECORD_ID, reinterpret_cast<const uint8_t*>(&state), sizeof(state)) == StorageStatus::Ok) {
		m_dirty = false;
	}
#endif // EFI_CONFIGURATION_STORAGE
}

void TripOdometer::load() {
#if EFI_CONFIGURATION_STORAGE
	trip_odometer_persistent_state_s state;
	if (storageRead(EFI_TRIP_ODOMETER_RECORD_ID, reinterpret_cast<uint8_t*>(&state), sizeof(state)) != StorageStatus::Ok) {
		return;
	}

	// This record has no CRC/version envelope yet, so guard against random flash content.
	// Corrupted remainder values could otherwise trigger very long loops in callbacks.
	bool validConsumedRemainder = std::isfinite(state.consumedRemainder)
		&& (state.consumedRemainder >= 0)
		&& (state.consumedRemainder < 1.0f);
	bool validDistanceRemainder = std::isfinite(state.distanceRemainder)
		&& (state.distanceRemainder >= 0)
		&& (state.distanceRemainder < 1.0f);
	if (!validConsumedRemainder || !validDistanceRemainder) {
		efiPrintf("TripOdometer: invalid persisted state, resetting");
		reset();
		return;
	}

	m_consumedGrams = state.consumedGrams;
	m_consumedRemainder = state.consumedRemainder;
	m_distanceMeters = state.distanceMeters;
	m_distanceRemainder = state.distanceRemainder;
	m_ignitionOnSeconds = state.ignitionOnSeconds;
	m_engineRunningSeconds = state.engineRunningSeconds;
	m_slowCallbackCounter = 0;
	m_rate = 0;
	m_timer.reset();
	m_dirty = false;
	m_stopWriteQueued = false;
	m_seenRunningSinceBoot = false;
#endif // EFI_CONFIGURATION_STORAGE
}

void TripOdometer::consumeFuel(float grams, efitick_t nowNt) {
// we have some drama with simulator busy loop in reality :(
#if EFI_PROD_CODE || EFI_UNIT_TEST
	m_stopWriteQueued = false;
	m_seenRunningSinceBoot = true;

	m_consumedRemainder += grams;

  // 1000grams of fuel between invocations of TripOdometer logic means something very wrong, we do not control cruise ship engines yet!
  if (m_consumedRemainder > 1000) {
    firmwareError(ObdCode::OBD_PCM_Processor_Fault, "m_consumedRemainder busy loop %f %f", m_consumedRemainder, grams);
    return;
  }
	// A racecar with a very large fuel tank might consume 60kg of fuel on a single run of the ECU
	// we use integers to gain dynamic range of about 10^9 which is more than float would give us
	// optimized for lots of small pulses
	while (m_consumedRemainder >= 1) {
		m_consumedRemainder--;
		m_consumedGrams++;
		m_dirty = true;
	}

	float elapsedSecond = m_timer.getElapsedSecondsAndReset(nowNt);

	// If it's been a long time since last injection, ignore this pulse
	if (elapsedSecond > 0.2f) {
		m_rate = 0;
	} else {
		m_rate = grams / elapsedSecond;
	}
#endif // EFI_PROD_CODE || EFI_UNIT_TEST
}

void TripOdometer::onEngineStop() {
#if EFI_CONFIGURATION_STORAGE
	// On single-bank STM32, flash erase/write stalls CPU for ~1s.
	// Avoid a write right after boot (engine is already "stopped") until we've seen a real run.
	if (!m_seenRunningSinceBoot) {
		return;
	}

	if (m_stopWriteQueued || !m_dirty) {
		return;
	}

	if (storageRequestWriteID(EFI_TRIP_ODOMETER_RECORD_ID, false)) {
		m_stopWriteQueued = true;
	}
#endif // EFI_CONFIGURATION_STORAGE
}

uint32_t TripOdometer::getConsumedGrams() const {
	return m_consumedGrams;
}

float TripOdometer::getConsumptionGramPerSecond() const {
	return m_rate;
}

void TripOdometer::onSlowCallback() {
	bool changed = false;

	float meterPerSecond = Sensor::getOrZero(SensorType::VehicleSpeed) / 3.6f;
	float metersThisTick = meterPerSecond * (SLOW_CALLBACK_PERIOD_MS / 1000.0f);

	m_distanceRemainder += metersThisTick;
	if (!std::isfinite(m_distanceRemainder) || (m_distanceRemainder < 0) || (m_distanceRemainder > 1000.0f)) {
		// Prevent runaway loop if state became corrupted.
		firmwareError(ObdCode::OBD_PCM_Processor_Fault, "TripOdometer bad distance remainder %f", m_distanceRemainder);
		m_distanceRemainder = 0;
	}
	while (m_distanceRemainder > 1.0f) {
		m_distanceMeters++;
		m_distanceRemainder--;
		changed = true;
	}

	constexpr float slowCallbackPerSecond = 1000 / SLOW_CALLBACK_PERIOD_MS;
	m_slowCallbackCounter++;
	if (m_slowCallbackCounter == slowCallbackPerSecond) {
		m_slowCallbackCounter = 0;

		m_ignitionOnSeconds++;
		changed = true;

#if EFI_SHAFT_POSITION_INPUT
		if (engine->rpmCalculator.isRunning()) {
			m_engineRunningSeconds++;
			m_stopWriteQueued = false;
			m_seenRunningSinceBoot = true;
		}
#endif // EFI_SHAFT_POSITION_INPUT
	}

	if (changed) {
		m_dirty = true;
	}
}

uint32_t TripOdometer::getDistanceMeters() const {
	return m_distanceMeters;
}

uint32_t TripOdometer::getIgnitionOnTime() const {
	return m_ignitionOnSeconds;
}

uint32_t TripOdometer::getEngineRunTime() const {
	return m_engineRunningSeconds;
}
