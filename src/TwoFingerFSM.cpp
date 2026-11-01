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

TwoFingerFSM::TwoFingerFSM(GestureRecognizer &rec, TwoFingerFSMConfig cfg) :
    _cfg(cfg),
    _recognizer(rec) 
{
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

std::optional<TimePoint> TwoFingerFSM::nextDeadline() { 
    if(_state == State::TapWait) return _tapDeadline;
    return std::nullopt;
}

void TwoFingerFSM::begin(const std::pair<TouchEvent, TouchEvent> &a, const std::pair<TouchEvent, TouchEvent> &b, const TimePoint &now) {
    const int cx = (a.first.x + b.first.x) / 2;
    const int cy = (a.first.y + b.first.y) / 2;

    bool secondTap = false;
    if(_state == State::TapWait) {
        secondTap = now < _tapDeadline && distance(float(cx), float(cy), float(_tapX), float(_tapY)) <= _cfg.doubleTapRadius;
        if(!secondTap) emitTap();
    } else if(_state != State::Idle) {
        reset();
    }

    const std::pair<TouchEvent, TouchEvent>* t[2] = {&a, &b};
    if(t[0]->second.id > t[1]->second.id) std::swap(t[0], t[1]);
    for(int i = 0; i < 2; ++i) {
        _f[i] = {
            t[i]->second.id, 
            t[i]->first.x, 
            t[i]->first.y,
            t[i]->second.x,  
            t[i]->second.y,
            t[i]->first.x, 
            t[i]->first.y
        };
    }
    _tapX = cx; 
    _tapY = cy;
    _downTime = now;
    _state = secondTap ? State::DoubleTapPressed : State::Pressed;
}

void TwoFingerFSM::onEvent(const TouchEvent &e, const TimePoint &now) {
    Finger* f = nullptr;
    for(auto& c : _f) { 
        if(c.id == e.id) 
            f = &c; 
    }
    
    if(!f || _state == State::Idle || _state == State::TapWait) return;

    f->x = e.x;
    f->y = e.y;
    if(e.type != TouchEvent::Type::End) return; //handle move in endFrame()

    switch(_state) {
        case(State::Pressed): {
            if(now - _downTime <= std::chrono::milliseconds(_cfg.tapTimeout)) {
                if(_cfg.doubleTapEnabled) {
                    _tapDeadline = now + std::chrono::milliseconds(_cfg.doubleTapTimeout);
                    _state = State::TapWait;
                } else {
                    emitTap();
                    _state = State::Idle;
                }
            } else {
                _state = State::Idle;
            }
        } break;
        case(State::DoubleTapPressed): {
            if(now - _downTime <= std::chrono::milliseconds(_cfg.tapTimeout)) 
                emitDoubleTap();
            _state = State::Idle;
        } break;
        case(State::Swipe): 
        case(State::Zoom): 
        case(State::Rotate): {
            finish();
        } break;
        default:
            _state = State::Idle;
        break;
    }
}

void TwoFingerFSM::endFrame() {
    switch(_state) {
        case(State::Pressed): {
            tryClassify();
        } break;
        case(State::Swipe): 
        case(State::Zoom): 
        case(State::Rotate): {
            if (fingersMoved(_cfg.moveStep)) 
                emit(GestureState::Move);
        } break;
        case(State::DoubleTapPressed): {
            const Metrics m = measure();
            if(std::max(m.travel, m.rel) >= _cfg.decideDistance) 
                _state = State::Swallow;
        } break;
        default: break;
    }
}

void TwoFingerFSM::tick(const TimePoint &now) {
   if (_state == State::TapWait && now >= _tapDeadline) {
        emitTap();
        _state = State::Idle;
    }
}
//   travel     -> Swipe
//   radial / 2  -> Zoom
//   tangential *len/2 -> Rotate
TwoFingerFSM::Metrics TwoFingerFSM::measure() const {
    const Finger &a = _f[0], &b = _f[1];
    const float cx = float((a.x - a.sx) + (b.x - b.sx)) / 2.f;
    const float cy = float((a.y - a.sy) + (b.y - b.sy)) / 2.f;
    const float v0x = float(b.sx - a.sx);
    const float v0y = float(b.sy - a.sy);
    const float dvx = float(b.x - a.x) - v0x; 
    const float dvy = float(b.y - a.y) - v0y;
    const float len0 = std::hypot(v0x, v0y);
    const float ux = len0 > 1e-3f ? v0x / len0 : 1.f;
    const float uy = len0 > 1e-3f ? v0y / len0 : 0.f;

    Metrics m;
    m.travel     = std::hypot(cx, cy);
    m.rel        = std::hypot(dvx, dvy) / 2.f;
    m.radial     = (dvx * ux + dvy * uy) / 2.f;
    m.tangential = (ux * dvy - uy * dvx) / 2.f;
    return m;
}


void TwoFingerFSM::tryClassify() {
    const Metrics m = measure();
    if(std::max(m.travel, m.rel) < _cfg.decideDistance) return;

    State s;
    if(m.travel > _cfg.swipeRatio * m.rel) {
        s = State::Swipe;
    } else if(_cfg.rotateEnabled && std::abs(m.tangential) > _cfg.rotateRatio * std::abs(m.radial)) {
        s = State::Rotate;
    } else {
        s = State::Zoom;
    }

    if(s == State::Swipe && !_cfg.swipeEnabled) { 
        _state = State::Swallow; 
        return; 
    }
    
    _state = s;

    int cur[4] = {
        _f[0].x, 
        _f[0].y, 
        _f[1].x,
        _f[1].y
    };
    
    for(auto& f : _f) { 
        f.x = f.rx = f.sx; 
        f.y = f.ry = f.sy; 
    }

    _lastAngle  = std::atan2(float(_f[1].y - _f[0].y), float(_f[1].x - _f[0].x));
    _totalAngle = 0.f;
    emit(GestureState::Start);
    _f[0].x = cur[0]; 
    _f[0].y = cur[1]; 
    _f[1].x = cur[2]; 
    _f[1].y = cur[3];
    emit(GestureState::Move);
}

bool TwoFingerFSM::fingersMoved(int step) const {
    for(const auto& f : _f) {
        const int dx = f.x - f.rx;
        const int dy = f.y - f.ry;
        if((dx || dy) && dx * dx + dy * dy >= step * step) return true;
    }
    return false;
}

void TwoFingerFSM::emit(GestureState gs) {
    const Finger &a = _f[0], &b = _f[1];
    const int cx  = (a.x + b.x) / 2,   cy  = (a.y + b.y) / 2;
    const int rcx = (a.rx + b.rx) / 2, rcy = (a.ry + b.ry) / 2;

    int lcx = lerp(cx, _driverWidth, _outputWidth);
    int lcy = lerp(cy, _driverWidth, _outputWidth);
    Gesture g;
    switch (_state) {
        case(State::Swipe): {
            g.type = Gestures::TwoFingerSwipe;
            g.tfSwipe.state = gs;
            g.tfSwipe.x = lcx;
            g.tfSwipe.y = lcy;
            g.tfSwipe.dx = lerp(cx, _driverWidth, _outputWidth) - 
                            lerp(rcx, _driverWidth, _outputWidth);
            g.tfSwipe.dy = lerp(cy, _driverWidth, _outputWidth) - 
                            lerp(rcy, _driverWidth, _outputWidth);
        } break;
        case(State::Zoom): {
            const float curDx   = std::abs(float(b.x - a.x)),   curDy   = std::abs(float(b.y - a.y));
            const float prevDx  = std::abs(float(b.rx - a.rx)), prevDy  = std::abs(float(b.ry - a.ry));
            const float startDx = std::abs(float(b.sx - a.sx)), startDy = std::abs(float(b.sy - a.sy));
            g.type = Gestures::Zoom;
            g.zoom.state = gs;
            g.zoom.xCenter = lcx;
            g.zoom.yCenter = lcy;
            g.zoom.horDif = ratio(curDx, prevDx);
            g.zoom.horAbs = ratio(curDx, startDx);
            g.zoom.verDif = ratio(curDy, prevDy);
            g.zoom.verAbs = ratio(curDy, startDy);
        } break;
        case(State::Rotate): {
            const float ang   = std::atan2(float(b.y - a.y), float(b.x - a.x));
            const float delta = normalizeAngle(ang - _lastAngle);
            _lastAngle = ang;
            _totalAngle += delta;
            g.type = Gestures::Rotate;
            g.rotate.state = gs;
            g.rotate.xCenter = lcx;
            g.rotate.yCenter = lcy;
            g.rotate.angle = _totalAngle;
            g.rotate.deltaAngle = delta;
        } break;
        default: 
            return; 
        break;
    }
    // _sink(g);
    _recognizer.pushGesture(g);
    for(auto& f : _f) { 
        f.rx = f.x; 
        f.ry = f.y; 
    }
}

void TwoFingerFSM::finish() {
    if (fingersMoved(0)) emit(GestureState::Move);
    emit(GestureState::End);
    _state = State::Idle;
}

void TwoFingerFSM::emitTap() {
    Gesture g;
    g.type = Gestures::TwoFingerTap;
    g.tfTap.x = lerp(_tapX, _driverWidth, _outputWidth);
    g.tfTap.y = lerp(_tapY, _driverWidth, _outputWidth);
    _recognizer.pushGesture(g);
}

void TwoFingerFSM::emitDoubleTap() {
    Gesture g;
    g.type = Gestures::TwoFingerDoubleTap;
    g.tfDoubleTap.x = lerp(_tapX, _driverWidth, _outputWidth);
    g.tfDoubleTap.y = lerp(_tapY, _driverWidth, _outputWidth);
    _recognizer.pushGesture(g);
}

void TwoFingerFSM::reset() {
    switch(_state) {
        case(State::Swipe): 
        case(State::Zoom): 
        case(State::Rotate): 
            finish(); 
        break;
        default: break;
    }
    _state = State::Idle;
}

void TwoFingerFSM::printstate(bool proc) {
    std::string text = proc ? "[TFS proc] " : "[TFS rest] ";
    // switch(_state) {
    //     case(State::Idle): {
    //         // if(_idleActive) text.append("Idle A");
    //         _idleActive ? text.append("Idle A") : text.append("Idle NA"); 
    //     } break;
    //     case(State::Stroke): text.append("Stroke"); break;
    //     case(State::Tap): text.append("Tap"); break;
    //     case(State::DoubleTap): text.append("Double Tap"); break;
    //     case(State::Swipe): text.append("Swipe"); break;
    //     case(State::SwipeOngoing): text.append("SwipeOngoing"); break;
    //     case(State::Zoom): text.append("Zoom"); break;
    //     case(State::ZoomOngoing): text.append("ZoomOngoing"); break;
    //     case(State::Rotate): text.append("Rotate"); break;
    // }
    std::cout << text << std::endl;
}

}
