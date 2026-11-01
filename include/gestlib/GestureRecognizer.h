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
#include <unordered_map>
#include <unordered_set>

namespace GestLib {

struct RecognizerConfig {
    int idleTimeout     = 300; //in ms
    int outputWidth     = 1920;
    int outputHeight    = 1080;
    int gestureToLockMs   = 150; //ms to lock on gesture

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

    /*
    void registerTouchCapture(TouchCapture *, int priority = 0);
    void unregisterTouchCapture(TouchCapture *);
    */
    
    private:
    GestLib::RecognizerConfig _cfg;

    enum class RecognizerState {
        Idle,
        OneFingerOngoing,
        TwoFingerOngoing
    };

    RecognizerState _state = RecognizerState::Idle;
    bool _sessionOpened = false;
    int _sessionId = -1;

    void reset();
    void cancelAll();
    void refresh();

    int nextPollTimeout(const TimePoint &);

    TouchDriver *_dri;
    OneFingerFSM _ofs;
    TwoFingerFSM _tfs;

    std::mutex _lock;
    std::vector<Gesture> _gesturesQueue;

    int _efd = -1;
    std::thread _recognizer;
    std::atomic<bool> _running;

    std::chrono::time_point<std::chrono::steady_clock> _lastAction;

    TouchEvent _lastEvent;
    std::unordered_map<int, std::pair<TouchEvent, TouchEvent>> _active;
    std::unordered_set<int> _ignored;

    void pushGesture(Gesture gest);
    friend class OneFingerFSM;
    friend class TwoFingerFSM;
/*
    struct CapturedTouch {
        TouchCapture* capture = nullptr;
        int priority = 0;
        std::mutex callMutex;
        std::atomic<bool> removed{false};
        int active = 0;
    };
    using CapturedPtr  = std::shared_ptr<CapturedTouch>;
    using CapturedList = std::vector<CapturedPtr>;

    std::unordered_map<int, CapturedPtr> _captured;
    std::mutex _captureLock;
    std::shared_ptr<const CapturedList> _captures = std::make_shared<const CapturedList>();

    bool tryCapture(const TouchEvent& e);
    void feedCaptured(int id, const TouchEvent& e);
    void releaseCaptures();
*/
};

}