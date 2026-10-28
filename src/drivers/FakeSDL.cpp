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
    _movementVertical = false;
    _pressed = false;
}

FakeSDLDriver::~FakeSDLDriver() {

}

bool FakeSDLDriver::init() {
    _width = _cfg.width;
    _height = _cfg.height;

    _fd = eventfd(0, EFD_NONBLOCK);
    if(_fd < 0) {
        std::cerr << "Failed to init fd" << std::endl;
        return false;
    }

    return true;
}

bool FakeSDLDriver::shutdown() {
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
    //need to filter down mouse events?...
    std::vector<SDL_Event> events;
    bool hasMoveRet = false;

    for(auto &ev : _inEvents) {
        switch(ev.type) {
            case(SDL_KEYDOWN): { 
                SDL_Scancode &code = ev.key.keysym.scancode;
                if(code == SDL_SCANCODE_C) {
                    if(!_gestureOngoing) {
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
                    }
                } else if(code == SDL_SCANCODE_B) {
                    if(!_gestureOngoing) {
                        //two touches opposite - zoomin
                        //two touches mode opposite move(for zoom)
                        _mode = Mode::TwoTouchDiverge;
                        _tfdata.id1 = _touchId++;
                        _tfdata.id2 = _touchId++;
                        _tfdata.x1 = _lastX;
                        _tfdata.y1 = _lastY;
                        if(_movementVertical) {
                            _tfdata.x2 = _lastX;
                            _tfdata.y2 = _lastY - 30;
                        } else {
                            _tfdata.x2 = _lastX + 30;
                            _tfdata.y2 = _lastY;
                        }
                        _tfdata.initx = _lastX;
                        _tfdata.inity = _lastY;
                        
                        auto now = std::chrono::steady_clock::now();
                        ret.push_back({
                            .id = _tfdata.id1,
                            .type = TouchEvent::Type::Begin,
                            .x = _tfdata.x1,
                            .y = _tfdata.y1,
                            .time = now
                        });
                        
                        ret.push_back({
                            .id = _tfdata.id2,
                            .type = TouchEvent::Type::Begin,
                            .x = _tfdata.x2,
                            .y = _tfdata.y2,
                            .time = now
                        });
                        _gestureOngoing = true;
                    }
                } else if(code == SDL_SCANCODE_V) {
                    //zoom out
                    if(!_gestureOngoing) {
                        _mode = Mode::TwoTouchConverge;
                        _tfdata.id1 = _touchId++;
                        _tfdata.id2 = _touchId++;
                        _tfdata.x1 = _lastX;
                        _tfdata.y1 = _lastY;
                        _tfdata.x2 = _cfg.width - _lastX;
                        _tfdata.y2 = _cfg.height - _lastY;
                        _tfdata.initx = _lastX;
                        _tfdata.inity = _lastY;
                        
                        auto now = std::chrono::steady_clock::now();
                        ret.push_back({
                            .id = _tfdata.id1,
                            .type = TouchEvent::Type::Begin,
                            .x = _tfdata.x1,
                            .y = _tfdata.y1,
                            .time = now
                        });
                        
                        ret.push_back({
                            .id = _tfdata.id2,
                            .type = TouchEvent::Type::Begin,
                            .x = _tfdata.x2,
                            .y = _tfdata.y2,
                            .time = now
                        });
                        _gestureOngoing = true;
                    }
                } else if(code == SDL_SCANCODE_N) {
                    //two touches mode same move(for tap, doubletap, swipe)
                    if(!_gestureOngoing) {
                        _mode = Mode::TwoTouchSame;
                        _tfdata.id1 = _touchId++;
                        _tfdata.id2 = _touchId++;
                        _tfdata.x1 = _lastX;
                        _tfdata.y1 = _lastY;
                        if(_movementVertical) {
                            _tfdata.x2 = _lastX;
                            _tfdata.y2 = _lastY - 30;
                        } else {
                            _tfdata.x2 = _lastX + 30;
                            _tfdata.y2 = _lastY;
                        }
                        _tfdata.initx = _lastX;
                        _tfdata.inity = _lastY;
                        
                        auto now = std::chrono::steady_clock::now();
                        ret.push_back({
                            .id = _tfdata.id1,
                            .type = TouchEvent::Type::Begin,
                            .x = _tfdata.x1,
                            .y = _tfdata.y1,
                            .time = now
                        });
                        
                        ret.push_back({
                            .id = _tfdata.id2,
                            .type = TouchEvent::Type::Begin,
                            .x = _tfdata.x2,
                            .y = _tfdata.y2,
                            .time = now
                        });
                        _gestureOngoing = true;
                    }
                } else if(code == SDL_SCANCODE_M) {
                } else if(code == SDL_SCANCODE_H) {
                    if(!_pressed) {
                        if(_movementVertical) {
                            std::cout << "Horiontal movement" << std::endl;
                            _movementVertical = false;
                        } else {
                            std::cout << "Vertical movement" << std::endl;
                            _movementVertical = true;
                        }
                        _pressed = true;
                    }
                }
            } break;
            case(SDL_KEYUP): {
                SDL_Scancode &code = ev.key.keysym.scancode;
                if(code == SDL_SCANCODE_C) {
                    //disable touch
                    if(ret.size() != 0) ret.clear();
                    _gestureOngoing = false;
                    ret.push_back({
                        .id = _touchId++,
                        .type = TouchEvent::Type::End,
                        .x = _lastX,
                        .y = _lastY,
                        .time = std::chrono::steady_clock::now()
                    });
                } else if(code == SDL_SCANCODE_B) {
                    
                    auto now = std::chrono::steady_clock::now();
                    ret.push_back({
                        .id = _tfdata.id1,
                        .type = TouchEvent::Type::End,
                        .x = _tfdata.x1,
                        .y = _tfdata.y1,
                        .time = now
                    });
                    
                    ret.push_back({
                        .id = _tfdata.id2,
                        .type = TouchEvent::Type::End,
                        .x = _tfdata.x2,
                        .y = _tfdata.y2,
                        .time = now
                    });
                    _gestureOngoing = false;
                } else if(code == SDL_SCANCODE_V) {
                    //zoom out
                    
                    auto now = std::chrono::steady_clock::now();
                    ret.push_back({
                        .id = _tfdata.id1,
                        .type = TouchEvent::Type::End,
                        .x = _tfdata.x1,
                        .y = _tfdata.y1,
                        .time = now
                    });
                    
                    ret.push_back({
                        .id = _tfdata.id2,
                        .type = TouchEvent::Type::End,
                        .x = _tfdata.x2,
                        .y = _tfdata.y2,
                        .time = now
                    });
                    _gestureOngoing = false;
                } else if(code == SDL_SCANCODE_N) {

                    auto now = std::chrono::steady_clock::now();
                    ret.push_back({
                        .id = _tfdata.id1,
                        .type = TouchEvent::Type::End,
                        .x = _tfdata.x1,
                        .y = _tfdata.y1,
                        .time = now
                    });
                    
                    ret.push_back({
                        .id = _tfdata.id2,
                        .type = TouchEvent::Type::End,
                        .x = _tfdata.x2,
                        .y = _tfdata.y2,
                        .time = now
                    });
                    _gestureOngoing = false;
                } else if(code == SDL_SCANCODE_H) {
                    if(_pressed) _pressed = false;
                }
            } break;
            case(SDL_MOUSEMOTION): {
                // int x = ev.motion.x;
                // int y = ev.motion.y;
                _lastX = ev.motion.x;
                _lastY = ev.motion.y;

                if(_gestureOngoing && !hasMoveRet) {
                    hasMoveRet = true;
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
                        case(Mode::TwoTouchDiverge): {
                            _tfdata.x1 = _lastX;
                            _tfdata.y1 = _lastY;
                            int diffx = _tfdata.initx - _lastX;
                            int diffy = _tfdata.inity - _lastY;
                            if(_movementVertical) {
                                _tfdata.x2 = _lastX;
                                _tfdata.y2 = _tfdata.inity + diffy;
                            } else {
                                _tfdata.x2 = _tfdata.initx + diffx;
                                _tfdata.y2 = _lastY;
                            }
                            auto now = std::chrono::steady_clock::now();
                            ret.push_back({
                                .id = _tfdata.id1,
                                .type = TouchEvent::Type::Move,
                                .x = _tfdata.x1,
                                .y = _tfdata.y1,
                                .time = now
                            });
                            
                            ret.push_back({
                                .id = _tfdata.id2,
                                .type = TouchEvent::Type::Move,
                                .x = _tfdata.x2,
                                .y = _tfdata.y2,
                                .time = now
                            });
                        } break;
                        case(Mode::TwoTouchConverge): {
                            _tfdata.x1 = _lastX;
                            _tfdata.y1 = _lastY;
                            int diffx = _tfdata.initx - _lastX;
                            int diffy = _tfdata.inity - _lastY;
                            
                            if(_movementVertical) {
                                _tfdata.x2 = _lastX;
                                _tfdata.y2 = (_cfg.height - _tfdata.inity) + diffy;
                            } else {
                                _tfdata.x2 = (_cfg.width - _tfdata.initx) + diffx;
                                _tfdata.y2 = _lastY;
                            }

                            auto now = std::chrono::steady_clock::now();
                            ret.push_back({
                                .id = _tfdata.id1,
                                .type = TouchEvent::Type::Move,
                                .x = _tfdata.x1,
                                .y = _tfdata.y1,
                                .time = now
                            });
                            
                            ret.push_back({
                                .id = _tfdata.id2,
                                .type = TouchEvent::Type::Move,
                                .x = _tfdata.x2,
                                .y = _tfdata.y2,
                                .time = now
                            });
                        } break;
                        case(Mode::TwoTouchSame): {
                            _tfdata.x1 = _lastX;
                            _tfdata.y1 = _lastY;
                            _tfdata.x2 = _lastX + 30;
                            _tfdata.y2 = _lastY - 30;
                            auto now = std::chrono::steady_clock::now();
                            ret.push_back({
                                .id = _tfdata.id1,
                                .type = TouchEvent::Type::Move,
                                .x = _tfdata.x1,
                                .y = _tfdata.y1,
                                .time = now
                            });
                            
                            ret.push_back({
                                .id = _tfdata.id2,
                                .type = TouchEvent::Type::Move,
                                .x = _tfdata.x2,
                                .y = _tfdata.y2,
                                .time = now
                            });
                        } break;
                    } // switch
                } //if(_gestureOngoing && !hasMoveRet) {
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
            To make a two finger touches to diverge from center press 'B' \n\
            To make a two finger touches to converge to center press 'V' \n\
            To make a two finger touches in same direction press 'N' \n\
            To toggle two finger movement between X or Y axis press 'H' \n\
        ");
}

}