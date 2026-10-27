// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <random>
#include <string>
#include <cstdio>
#include <iostream>
#include <thread>
#include <chrono>

#include "gestlib/drivers/Generic.h"
#include "gestlib/drivers/FakeSDL.h"

//requires sudo apt install libsdl2-ttf-dev
SDL_Window * window;
SDL_Renderer * renderer;
TTF_Font* font;
constexpr int boxSize = 40;
constexpr int boxOffset = 8;

struct drawable {
    SDL_Color color;
    int lastx;
    int lasty;
    int id;
    std::vector<int> lastIds;
};

bool initSDL(int w, int h);
void DrawBox(SDL_Renderer *r, SDL_Color &color, int x, int y, int logicalid, std::vector<int> &rawid);
void DrawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color);
void DrawCircle(SDL_Renderer* renderer, SDL_Color &color, int centerX, int centerY, float radius);


int main() {
    if(!initSDL(1920, 1080)) {
        return 1;
    }

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> colorDist(50, 255);
    SDL_Color currentColor = {255, 255, 255, 255};

    std::deque<drawable> _draws;

    bool running = true;

    GestLib::GenericDriver _dri(GestLib::GenericDriverConfig{});
    _dri.init(true);
    // GestLib::FakeSDLDriver _dri(GestLib::FakeSDLDriverConfig{});
    // _dri.init();

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYUP: {
                    if(event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                        _draws.clear();
                    }
                } break;
            }
            // _dri.feedSDLEvent(event);
        }

        // Очистка экрана
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        std::vector<GestLib::TouchEvent> ev = _dri.getEvents();

        for(auto &t: ev) {
            bool found = false;
            for(auto &d : _draws) {
                if(d.id == t.id) {
                    found = true;
                    d.lastx = GestLib::lerp(t.x, _dri.width(), 1920);
                    d.lasty = GestLib::lerp(t.y, _dri.height(), 1080);
                    
                    bool notFound = true;
                    for(auto &i : d.lastIds) {
                        if(t.rawid == i) { notFound = false; break; }
                    }

                    if(notFound) {
                        d.lastIds.push_back(t.rawid);
                    }
                    break;
                }
            }

            if(!found) {
                if(_draws.size() >= 8) _draws.pop_front();
                _draws.push_back({
                    .color = {
                        (Uint8)colorDist(rng),
                        (Uint8)colorDist(rng),
                        (Uint8)colorDist(rng),
                        255
                    },
                    .lastx = GestLib::lerp(t.x, _dri.width(), 1920),
                    .lasty = GestLib::lerp(t.y, _dri.height(), 1080),
                    .id = t.id
                });
            }
        }

        for(auto &d : _draws) {
            DrawBox(renderer, d.color, d.lastx, d.lasty, d.id, d.lastIds);
        }
        
        SDL_RenderPresent(renderer);
        SDL_Delay(1);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    return 0;
}


bool initSDL(int w, int h) {
     if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return false;

    if (TTF_Init() != 0) {
        SDL_Quit();
        return false;
    }

    window = SDL_CreateWindow(
        "SDL2 Paint",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_SHOWN
    );

    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED
    );

    // Измени путь на путь к своему TTF-шрифту.
    font = TTF_OpenFont(
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        16
    );

    if (!window || !renderer || !font) {
        if (font)
            TTF_CloseFont(font);
        if (renderer)
            SDL_DestroyRenderer(renderer);
        if (window)
            SDL_DestroyWindow(window);

        TTF_Quit();
        SDL_Quit();
        return false;
    }

    return true;
}

void DrawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(
        font, text.c_str(), color
    );

    if (!surface)
        return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(
        renderer, surface
    );

    if (texture) {
        SDL_Rect dst = {
            x, y, surface->w, surface->h
        };

        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }

    SDL_FreeSurface(surface);
}

void DrawBox(SDL_Renderer *r, SDL_Color &color, int x, int y, int logicalid, std::vector<int> &rawid) {
    //box
    SDL_Rect box = {x - boxSize / 2, y - boxSize / 2, boxSize, boxSize };
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, 255);
    SDL_RenderDrawRect(r, &box);

    //text
    char colorText[64];
    // char rawidText[64];
    std::string ridtext;
    char positionText[64];

    std::snprintf(colorText, sizeof(colorText), "lid: %d", logicalid);
    // std::snprintf(rawidText, sizeof(rawidText), "rawid: %d", rawid);
    std::snprintf(positionText, sizeof(positionText), "position x: %d y:%d", x, y);

    for(auto &i : rawid) {
        ridtext.append(std::to_string(i));
        ridtext.append(", ");
    }

    SDL_Color textColor = {255, 255, 255, 255};

    int textX = x - boxSize / 2;
    int textY = y - boxSize / 2 - 64;

    if (textY < 0)
        textY = 0;

    DrawText(r, font, colorText, textX, textY, textColor);
    DrawText(r, font, ridtext, textX, textY+20, textColor);
    DrawText(r, font, positionText, textX, textY + 40, textColor);
}

void DrawCircle(SDL_Renderer* renderer, SDL_Color &color, int centerX, int centerY, float radius) {
    if(radius < 2) radius = 2.0f;
    int x = radius - 1;
    int y = 0;
    int tx = 1;
    int ty = 1;
    int error = tx - (radius * 2);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    
    while (x >= y) {
        SDL_RenderDrawPoint(renderer, centerX + x, centerY - y);
        SDL_RenderDrawPoint(renderer, centerX + x, centerY + y);
        SDL_RenderDrawPoint(renderer, centerX - x, centerY - y);
        SDL_RenderDrawPoint(renderer, centerX - x, centerY + y);
        SDL_RenderDrawPoint(renderer, centerX + y, centerY - x);
        SDL_RenderDrawPoint(renderer, centerX + y, centerY + x);
        SDL_RenderDrawPoint(renderer, centerX - y, centerY - x);
        SDL_RenderDrawPoint(renderer, centerX - y, centerY + x);

        if (error <= 0) {
            y++;
            error += ty;
            ty += 2;
        }
        if (error > 0) {
            x--;
            tx += 2;
            error += tx - (radius * 2);
        }
    }
}