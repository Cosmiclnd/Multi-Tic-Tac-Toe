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

SDL_Window *g_window;
SDL_Surface *g_screen;
SDL_Renderer *g_renderer;
FPSmanager g_fpsManager;

enum class Chess {
	NONE,
	COMPUTER,
	PLAYER
};

class SubChessboard {
	Chess __board[9];

public:
	SubChessboard();

	void show(int x, int y);
};

class Chessboard {
	SubChessboard *__boards[9];

public:
	Chessboard();
	~Chessboard();

	void show();
} g_chessboard;

SubChessboard::SubChessboard()
{
	for (int i = 0; i < 9; i++)
		__board[i] = Chess::NONE;
}

void SubChessboard::show(int x, int y)
{
	lineColor(g_renderer, x + 5, y + 45, x + 125, y + 45, 0xff707070);
	lineColor(g_renderer, x + 5, y + 85, x + 125, y + 85, 0xff707070);
	lineColor(g_renderer, x + 45, y + 5, x + 45, y + 125, 0xff707070);
	lineColor(g_renderer, x + 85, y + 5, x + 85, y + 125, 0xff707070);
}

Chessboard::Chessboard()
{
	for (int i = 0; i < 9; i++)
		__boards[i] = new SubChessboard();
}

Chessboard::~Chessboard()
{
	for (int i = 0; i < 9; i++)
		delete __boards[i];
}

void Chessboard::show()
{
	for (int i = 0; i < 9; i++)
		__boards[i]->show((i % 3) * 130 + 20, (i / 3) * 130 + 170);
	thickLineColor(g_renderer, 15, 300, 415, 300, 2, 0xff000000);
	thickLineColor(g_renderer, 15, 430, 415, 430, 2, 0xff000000);
	thickLineColor(g_renderer, 150, 165, 150, 565, 2, 0xff000000);
	thickLineColor(g_renderer, 280, 165, 280, 565, 2, 0xff000000);
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
	g_renderer = SDL_GetRenderer(g_window);
	SDL_initFramerate(&g_fpsManager);
	SDL_setFramerate(&g_fpsManager, 60);
}

void quit()
{
	SDL_DestroyRenderer(g_renderer);
	SDL_FreeSurface(g_screen);
	SDL_DestroyWindow(g_window);
	SDL_Quit();
	exit(0);
}

void update()
{
	SDL_SetRenderDrawColor(g_renderer, 220, 220, 220, 255);
	SDL_RenderFillRect(g_renderer, 0);
	g_chessboard.show();
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
		update();
		SDL_RenderPresent(g_renderer);
		SDL_framerateDelay(&g_fpsManager);
	}
	quit();
	return 0;
}
