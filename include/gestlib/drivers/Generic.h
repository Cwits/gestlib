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

#include "gestlib/TouchDriver.h"
#include "gestlib/defines.h"

class libevdev;

namespace GestLib {

struct GenericDriverConfig {
    int maxFingers = 5; 
    int idleThreshold = 250;        //ms
    int activeToLost = 50;          //ms
    int lostToDelete = 60;          //100ms too much, mby even less than 50
    int lostDistance = 50;          //pixels
    int debounceThresholdMax = 100; //ms
    int debounceCount = 4;          //iterations when finger presented
    int noiseFilterThresholdMin = 2; //pixels
    float speedDiv = 20.0f;         //
};

class GenericDriver : public TouchDriver {
    public:
    GenericDriver(const GenericDriverConfig);
    ~GenericDriver();

    bool init(bool pickFirstDev, std::string path = "");
    bool shutdown();

    std::vector<TouchEvent> getEvents() override;
    
    std::vector<DeviceDesc> listDevices() const;
    const std::string & uniq() const { return _name; }
    const std::string & path() const { return _path; }   

    int fd() const override { return _fd; } //ugh... don't like this move

    private:
    GenericDriverConfig _cfg;
    //config related
    int _maxFingers;
    std::chrono::milliseconds _idleThreshold;
    std::chrono::milliseconds _activeToLost;
    std::chrono::milliseconds _lostToDelete;
    int _lostDistance;
    std::chrono::milliseconds _debounceMax;
    int _debounceCount;
    int _noiseFilterThresholdMin;
    float _speedDiv;

    int _fd;
    libevdev * _dev;

    std::string _name;
    std::string _path;

    struct RawFinger {
        int trackId = -1;
        bool active = false;
        int touchMajor = 0;
        int touchMinor = 0;
        int x = 0;
        int y = 0;
        std::chrono::time_point<std::chrono::steady_clock> lastAction;
    };

    enum class FingerState {
        Debouncing,
        Active,
        Lost
    };

    struct DebouncedTouch {
        int logicalId;
        int lastTrackedId;
        FingerState state;

        int x;
        int y;
        int lastX; //better call this as prevN
        int lastY; //better call this as prevN
        int iterBeforeActive;
        bool updatedThisFrame;
        bool markToDelete;
        std::chrono::time_point<std::chrono::steady_clock> lastAction;
        std::chrono::time_point<std::chrono::steady_clock> prevAction;
        std::chrono::time_point<std::chrono::steady_clock> firstAction;

        float speed;
    };

    int _currentSlot;
    std::vector<RawFinger> _raw;
    std::vector<RawFinger> _currentFrameFingers;
    std::vector<DebouncedTouch> _debounced;

    std::vector<DebouncedTouch> _log;
    std::vector<RawFinger> _rlog;
    
    int _internalIdCounter;
    
    struct TouchResolver {
        void prepare(int maxFingers);
        void clear();
        std::vector<DebouncedTouch*> oldTouches;
        std::vector<RawFinger*> newTouches;

        std::vector<std::vector<float>> distanceMatrix;
    };

    TouchResolver _resolver;
    

    bool collectRaw();
    inline void parseRaw(const input_event &ev);
    std::vector<TouchEvent> process(bool newData);
    std::vector<TouchEvent> process1(bool newData);
    inline bool isMoved(const DebouncedTouch &finger); 
};

}