#include "pch.h"

#include "cruise_control.h"
#include "sensor.h"

namespace {
constexpr float kCruisePidP = 0.5f;
constexpr float kCruisePidI = 0.05f;
constexpr float kCruisePidD = 0.0f;
constexpr int16_t kCruisePidMin = -30;
constexpr int16_t kCruisePidMax = 30;
constexpr float kCruiseSetpointRampKphPerS = 2.0f;
}

PUBLIC_API_WEAK float boardCruiseSetpointRampRate() {
	return kCruiseSetpointRampKphPerS;
}

void CruiseControl::initNoConfiguration() {
	m_pid.initPidClass(&m_pidConfig);

	m_shouldResetPid = true;
}

void CruiseControl::setDefaultConfiguration() {
	engineConfiguration->cruisePid.pFactor = kCruisePidP;
	engineConfiguration->cruisePid.iFactor = kCruisePidI;
	engineConfiguration->cruisePid.dFactor = kCruisePidD;
	engineConfiguration->cruisePid.periodMs = FAST_CALLBACK_PERIOD_MS;
	engineConfiguration->cruisePid.minValue = kCruisePidMin;
	engineConfiguration->cruisePid.maxValue = kCruisePidMax;

	engineConfiguration->cruiseMinSpeed = 42;
	engineConfiguration->cruiseMaxSpeed = 146;
}

void CruiseControl::onConfigurationChange(engine_configuration_s const * /*previousConfig*/) {
	m_shouldResetPid = true;
}

void CruiseControl::onFastCallback() {
	if (m_status != CruiseControlStatus::Enabled) {
		if (m_shouldResetPid) {
			resetPid();
		}
		m_throttleOffset = 0;
		m_throttlePosition = 0;
	} else {
		ClosedLoopController::update();
	}

	engine->outputChannels.cruiseDesiredSpeed = m_desiredSpeedKph;
	engine->outputChannels.cruiseStatus = static_cast<uint8_t>(m_status);
	engine->outputChannels.cruiseThrottleOffset = m_throttleOffset;
}

void CruiseControl::onEngineStop() {
	resetPid();
	m_status = CruiseControlStatus::Disabled;
}

void CruiseControl::onIgnitionStateChanged(bool ignitionOn) {
	if (!ignitionOn) {
		m_status = CruiseControlStatus::Disabled;
		resetPid();
	}
}

float CruiseControl::getDesiredCCSpeed() const {
	return m_desiredSpeedKph;
}

void CruiseControl::setDesiredCCSpeed(float speedKph) {
	m_desiredSpeedKph = sanitizeSpeed(speedKph);
}

void CruiseControl::increaseDesiredCCSpeed() {
	m_desiredSpeedKph = sanitizeSpeed(m_desiredSpeedKph + 1.0f);
}

void CruiseControl::decreaseDesiredCCSpeed() {
	m_desiredSpeedKph = sanitizeSpeed(m_desiredSpeedKph - 1.0f);
}

void CruiseControl::setCCStatus(CruiseControlStatus status) {
	if (m_status == status) {
		return;
	}

	m_status = status;
	m_shouldResetPid = true;

	if (m_status != CruiseControlStatus::Enabled) {
		m_throttleOffset = 0;
		m_throttlePosition = 0;
	} else {
		// Start the setpoint ramp from the current speed
		m_setpointKph = Sensor::get(SensorType::VehicleSpeed).value_or(m_desiredSpeedKph);
	}
}

CruiseControlStatus CruiseControl::getCCStatus() const {
	return m_status;
}

void CruiseControl::engageCCAtCurrentSpeed() {
	auto speed = Sensor::get(SensorType::VehicleSpeed);
	if (!speed.Valid) {
		return;
	}

	float minSpeed = static_cast<float>(engineConfiguration->cruiseMinSpeed);
	float maxSpeed = static_cast<float>(engineConfiguration->cruiseMaxSpeed);

	if (((minSpeed > 0) && (speed.Value < minSpeed)) || ((maxSpeed > 0) && (speed.Value > maxSpeed))) {
		return;
	}

	m_desiredSpeedKph = sanitizeSpeed(speed.Value);
	if (m_desiredSpeedKph) {
		setCCStatus(CruiseControlStatus::Enabled);
	}
}

void CruiseControl::resumeCC() {
	float minSpeed = static_cast<float>(engineConfiguration->cruiseMinSpeed);
	float maxSpeed = static_cast<float>(engineConfiguration->cruiseMaxSpeed);

	if (((minSpeed > 0) && (m_desiredSpeedKph < minSpeed)) || ((maxSpeed > 0) && (m_desiredSpeedKph > maxSpeed))) {
		return;
	}

	if (m_desiredSpeedKph) {
		setCCStatus(CruiseControlStatus::Enabled);
	}
}

