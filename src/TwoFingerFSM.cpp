// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "gestlib/TwoFingerFSM.h"

#include "gestlib/GestureRecognizer.h"
#include "gestlib/Gestures.h"
#include "gestlib/defines.h"

#include <cmath>
#include <algorithm>
#include <iostream>


namespace GestLib {

static const auto idleThreshold = std::chrono::milliseconds(10);

TwoFingerFSM::TwoFingerFSM(GestureRecognizer &rec, TwoFingerFSMConfig cfg) :
    _cfg(cfg),
    _recognizer(rec) 
{
    _state = state::Idle;
    _idleActive = false;

}

TwoFingerFSM::~TwoFingerFSM() {

}
 

bool TwoFingerFSM::init(int driverWidth, int driverHeight) {
    _driverWidth = driverWidth;
    _driverHeight = driverHeight;
    return true;
}

void TwoFingerFSM::outputResolution(int w, int h) {
    _outputWidth = w;
    _outputHeight = h;
}

/*
    ugh, things getting difficult here...
    ideally - must track both fingers changing state and drop event only if both fingers ended
    here will be bunch of permutations - e.g. one of fingers disappeared and appeared again, than 
    need to decide what to do either resotre the gesture or drop and...?
    how to check for ID's?

    i'd say that for now let's assume that we receiving only stable touches with consistent ID's and, 
    when one disappeares than gesture ended

*/
void TwoFingerFSM::process(std::vector<TouchEvent> & touches) {
    TouchEvent *te1, *te2;
    if(touches.size() == 1) {
        if(_lastTouches.first.id == touches[0].id) {
            te1 = &touches[0];
            te2 = &_lastTouches.second;
        } else if(_lastTouches.second.id == touches[0].id) {
            te1 = &_lastTouches.first;
            te2 = &touches[0];
        } else {
            std::cout << "Lost both touches" << std::endl;
            reset();
	    return;
            //idk yet what to do... i guess reset?
        }
    } else if(touches.size() == 2) {
        te1 = &touches[0];
        te2 = &touches[1];
    }
    TouchEvent &ev1 = *te1;
    TouchEvent &ev2 = *te2;
    TimePoint now = std::chrono::steady_clock::now();

#if DEBUG_PRINT == 1
    printstate(true); 
#endif

    switch(_state) {
        case(state::Idle): {
            if(ev1.type != TouchEvent::Type::End) {
                if(!_idleActive) {
                    _startTouches = std::make_pair(touches[0], touches[1]);
                    _tapTimer.start(300, now);

                    _idleToStroke = now;
                    _idleActive = true;
                } else {
                    if(std::chrono::duration_cast<std::chrono::milliseconds>(now - _idleToStroke) >= idleThreshold) {
                        _recognizer._state = GestureRecognizer::RecognizerState::TwoFingerOngoing;

                        _state = state::Stroke;

                        int dx = (_lastTouches.first.x + _lastTouches.second.x) / 2;
                        int dy = (_lastTouches.first.y + _lastTouches.second.y) / 2;
#if DEBUG_PRINT == 1
                        std::cout << "[TFS] ------- td -------" << std::endl;
#endif
                        Gesture t;
                        t.type = Gestures::TouchDown;
                        t.touchDown = {
                            .x = lerp(dx, _driverWidth, _outputWidth),
                            .y = lerp(dy, _driverHeight, _outputHeight),
                            .touchId = -1
                        };

                        _gestureToLost.start(_cfg.gestureToLost, now);
                        _recognizer.pushGesture(t);
                    }
                }
            }
        } break;
        case(state::Stroke): {
            // std::cerr << "Two finger stroke" << std::endl;
            if(ev1.type != TouchEvent::Type::End && ev1.id == _startTouches.first.id) {
                _tapTimer.stop();

                if(_eventHistory.size() < 3) { break; } //need some history

                float dist = 0.0f;
                float prevDist = 0.0f;
                float dx = 0.0f;
                float dy = 0.0f;
                int lessThanMin = 0;
                bool skipIteration = false;
                const float minValue = 16.0f;

                for(int i=3; i>0; --i) {
                    Event &ev = _eventHistory[_eventHistory.size()-i];
                    if(ev.first.id != ev1.id) { skipIteration = true; break; }
                    
                    dx = std::abs(ev.first.x - ev.second.x);
                    dy = std::abs(ev.first.y - ev.second.y);
                    dist = std::hypot(dx, dy);
                    if(std::abs(dist-prevDist) < minValue) lessThanMin++;

                    prevDist = dist;
                }
                if(skipIteration) break;

                dx = std::abs(ev1.x - ev2.x);
                dy = std::abs(ev1.y - ev2.y);
                
                dist = std::hypot(dx, dy);
                if(std::abs(dist-prevDist) < minValue) lessThanMin++;
                    
                // Gestures gest = changing ? Gestures::Zoom : Gestures::Swipe;
                Gestures gest = lessThanMin >= 2 ? Gestures::TwoFingerSwipe : Gestures::Zoom;
                
                if(gest == Gestures::TwoFingerSwipe) {
                    float dist1 = distance(ev1, _startTouches.first);
                    float dist2 = distance(ev2, _startTouches.second);
                    
                    //can be swipe, can be zoom... need to check history and track the distance change directions
                    //if points 
                    if(dist1 >= 2 && dist2 >= 2) {
                        Gesture swipe;
                    
                        swipe.type = Gestures::TwoFingerSwipe;
                        swipe.tfSwipe = {
                            .state = GestureState::Start,
                            // .x = lerp(dx1, _driverWidth, _outputWidth),
                            // .y = lerp(dy1, _driverHeight, _outputHeight),
                            
                            .x = lerp(5, _driverWidth, _outputWidth),
                            .y = lerp(5, _driverHeight, _outputHeight),
                            .dx = 0,
                            .dy = 0 
                        };
                        _gestureToLost.update(now);
                        _recognizer.pushGesture(swipe);
                        _state = state::SwipeOngoing;
                    }    
                } else if(gest == Gestures::Zoom) {
                    float dist1 = distance(ev1, _startTouches.first);
                    float dist2 = distance(ev2, _startTouches.second);
                    
                    //can be swipe, can be zoom... need to check history and track the distance change directions
                    //if points 
                    if(dist1 >= 2 && dist2 >= 2) {
                        Gesture zoom;
                    
                        zoom.type = Gestures::Zoom;
                        zoom.zoom = {
                            .state = GestureState::Start,
                            .xCenter = lerp((ev1.x + ev2.x)/2, _driverWidth, _outputWidth),
                            .yCenter = lerp((ev1.y + ev2.y)/2, _driverHeight, _outputHeight),
                            .horDif = 1.0f,
                            .horAbs = 1.0f,
                            .verDif = 1.0f,
                            .verAbs = 1.0f
                        };
                        _gestureToLost.update(now);
                        _recognizer.pushGesture(zoom);
                        _state = state::ZoomOngoing;
                    }    
                }
                    
            } else if(ev1.type == TouchEvent::Type::Begin && ev1.id != _startTouches.first.id) {
                //possible double tap
            } 
        } break;
        case(state::SwipeOngoing): {
            GestureState toSend;
            if(ev1.type == TouchEvent::Type::Move) {
                toSend = GestureState::Move;
            } else if(ev1.type == TouchEvent::Type::End) {
                toSend = GestureState::End;
            }
            
            float dist1 = distance(ev1, _lastTouches.first);
            float dist2 = distance(ev2, _lastTouches.second);

            if(dist1 >= 5 || dist2 >= 5) {
                int dx1 = 
                lerp(ev1.x, _driverWidth, _outputWidth) -
                lerp(_lastTouches.first.x, _driverHeight, _outputHeight);
                int dy1 = 
                lerp(ev1.y, _driverWidth, _outputWidth) - 
                lerp(_lastTouches.first.y, _driverHeight, _outputHeight);
                
                Gesture swipe;
                swipe.type = Gestures::TwoFingerSwipe;
                swipe.tfSwipe = {
                    .state = toSend,
                    .x = lerp(ev1.x, _driverWidth, _outputWidth),
                    .y = lerp(ev1.y, _driverHeight, _outputHeight),
                    .dx = dx1,
                    .dy = dy1 
                };
                _gestureToLost.update(now);
                _recognizer.pushGesture(swipe);

                if(ev1.type == TouchEvent::Type::End) reset();
            }
        } break;
        case(state::ZoomOngoing): {
            GestureState toSend;
            if(ev1.type == TouchEvent::Type::Move) {
                toSend = GestureState::Move;
            } else if(ev1.type == TouchEvent::Type::End) {
                toSend = GestureState::End;
            }
            
            float dist1 = distance(ev1, _lastTouches.first);
            float dist2 = distance(ev2, _lastTouches.second);

            if(std::abs(dist1) >= 5 || std::abs(dist2) >= 5) {
                float prevDx = _lastTouches.second.x - _lastTouches.first.x;
                float currDx = ev2.x - ev1.x;                
                float prevDy = _lastTouches.second.y - _lastTouches.first.y;
                float currDy = ev2.y - ev1.y;
                
                float dHor = std::abs(prevDx) > 1e-5f
                    ? currDx / prevDx
                    : 1.0f;

                float dVer = std::abs(prevDy) > 1e-5f
                    ? currDy / prevDy
                    : 1.0f;

                prevDx = _startTouches.second.x - _startTouches.first.x;
                prevDy = _startTouches.second.y - _startTouches.first.y;

                float aHor = std::abs(prevDx) > 1e-5f ? currDx / prevDx : 0.0f;
                float aVer = std::abs(prevDy) > 1e-5f ? currDy / prevDy : 0.0f;

                Gesture zoom;
                zoom.type = Gestures::Zoom;
                zoom.zoom = {
                    .state = toSend,    
                    .xCenter = lerp((ev1.x + ev2.x)/2, _driverWidth, _outputWidth),
                    .yCenter = lerp((ev1.y + ev2.y)/2, _driverHeight, _outputHeight),
                    .horDif = dHor, 
                    .horAbs = aHor,   //abs means if zoom stayed at _startEvent than res would be 1.0f
                    .verDif = dVer, 
                    .verAbs = aVer    //abs means if zoom stayed at _startEvent than res would be 1.0f
                };
                _gestureToLost.update(now);
                _recognizer.pushGesture(zoom);

                if(ev1.type == TouchEvent::Type::End) reset();
            }
        } break;
    }

    _lastTouches = std::make_pair(ev1, ev2);
    _eventHistory.push_back(_lastTouches);
    if(_eventHistory.size() >= 35) {
        _eventHistory.pop_front();
    }
}

void TwoFingerFSM::resetOrProcess() {
    auto now = std::chrono::steady_clock::now();    
#if DEBUG_PRINT == 1
    if(_idleActive || _state != state::Idle) printstate(false);
#endif

    switch(_state) {
        case(state::Idle): {
            if(_idleActive) {
                if(std::chrono::duration_cast<std::chrono::milliseconds>(now - _idleToStroke) >= idleThreshold) {
                    _recognizer._state = GestureRecognizer::RecognizerState::TwoFingerOngoing;

                    _state = state::Stroke;

                    int dx = (_lastTouches.first.x + _lastTouches.second.x) / 2;
                    int dy = (_lastTouches.first.y + _lastTouches.second.y) / 2;

#if DEBUG_PRINT == 1
                    std::cerr << "[TFS] ----- td ------" << std::endl;
#endif
                    Gesture t;
                    t.type = Gestures::TouchDown;
                    t.touchDown = {
                        .x = lerp(dx, _driverWidth, _outputWidth),
                        .y = lerp(dy, _driverHeight, _outputHeight),
                        .touchId = -1
                    };
                    _gestureToLost.start(_cfg.gestureToLost, now);
                    _recognizer.pushGesture(t);
                }
            }
        } break;
        case(state::Stroke): {
            if(_lastTouches.first.type == TouchEvent::Type::End) {
                if(_tapTimer.active() && _tapTimer.expired(now)) {
                    //tap?
                    
                    int dx = std::abs(_lastTouches.first.x - _lastTouches.second.x);
                    int dy = std::abs(_lastTouches.first.y - _lastTouches.second.y);

                    Gesture t;
                    t.type = Gestures::TwoFingerTap;
                    t.tfTap = {
                        .x = lerp(dx, _driverWidth, _outputWidth),
                        .y = lerp(dy, _driverHeight, _outputHeight)
                    };
                    _recognizer.pushGesture(t);

                    reset();
                }
            }
        } break;
        case(state::SwipeOngoing): {
            if(_gestureToLost.active() && _gestureToLost.expired(now))
                reset();
        } break;
        case(state::ZoomOngoing): {
            //... smth
            if(_gestureToLost.active() && _gestureToLost.expired(now))
                reset();
        } break;
    }
}

void TwoFingerFSM::reset() {
#if DEBUG_PRINT == 1
    std::cout << "[TFS] reset" << std::endl;
#endif

    if(_state != state::Idle) {
        int dx = (_lastTouches.first.x + _lastTouches.second.x) / 2;
        int dy = (_lastTouches.first.y + _lastTouches.second.y) / 2;

#if DEBUG_PRINT == 1        
        std::cout << "[TFS] ------- tu -------" << std::endl;
#endif
        Gesture t;
        t.type = Gestures::TouchUp;
        t.touchUp = {
            .x = lerp(dx, _driverWidth, _outputWidth),
            .y = lerp(dy, _driverHeight, _outputHeight),
            .touchId = -1
        };
        _recognizer.pushGesture(t);
    }
    _state = state::Idle;
    _idleActive = false;
    _gestureToLost.stop();
    _tapTimer.stop();

    _recognizer.reset();
}

void TwoFingerFSM::printstate(bool proc) {
    std::string text = proc ? "[TFS proc] " : "[TFS rest] ";
    switch(_state) {
        case(state::Idle): {
            // if(_idleActive) text.append("Idle A");
            _idleActive ? text.append("Idle A") : text.append("Idle NA"); 
        } break;
        case(state::Stroke): text.append("Stroke"); break;
        case(state::Tap): text.append("Tap"); break;
        case(state::DoubleTap): text.append("Double Tap"); break;
        case(state::Swipe): text.append("Swipe"); break;
        case(state::SwipeOngoing): text.append("SwipeOngoing"); break;
        case(state::Zoom): text.append("Zoom"); break;
        case(state::ZoomOngoing): text.append("ZoomOngoing"); break;
        case(state::Rotate): text.append("Rotate"); break;
    }
    std::cout << text << std::endl;
}

}
