// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include "defines.h"
#include "Gestures.h"
#include "TouchDriver.h"
#include "OneFingerFSM.h"
#include "TwoFingerFSM.h"

#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

namespace GestLib {

struct RecognizerConfig {
    int idleTimeout = 300; //in ms
    int outputWidth = 1920;
    int outputHeight = 1080;

    OneFingerFSMConfig oneCfg;
    TwoFingerFSMConfig twoCfg;
};

class GestureRecognizer {
    public:
    GestureRecognizer(GestLib::RecognizerConfig);
    ~GestureRecognizer();

    bool init(TouchDriver *dri);
    bool start();
    bool shutdown();

    std::vector<Gesture> fetchGestures();

    // void registerTouchCapture(TouchCapture *tc, int touchId); //must be threadsafe
    // void unregisterTouchCapture(TouchCapture *tc, int touchId); //must be threadsafe

    private:
    GestLib::RecognizerConfig _cfg;

    enum class RecognizerState {
        Idle,
        OneFingerOngoing,
        TwoFingerOngoing
    };

    RecognizerState _state;
    void reset() { _state = RecognizerState::Idle; }

    TouchDriver *_dri;
    OneFingerFSM _ofs;
    TwoFingerFSM _tfs;

    std::mutex _lock;
    std::thread _recognizer;

    std::atomic<bool> _running;
    std::vector<Gesture> _gesturesQueue;

    std::chrono::time_point<std::chrono::steady_clock> _lastAction;
    int _efd = -1;

    void pushGesture(Gesture gest);
    friend class OneFingerFSM;
    friend class TwoFingerFSM;


    // struct CapturedTouch {
    //     TouchCapture *tc;
    //     int touchId;
    // };
    // std::vector<CapturedTouch> _captured;
};

}