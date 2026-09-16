#include "BacklightManager.h"

void BacklightManager::begin(uint8_t initialPercent) {
	ledcSetup(kPwmChannel, kPwmFreqHz, kPwmResolutionBits);
	ledcAttachPin(BACKLIGHT_PIN, kPwmChannel);
	// XL-Brownout-Massnahme (siehe project_sm-display-XL): die 7"-Hintergrund-
	// beleuchtung sanft hochrampen statt in einem harten Sprung auf volle
	// Helligkeit - der schlagartige Stromsprung addiert sich sonst zum Panel-
	// und WLAN-Einschaltstrom und kippt die Versorgung (Brownout). Rampe ueber
	// ~300 ms; auf dem 2,8"-Board unnoetig, aber auch dort unschaedlich.
	if (initialPercent > 100) initialPercent = 100;
	for (uint8_t p = 0; p < initialPercent; p = static_cast<uint8_t>(p + 5)) {
		setPercent(p);
		delay(15);
	}
	setPercent(initialPercent);
}

void BacklightManager::setPercent(uint8_t percent) {
	if (percent > 100) percent = 100;
	currentPercent = percent;
	uint32_t maxDuty = (1u << kPwmResolutionBits) - 1;
	uint32_t duty = (static_cast<uint32_t>(percent) * maxDuty) / 100;
	ledcWrite(kPwmChannel, duty);
}
