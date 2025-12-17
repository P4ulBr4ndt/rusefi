#ifndef EFI_HD_ADVANCED_ACR
#define EFI_HD_ADVANCED_ACR FALSE
#endif

#ifndef EFI_HD_ADVANCED_ACR_DEBUG
#define EFI_HD_ADVANCED_ACR_DEBUG FALSE
#endif

#pragma once

#if EFI_HD_ADVANCED_ACR

#include "engine_module.h"

#ifndef HARLEY_ADVANCED_ACR_R_OPEN
#define HARLEY_ADVANCED_ACR_R_OPEN 540
#endif

#ifndef HARLEY_ADVANCED_ACR_R_CLOSE
#define HARLEY_ADVANCED_ACR_R_CLOSE 715
#endif

#ifndef HARLEY_ADVANCED_ACR_F_OPEN
#define HARLEY_ADVANCED_ACR_F_OPEN 225
#endif

#ifndef HARLEY_ADVANCED_ACR_F_CLOSE
#define HARLEY_ADVANCED_ACR_F_CLOSE 400
#endif

struct HarleyAdvancedAcrActor;

class HarleyAdvancedAcr : public EngineModule {
public:
	void updateAdvancedAcr();
	void onSlowCallback() override;
	bool isEnabled() const;
	bool getFailSafeLevel() const;

private:
	void initializeActors();
	void setOutputs(bool high);
	void armSchedule(int syncCounter);

	enum class AcrMode {
		ForceOn,
		Windowed,
		Off,
	};

	bool m_initialized = false;
	bool m_scheduled = false;
	int m_lastSyncCounter = -1;
	AcrMode m_mode = AcrMode::Off;

	HarleyAdvancedAcrActor* m_rear = nullptr;
	HarleyAdvancedAcrActor* m_front = nullptr;
};

#endif // EFI_HD_ADVANCED_ACR
