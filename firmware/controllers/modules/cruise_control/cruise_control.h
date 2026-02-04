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

	float getDesiredSpeed() const;
	void setDesiredSpeed(float speedKph);
	void IncreaseDesiredSpeed();
	void DecreaseDesiredSpeed();
	void setStatus(CruiseControlStatus status);
	CruiseControlStatus getStatus() const;
	void engageAtCurrentSpeed();

	percent_t getThrottleOffset() const {
		return m_throttleOffset;
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
	percent_t m_throttleOffset = 0;
	bool m_shouldResetPid = true;
};

float getDesiredSpeed();
void setDesiredSpeed(float speedKph);
void IncreaseDesiredSpeed();
void DecreaseDesiredSpeed();
void setStatus(CruiseControlStatus status);
CruiseControlStatus getStatus();
void engageAtCurrentSpeed();
percent_t getCruiseControlThrottleOffset();