expected<float> CruiseControl::getSetpoint() {
	if (m_desiredSpeedKph <= 0) {
		return unexpected;
	}

	// Ramp up to the desired speed so resuming from far below doesn't kick
	float rate = boardCruiseSetpointRampRate();
	if (rate <= 0) {
		// Unset in older tunes
		rate = kCruiseSetpointRampKphPerS;
	}
	float maxStep = rate * FAST_CALLBACK_PERIOD_MS / 1000.0f;
	m_setpointKph = std::min(m_setpointKph + maxStep, m_desiredSpeedKph);

	return m_setpointKph;
}

expected<float> CruiseControl::observePlant() {
	auto speed = Sensor::get(SensorType::VehicleSpeed);
	if (!speed.Valid) {
		return unexpected;
	}

	return speed.Value;
}

expected<percent_t> CruiseControl::getOpenLoop(float /*target*/) {
	return 0;
}

expected<percent_t> CruiseControl::getClosedLoop(float target, float observation) {
	if (m_shouldResetPid) {
		resetPid();
		// Take over from the current throttle so releasing the grip doesn't drop it
		m_basePosition = Sensor::getOrZero(SensorType::Tps1);
	}

	if (Sensor::getOrZero(SensorType::Rpm) <= 0) {
		m_pid.reset();
		return 0;
	}

	// Hold the output while the grip overrides, so the PID doesn't wind down
	if (m_pedalOverride) {
		return m_throttleOffset;
	}

	return m_pid.getOutput(target, observation, FAST_CALLBACK_PERIOD_MS / 1000.0f);
}

void CruiseControl::setOutput(expected<percent_t> outputValue) {
	if (!outputValue.Valid) {
		m_pid.reset();
		m_throttleOffset = 0;
		m_throttlePosition = 0;
		return;
	}

	m_throttleOffset = outputValue.Value;
	m_throttlePosition = clampPercentValue(m_basePosition + m_throttleOffset);
}

void CruiseControl::resetPid() {
	// Cruise starts from the takeover position, so there is no fixed PID offset
	m_pidConfig = engineConfiguration->cruisePid;
	m_pidConfig.offset = 0;

	m_pid.reset();
	// Keep the I-term inside the output limits so it can't wind up behind the clamp
	m_pid.iTermMin = m_pidConfig.minValue;
	m_pid.iTermMax = m_pidConfig.maxValue;
	m_throttleOffset = 0;
	m_throttlePosition = 0;
	m_shouldResetPid = false;
}

float CruiseControl::sanitizeSpeed(float speedKph) const {
	if (speedKph == 0.0f) {
		return 0.0f;
	}
	float result = maxF(0.0f, speedKph);
	float minSpeed = static_cast<float>(engineConfiguration->cruiseMinSpeed);
	float maxSpeed = static_cast<float>(engineConfiguration->cruiseMaxSpeed);

	if (maxSpeed > 0) {
		result = minF(result, maxSpeed);
	}

	if (minSpeed > 0) {
		result = maxF(result, minSpeed);
	}

	return result;
}

float getDesiredCCSpeed() {
	return engine->module<CruiseControl>().unmock().getDesiredCCSpeed();
}

void setDesiredCCSpeed(float speedKph) {
	engine->module<CruiseControl>().unmock().setDesiredCCSpeed(speedKph);
}

void increaseDesiredCCSpeed() {
	engine->module<CruiseControl>().unmock().increaseDesiredCCSpeed();
}

void decreaseDesiredCCSpeed() {
	engine->module<CruiseControl>().unmock().decreaseDesiredCCSpeed();
}

void setCCStatus(CruiseControlStatus status) {
	engine->module<CruiseControl>().unmock().setCCStatus(status);
}

CruiseControlStatus getCCStatus() {
	return engine->module<CruiseControl>().unmock().getCCStatus();
}

void engageCCAtCurrentSpeed() {
	engine->module<CruiseControl>().unmock().engageCCAtCurrentSpeed();
}

void resumeCC() {
	engine->module<CruiseControl>().unmock().resumeCC();
}

percent_t getCruiseControlThrottlePosition() {
	return engine->module<CruiseControl>().unmock().getThrottlePosition();
}

void setCruiseControlPedalOverride(bool isOverriding) {
	engine->module<CruiseControl>().unmock().setPedalOverride(isOverriding);
}
