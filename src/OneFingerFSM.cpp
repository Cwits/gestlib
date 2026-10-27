// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "gestlib/OneFingerFSM.h"

#include "gestlib/GestureRecognizer.h"
#include "gestlib/Gestures.h"
#include "gestlib/defines.h"

#include <cmath>
#include <algorithm>
#include <iostream>

static const auto idleThreshold = std::chrono::milliseconds(20);

namespace GestLib {

OneFingerFSM::OneFingerFSM(GestureRecognizer & rec, OneFingerFSMConfig cfg) : _recognizer(rec) {
    _tapTimeout = cfg.tapTimeout;
    _dragThreshold = cfg.dragThreshold;
    _holdThreshold = cfg.holdThreshold;
    _doubleTapTimeout = cfg.doubleTapTimeout;
    _maxHistoryLength = cfg.maxHistoryLength;

    _doubleTapHelper = Undetected;
}

bool OneFingerFSM::init(int driverWidth, int driverHeight) {
    _driverWidth = driverWidth;
    _driverHeight = driverHeight;
    return true;
}

void OneFingerFSM::outputResolution(int w, int h) {
    _outputWidth = w;
    _outputHeight = h;
}

void OneFingerFSM::process(std::vector<TouchEvent> & touches) {
    TouchEvent & event = touches[0];
    TimePoint now = std::chrono::steady_clock::now();
#if DEBUG_PRINT == 1
    printState(_state, true);
#endif

    switch(_state) {
        case(Idle): {
            if(event.type == TouchEvent::Type::Begin) {
                if(!_idleActive) {    
                    _startEvent = event;
                    _tapTimer.start(_tapTimeout, now);
                    _dragOrHoldTimer.start(_dragThreshold, now);
                    _holdTimer.start(_holdThreshold, now);
                    
                    _idleActive = true;
                    _idleToStroke = now;
                }
            } else {
                if(_idleActive) {
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - _idleToStroke);
                    if(diff > idleThreshold) {
                        _recognizer._state = GestureRecognizer::RecognizerState::OneFingerOngoing;

                        _state = Stroke;
                        
                        // std::cout << "[OFS] ------- td -------" << std::endl;
                        Gesture t;
                        t.type = Gestures::TouchDown;
                        t.touchDown = {
                            .x = lerp(_lastEvent.x, _driverWidth, _outputWidth),
                            .y = lerp(_lastEvent.y, _driverHeight, _outputHeight),
                            .touchId = _lastEvent.id
                        };
                        _recognizer.pushGesture(t);
                    }
                }
            }
        } break;
        case(Stroke): {
            if(event.type != TouchEvent::Type::End && event.id == _startEvent.id) {
                _tapTimer.stop();
                if(event.id == _startEvent.id) {
                    if(_lastEvent.type != TouchEvent::Type::Move) {
                        if(_dragOrHoldTimer.active() && _dragOrHoldTimer.expired(now)) {
                            _state = DragOrHold;
                        } 
                    } else {
                        float dist = distance(event, _startEvent);
                        if(dist >= 2) {
                            Gesture swipe;
                            swipe.type = Gestures::Swipe;
                            swipe.swipe = {
                                .state = GestureState::Start,
                                .x = lerp(_startEvent.x, _driverWidth, _outputWidth),
                                .y = lerp(_startEvent.y, _driverHeight, _outputHeight),
                                .dx = 0,
                                .dy = 0
                            }; 
                            _recognizer.pushGesture(swipe);
                            _state = SwipeOngoing;
                        }
                    }
                }
            } else if(event.type == TouchEvent::Type::Begin && event.id != _startEvent.id) {
                _doubleTapTimer.start(_doubleTapTimeout, now);
                _state = DoubleTapPossible;
            }
        } break;
        case(DragOrHold): {
            if(event.id != _startEvent.id) {
                reset();
            }
            
            if(event.type == TouchEvent::Type::Move) {
                _dragOrHoldTimer.stop();
                _holdTimer.stop();
                float dist = distance(event, _startEvent);
                if(dist >= 20) {
                    Gesture drag;
                    drag.type = Gestures::Drag;
                    drag.drag = {
                        .state = GestureState::Start,
                        .x = lerp(_startEvent.x, _driverWidth, _outputWidth),
                        .y = lerp(_startEvent.y, _driverHeight, _outputHeight),
                        .dx = 0,
                        .dy = 0
                    };
                    _recognizer.pushGesture(drag);
                    _state = DragOngoing;
                }   
            }
        } break;
        case(DragOngoing): {
            float dist = distance(event, _lastEvent);
            if(event.type == TouchEvent::Type::Move) {
                int dx = lerp(event.x, _driverWidth, _outputWidth) - lerp(_lastEvent.x, _driverWidth, _outputWidth);
                int dy = lerp(event.y, _driverHeight, _outputHeight) - lerp(_lastEvent.y, _driverHeight, _outputHeight);
                Gesture drag;
                drag.type = Gestures::Drag;
                drag.drag  = {
                    .state = GestureState::Move,
                    .x = lerp(event.x, _driverWidth, _outputWidth),
                    .y = lerp(event.y, _driverHeight, _outputHeight),
                    .dx = dx,
                    .dy = dy
                };
                _recognizer.pushGesture(drag);
            }
        } break;
        case(DoubleTapPossible): {
            if(event.type == TouchEvent::Type::Move && event.id == _lastEvent.id) { 
                if(_eventsHistory.size() < 5) break;

                //check all 5 are move
                bool skip = false;
                std::size_t size = _eventsHistory.size();
                for(int i=size-5; i<size; ++i) {
                    if(_eventsHistory[i].type != TouchEvent::Type::Move) {
                        skip = true;
                        break;
                    }
                }

                if(skip) break;

                if (_eventsHistory.size() % 5 == 0) {
                    //calc radius
                    float aproxRad = 0.0f;
                    {  
                        bool skip = false;
                        std::size_t size = _eventsHistory.size();
                        if(size < 10) skip = true;

                        if(!skip) {
                            for(std::size_t s=size-10; s<size; ++s) {
                                TouchEvent & t = _eventsHistory[s];
                                aproxRad = std::sqrt(std::pow(t.x-_startEvent.x, 2)+std::pow(t.y-_startEvent.y, 2));
                            }
                            aproxRad /= 10;
                            // std::cout << "aprox radius: " << aproxRad << std::endl;
                        }
                    }

                    float deltaAngle = 0.0f;
                    {//calc polar angle
                        std::size_t size = _eventsHistory.size();
                        float tmp = 0.0f;
                        float tmp2 = 0.0f;
                        for(std::size_t s=size-4; s<size; ++s) {
                            TouchEvent &t = _eventsHistory[s];
                            TouchEvent &t1 = _eventsHistory[s-1];
                            tmp = std::atan2(t.y-_startEvent.y, t.x-_startEvent.x);
                            tmp2 = std::atan2(t1.y-_startEvent.y, t1.x-_startEvent.x);
                            float tmp3 = tmp - tmp2;
                            deltaAngle += normalize_angle(tmp3);
                        }
                        std::cout << "angle rotation: " << deltaAngle << std::endl;
                    }



                    float turnSum = computeTurnAngleSum(_eventsHistory);
                    // std::cout << "turnsum: " << turnSum << std::endl;
                    float triqpi = M_PI * 0.95f;
                    if (turnSum >= triqpi) {
                        _doubleTapHelper = Circular;
                    } else {
                        _doubleTapHelper = Linear;
                    }
                    _predictions++;
                }

                if(_predictions >= 10) {
                    if(_doubleTapHelper == Circular) {
                        Gesture circ;
                        circ.type = Gestures::DoubleTapCircular;
                        circ.dtCircular = {
                            .state = GestureState::Start,
                            .x = lerp(_startEvent.x, _driverWidth, _outputWidth),
                            .y = lerp(_startEvent.y, _driverHeight, _outputHeight),
                            .dx = 0,
                            .dy = 0,
                            .angle = 0.f,
                            .deltaAngle = 0.f,
                            .radius = 0.f,
                            .speed = 0.f
                        };
                        _recognizer.pushGesture(circ);

                        _state = DoubleTapCircularOngoing;
                    } else if(_doubleTapHelper == Linear) {
                        Gesture swipe;
                        swipe.type = Gestures::DoubleTapSwipe;
                        swipe.dtSwipe = {
                            .state = GestureState::Start,
                            .x = lerp(_startEvent.x, _driverWidth, _outputWidth),
                            .y = lerp(_startEvent.y, _driverHeight, _outputHeight),
                            .dx = 0,
                            .dy = 0
                        };
                        _recognizer.pushGesture(swipe);

                        _state = DoubleTapSwipeOngoing;
                    }
                }
            } else if(event.type == TouchEvent::Type::Begin && event.id != _lastEvent.id) {
                std::cout << "Holly cow!" << std::endl;
            }
        } break;
        case(DoubleTapSwipeOngoing): {
            float dist = distance(event, _lastEvent);
            if(event.type == TouchEvent::Type::Move && dist >= 2) {
                int dx = lerp(event.x, _driverWidth, _outputWidth) - lerp(_lastEvent.x, _driverWidth, _outputWidth);
                int dy = lerp(event.y, _driverHeight, _outputHeight) - lerp(_lastEvent.y, _driverHeight, _outputHeight);
                Gesture swipe;
                swipe.type = Gestures::DoubleTapSwipe;
                swipe.dtSwipe = {
                    .state = GestureState::Move,
                    .x = lerp(event.x, _driverWidth, _outputWidth),
                    .y = lerp(event.y, _driverHeight, _outputHeight),
                    .dx = dx,
                    .dy = dy
                };
                _recognizer.pushGesture(swipe);

            }
        } break;
        case(DoubleTapCircularOngoing): {
            float dist = distance(event, _lastEvent);
            if(event.type == TouchEvent::Type::Move && dist > 2) {
                
                float anglePrev = getAngle(_lastEvent.x, _lastEvent.y, _startEvent.x, _startEvent.y);
                float angleCurr = getAngle(event.x, event.y, _startEvent.x, _startEvent.y);
                
                float deltaAngle = angleCurr - anglePrev;
                if (deltaAngle > M_PI) deltaAngle -= 2 * M_PI;
                if (deltaAngle < -M_PI) deltaAngle += 2 * M_PI;

                // _totalAngle += deltaAngle;

                float dx = event.x - _startEvent.x;
                float dy = event.y - _startEvent.y;
                float radius = std::sqrt(dx*dx + dy*dy);

                float dt = std::chrono::duration<float>(now - event.time).count();
                float speed = deltaAngle / dt;

                /*std::cout << "Double tap circular" << 
                    " move angle: " << angleCurr << 
                    " delta angle: " << deltaAngle << 
                    " radius: " << radius << 
                    " speed: " << speed << 
                    " x: " << event.x << 
                    " y: " << event.y << std::endl;*/
                Gesture circ;
                circ.type = Gestures::DoubleTapCircular;
                circ.dtCircular = {
                    .state = GestureState::Move,
                    .x = lerp(event.x, _driverWidth, _outputWidth),
                    .y = lerp(event.y, _driverHeight, _outputHeight),
                    .dx = 0,
                    .dy = 0,
                    .angle = angleCurr,
                    .deltaAngle = deltaAngle,
                    .radius = radius,
                    .speed = speed
                };
                _recognizer.pushGesture(circ);
            }
        } break;
        case(SwipeOngoing): {
            float dist = distance(event, _lastEvent);
            if(event.type == TouchEvent::Type::Move && dist > 5) {
                int dx = lerp(event.x, _driverWidth, _outputWidth) - lerp(_lastEvent.x, _driverWidth, _outputWidth);
                int dy = lerp(event.y, _driverHeight, _outputHeight) - lerp(_lastEvent.y, _driverHeight, _outputHeight);
                Gesture swipe;
                swipe.type = Gestures::Swipe;
                swipe.swipe = {
                    .state = GestureState::Move,
                    .x = lerp(event.x, _driverWidth, _outputWidth),
                    .y = lerp(event.y, _driverHeight, _outputHeight),
                    .dx = dx,
                    .dy = dy
                };
                _recognizer.pushGesture(swipe);
            } else if(event.type == TouchEvent::Type::End) {
                Gesture swipe;
                swipe.type = Gestures::Swipe;
                swipe.swipe = {
                    .state = GestureState::End,
                    .x = lerp(event.x, _driverWidth, _outputWidth),
                    .y = lerp(event.y, _driverHeight, _outputHeight),
                    .dx = 0,
                    .dy = 0
                };
                _recognizer.pushGesture(swipe);
                
                reset();
            }
        } break;
        case(Hold): {
            //do nothing
            if(event.id != _startEvent.id) {
                reset();
            }
        } break;
        case(HoldOngoing): {
            if(event.id != _startEvent.id) {
                reset();
            }

            if(event.type == TouchEvent::Type::Move) {
                int dx = lerp(event.x, _driverWidth, _outputWidth) - lerp(_lastEvent.x, _driverWidth, _outputWidth);
                int dy = lerp(event.y, _driverHeight, _outputHeight) - lerp(_lastEvent.y, _driverHeight, _outputHeight);
                
                Gesture hold;
                hold.type = Gestures::Hold;
                hold.hold = {
                    .state = GestureState::Move,
                    .x = lerp(_lastEvent.x, _driverWidth, _outputWidth), 
                    .y = lerp(_lastEvent.y, _driverHeight, _outputHeight),
                    .dx = dx,
                    .dy = dy,
                };
                _recognizer.pushGesture(hold);
            }
        } break;
    }
    _lastEvent = event;
    if(_eventsHistory.size() >= _maxHistoryLength) {
        _eventsHistory.pop_front();
    }
    _eventsHistory.push_back(event);

}

