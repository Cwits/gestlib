// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include <chrono>
namespace GestLib {

struct TouchEvent {
    int id;
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

enum class DriverType {
    Real, 
    Fake
};

struct Vec2i {
    int x;
    int y;
};

}