#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "DisplayManager.h"
#include "TouchManager.h"

// Snake-Modus (lastenheft.txt Abschnitt 6.3): Touch-Drittel-Steuerung
// (oben=hoch, unten=runter, links=links, rechts=rechts), Game-Over zeigt
// 20s den Punktestand, dann zurueck zum Aufrufer (Einstellungen-Menu).
// Blockierend fuer die Dauer des Spiels, aber intern nicht-blockierend
// (Touch-Abfrage + fester Zug-Takt nebeneinander, siehe docs/entscheidungen.md).
class SnakeGame {
public:
	static constexpr uint16_t kCellSize = 16;
	static constexpr uint16_t kCols = DisplayManager::kScreenWidth / kCellSize;    // 20
	static constexpr uint16_t kPlayfieldTop = 24;                                  // Platz fuer Punkteanzeige
	static constexpr uint16_t kRows =
	    (DisplayManager::kScreenHeight - kPlayfieldTop) / kCellSize;               // 13
	static constexpr uint16_t kMaxLength = kCols * kRows;
	static constexpr uint32_t kTickIntervalMs = 200;
	static constexpr uint32_t kGameOverDisplayMs = 20000;

	void run(DisplayManager &display, TouchManager &touch);

private:
	enum class Direction { Up, Down, Left, Right };
	struct Segment {
		uint8_t x, y;
	};

	void reset();
	void spawnFood();
	void advance();
	bool isOpposite(Direction a, Direction b) const;
	Direction zoneForTouch(int16_t x, int16_t y) const;

	void draw(DisplayManager &display) const;
	void drawGameOver(DisplayManager &display) const;

	uint16_t loadHighscore() const;
	void saveHighscoreIfNeeded();

	Segment body[kMaxLength];
	uint16_t length = 0;
	Direction direction = Direction::Right;
	Direction pendingDirection = Direction::Right;
	Segment food{0, 0};
	uint16_t score = 0;
	bool gameOver = false;
};