void OneFingerFSM::resetOrProcess() {
    TimePoint now = std::chrono::steady_clock::now();
#if DEBUG_PRINT == 1
    if(_idleActive || _state != state::Idle)  printState(_state, false);
#endif
    switch(_state) {
        case(Idle): {
            //do nothing
            if(_idleActive) {
                if(std::chrono::duration_cast<std::chrono::milliseconds>(now - _idleToStroke) > idleThreshold) {
                    _recognizer._state = GestureRecognizer::RecognizerState::OneFingerOngoing;

                    _state = Stroke;

                    Gesture t;
                    t.type = Gestures::TouchDown;
                    t.touchDown = {
                        .x = lerp(_lastEvent.x, _driverWidth, _outputWidth),
                        .y = lerp(_lastEvent.y, _driverHeight, _outputHeight),
                        .touchId = _lastEvent.id
                    };
                    _recognizer.pushGesture(t);
                }
            }
        } break;
        case(Stroke): {
                if(_lastEvent.type == TouchEvent::Type::Begin) {
                    if(_holdTimer.active() && _holdTimer.expired(now)) {
                        _state = Hold;
                    }
                } else if(_lastEvent.type == TouchEvent::Type::End) {
                    if(_tapTimer.active() && _tapTimer.expired(now)) { //must be expired
                        Gesture tap;
                        tap.type = Gestures::Tap;
                        tap.tap = {
                            .x = lerp(_lastEvent.x, _driverWidth, _outputWidth), 
                            .y = lerp(_lastEvent.y, _driverHeight, _outputHeight)
                        };
                        _recognizer.pushGesture(tap);
                        reset();
                    }
                }
        } break;
        case(DragOrHold): {
            // ???
            //the fakeSDL driver fails here due to too fast updates, dunno how it will affect real driver
            // reset(); //TODO:need some time-to-lost timer here??
        } break;
        case(DragOngoing): {
            if(_lastEvent.type == TouchEvent::Type::End) {
                Gesture drag;
                drag.type = Gestures::Drag;
                drag.drag = {
                    .state = GestureState::End,
                    .x = lerp(_lastEvent.x, _driverWidth, _outputWidth), 
                    .y = lerp(_lastEvent.y, _driverHeight, _outputHeight),
                    .dx = 0,
                    .dy = 0
                };
                _recognizer.pushGesture(drag);

                reset();
            }
        } break;
        case(DoubleTapPossible): {
            if(_lastEvent.type == TouchEvent::Type::End) {
                if(_doubleTapTimer.active() && _doubleTapTimer.expired(now)) {
                    Gesture dtap;
                    dtap.type = Gestures::DoubleTap;
                    dtap.doubleTap = {
                        .x = lerp(_lastEvent.x, _driverWidth, _outputWidth), 
                        .y = lerp(_lastEvent.y, _driverHeight, _outputHeight)
                    };
                    _recognizer.pushGesture(dtap);
                    
                    reset();
                }
            }
        } break;
        case(DoubleTapSwipeOngoing): {
            if(_lastEvent.type == TouchEvent::Type::End) {
                Gesture swipe;
                swipe.type = Gestures::DoubleTapSwipe;
                swipe.dtSwipe = {
                    .state = GestureState::End,
                    .x = lerp(_lastEvent.x, _driverWidth, _outputWidth), 
                    .y = lerp(_lastEvent.y, _driverHeight, _outputHeight),
                    .dx = 0,
                    .dy = 0
                };
                _recognizer.pushGesture(swipe);
                
                reset();
            }
        } break;
        case(DoubleTapCircularOngoing): {
            if(_lastEvent.type == TouchEvent::Type::End) {
                Gesture circ;
                circ.type = Gestures::DoubleTapCircular;
                circ.dtCircular = {
                    .state = GestureState::End,
                    .x = lerp(_lastEvent.x, _driverWidth, _outputWidth), 
                    .y = lerp(_lastEvent.y, _driverHeight, _outputHeight),
                    .dx = 0,
                    .dy = 0,
                    .angle = 0.f,
                    .deltaAngle = 0.f,
                    .radius = 0.f,
                    .speed = 0.f
                };
                _recognizer.pushGesture(circ);
                
                reset();
            }
        } break;
        case(SwipeOngoing): {
            if(_lastEvent.type == TouchEvent::Type::End) {
                Gesture swipe;
                swipe.type = Gestures::Swipe;
                swipe.swipe = {
                    .state = GestureState::End,
                    .x = lerp(_lastEvent.x, _driverWidth, _outputWidth), 
                    .y = lerp(_lastEvent.y, _driverHeight, _outputHeight),
                    .dx = 0,
                    .dy = 0
                };
                _recognizer.pushGesture(swipe);

                reset();
            }
        } break;
        case(Hold): {
            if(_lastEvent.id != _startEvent.id) {
                reset();
            }

            if(_lastEvent.type == TouchEvent::Type::Begin) {
                Gesture hold;
                hold.type = Gestures::Hold;
                hold.hold = {
                    .state = GestureState::Start,
                    .x = lerp(_lastEvent.x, _driverWidth, _outputWidth), 
                    .y = lerp(_lastEvent.y, _driverHeight, _outputHeight),
                    .dx = 0,
                    .dy = 0,
                };
                _recognizer.pushGesture(hold);
                _state = HoldOngoing;
            }
        } break;
        case(HoldOngoing): {
            if(_lastEvent.id != _startEvent.id) {
                reset();
            }

            if(_lastEvent.type == TouchEvent::Type::End) {
                Gesture hold;
                hold.type = Gestures::Hold;
                hold.hold = {
                    .state = GestureState::End,
                    .x = lerp(_lastEvent.x, _driverWidth, _outputWidth), 
                    .y = lerp(_lastEvent.y, _driverHeight, _outputHeight),
                    .dx = 0,
                    .dy = 0,
                };
                _recognizer.pushGesture(hold);
                reset();
            }
        } break;
    }
}

