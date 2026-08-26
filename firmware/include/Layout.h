#pragma once

#include "DisplayManager.h"

// Gemeinsame Bildschirmaufteilung: zwei permanente Statusleisten (oben,
// unten) auf den beiden langen Bildschirmkanten, dazwischen der
// Inhaltsbereich fuer die jeweils aktive Datenquelle. Siehe lastenheft.txt
// Abschnitt 4: Symbole duerfen zusammen max. 15% der langen Kante (320px)
// einnehmen - 36px je Leiste = 11,25%, haelt die Reserve ein.
namespace Layout {

constexpr int16_t kStatusBarHeight = 36;
constexpr int16_t kContentTop = kStatusBarHeight;
constexpr int16_t kContentBottom = DisplayManager::kScreenHeight - kStatusBarHeight;
constexpr int16_t kContentHeight = kContentBottom - kContentTop;

} // namespace Layout
