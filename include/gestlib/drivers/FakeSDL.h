// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include <string>
#include <queue>
#include <vector>
#include <deque>
#include <chrono>
#include <linux/input.h>
#include <mutex>

#include <SDL2/SDL.h>
#include "gestlib/TouchDriver.h"
#include "gestlib/defines.h"

class libevdev;

namespace GestLib {

struct FakeSDLDriverConfig {

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
    int _scndX;
    int _scndY;

    enum class Mode {
        OneTouch,
        TwoTouchOpposite,
        TwoTouchSame
    }; 
    Mode _mode;
    
    std::vector<TouchEvent> _currentFrameEvents;

    int _internalIdCounter; 
};

}