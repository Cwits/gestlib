// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include <string>
#include <queue>
#include <vector>
#include <deque>
#include <chrono>

#include <fstream>
#include "defines.h"

class libevdev;

namespace GestLib {

class TouchDriver {
    public:
    TouchDriver(int maxFingers = 5);
    ~TouchDriver();

    bool init();
    bool shutdown();
    void printCapabilities();

    std::vector<TouchEvent> getEvents();

    const std::string & uniq() const { return _uniq; }
    const std::string & path() const { return _path; }
    const int resolutionX() const { return _resX; }
    const int resolutionY() const { return _resY; }   

    int fd() const { return _fd; } //ugh... don't like this move

    private:

    //maximal possible simultanious finger touches
    int _maxFingers;

    //touchscreen things
    int _fd;
    libevdev * _dev;

    std::string _uniq;
    std::string _path;
    int _resX;
    int _resY;

    struct RawFinger {
        int trackId = -1;
        bool active = false;
        int touchMajor = 0;
        int touchMinor = 0;
        int x = 0;
        int y = 0;
        std::chrono::time_point<std::chrono::steady_clock> lastAction;
    };

    int _currentSlot;
    std::vector<RawFinger> _raw;

    enum class FingerState {
        Probation,
        Active,
        Lost
    };

    struct LogicalFinger {
        int logicalId;
        int lastTrackedId;
        FingerState state;

        int x;
        int y;
        int lastX;
        int lastY;
        int activeCount;
        bool updatedThisFrame;
        bool markToDelete;
        std::chrono::time_point<std::chrono::steady_clock> lastAction;
    };

    std::vector<LogicalFinger> _logicalFingers;
    std::vector<RawFinger> _currentFrameFingers;
    int _internalIdCounter;

    bool readData();
    std::vector<TouchEvent> process(bool newData);
    void probablyMove(LogicalFinger & finger, std::vector<TouchEvent> & ret);
};

}