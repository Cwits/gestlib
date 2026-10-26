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

#include "gestlib/drivers/Generic.h"
#include "gestlib/GestureRecognizer.h"
//requires sudo apt install libsdl2-ttf-dev

enum class Scene {
    SomeExample,
    HoldAndDragExample,
    SwipeExample,
    ZoomExample
};

bool initSDL(int w, int h);
void createScenes();
void DrawRect(SDL_Renderer *r, SDL_Color &color, int x, int y, int w, int h);
void DrawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color);
void DrawCircle(SDL_Renderer* renderer, SDL_Color &color, int centerX, int centerY, float radius);

struct Button {
    int x;
    int y;
    int width;
    int height;
    std::string text;
    Scene scene;

    bool within(int _x, int _y);
    void draw(SDL_Renderer *r);
};

struct SceneBase {
    virtual void handleGesture(const GestLib::Gesture &) = 0;
    virtual void drawScene() = 0;
};

struct JoystickScene : public SceneBase {
    void handleGesture(const GestLib::Gesture &) override;
    void drawScene() override;
    
    struct Joystick : public GestLib::TouchCapture {
        void onMove(const GestLib::TouchEvent &touch) override;
        void onCancle() override;
    };

    Joystick joystick;
    GestLib::GestureRecognizer *rec = nullptr;
};

struct HoldAndDragScene : public SceneBase {
    void handleGesture(const GestLib::Gesture &) override;
    void drawScene() override;
    
    struct Box {
        SDL_Color color;
        int lastx;
        int lasty;
        bool within(int _x, int _y);
    };

    std::vector<Box> boxes;
    Box * capturedBox = nullptr;
    std::string text;
    bool inEvent = false;
};

struct ZoomScene : public SceneBase {
    void handleGesture(const GestLib::Gesture &) override;
    void drawScene() override;

    int radius = 100;
};


SDL_Window * window;
SDL_Renderer * renderer;
TTF_Font* font;
TTF_Font* font12;
TTF_Font* font28;

std::vector<Button> buttons;
SDL_Color whiteColor = {255, 255, 255, 255};
std::mt19937 rng;
std::uniform_int_distribution<unsigned char> colorDist;

ZoomScene zoomScene;
HoldAndDragScene hadScene;
JoystickScene joyScene;

int main() {
    if(!initSDL(1920, 1080)) {
        return 1;
    }
    createScenes();

    rng = std::mt19937(std::random_device{}());
    colorDist = std::uniform_int_distribution<unsigned char>(100, 255);

    GestLib::GenericDriverConfig dcfg{};
    GestLib::GenericDriver driver(dcfg);
    driver.init(true);

    GestLib::RecognizerConfig cfg{};
    GestLib::GestureRecognizer recn(cfg);
    if(!recn.init(&driver)) {
        return 1;
    }

    // joyScene->rec = &recn;
    recn.start();
    
    int fps = 1 / 60.f * 1000;
    bool running = true;
    Scene currentScene = Scene::ZoomExample;
    
    // std::deque<SDL_Event> fakeBatch;
    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYUP: {
                    // fakeBatch.push_back(event);
                    if(event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                        
                    }
                } break;
            }
        }

        // Очистка экрана
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        std::vector<GestLib::Gesture> gestures = recn.fetchGestures();
        for(const GestLib::Gesture &g : gestures) {
            // std::string text = GestLib::gestureToText(g.type);
            // std::cerr << "[Main] ------------------ " << text << " -----------------------" << std::endl;
            switch(currentScene) {
                case(Scene::SomeExample): break;
                case(Scene::HoldAndDragExample): hadScene.handleGesture(g); break;
                case(Scene::SwipeExample): break;
                case(Scene::ZoomExample): zoomScene.handleGesture(g); break;
            }
            switch(g.type) {
                case(GestLib::Gestures::Tap): {
                    for(auto &b : buttons) {
                        if(b.within(g.tap.x, g.tap.y)) {
                            currentScene = b.scene;
                            break;
                        }
                    }
                } break;
            }
        }
        
        for(auto &b : buttons) {
            b.draw(renderer);
        }

        switch(currentScene) {
            case(Scene::SomeExample): break;
            case(Scene::HoldAndDragExample): hadScene.drawScene(); break;
            case(Scene::SwipeExample): break;
            case(Scene::ZoomExample): zoomScene.drawScene(); break;
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(fps);
    }

    recn.shutdown();
    driver.shutdown();

    TTF_CloseFont(font12);
    TTF_CloseFont(font28);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    return 0;
}


