// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include <string>
#include <vector>
#include <linux/input.h>
#include <mutex>

#include <SDL2/SDL.h>
#include "gestlib/TouchDriver.h"
#include "gestlib/defines.h"

class libevdev;

namespace GestLib {

struct FakeSDLDriverConfig {
    int width = 1920;
    int height = 1080;
    SDL_Scancode oneFingerButton = SDL_SCANCODE_C;
    SDL_Scancode twofingerButtonOpp = SDL_SCANCODE_B;
    SDL_Scancode twoFingerButtonSame = SDL_SCANCODE_N;
};

class FakeSDLDriver : public TouchDriver {
    public:
    FakeSDLDriver(const FakeSDLDriverConfig);
    ~FakeSDLDriver();

    bool init();
    bool shutdown();

    std::vector<TouchEvent> getEvents() override;
    void feedSDLEvent(SDL_Event &ev);

    int fd() const override { return _fd; } //ugh... don't like this move

    std::string helpString() const;

    private:
    FakeSDLDriverConfig _cfg;
    int _fd;

    std::mutex _mutex;
    std::deque<SDL_Event> _inEvents;
    std::vector<TouchEvent> parseInEvents();

    int _touchId = 1;

    int _lastX;
    int _lastY;

    bool _gestureOngoing;
    
    enum class Mode {
        OneTouch,
        TwoTouchDiverge, //zoomin
        TwoTouchConverge, //zoomout
        TwoTouchSame
    }; 
    Mode _mode;
    
    struct TwoFingerData {
        int initx;
        int inity;
        int x1;
        int y1;
        int id1;

        int x2;
        int y2;
        int id2;
    };

    TwoFingerData _tfdata;
    bool _movementVertical;
    bool _pressed;

    std::vector<TouchEvent> _currentFrameEvents;

    int _internalIdCounter; 
};

}