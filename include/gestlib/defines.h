// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <chrono>
#include <cmath>

#define DEBUG_PRINT 0

namespace GestLib {

struct TouchEvent {
    int id;
    int rawid; //TODO: for test use only, remove later
    float speed;
    enum class Type { Begin, Move, End } type;
    int x;
    int y;
    std::chrono::time_point<std::chrono::steady_clock> time;
};

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

struct timer {
    timer() { _active = false; }
    void start(int timeout, const TimePoint &now) {
        _active = true;  
        _timeout = std::chrono::milliseconds(timeout);  
        _startTime = now;
    }
    void stop() {
        _active = false;
    }
    bool active() { return _active; }

    bool expired(const TimePoint &now) {
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - _startTime);
        return diff >= _timeout;
    }

    void update(const TimePoint &now) {
        _startTime = now;
    }
    
    TimePoint when() const {
        return _startTime + _timeout;
    }
    
    private:
    bool _active;
    std::chrono::milliseconds _timeout;
    TimePoint _startTime;
};


inline int lerp(int x, int inMax, int outMax) {
    return x * outMax / inMax;
}

inline float distance(const GestLib::TouchEvent & p1, const GestLib::TouchEvent & p2) {
    return std::hypot(float(p2.x - p1.x), float(p2.y - p1.y));
}

inline float distance(float x0, float y0, float x1, float y1) { 
    return std::hypot(x1 - x0, y1 - y0); 
}

inline float ratio(float cur, float ref) { 
    constexpr float kMinSeparation = 8.f;
    return ref < kMinSeparation ? 1.f : cur / ref; 
}

inline float normalizeAngle(float a) {
    while (a >  M_PI) a -= 2.0f * M_PI;
    while (a < -M_PI) a += 2.0f * M_PI;
    return a;
}

}