bool initSDL(int w, int h) {
    if(SDL_Init(SDL_INIT_VIDEO) != 0)
        return false;

    if(TTF_Init() != 0) {
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
    font28 = TTF_OpenFont(
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        28
    );
    font12 = TTF_OpenFont(
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        12
    );

    if(!window || !renderer || !font28 || !font12) {
        if(font12) TTF_CloseFont(font12);
        if(font28) TTF_CloseFont(font28);
        if(renderer) SDL_DestroyRenderer(renderer);
        if(window) SDL_DestroyWindow(window);

        TTF_Quit();
        SDL_Quit();
        return false;
    }

    return true;
}

void createScenes() {
    Button b;
    b.x = 100;
    b.y = 50;
    b.width = 200;
    b.height = 100;
    b.text = "Test 1";
    b.scene = Scene::SomeExample;
    buttons.push_back(b);

    b.x += 300;
    b.text = "HDS";
    b.scene = Scene::HoldAndDragExample;
    buttons.push_back(b);

    b.x += 300;
    b.text = "Test 3";
    b.scene = Scene::SwipeExample;
    buttons.push_back(b);

    b.x += 300;
    b.text = "Zoom";
    b.scene = Scene::ZoomExample;
    buttons.push_back(b);

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

void DrawRect(SDL_Renderer *r, SDL_Color &color, int x, int y, int w, int h) {
    //box
    SDL_Rect box = {x, y, w, h};
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, 255);
    SDL_RenderDrawRect(r, &box);
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
        // Рисуем пиксели во всех 8 октантах окружности
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

bool Button::within(int _x, int _y) {
    bool t1 = _x >= x;
    bool t2 = _x <= width+x;
    bool t3 = _y >= y;
    bool t4 = _y <= height+y;
    
    return t1 && t2 && t3 && t4;
}

void Button::draw(SDL_Renderer *r) {
    DrawRect(r, whiteColor, x, y, width, height);
    DrawText(r, font28, text, x+50, y + 30, whiteColor);
}


void ZoomScene::handleGesture(const GestLib::Gesture &g) {
    switch(g.type) {
        case(GestLib::Gestures::Zoom): {
            const GestLib::ZoomGesture &z = g.zoom;

            radius *= z.horDif;
            if(radius < 10) radius = 10;
            if(radius > 700) radius = 700;
        } break;
    }
}

void ZoomScene::drawScene() {    
    DrawCircle(renderer, whiteColor, 1920/2, 1080/2, radius);
}


void HoldAndDragScene::handleGesture(const GestLib::Gesture &g) {
    switch(g.type) {
        case(GestLib::Gestures::TouchDown): {
            const GestLib::TouchDownEvent &td = g.touchDown;
            std::cout << "id: " << td.touchId << std::endl;
            
            bool notFound = true;
            for(auto &b : boxes) {
                if(b.within(td.x, td.y)) {
                    capturedBox = &b;
                    notFound = false;
                    break;
                }
            }

            inEvent = true;
        } break;
        case(GestLib::Gestures::TouchUp): {
            const GestLib::TouchUpEvent &tu = g.touchUp;
            
            capturedBox = nullptr;
            inEvent = false;
        } break;
        case(GestLib::Gestures::Hold): {
            const GestLib::HoldGesture &hold = g.hold;
            if(!capturedBox && hold.state == GestLib::GestureState::Start) { 
                boxes.push_back({
                    .color = {
                        colorDist(rng),
                        colorDist(rng),
                        colorDist(rng),
                        255
                    },
                    .lastx = hold.x,
                    .lasty = hold.y
                });
                capturedBox = &boxes.back();
            }

            capturedBox->lastx = hold.x;
            capturedBox->lasty = hold.y;
            if(hold.state == GestLib::GestureState::Start) text = "Hold Start";
            else if(hold.state == GestLib::GestureState::Move) text = "Hold Move";
            else if(hold.state == GestLib::GestureState::End) text = "Hold End";
        } break;
        case(GestLib::Gestures::Drag): {
            const GestLib::DragGesture &drag = g.drag;
            if(!capturedBox && drag.state == GestLib::GestureState::Start) { 
                boxes.push_back({
                    .color = {
                        colorDist(rng),
                        colorDist(rng),
                        colorDist(rng),
                        255
                    },
                    .lastx = drag.x,
                    .lasty = drag.y
                });
                capturedBox = &boxes.back();
            }

            capturedBox->lastx = drag.x;
            capturedBox->lasty = drag.y;
            
            if(drag.state == GestLib::GestureState::Start) text = "Drag Start";
            else if(drag.state == GestLib::GestureState::Move) text = "Drag Move";
            else if(drag.state == GestLib::GestureState::End) text = "Drag End";
        } break;
        case(GestLib::Gestures::Swipe): {
            boxes.clear();
        } break;
        default:
            if(inEvent) text = GestLib::gestureToText(g.type);
        break;
    }
}

void HoldAndDragScene::drawScene() {
    for(auto &b : boxes) {
        int x = b.lastx - 60 / 2;
        int y = b.lasty - 60 / 2;
        DrawRect(renderer, b.color, x, y, 60, 60);
    }

    std::string noAction = "No Action";
    if(inEvent) DrawText(renderer, font28, text, 1500, 100, whiteColor);
    else DrawText(renderer, font28, noAction, 1500, 100, whiteColor);
}

bool HoldAndDragScene::Box::within(int _x, int _y) {
    bool t1 = _x >= lastx;
    bool t2 = _x <= 60+lastx;
    bool t3 = _y >= lasty;
    bool t4 = _y <= 60+lasty;
    
    return t1 && t2 && t3 && t4;
}

void JoystickScene::handleGesture(const GestLib::Gesture &g) {
    switch(g.type) {
        case(GestLib::Gestures::TouchDown): {
            const GestLib::TouchDownEvent &td = g.touchDown;

            if(td.touchId != -1) {}
                // rec->registerTouchCapture(&joystick, td.touchId);
        } break;
        case(GestLib::Gestures::TouchUp): {
            const GestLib::TouchUpEvent &tu = g.touchUp;

            if(tu.touchId != -1) {}
                // rec->unregisterTouchCapture(&joystick, tu.touchId);
        } break;
    }
}

void JoystickScene::drawScene() {

}

void JoystickScene::Joystick::onMove(const GestLib::TouchEvent &touch) {
    
}

void JoystickScene::Joystick::onCancle() {
    
}
    