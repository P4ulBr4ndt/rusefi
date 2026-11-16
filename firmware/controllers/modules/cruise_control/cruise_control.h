#pragma once

#include "engine_module.h"
#include "closed_loop_controller.h"
#include "rusefi_types.h"
#include "efi_pid.h"

enum class CruiseControlStatus : uint8_t {
	Disabled = 0,
	Standby = 1,
	Enabled = 2,
};

class CruiseControl : public EngineModule, public ClosedLoopController<float, percent_t> {
public:
	void initNoConfiguration() override;
	void setDefaultConfiguration() override;
	void onFastCallback() override;
	void onEngineStop() override;
	void onIgnitionStateChanged(bool ignitionOn) override;
	void onConfigurationChange(engine_configuration_s const * previousConfig) override;

	float getDesiredCCSpeed() const;
	void setDesiredCCSpeed(float speedKph);
	void increaseDesiredCCSpeed();
	void decreaseDesiredCCSpeed();
	void setCCStatus(CruiseControlStatus status);
	CruiseControlStatus getCCStatus() const;
	void engageCCAtCurrentSpeed();
	void resumeCC();

	// Absolute throttle position requested by cruise, 0 when inactive
	percent_t getThrottlePosition() const {
		return m_throttlePosition;
	}

	// Set by the ETB when the grip opens further than cruise
	void setPedalOverride(bool isOverriding) {
		m_pedalOverride = isOverriding;
	}

private:
	expected<float> getSetpoint() override;
	expected<float> observePlant() override;
	expected<percent_t> getOpenLoop(float target) override;
	expected<percent_t> getClosedLoop(float target, float observation) override;
	void setOutput(expected<percent_t> outputValue) override;

	void resetPid();
	float sanitizeSpeed(float speedKph) const;

	Pid m_pid;
	CruiseControlStatus m_status = CruiseControlStatus::Disabled;
	float m_desiredSpeedKph = 0;
	float m_setpointKph = 0;
	percent_t m_basePosition = 0;
	percent_t m_throttleOffset = 0;
	percent_t m_throttlePosition = 0;
	bool m_shouldResetPid = true;
	bool m_pedalOverride = false;
};

float getDesiredCCSpeed();
void setDesiredCCSpeed(float speedKph);
void increaseDesiredCCSpeed();
void decreaseDesiredCCSpeed();
void setCCStatus(CruiseControlStatus status);
CruiseControlStatus getCCStatus();
void engageCCAtCurrentSpeed();
void resumeCC();
percent_t getCruiseControlThrottlePosition();
void setCruiseControlPedalOverride(bool isOverriding);

// Board hook: setpoint ramp rate in km/h per second
float boardCruiseSetpointRampRate();
