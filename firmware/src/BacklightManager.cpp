#include "BacklightManager.h"

void BacklightManager::begin(uint8_t initialPercent) {
	ledcSetup(kPwmChannel, kPwmFreqHz, kPwmResolutionBits);
	ledcAttachPin(BACKLIGHT_PIN, kPwmChannel);
	setPercent(initialPercent);
}

void BacklightManager::setPercent(uint8_t percent) {
	if (percent > 100) percent = 100;
	currentPercent = percent;
	uint32_t maxDuty = (1u << kPwmResolutionBits) - 1;
	uint32_t duty = (static_cast<uint32_t>(percent) * maxDuty) / 100;
	ledcWrite(kPwmChannel, duty);
}
