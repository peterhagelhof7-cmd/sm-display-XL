#include "SnakeGame.h"

void SnakeGame::reset() {
	length = 3;
	uint8_t startX = kCols / 2;
	uint8_t startY = kRows / 2;
	for (uint16_t i = 0; i < length; i++) {
		body[i] = {static_cast<uint8_t>(startX - i), startY};
	}
	direction = Direction::Right;
	pendingDirection = Direction::Right;
	score = 0;
	gameOver = false;
	spawnFood();
}

void SnakeGame::spawnFood() {
	// Einfacher Ansatz: zufaellige Zelle wuerfeln, bei Kollision mit dem
	// Koerper neu wuerfeln. Bei einer fast bildschirmfuellenden Schlange
	// (nahe kMaxLength) koennte das theoretisch oft neu wuerfeln, ist aber
	// bei einem 20x13-Feld in der Praxis vernachlaessigbar selten relevant.
	bool onBody;
	do {
		food.x = static_cast<uint8_t>(random(kCols));
		food.y = static_cast<uint8_t>(random(kRows));
		onBody = false;
		for (uint16_t i = 0; i < length; i++) {
			if (body[i].x == food.x && body[i].y == food.y) {
				onBody = true;
				break;
			}
		}
	} while (onBody);
}

bool SnakeGame::isOpposite(Direction a, Direction b) const {
	return (a == Direction::Up && b == Direction::Down) ||
	       (a == Direction::Down && b == Direction::Up) ||
	       (a == Direction::Left && b == Direction::Right) ||
	       (a == Direction::Right && b == Direction::Left);
}

SnakeGame::Direction SnakeGame::zoneForTouch(int16_t x, int16_t y) const {
	// Prioritaet vertikal vor horizontal, da sich "oberes/unteres Drittel"
	// und "linkes/rechtes Drittel" in den Ecken ueberschneiden -
	// lastenheft.txt legt die Aufloesung nicht fest, siehe entscheidungen.md.
	if (y < DisplayManager::kScreenHeight / 3) return Direction::Up;
	if (y > (DisplayManager::kScreenHeight * 2) / 3) return Direction::Down;
	if (x < DisplayManager::kScreenWidth / 3) return Direction::Left;
	if (x > (DisplayManager::kScreenWidth * 2) / 3) return Direction::Right;
	return direction; // Mittelzone: keine Aenderung
}

void SnakeGame::advance() {
	direction = pendingDirection;

	int16_t dx = 0, dy = 0;
	switch (direction) {
		case Direction::Up: dy = -1; break;
		case Direction::Down: dy = 1; break;
		case Direction::Left: dx = -1; break;
		case Direction::Right: dx = 1; break;
	}

	int16_t newX = static_cast<int16_t>(body[0].x) + dx;
	int16_t newY = static_cast<int16_t>(body[0].y) + dy;

	if (newX < 0 || newX >= static_cast<int16_t>(kCols) || newY < 0 ||
	    newY >= static_cast<int16_t>(kRows)) {
		gameOver = true;
		return;
	}

	bool eatsFood = (newX == food.x && newY == food.y);

	// Selbstkollision: Schwanzende zaehlt nicht, wenn es sich in diesem Zug
	// wegbewegt (kein Wachstum) - sonst waere ein Zug "auf die eigene
	// bisherige Schwanzspitze" faelschlich ein Verlust.
	uint16_t checkLength = eatsFood ? length : length - 1;
	for (uint16_t i = 0; i < checkLength; i++) {
		if (body[i].x == newX && body[i].y == newY) {
			gameOver = true;
			return;
		}
	}

	if (eatsFood && length < kMaxLength) {
		for (uint16_t i = length; i > 0; i--) {
			body[i] = body[i - 1];
		}
		length++;
		score++;
		spawnFood();
	} else {
		for (uint16_t i = length - 1; i > 0; i--) {
			body[i] = body[i - 1];
		}
	}
	body[0] = {static_cast<uint8_t>(newX), static_cast<uint8_t>(newY)};
}

void SnakeGame::draw(DisplayManager &display) const {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);

	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(2);
	tft.drawString("Punkte: " + String(score), 6, 4);

	for (uint16_t i = 0; i < length; i++) {
		int16_t px = body[i].x * kCellSize;
		int16_t py = kPlayfieldTop + body[i].y * kCellSize;
		tft.fillRect(px + 1, py + 1, kCellSize - 2, kCellSize - 2, TFT_BLACK);
	}

	int16_t fx = food.x * kCellSize;
	int16_t fy = kPlayfieldTop + food.y * kCellSize;
	tft.fillRect(fx + 2, fy + 2, kCellSize - 4, kCellSize - 4, TFT_RED);
}

void SnakeGame::drawGameOver(DisplayManager &display) const {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(MC_DATUM);
	tft.setTextFont(2);
	tft.drawString("Spiel vorbei", DisplayManager::kScreenWidth / 2, 60);

	tft.setTextFont(7);
	tft.drawString(String(score), DisplayManager::kScreenWidth / 2, 120);

	tft.setTextFont(2);
	uint16_t best = loadHighscore();
	tft.drawString("Bestwert: " + String(best), DisplayManager::kScreenWidth / 2, 170);
	tft.setTextDatum(TL_DATUM);
}

uint16_t SnakeGame::loadHighscore() const {
	Preferences prefs;
	prefs.begin("snake", true);
	uint16_t best = prefs.getUShort("highscore", 0);
	prefs.end();
	return best;
}

void SnakeGame::saveHighscoreIfNeeded() {
	Preferences prefs;
	prefs.begin("snake", false);
	uint16_t best = prefs.getUShort("highscore", 0);
	if (score > best) {
		prefs.putUShort("highscore", score);
	}
	prefs.end();
}

void SnakeGame::run(DisplayManager &display, TouchManager &touch) {
	reset();
	draw(display);

	uint32_t lastTick = millis();
	while (!gameOver) {
		int16_t x, y;
		if (touch.read(x, y)) {
			Direction wanted = zoneForTouch(x, y);
			if (!isOpposite(wanted, direction)) {
				pendingDirection = wanted;
			}
		}

		uint32_t now = millis();
		if (now - lastTick >= kTickIntervalMs) {
			lastTick = now;
			advance();
			if (!gameOver) {
				draw(display);
			}
		}
		delay(15);
	}

	saveHighscoreIfNeeded();
	drawGameOver(display);
	delay(kGameOverDisplayMs);
}
