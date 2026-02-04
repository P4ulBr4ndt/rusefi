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

	engineConfiguration->cruiseMinSpeed = 0;
	engineConfiguration->cruiseMaxSpeed = 0;
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

float CruiseControl::getDesiredSpeed() const {
	return m_desiredSpeedKph;
}

void CruiseControl::setDesiredSpeed(float speedKph) {
	m_desiredSpeedKph = sanitizeSpeed(speedKph);
}

void CruiseControl::IncreaseDesiredSpeed() {
	m_desiredSpeedKph = sanitizeSpeed(m_desiredSpeedKph + 1.0f);
}

void CruiseControl::DecreaseDesiredSpeed() {
	m_desiredSpeedKph = sanitizeSpeed(m_desiredSpeedKph - 1.0f);
}

void CruiseControl::setStatus(CruiseControlStatus status) {
	if (m_status == status) {
		return;
	}

	m_status = status;
	m_shouldResetPid = true;

	if (m_status != CruiseControlStatus::Enabled) {
		m_throttleOffset = 0;
	}
}

CruiseControlStatus CruiseControl::getStatus() const {
	return m_status;
}

void CruiseControl::engageAtCurrentSpeed() {
	auto speed = Sensor::get(SensorType::VehicleSpeed);
	if (!speed.Valid) {
		return;
	}

	m_desiredSpeedKph = sanitizeSpeed(speed.Value);
	setStatus(CruiseControlStatus::Enabled);
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

float getDesiredSpeed() {
	return engine->module<CruiseControl>().unmock().getDesiredSpeed();
}

void setDesiredSpeed(float speedKph) {
	engine->module<CruiseControl>().unmock().setDesiredSpeed(speedKph);
}

void IncreaseDesiredSpeed() {
	engine->module<CruiseControl>().unmock().IncreaseDesiredSpeed();
}

void DecreaseDesiredSpeed() {
	engine->module<CruiseControl>().unmock().DecreaseDesiredSpeed();
}

void setStatus(CruiseControlStatus status) {
	engine->module<CruiseControl>().unmock().setStatus(status);
}

CruiseControlStatus getStatus() {
	return engine->module<CruiseControl>().unmock().getStatus();
}

void engageAtCurrentSpeed() {
	engine->module<CruiseControl>().unmock().engageAtCurrentSpeed();
}

percent_t getCruiseControlThrottleOffset() {
	return engine->module<CruiseControl>().unmock().getThrottleOffset();
}