void OneFingerFSM::reset() {
#if DEBUG_PRINT == 1
    std::cout << "[OFS] reset" << std::endl;
#endif

    _doubleTapHelper = Undetected;
    _predictions = 0;
    _tapTimer.stop();
    _doubleTapTimer.stop();
    _dragOrHoldTimer.stop();
    _holdTimer.stop();
    if(_state != Idle) {
#if DEBUG_PRINT == 1
        std::cout << "[OFS] ------- tu -------" << std::endl;
#endif
        Gesture touch;
        touch.type = Gestures::TouchUp;
        touch.touchUp = {
            .x = lerp(_lastEvent.x, _driverWidth, _outputWidth), 
            .y = lerp(_lastEvent.y, _driverHeight, _outputHeight),
            .touchId = _lastEvent.id
        };
        _recognizer.pushGesture(touch);
    }
    _state = Idle;
    _idleActive = false;

    _recognizer.reset();
}

void OneFingerFSM::printState(state state, bool proc) {
    std::string text = proc ? "[OFS proc] " : "[OFS rest] ";
    switch(state) {
        case(Idle): { 
            if(!_idleActive) text.append("Idle NA"); 
            else text.append("Idle A"); 
            // if(_idleActive) text.append("Idle A");
        } break;
        case(Stroke): text.append("Stroke"); break;
        case(DragOrHold): text.append("DragOrHold"); break;
        case(DragOngoing): text.append("DragOngoing"); break;
        case(DoubleTapPossible): text.append("DoubleTapPossible"); break;
        case(DoubleTapSwipeOngoing): text.append("DoubleTapSwipeOngoing"); break;
        case(DoubleTapCircularOngoing): text.append("DoubleTapCircularOngoing"); break;
        case(SwipeOngoing): text.append("SwipeOngoing"); break;
        case(Hold): text.append("Hold"); break;
        case(HoldOngoing): text.append("HoldOngoing"); break;
    }
    std::cout << text << std::endl;
}

}//namespace GestLib