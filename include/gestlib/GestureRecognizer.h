// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include "defines.h"
#include "Gestures.h"
#include "TouchDriver.h"
#include "OneFingerFSM.h"

#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

namespace GestLib {

class GestureRecognizer {
    public:
    GestureRecognizer();
    ~GestureRecognizer();

    bool init();
    void windowSize(int x, int y) { _ofs.expectedResolution(x, y); }
    bool start();
    bool shutdown();

    std::vector<Gesture> fetchGestures();

    private:
    TouchDriver _dri;
    OneFingerFSM _ofs;

    std::mutex _lock;
    std::thread _recognizer;

    std::atomic<bool> _running;
    std::vector<Gesture> _gesturesQueue;

    std::chrono::time_point<std::chrono::steady_clock> _lastAction;
    int _efd = -1;

    void pushGesture(Gesture gest);
    friend class OneFingerFSM;
};

}