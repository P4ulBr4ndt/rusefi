// Advanced per-cylinder Harley ACR scheduling
// Both actuators stay on until sync is achieved, then follow angle windows.
// Once the engine is running, both actuators are held off.

#include "pch.h"

#if EFI_HD_ADVANCED_ACR

#include "harley_advanced_acr.h"
#include "trigger_central.h"
#include "trigger_scheduler.h"

struct HarleyAdvancedAcrActor {
	HarleyAdvancedAcr* owner = nullptr;
	NamedOutputPin* output = nullptr;
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

static void harleyAdvAcrClose(HarleyAdvancedAcrActor* actor) {
	if (!actor) {
		return;
	}

	bool fallbackHigh = actor->owner && actor->owner->getFailSafeLevel();

	if (!actor->owner || !actor->owner->isEnabled()) {
		actor->output->setValue(fallbackHigh);
		return;
	}

	actor->output->setLow();
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

	actor->output->setHigh();

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
}

void HarleyAdvancedAcr::armSchedule(int syncCounter) {
	if (!m_rear || !m_front) {
		return;
	}

	m_lastSyncCounter = syncCounter;
	m_scheduled = true;

	scheduleOpen(*m_rear);
	scheduleOpen(*m_front);
}

void HarleyAdvancedAcr::updateAdvancedAcr() {
	initializeActors();

	if (!isBrainPinValid(engineConfiguration->acrPin) || !isBrainPinValid(engineConfiguration->acrPin2)) {
		m_mode = AcrMode::Off;
		setOutputs(false);
		return;
	}

	bool running = engine->rpmCalculator.isRunning();
	bool cranking = engine->rpmCalculator.isCranking();
	bool synced = getTriggerCentral()->triggerState.getShaftSynchronized();
	AcrMode desiredMode = AcrMode::Off;

	if (running) {
		desiredMode = AcrMode::Off;
	} else if (!synced && cranking) {
		desiredMode = AcrMode::ForceOn;
	} else if (synced) {
		desiredMode = AcrMode::Windowed;
	} else {
		desiredMode = AcrMode::Off;
	}

	if (desiredMode != m_mode) {
		if (desiredMode != AcrMode::Windowed) {
			m_scheduled = false;
		}
		m_mode = desiredMode;
	}

	switch (m_mode) {
	case AcrMode::ForceOn:
		setOutputs(true);
		break;
	case AcrMode::Off:
		setOutputs(false);
		break;
	case AcrMode::Windowed:
		setOutputs(false); // default low, windows will raise as needed
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
