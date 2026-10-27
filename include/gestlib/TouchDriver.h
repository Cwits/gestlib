// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include <string>
#include <queue>
#include <vector>
#include <deque>
#include <chrono>
#include <linux/input.h>

#include <fstream>
#include "defines.h"

namespace GestLib {

class TouchDriver {
    public:
    TouchDriver();
    ~TouchDriver();

    //bool init(bool pickFirstDev, std::string path = "");
    // virtual bool shutdown() = 0;

    virtual std::vector<TouchEvent> getEvents() = 0;

    int width() const { return _width; }
    int height() const { return _height; }   
    
    virtual int fd() const = 0; //ugh... don't like this move

    protected:
    int _width;
    int _height;
};

}