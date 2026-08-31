// Advanced per-cylinder Harley ACR scheduling
// Initially off, once cranking both actuators stay on until sync is achieved, then follow angle windows.
// Once the engine is running, both actuators are held off.
// Angles should be tuned to be open at compression phase and closed at about TDC of each cylinder

#include "pch.h"

#if EFI_HD_ADVANCED_ACR

#include "harley_advanced_acr.h"
#include "trigger_central.h"
#include "trigger_scheduler.h"
#include "rusefi_enums.h"

#if EFI_HD_ADVANCED_ACR_DEBUG
#define ACR_DEBUG(fmt, ...) efiPrintf("ADV_ACR: " fmt, ##__VA_ARGS__)
#else
#define ACR_DEBUG(fmt, ...) do { } while (0)
#endif

struct HarleyAdvancedAcrActor {
	HarleyAdvancedAcr* owner = nullptr;
	RegisteredOutputPin* output = nullptr;
	AngleBasedEvent openEvent;
	AngleBasedEvent closeEvent;
	angle_t openAngle = 0;
	angle_t closeAngle = 0;
};

namespace {

angle_t normalizeAngle(angle_t angle) {
	while (angle < 0) {
		angle += 720;
	}
	while (angle >= 720) {
		angle -= 720;
	}
	return angle;
}

static void scheduleOpen(HarleyAdvancedAcrActor& actor);

static void scheduleClose(HarleyAdvancedAcrActor& actor);

static void harleyAdvAcrClose(HarleyAdvancedAcrActor* actor) {
	if (!actor) {
		return;
	}

	bool fallbackHigh = actor->owner && actor->owner->getFailSafeLevel();

	if (!actor->owner || !actor->owner->isEnabled()) {
		actor->output->setValue(fallbackHigh);
		return;
	}

	actor->output->setValue(false);
}

static void harleyAdvAcrOpen(HarleyAdvancedAcrActor* actor) {
	if (!actor) {
		return;
	}

	bool fallbackHigh = actor->owner && actor->owner->getFailSafeLevel();

	if (!actor->owner || !actor->owner->isEnabled()) {
		actor->output->setValue(fallbackHigh);
		return;
	}

	actor->output->setValue(true);

	scheduleClose(*actor);
	scheduleOpen(*actor); // plan next cycle
}

static void scheduleClose(HarleyAdvancedAcrActor& actor) {
	engine->module<TriggerScheduler>()->schedule(
			"harley-adv-acr",
			&actor.closeEvent,
			actor.closeAngle,
			action_s::make<harleyAdvAcrClose>(&actor));
}

static void scheduleOpen(HarleyAdvancedAcrActor& actor) {
	engine->module<TriggerScheduler>()->schedule(
			"harley-adv-acr",
			&actor.openEvent,
			actor.openAngle,
			action_s::make<harleyAdvAcrOpen>(&actor));
}

} // namespace

bool HarleyAdvancedAcr::isEnabled() const {
	return m_mode == AcrMode::Windowed;
}

bool HarleyAdvancedAcr::getFailSafeLevel() const {
	return m_mode == AcrMode::ForceOn;
}

void HarleyAdvancedAcr::setOutputs(bool high) {
	if (m_rear) {
		m_rear->output->setValue(high);
	}
	if (m_front) {
		m_front->output->setValue(high);
	}
	engine->engineState.acrActive = high;
}

void HarleyAdvancedAcr::initializeActors() {
	if (m_initialized) {
		return;
	}

	static HarleyAdvancedAcrActor rear;
	static HarleyAdvancedAcrActor front;

	rear.owner = this;
	rear.output = &enginePins.harleyAcr;
	rear.openAngle = normalizeAngle(HARLEY_ADVANCED_ACR_R_OPEN);
	rear.closeAngle = normalizeAngle(HARLEY_ADVANCED_ACR_R_CLOSE);

	front.owner = this;
	front.output = &enginePins.harleyAcr2;
	front.openAngle = normalizeAngle(HARLEY_ADVANCED_ACR_F_OPEN);
	front.closeAngle = normalizeAngle(HARLEY_ADVANCED_ACR_F_CLOSE);

	m_rear = &rear;
	m_front = &front;
	m_initialized = true;
	ACR_DEBUG("init rear[%d-%d] front[%d-%d]", (int)rear.openAngle, (int)rear.closeAngle, (int)front.openAngle, (int)front.closeAngle);
}

void HarleyAdvancedAcr::armSchedule(int syncCounter) {
	if (!m_rear || !m_front) {
		return;
	}

	m_lastSyncCounter = syncCounter;
	m_scheduled = true;

	scheduleOpen(*m_rear);
	scheduleOpen(*m_front);
	ACR_DEBUG("arm schedule syncCnt=%d", syncCounter);
}

void HarleyAdvancedAcr::updateAdvancedAcr() {
	initializeActors();

	if (!isBrainPinValid(engineConfiguration->acrPin) || !isBrainPinValid(engineConfiguration->acrPin2)) {
		m_mode = AcrMode::Off;
		setOutputs(false);
		ACR_DEBUG("pins invalid p1=%d p2=%d", (int)engineConfiguration->acrPin, (int)engineConfiguration->acrPin2);
		return;
	}

	bool running = engine->rpmCalculator.isRunning();
	bool cranking = engine->rpmCalculator.isCranking();
	bool spinningUp = engine->rpmCalculator.isSpinningUp();
	bool synced = getTriggerCentral()->triggerState.getShaftSynchronized();
	uint8_t syncCounter = engineConfiguration->vvtMode[0] == VVT_BOSCH_QUICK_START ? getTriggerCentral()->vvtCamCounter : getTriggerCentral()->mapVvt_sync_counter;
	AcrMode desiredMode = AcrMode::Off;

	if (running) {
		desiredMode = AcrMode::Off;
	} else if (!synced && (cranking || spinningUp)) {
		desiredMode = AcrMode::ForceOn;
	} else if (synced && syncCounter < 2 && (cranking || spinningUp)) {
		// keep valves open until we've seen enough instant MAP sync confirmations
		desiredMode = AcrMode::ForceOn;
	} else if (synced) {
		desiredMode = AcrMode::Windowed;
	} else {
		desiredMode = AcrMode::Off;
	}

	AcrMode prevMode = m_mode;
	if (desiredMode != m_mode) {
		if (desiredMode != AcrMode::Windowed) {
			m_scheduled = false;
		}
		ACR_DEBUG("mode %d->%d run=%d crank=%d spin=%d sync=%d mapCnt=%d syncCnt=%d", (int)m_mode, (int)desiredMode, running, cranking, spinningUp, synced, syncCounter, getTriggerCentral()->triggerState.getSynchronizationCounter());
		m_mode = desiredMode;
	}
	bool enteringWindowed = (m_mode == AcrMode::Windowed && prevMode != AcrMode::Windowed);

	switch (m_mode) {
	case AcrMode::ForceOn:
		setOutputs(true);
		break;
	case AcrMode::Off:
		setOutputs(false);
		break;
	case AcrMode::Windowed:
		if (enteringWindowed) {
			// keep valves open until the first scheduled close fires
			setOutputs(true);
		}
		if (!m_scheduled || m_lastSyncCounter != getTriggerCentral()->triggerState.getSynchronizationCounter()) {
			armSchedule(getTriggerCentral()->triggerState.getSynchronizationCounter());
		}
		break;
	}
}

void HarleyAdvancedAcr::onSlowCallback() {
	updateAdvancedAcr();
}

#endif // EFI_HD_ADVANCED_ACR
