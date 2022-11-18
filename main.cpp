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
#include <iostream>
#include <string>
#include <thread>

SDL_Window *g_window;
SDL_Surface *g_screen;
SDL_Renderer *g_renderer;
FPSmanager g_fpsManager;

enum class Chess {
	NONE = 0,
	COMPUTER = 1,
	PLAYER = 2
};
Chess g_win = Chess::NONE;

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

	void show(int x, int y);
	void showChess(int x, int y, int i, Chess c);
	bool onMousedown(int id, int mx, int my, Chess c);
	Chess getChess(int i, int j);
	bool setChess(int i, int j, Chess c);
	bool full();
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
	~Chessboard();

	void show();
	void showBanner();
	void onMousedown(int mx, int my);
	bool setChess(int id, int i, int j, Chess c);
	void updateActive();
	SDL_Rect getActive();

	friend void doComputerTurn();
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

bool SubChessboard::full()
{
	return __lattices == 0;
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
		s = __total->has(3, c);
		if (s > 0) {
			g_win = c;
			__targetAlpha = 120;
		}
	}
	__active = __boards[i + j * 3]->full() ? -1 : i + j * 3;
	__targetRect = getActive();
	return true;
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
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
	g_window = SDL_CreateWindow("Multi-tic-tac-toe", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, 430, 580, SDL_WINDOW_SHOWN);
	if (!g_window) error("failed to create window");
	g_screen = SDL_GetWindowSurface(g_window);
	g_renderer = SDL_CreateSoftwareRenderer(g_screen);
	SDL_initFramerate(&g_fpsManager);
	SDL_setFramerate(&g_fpsManager, 40);
}

void quit()
{
	SDL_DestroyRenderer(g_renderer);
	SDL_FreeSurface(g_screen);
	SDL_DestroyWindow(g_window);
	SDL_Quit();
	exit(0);
}

void doComputerTurn()
{
	for (int i = 0; i < 9; i++) {
		if (g_chessboard.__active != -1 && i != g_chessboard.__active)
			continue;
		bool flag = false;
		for (int j = 0; j < 9; j++) {
			if (g_chessboard.setChess(i, j % 3, j / 3, Chess::COMPUTER)) {
				flag = true;
				break;
			}
		}
		if (flag) break;
	}
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
	if (g_win == Chess::NONE)
		g_chessboard.onMousedown(mx, my);
}

int main()
{
	init();
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
