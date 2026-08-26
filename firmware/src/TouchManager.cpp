#include "TouchManager.h"
#include "DisplayManager.h"

void TouchManager::begin(DisplayManager &display) {
	_display = &display;
}

bool TouchManager::read(int16_t &screenX, int16_t &screenY) {
	if (!_display) return false;
	int32_t x = 0, y = 0;
	// getTouch() liefert die Anzahl erkannter Punkte (>0 = beruehrt).
	if (_display->raw().getTouch(&x, &y)) {
		screenX = static_cast<int16_t>(x);
		screenY = static_cast<int16_t>(y);
		return true;
	}
	return false;
}
