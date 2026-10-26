// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.
// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.
#include "gestlib/drivers/FakeSDL.h"

#include <sys/eventfd.h>
#include <unistd.h> //close(fd)
#include <fcntl.h> //open() O_RDONLY
#include <iostream>
#include <chrono>

namespace GestLib {

FakeSDLDriver::FakeSDLDriver(const FakeSDLDriverConfig cfg) :
    _cfg(cfg)
{
    _width = 1920;
    _height = 1080;
}

FakeSDLDriver::~FakeSDLDriver() {

}

bool FakeSDLDriver::init() {

    _fd = eventfd(0, EFD_NONBLOCK);
    if(_fd < 0) {
        std::cerr << "Failed to init fd" << std::endl;
        return false;
    }

    return true;
}

bool FakeSDLDriver::shutdown() {
    // uint64_t fd = 1;
    // write(_fd, &fd, sizeof(fd));
    
    close(_fd);
    return true;
}

std::vector<TouchEvent> FakeSDLDriver::getEvents() {
    std::vector<TouchEvent> ret;
    ret.reserve(5);
    
    {
        std::lock_guard<std::mutex> l(_mutex);
        if(!_inEvents.empty()) {
            ret = parseInEvents();
        }
    }

    return ret;
}

void FakeSDLDriver::feedSDLEvent(SDL_Event &ev) {
    {
        std::lock_guard<std::mutex> l(_mutex);
        _inEvents.push_back(ev);
    }
    
    uint64_t wakeup = 1;
    write(_fd, &wakeup, sizeof(wakeup));
}

std::vector<TouchEvent> FakeSDLDriver::parseInEvents() {
    std::vector<TouchEvent> ret;
    for(auto &ev : _inEvents) {
        switch(ev.type) {
            case(SDL_KEYDOWN): {
                SDL_Scancode &code = ev.key.keysym.scancode;
                if(code == SDL_SCANCODE_C) {
                    //enable touch
                    _mode = Mode::OneTouch;
                    ret.push_back({
                        .id = _touchId,
                        .type = TouchEvent::Type::Begin,
                        .x = _lastX,
                        .y = _lastY,
                        .time = std::chrono::steady_clock::now()
                    });
                    _gestureOngoing = true;
                } else if(code == SDL_SCANCODE_B) {
                    //two touches opposite
                    //two touches mode opposite move(for zoom)
                } else if(code == SDL_SCANCODE_N) {
                    //two touches mode same move(for tap, doubletap, swipe)
                } else if(code == SDL_SCANCODE_M) {
                }
            } break;
            case(SDL_KEYUP): {
                SDL_Scancode &code = ev.key.keysym.scancode;
                if(code == SDL_SCANCODE_C) {
                    //disable touch
                    _gestureOngoing = false;
                    ret.push_back({
                        .id = _touchId++,
                        .type = TouchEvent::Type::End,
                        .x = _lastX,
                        .y = _lastY,
                        .time = std::chrono::steady_clock::now()
                    });
                } else if(code == SDL_SCANCODE_B) {

                } else if(code == SDL_SCANCODE_N) {

                }
            } break;
            case(SDL_MOUSEMOTION): {
                // int x = ev.motion.x;
                // int y = ev.motion.y;
                
                _lastX = ev.motion.x;
                _lastY = ev.motion.y;

                if(_gestureOngoing) {
                    switch(_mode) {
                        case(Mode::OneTouch): {
                            ret.push_back({
                                .id = _touchId,
                                .type = TouchEvent::Type::Move,
                                .x = _lastX,
                                .y = _lastY,
                                .time = std::chrono::steady_clock::now(),
                            });
                        } break;
                        case(Mode::TwoTouchOpposite): {
                        } break;
                        case(Mode::TwoTouchSame): {
                        } break;
                    }
                }
            } break;
            case(SDL_MOUSEBUTTONDOWN): {
                if(ev.button.button != 1) continue; // == 3 for left click
                // int x = ev.button.button.x;
                // int y = ev.button.button.y;

            } break;
            case(SDL_MOUSEBUTTONUP): {
                if(ev.button.button != 1) continue; // == 3 for left click
                // int x = ev.button.button.x;
                // int y = ev.button.button.y;

            } break;
            default:
                // std::cerr << "Unrelevant event" << std::endl;
            break;
        }
    }

    _inEvents.clear();
    return ret;
}


std::string FakeSDLDriver::helpString() const {
    return std::string(" \n\
            Fake SDL Driver uses SDL to simulate Touch Events to Gesture Recognizer \n\
            To trigger events use keyboard, if needed to make a moving event press and hold keyboard key and move mouse\n\
            To make a single touch event press press 'C' \n\
            To make a two finger touches to opposite sides from center press 'B' \n\
            To make a two finger touches in same direction press 'N' \n\
        ");
}

}