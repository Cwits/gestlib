// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <chrono>
#include <string>
#include <cmath>
#include <algorithm>
#include <deque>

#define DEBUG_PRINT 1

namespace GestLib {

struct TouchEvent {
    int id;
    int rawid; //TODO: for test use only, remove later
    float speed;
    enum class Type { Begin, Move, End } type;
    int x;
    int y;
    std::chrono::time_point<std::chrono::steady_clock> time;
    
    // TouchEvent operator-(const TouchEvent& other) const {
    //     return {x - other.x, y - other.y};
    // }

    
    bool operator==(TouchEvent & other) {
        if(other.id == id && 
            other.x == x &&
            other.y == y &&
            other.type == type) {
                return true;
            } else {
                return false;
            }
    }
    TouchEvent & operator=(const TouchEvent & other) {
        id = other.id;
        x = other.x;
        y = other.y;
        type = other.type;
        // time = other.time;
        return *this;
    }
};

struct DeviceDesc {
    std::string path;
    std::string device;
    std::string vendorId;
    std::string productId;
    std::string bustype;
    std::string name;
    std::string driver;
    std::string driverfw;

    int mtslots;
    int minWidth;
    int maxWidth;
    int widthResolution;
    int minHeight;
    int maxHeight;
    int heightResolution;


    void print();
};


enum class DriverType {
    Real, 
    Fake
};

struct Vec2i {
    int x;
    int y;
};

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

struct timer {
    timer() { _active = false; }
    void start(int timeout, TimePoint & now) {
        _active = true;  
        _timeout = std::chrono::milliseconds(timeout);  
        _startTime = now;
    }
    void stop() {
        _active = false;
    }
    bool active() { return _active; }

    bool expired(TimePoint & now) {
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - _startTime);
        return diff >= _timeout;
    }

    void update(TimePoint &now) {
        _startTime = now;
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
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    return std::sqrt(dx * dx + dy * dy);
}

inline float dotProduct(const GestLib::TouchEvent & a, const GestLib::TouchEvent & b) {
    return a.x * b.x + a.y * b.y;
}

inline float getAngle(float x, float y, float cx, float cy) {
    return std::atan2(y - cy, x - cx); // ∈ [-π, π]
}

inline float pointToLineDistance(float x1, float y1, float x2, float y2, float px, float py) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float lenSq = dx*dx + dy*dy;
    float t = ((px - x1) * dx + (py - y1) * dy) / (lenSq + 1e-6);
    t = std::clamp(t, 0.f, 1.f);
    float projX = x1 + t * dx;
    float projY = y1 + t * dy;
    float distX = px - projX;
    float distY = py - projY;
    return std::sqrt(distX*distX + distY*distY);
}

inline float computeTurnAngleSum(const std::deque<GestLib::TouchEvent>& history) {
    std::size_t size = history.size();
    if (size < 10+2) return 0.0f;

    float totalAngle = 0.0f;
    //let's use some window of last 10 events
    for (size_t i = size-10; i < size; ++i) {
        float dx1 = history[i - 1].x - history[i - 2].x;
        float dy1 = history[i - 1].y - history[i - 2].y;
        float dx2 = history[i].x - history[i - 1].x;
        float dy2 = history[i].y - history[i - 1].y;

        float len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
        float len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

        if (len1 < 1e-3f || len2 < 1e-3f)
            continue;

        float dot = (dx1 * dx2 + dy1 * dy2) / (len1 * len2);
        dot = std::clamp(dot, -1.0f, 1.0f);  

        float angle = std::acos(dot);
        totalAngle += std::abs(angle);
    }

    return totalAngle;
}

inline float normalize_angle(float angle) {
    while (angle <= -M_PI) angle += 2.0f * M_PI;
    while (angle >  M_PI)  angle -= 2.0f * M_PI;
    return angle;
}



}