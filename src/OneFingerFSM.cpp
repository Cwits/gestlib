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

OneFingerFSM::OneFingerFSM(GestureRecognizer & rec, OneFingerFSMConfig cfg) : 
    _cfg(cfg), 
    _recognizer(rec) 
{
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

std::optional<TimePoint> OneFingerFSM::nextDeadline() {
    if(_state == state::Stroke) return _holdDeadline.when(); //_lastEventTime + std::chrono::milliseconds(_cfg.holdThreshold);
    if(_state == state::WaitingTap) return _tapDeadline.when(); //_lastEventTime + std::chrono::milliseconds(_cfg.tapTimeout);
    if(_state == state::WaitingDoubleTap) return _doubleTapDeadline.when();
    return std::nullopt;
}

bool OneFingerFSM::lockedOnGesture(const std::chrono::milliseconds timeout, const TimePoint &now) {
    return (_state != state::Stroke && (now - _startEventTime > timeout));
}
    
void OneFingerFSM::onEvent(const TouchEvent &ev, const TimePoint &now) {
    // std::cout << "On Event State ";
    // printState(_state, false);
    switch(_state) {
        case(state::Idle): {
            if(ev.type == TouchEvent::Type::Begin) {
                _holdDeadline.start(_cfg.holdThreshold, now);
                _state = state::Stroke;
                _startEvent = ev;
                _startEventTime = now;
            }
        } break;
        case(state::Stroke): {
            //assuming that we cant get here with ev.id != _startEvent.id
            if(ev.id != _startEvent.id) {
                std::cerr << "New event id not equal to captured start event id, resetting" << std::endl;
                reset();
            }
            if(ev.type == TouchEvent::Type::Move) {
                //swipe or drag
                bool earlyForDrag = (now - _lastEventTime) < std::chrono::milliseconds(_cfg.dragThreshold);
                float dist = distance(ev, _lastEvent);
            
                if(dist < _cfg.minMoveThreshold) break;

                if(earlyForDrag) {
                    //swipe
                    _state = state::SwipeOngoing;
                    emitStart(Gestures::Swipe, ev.x, ev.y);
                } else if(!earlyForDrag) {
                    //drag
                    _state = state::DragOngoing;
                    emitStart(Gestures::Drag, ev.x, ev.y);
                }
            } else if(ev.type == TouchEvent::Type::End) {
                if(_cfg.doubleTapEnabled) {
                    _tapDeadline.start(_cfg.tapTimeout, now);
                    _state = state::WaitingTap;
                } else {
                    emitStart(Gestures::Tap, ev.x, ev.y);
                    reset();
                }
            }
        } break;
        case(state::WaitingTap): {
            if(ev.type == TouchEvent::Type::Begin && _lastEvent.id != ev.id) {
                _tapDeadline.stop();
                _doubleTapDeadline.start(_cfg.doubleTapTimeout, now);
                _state = state::WaitingDoubleTap;
            }
        } break;
        case(state::WaitingDoubleTap): {
            if(ev.type == TouchEvent::Type::Move) {
                //double tap drag...
                _doubleTapDeadline.stop();
                emitStart(Gestures::DoubleTapSwipe, ev.x, ev.y);
                _state = state::DoubleTapSwipeOngoing;
            } else if(ev.type == TouchEvent::Type::End) {
                emitStart(Gestures::DoubleTap, ev.x, ev.y);
                reset();
            }
        } break;
        case(state::SwipeOngoing):
        case(state::DragOngoing): 
        case(state::HoldOngoing): 
        case(state::DoubleTapSwipeOngoing): {
            //can we get here with ev.id != _startEvent.id???
            Gestures g;
            if(_state == state::SwipeOngoing) g = Gestures::Swipe;
            else if(_state == state::DragOngoing) g = Gestures::Drag;
            else if(_state == state::HoldOngoing) g = Gestures::Hold;
            else if(_state == state::DoubleTapSwipeOngoing) g = Gestures::DoubleTapSwipe;

            if(ev.type == TouchEvent::Type::Move) {
                emitMove(g, ev.x, ev.y, _lastEvent.x - ev.x, _lastEvent.y - ev.y);
            } else if(ev.type == TouchEvent::Type::End) {
                // emitEnd(g, ev.x, ev.y, _lastEvent.x - ev.x, _lastEvent.y - ev.y); -> end at reset()
                reset();
            }
        } break;
        default: {
            std::cerr << "smth wrong" << std::endl;
            reset();
        } break;
    }
    _lastEvent = ev;
    _lastEventTime = now;
}

void OneFingerFSM::tick(const TimePoint &now) {
    // printState(_state, false);
    switch(_state) {
        case(state::Idle): break;
        case(state::Stroke): {
            if(_holdDeadline.active() && _holdDeadline.expired(now)) {
                //hold action
                _state = state::HoldOngoing;
                emitStart(Gestures::Hold, _lastEvent.x, _lastEvent.y);
                _holdDeadline.stop();
            }
        } break;
        case(state::WaitingTap): {
            if(_lastEvent.id == _startEvent.id) {
                if(_tapDeadline.active() && _tapDeadline.expired(now)) {
                    emitStart(Gestures::Tap, _lastEvent.x, _lastEvent.y);
                    reset();
                }
            }
        } break;
        case(state::WaitingDoubleTap): {
            //can add drag or smth??
            // if(_lastEvent.id != _startEvent.id) {
            //     if(_doubleTapDeadline.active() && _doubleTapDeadline.expired(now)) {
            //         emitStart(Gestures::DoubleTap, _lastEvent.x, _lastEvent.y);
            //         reset();
            //     }
            // }
        } break;
        case(state::SwipeOngoing):
        case(state::DragOngoing): 
        case(state::HoldOngoing): 
        case(state::DoubleTapSwipeOngoing): {
            // std::cerr << "Shouldn't be here?" << std::endl;
            // reset();
        } break;
        default: {
            std::cerr << "smth wrong" << std::endl;
            reset();
        } break;
    }
}


void OneFingerFSM::reset() {
    // std::cout << "Reset state "; printState(_state, false);
    _holdDeadline.stop();
    _doubleTapDeadline.stop();
    if(_state == state::SwipeOngoing ||
        _state == state::DragOngoing ||
        _state == state::HoldOngoing ||
        _state == state::DoubleTapSwipeOngoing)
    {
        Gestures g;
        if(_state == state::SwipeOngoing) g = Gestures::Swipe;
        else if(_state == state::DragOngoing) g = Gestures::Drag;
        else if(_state == state::HoldOngoing) g = Gestures::Hold;
        else if(_state == state::DoubleTapSwipeOngoing) g = Gestures::DoubleTapSwipe;
        emitEnd(g, _lastEvent.x, _lastEvent.y, 0, 0);
    }
    _state = state::Idle;
}

inline void OneFingerFSM::emitStart(Gestures g, int x, int y) {
    Gesture ret;
    const int cx = lerp(x, _driverWidth, _outputWidth);
    const int cy = lerp(y, _driverWidth, _outputWidth);
    switch(g) {
        case(Gestures::Tap): {
            ret.type = Gestures::Tap; 
            ret.tap.x = cx;
            ret.tap.y = cy;
        } break;
        case(Gestures::Hold): {
            ret.type = Gestures::Hold;
            ret.hold.state = GestureState::Start;
            ret.hold.x = cx;
            ret.hold.y = cy;
            ret.hold.dx = 0;
            ret.hold.dy = 0;
        } break;
        case(Gestures::Swipe): {
            ret.type = Gestures::Swipe;
            ret.swipe.state = GestureState::Start;
            ret.swipe.x = cx;
            ret.swipe.y = cy;
            ret.swipe.dx = 0;
            ret.swipe.dy = 0;
        } break;
        case(Gestures::Drag): {
            ret.type = Gestures::Drag;
            ret.drag.state = GestureState::Start;
            ret.drag.x = cx;
            ret.drag.y = cy;
            ret.drag.dx = 0;
            ret.drag.dy = 0;
        } break;
        case(Gestures::DoubleTap): {
            ret.type = Gestures::DoubleTap; 
            ret.doubleTap.x = cx;
            ret.doubleTap.y = cy;
        } break;
        case(Gestures::DoubleTapSwipe): {
            ret.type = Gestures::DoubleTapSwipe;
            ret.dtSwipe.state = GestureState::Start;
            ret.dtSwipe.x = cx;
            ret.dtSwipe.y = cy;
            ret.dtSwipe.dx = 0;
            ret.dtSwipe.dy = 0;
        } break;
        default: {
            std::string text = gestureToText(g);
            std::cerr << "Gesture " << text << " unsupported by OFS" << std::endl;
            return;
        } break;
    }
    _recognizer.pushGesture(ret);
}
inline void OneFingerFSM::emitMove(Gestures g, int x, int y, int dx, int dy) {
    Gesture ret;
    const int cx = lerp(x, _driverWidth, _outputWidth);
    const int cy = lerp(y, _driverWidth, _outputWidth);
    const int cdx = lerp(dx, _driverWidth, _outputWidth);
    const int cdy = lerp(dy, _driverWidth, _outputWidth);
    switch(g) {
        case(Gestures::Hold): {
            ret.type = Gestures::Hold;
            ret.hold.state = GestureState::Move;
            ret.hold.x = cx;
            ret.hold.y = cy;
            ret.hold.dx = cdx;
            ret.hold.dy = cdy;
        } break;
        case(Gestures::Swipe): {
            ret.type = Gestures::Swipe;
            ret.swipe.state = GestureState::Move;
            ret.swipe.x = cx;
            ret.swipe.y = cy;
            ret.swipe.dx = cdx;
            ret.swipe.dy = cdy;
        } break;
        case(Gestures::Drag): {
            ret.type = Gestures::Drag;
            ret.drag.state = GestureState::Move;
            ret.drag.x = cx;
            ret.drag.y = cy;
            ret.drag.dx = cdx;
            ret.drag.dy = cdy;
        } break;
        case(Gestures::DoubleTapSwipe): {
            ret.type = Gestures::DoubleTapSwipe;
            ret.dtSwipe.state = GestureState::Move;
            ret.dtSwipe.x = cx;
            ret.dtSwipe.y = cy;
            ret.dtSwipe.dx = cdx;
            ret.dtSwipe.dy = cdy;
        } break;
        default: {
            std::string text = gestureToText(g);
            std::cerr << "Gesture " << text << " unsupported by OFS or can't produce move event" << std::endl;
            return;
        } break;
    }
    _recognizer.pushGesture(ret);
}

inline void OneFingerFSM::emitEnd(Gestures g, int x, int y, int dx, int dy) {
    Gesture ret;
    const int cx = lerp(x, _driverWidth, _outputWidth);
    const int cy = lerp(y, _driverWidth, _outputWidth);
    const int cdx = lerp(dx, _driverWidth, _outputWidth);
    const int cdy = lerp(dy, _driverWidth, _outputWidth);
    switch(g) {
        case(Gestures::Hold): {
            ret.type = Gestures::Hold;
            ret.hold.state = GestureState::End;
            ret.hold.x = cx;
            ret.hold.y = cy;
            ret.hold.dx = cdx;
            ret.hold.dy = cdy;
        } break;
        case(Gestures::Swipe): {
            ret.type = Gestures::Swipe;
            ret.swipe.state = GestureState::End;
            ret.swipe.x = cx;
            ret.swipe.y = cy;
            ret.swipe.dx = cdx;
            ret.swipe.dy = cdy;
        } break;
        case(Gestures::Drag): {
            ret.type = Gestures::Drag;
            ret.drag.state = GestureState::End;
            ret.drag.x = cx;
            ret.drag.y = cy;
            ret.drag.dx = cdx;
            ret.drag.dy = cdy;
        } break;
        case(Gestures::DoubleTapSwipe): {
            ret.type = Gestures::DoubleTapSwipe;
            ret.dtSwipe.state = GestureState::End;
            ret.dtSwipe.x = cx;
            ret.dtSwipe.y = cy;
            ret.dtSwipe.dx = cdx;
            ret.dtSwipe.dy = cdy;
        } break;
        default: {
            std::string text = gestureToText(g);
            std::cerr << "Gesture " << text << " unsupported by OFS or can't produce move event" << std::endl;
            return;
        } break;
    }
    _recognizer.pushGesture(ret);
}

void OneFingerFSM::printState(state state, bool proc) {
    std::string text = proc ? "[OFS proc] " : "[OFS rest] ";
    switch(state) {
        case(Idle): text.append("Idle"); break;
        case(Stroke): text.append("Stroke"); break;
        case(DragOngoing): text.append("DragOngoing"); break;
        case(WaitingTap): text.append("WaitingTap"); break;
        case(WaitingDoubleTap): text.append("WaitingDoubleTap"); break;
        case(DoubleTapSwipeOngoing): text.append("DoubleTapSwipeOngoing"); break;
        case(SwipeOngoing): text.append("SwipeOngoing"); break;
        case(HoldOngoing): text.append("HoldOngoing"); break;
    }
    std::cout << text << std::endl;
}

}//namespace GestLib