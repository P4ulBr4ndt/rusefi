#include "pch.h"

#include "cruise_control.h"
#include "sensor.h"

namespace {
constexpr float kCruisePidP = 0.5f;
constexpr float kCruisePidI = 0.05f;
constexpr float kCruisePidD = 0.0f;
constexpr int16_t kCruisePidMin = -30;
constexpr int16_t kCruisePidMax = 30;
}

void CruiseControl::initNoConfiguration() {
	m_pid.initPidClass(&engineConfiguration->cruisePid);
	m_pid.iTermMin = kCruisePidMin;
	m_pid.iTermMax = kCruisePidMax;

	m_shouldResetPid = true;
}

void CruiseControl::setDefaultConfiguration() {
	engineConfiguration->cruisePid.pFactor = kCruisePidP;
	engineConfiguration->cruisePid.iFactor = kCruisePidI;
	engineConfiguration->cruisePid.dFactor = kCruisePidD;
	engineConfiguration->cruisePid.offset = 0;
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

	m_desiredSpeedKph = sanitizeSpeed(speed.Value);
	if (m_desiredSpeedKph) {
		setCCStatus(CruiseControlStatus::Enabled);
}
}

expected<float> CruiseControl::getSetpoint() {
	if (m_desiredSpeedKph <= 0) {
		return unexpected;
	}

	return m_desiredSpeedKph;
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
	}

	if (Sensor::getOrZero(SensorType::Rpm) <= 0) {
		m_pid.reset();
		return 0;
	}

	return m_pid.getOutput(target, observation, FAST_CALLBACK_PERIOD_MS / 1000.0f);
}

void CruiseControl::setOutput(expected<percent_t> outputValue) {
	if (!outputValue.Valid) {
		m_pid.reset();
		m_throttleOffset = 0;
		return;
	}

	m_throttleOffset = outputValue.Value;
}

void CruiseControl::resetPid() {
	m_pid.reset();
	m_throttleOffset = 0;
	m_shouldResetPid = false;
}

float CruiseControl::sanitizeSpeed(float speedKph) const {
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

percent_t getCruiseControlThrottleOffset() {
	return engine->module<CruiseControl>().unmock().getThrottleOffset();
}
