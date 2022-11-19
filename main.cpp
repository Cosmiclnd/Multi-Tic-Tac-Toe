/*

Copyright (C) 2022 Cosmicland

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include <SDL2/SDL.h>
#include <SDL2/SDL2_framerate.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <string>
#include <thread>

SDL_Window *g_window;
SDL_Surface *g_screen;
SDL_Renderer *g_renderer;
FPSmanager g_fpsManager;
TTF_Font *g_libian;

enum class Chess {
	NONE = 0,
	COMPUTER = 1,
	PLAYER = 2
};
Chess g_win = Chess::NONE;

// config for subchessboard
struct SubConfig {
	int none, row2, row3;
	double row2times, row3times, corners, sides, center, oppo;
};

namespace Config {
	SubConfig sub, total;
	int difficulty;
}

// config functions to choice
namespace Config {
	void configDefault();
}

typedef unsigned int Color;

const Color colorMap[3] = { 0xffffffff, 0xff0000ff, 0xffff0000 };

template <typename Tp>
Tp smoothen(Tp current, Tp target, int change);
Color smoothenColor(Color current, Color target, int change);
void doComputerTurn();

class SubChessboard {
	Chess __board[9];
	Color __colors[9], __targets[9];  // colors to apply gradual changes
	Color __color;  // color to draw lines
	int __lattices;

public:
	SubChessboard(Color color = 0xff707070);
	SubChessboard(SubChessboard *old);

	void show(int x, int y);
	void showChess(int x, int y, int i, Chess c);
	bool onMousedown(int id, int mx, int my, Chess c);
	Chess getChess(int i, int j);
	bool setChess(int i, int j, Chess c);
	void erase(int i, int j);
	bool full();
	int count(Chess c);
	int has(int len, Chess c);

	friend void doComputerTurn();
};

class Chessboard {
	SubChessboard *__boards[9];
	SubChessboard *__total;
	Chess __turn;
	int __active;
	SDL_Rect __activeRect, __targetRect;
	Color __activeColor, __targetColor;
	int __activeCount;
	int __alpha, __targetAlpha;

public:
	Chessboard();
	Chessboard(Chessboard *old);
	~Chessboard();

	void show();
	void showBanner();
	void onMousedown(int mx, int my);
	bool setChess(int id, int i, int j, Chess c);
	bool hasWinner();
	void setWinner();
	Chess checkRawWin();
	Chess checkSumWin();
	void doWin(Chess c);
	void updateActive();
	SDL_Rect getActive();
	Chess turn();

	friend void doComputerTurn();
	friend int singleComputerTurn(Chessboard &board, Chess c,
		const SubConfig &config, int *pid, int *pi, int *pj, int diff);
	friend int subComputerTurn(Chessboard &copy, SubChessboard *board,
		SubChessboard *total, Chess c, int *pi, int *pj,
		int id, int diff);
} g_chessboard;

SubChessboard::SubChessboard(Color color)
{
	for (int i = 0; i < 9; i++) {
		__board[i] = Chess::NONE;
		__colors[i] = __targets[i] = 0xffdcdcdc;
	}
	__color = color;
	__lattices = 9;
}

SubChessboard::SubChessboard(SubChessboard *old)
{
	for (int i = 0; i < 9; i++) {
		__board[i] = old->__board[i];
	}
	__lattices = old->__lattices;
}

void SubChessboard::show(int x, int y)
{
	lineColor(g_renderer, x + 5, y + 45, x + 125, y + 45, __color);
	lineColor(g_renderer, x + 5, y + 85, x + 125, y + 85, __color);
	lineColor(g_renderer, x + 45, y + 5, x + 45, y + 125, __color);
	lineColor(g_renderer, x + 85, y + 5, x + 85, y + 125, __color);
	for (int i = 0; i < 9; i++) {
		__colors[i] = smoothenColor(__colors[i], __targets[i], 10);
		showChess(x + i % 3 * 40 + 5, y + i / 3 * 40 + 5, i,  __board[i]);
	}
}

void SubChessboard::showChess(int x, int y, int i, Chess c)
{
	if (c == Chess::NONE) return;
	if (c == Chess::PLAYER) {
		circleColor(g_renderer, x + 20, y + 20, 14, __colors[i]);
	}
	else {
		lineColor(g_renderer, x + 6, y + 6, x + 34, y + 34, __colors[i]);
		lineColor(g_renderer, x + 6, y + 34, x + 34, y + 6, __colors[i]);
	}
}

bool SubChessboard::onMousedown(int id, int mx, int my, Chess c)
{
	if (mx < 5 || my < 5 || mx >= 125 || my >= 125) return false;
	int i = (mx - 5) / 40;
	int j = (my - 5) / 40;
	return g_chessboard.setChess(id, i, j, c);
}

Chess SubChessboard::getChess(int i, int j)
{
	if (i < 0 || j < 0 || i >= 3 || j >= 3) return Chess::NONE;
	return __board[i + j * 3];
}

bool SubChessboard::setChess(int i, int j, Chess c)
{
	if (__board[i + j * 3] != Chess::NONE) return false;
	__lattices--;
	__board[i + j * 3] = c;
	__targets[i + j * 3] = colorMap[int(c)];
	return true;
}

void SubChessboard::erase(int i, int j)
{
	if (__board[i + j * 3] != Chess::NONE) __lattices++;
	__board[i + j * 3] = Chess::NONE;
}

bool SubChessboard::full()
{
	return __lattices == 0;
}

int SubChessboard::count(Chess c)
{
	int sum = 0;
	for (int i = 0; i < 9; i++) {
		if (__board[i] == c) sum++;
	}
	return sum;
}

int SubChessboard::has(int len, Chess c)
{
	static int dx[4] = { 1, 1, 0, -1 };
	static int dy[4] = { 0, 1, 1, 1 };
	int sum = 0;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			for (int d = 0; d < 4; d++) {
				bool flag = false;
				for (int k = 0; k < len; k++) {
					if (getChess(i + k * dx[d], j + k * dy[d]) != c) {
						flag = true;
						break;
					}
				}
				if (getChess(i - dx[d], j - dy[d]) == c) continue;
				if (getChess(i + (len + 1) * dx[d], j + (len + 1) * dy[d]) ==
					c) continue;
				if (!flag) sum++;
			}
		}
	}
	return sum;
}

Chessboard::Chessboard()
{
	for (int i = 0; i < 9; i++)
		__boards[i] = new SubChessboard();
	__total = new SubChessboard(0xff000000);
	__turn = Chess::PLAYER;
	__active = -1;
	__activeRect = __targetRect = getActive();
	__activeColor = __targetColor = 0xff00e000;
	__activeCount = 0;
	__alpha = __targetAlpha = 0;
}

Chessboard::Chessboard(Chessboard *old)
{
	for (int i = 0; i < 9; i++) {
		__boards[i] = new SubChessboard(old->__boards[i]);
	}
	__total = new SubChessboard(old->__total);
	__turn = old->__turn;
	__active = old->__active;
}

Chessboard::~Chessboard()
{
	for (int i = 0; i < 9; i++)
		delete __boards[i];
	delete __total;
}

void Chessboard::show()
{
	updateActive();
	for (int i = 0; i < 9; i++)
		__boards[i]->show(i % 3 * 130 + 20, i / 3 * 130 + 170);
	__total->show(150, 20);
	thickLineColor(g_renderer, 15, 300, 415, 300, 2, 0xff000000);
	thickLineColor(g_renderer, 15, 430, 415, 430, 2, 0xff000000);
	thickLineColor(g_renderer, 150, 165, 150, 565, 2, 0xff000000);
	thickLineColor(g_renderer, 280, 165, 280, 565, 2, 0xff000000);
	rectangleColor(g_renderer, __activeRect.x, __activeRect.y,
		__activeRect.x + __activeRect.w, __activeRect.y + __activeRect.h,
		__activeColor);
	showBanner();
}

void Chessboard::showBanner()
{
	if (g_win != Chess::NONE) {
		__alpha = smoothen(__alpha, __targetAlpha, 10);
		Color color = colorMap[int(g_win)];
		static short vx[4] = { 0, 430, 430, 0 }, vy[4] = { 220, 220, 340, 340 };
		filledPolygonRGBA(g_renderer, vx, vy, 4,
			color & 0xff, (color >> 8) & 0xff, (color >> 16) & 0xff,
			__alpha);
		SDL_Color fg = { 255, 255, 255, (unsigned char)(__alpha * 255 / __targetAlpha) };
		char *msg;
		if (g_win == Chess::COMPUTER) msg = (char *) "Computer wins!";
		else msg = (char *) "Player wins!";
		SDL_Surface *surface = TTF_RenderText_Blended(g_libian, msg, fg);
		SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
		SDL_Rect rect = { 0, 0, surface->w, surface->h };
		rect.x = (430 - rect.w) / 2;
		rect.y = (580 - rect.h) / 2;
		SDL_BlitSurface(surface, 0, g_screen, &rect);
	}
}

void Chessboard::onMousedown(int mx, int my)
{
	if (__turn != Chess::PLAYER) return;
	SDL_Point p = { mx, my };
	if (!SDL_PointInRect(&p, &__targetRect)) {
		__targetColor = 0xff00ffff;
		__activeCount = 3;
		return;
	}
	mx -= 20;
	my -= 170;
	int id = mx / 130 + my / 130 * 3;
	if (!__boards[id]->onMousedown(id, mx % 130, my % 130, Chess::PLAYER))
		return;
	__turn = Chess::COMPUTER;
	std::thread thread(doComputerTurn);
	thread.detach();
}

bool Chessboard::setChess(int id, int i, int j, Chess c)
{
	if (!__boards[id]->setChess(i, j, c)) return false;
	int s = __boards[id]->has(3, c);
	if (s > 0) {
		__total->setChess(id % 3, id / 3, c);
		if (hasWinner()) {
			setWinner();
		}
	}
	__active = __boards[i + j * 3]->full() ? -1 : i + j * 3;
	__targetRect = getActive();
	return true;
}

bool Chessboard::hasWinner()
{
	if (checkRawWin() != Chess::NONE) return true;
	if (__total->full()) return true;
	for (int i = 0; i < 9; i++) {
		if (!__boards[i]->full()) {
			return false;
		}
	}
	return true;
}

void Chessboard::setWinner()
{
	Chess winner = checkRawWin();
	if (winner == Chess::NONE) {
		for (int i = 0; i < 9; i++) {
			int s1 = __boards[i]->count(Chess::COMPUTER);
			int s2 = __boards[i]->count(Chess::PLAYER);
			if (s1 > s2) __total->setChess(i % 3, i / 3, Chess::COMPUTER);
			else __total->setChess(i % 3, i / 3, Chess::PLAYER);
		}
		winner = checkSumWin();
	}
	doWin(winner);
}

Chess Chessboard::checkRawWin()
{
	int s = __total->has(3, Chess::COMPUTER);
	if (s > 0) return Chess::COMPUTER;
	s = __total->has(3, Chess::PLAYER);
	if (s > 0) return Chess::PLAYER;
	return Chess::NONE;
}

Chess Chessboard::checkSumWin()
{
	int s1 = __total->count(Chess::COMPUTER);
	int s2 = __total->count(Chess::PLAYER);
	if (s1 > s2) return Chess::COMPUTER;
	else return Chess::PLAYER;
}

void Chessboard::doWin(Chess c)
{
	g_win = c;
	__targetAlpha = 120;
}

void Chessboard::updateActive()
{
	__activeRect.x = smoothen(__activeRect.x, __targetRect.x, 10);
	__activeRect.y = smoothen(__activeRect.y, __targetRect.y, 10);
	__activeRect.w = smoothen(__activeRect.w, __targetRect.w, 10);
	__activeRect.h = smoothen(__activeRect.h, __targetRect.h, 10);
	__activeColor = smoothenColor(__activeColor, __targetColor, 25);
	if (__activeColor == 0xff00ffff) {
		__targetColor = 0xff00e000;
	}
	else if (__activeColor == 0xff00e000) {
		if (__activeCount > 0) {
			__activeCount--;
			__targetColor = 0xff00ffff;
		}
	}
}

SDL_Rect Chessboard::getActive()
{
	SDL_Rect rect;
	if (__active == -1) rect = { 20, 170, 390, 390 };
	else {
		rect = {
			__active % 3 * 130 + 20,
			__active / 3 * 130 + 170,
			130, 130
		};
	}
	return rect;
}

Chess Chessboard::turn()
{
	return __turn;
}

template <typename Tp>
Tp smoothen(Tp current_, Tp target_, int change)
{
	// cast to `long long`: avoid underflowing
	long long current = current_, target = target_;
	if (current < target) {
		current = std::min(current + change, target);
	}
	else if (current > target) {
		current = std::max(current - change, target);
	}
	return Tp(current);
}

Color smoothenColor(Color current, Color target, int change)
{
	Color r = 0xff000000;
	// apply gradual changes to every tricolor by using bit operations
	r |= (smoothen(
		current & 0xff, target & 0xff, change) & 0xff);
	r |= (smoothen(
		(current >> 8) & 0xff, (target >> 8) & 0xff, change) & 0xff) << 8;
	r |= (smoothen(
		(current >> 16) & 0xff, (target >> 16) & 0xff, change) & 0xff) << 16;
	return r;
}

void error(std::string msg)
{
	std::cerr << "Multi-tic-tac-toe: " << msg;
	abort();
}

void init()
{
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) error("failed to initalize SDL2");
	TTF_Init();
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
	g_window = SDL_CreateWindow("Multi-tic-tac-toe", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, 430, 580, SDL_WINDOW_SHOWN);
	if (!g_window) error("failed to create window");
	g_screen = SDL_GetWindowSurface(g_window);
	g_renderer = SDL_CreateSoftwareRenderer(g_screen);
	SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
	SDL_initFramerate(&g_fpsManager);
	SDL_setFramerate(&g_fpsManager, 80);
	g_libian = TTF_OpenFont("libian.ttc", 60);
}

void quit()
{
	SDL_DestroyRenderer(g_renderer);
	SDL_FreeSurface(g_screen);
	SDL_DestroyWindow(g_window);
	TTF_CloseFont(g_libian);
	TTF_Quit();
	SDL_Quit();
	exit(0);
}

int getPos(int i, int j)
{
	static int map[9] = {
		2, 1, 2,
		1, 0, 1,
		2, 1, 2
	};
	return map[i + j * 3];
}

double getTimes(int i, int j, const SubConfig &config)
{
	int p = getPos(i, j);
	if (p == 2) return config.corners;
	if (p == 1) return config.sides;
	if (p == 0) return config.center;
	return 0.0;
}

int calcTimes(int x, int y, double times)
{
	double t = 1.0;
	int sum = 0;
	for (int i = 0; i < y; i++) {
		sum += x * t;
		t *= times;
	}
	return sum;
}

int subComputerTurn(Chessboard &copy, SubChessboard *board,
	SubChessboard *total, Chess c, int *pi, int *pj,
	int id, int diff)
{
	const SubConfig &config = Config::sub, &t = Config::total;
	int has2 = board->has(2, c);
	int has3 = board->has(3, c);
	int thas2 = total->has(2, c);
	int thas3 = total->has(3, c);
	int max = INT_MIN, ri = 0, rj = 0;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			int active = copy.__active;
			if (!board->setChess(i, j, c)) continue;
			copy.__active = copy.__boards[i + j * 3]->full() ?
				-1 : i + j * 3;
			int score = config.none;
			int s = board->has(3, c);
			int new2 = 0, new3 = 0;
			if (total->getChess(id % 3, id / 3) == Chess::NONE) {
				new2 = std::max(board->has(2, c) - has2, 0);
				new3 = std::max(s - has3, 0);
			}
			bool flag = total->getChess(id % 3, id / 3) != Chess::NONE;
			if (s > 0) {
				if (total->setChess(id % 3, id / 3, c)) {
					int tnew2 = std::max(total->has(2, c) - thas2, 0);
					int tnew3 = std::max(total->has(3, c) - thas3, 0);
					int tscore = t.none;
					tscore += calcTimes(t.row2, tnew2, t.row2times);
					tscore += calcTimes(t.row3, tnew3, t.row3times);
					tscore *= getTimes(id % 3, id / 3, t);
					score += tscore;
					total->erase(id % 3, id / 3);
				}
			}
			score += calcTimes(config.row2, new2, config.row2times);
			score += calcTimes(config.row3, new3, config.row3times);
			if (flag) {
				score *= 0.2;
			}
			int x;
			score -= singleComputerTurn(copy,
				c == Chess::COMPUTER ? Chess::PLAYER : Chess::COMPUTER,
				config, &x, &x, &x, diff - 1) * config.oppo;
			score *= getTimes(i, j, config);
			if (score > max || (score == max && rand() % 3 < 2)) {
				max = score;
				ri = i, rj = j;
			}
			board->erase(i, j);
			copy.__active = active;
		}
	}
	*pi = ri;
	*pj = rj;
	return max;
}

int singleComputerTurn(Chessboard &board, Chess c, const SubConfig &config,
	int *pid, int *pi, int *pj, int diff)
{
	if (diff <= 0) return 0;
	int max = INT_MIN, id = 0, ri = 0, rj = 0, rk = 0;
	for (int k = 0; k < 9; k++) {
		if (board.__active != -1 && k != board.__active) continue;
		int i, j;
		int score = subComputerTurn(
			board, board.__boards[k], board.__total, c, &i, &j, k,
			diff);
		if (score > max || (score == max && rand() % 3 < 2)) {
			max = score;
			ri = i, rj = j, rk = k;
		}
	}
	*pid = rk;
	*pi = ri;
	*pj = rj;
	return max;
}

void doComputerTurn()
{
	std::chrono::high_resolution_clock::time_point tp =
		std::chrono::high_resolution_clock::now() +
		std::chrono::milliseconds(750);
	Chessboard copy = &g_chessboard;
	int id, i, j;
	singleComputerTurn(copy, Chess::COMPUTER, Config::sub, &id, &i, &j,
		Config::difficulty);
	std::this_thread::sleep_until(tp);
	g_chessboard.setChess(id, i, j, Chess::COMPUTER);
	g_chessboard.__turn = Chess::PLAYER;
}

void update()
{
	SDL_SetRenderDrawColor(g_renderer, 220, 220, 220, 255);
	SDL_RenderFillRect(g_renderer, 0);
	g_chessboard.show();
}

void onMousedown(const SDL_Event &event)
{
	// only handle left button pressing
	if (event.button.button != SDL_BUTTON_LEFT) return;
	int mx = event.button.x, my = event.button.y;
	if (g_win == Chess::NONE && g_chessboard.turn() == Chess::PLAYER)
		g_chessboard.onMousedown(mx, my);
}

int main()
{
	init();
	Config::configDefault();
	SDL_Event event;
	while (1) {
		SDL_PollEvent(&event);
		if (event.type == SDL_QUIT) {
			break;
		}
		else if (event.type == SDL_MOUSEBUTTONDOWN) {
			onMousedown(event);
		}
		update();
		SDL_UpdateWindowSurface(g_window);
		SDL_RenderPresent(g_renderer);
		SDL_framerateDelay(&g_fpsManager);
	}
	quit();
	return 0;
}

void Config::configDefault()
{
	sub.none = 1;
	sub.row2 = 3;
	sub.row2times = 0.8;
	sub.row3 = 12;
	sub.row3times = 0.2;
	sub.corners = 1.0;
	sub.sides = 0.95;
	sub.center = 0.9;
	sub.oppo = 0.9;
	total.none = 15;
	total.row2 = 45;
	total.row2times = 0.8;
	total.row3 = 2048;
	total.row3times = 0.2;
	total.corners = 1.0;
	total.sides = 0.95;
	total.center = 0.9;
	difficulty = 4;
}
