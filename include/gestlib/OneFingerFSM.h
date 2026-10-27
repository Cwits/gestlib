// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include "defines.h"
#include <vector>
#include <deque>
#include <chrono>

namespace GestLib {

class GestureRecognizer;

struct OneFingerFSMConfig {
    int tapTimeout          = 300;  //ms can do 200-250?
    int dragThreshold       = 200;  //ms
    int holdThreshold       = 800;  //ms
    int doubleTapTimeout    = 200;  //ms
    int maxHistoryLength    = 35;   //element count
    int gestureToLost       = 60;   //ms
};

class OneFingerFSM {
    public:
    OneFingerFSM(GestureRecognizer & rec, OneFingerFSMConfig cfg);

    bool init(int driverWidth, int driverHeight);
    void outputResolution(int w, int h);

    void process(std::vector<TouchEvent> & touches);
    void resetOrProcess();
    void reset();

    private:
    OneFingerFSMConfig _cfg;
    int _driverWidth;
    int _driverHeight;
    int _outputWidth;
    int _outputHeight;

    //cfg related
    int _tapTimeout;
    int _holdThreshold;
    int _dragThreshold;
    int _doubleTapTimeout;
    int _maxHistoryLength;

    enum state {
        Idle,
        Stroke,
        DragOrHold,
        DragOngoing,
        DoubleTapPossible,
        DoubleTapSwipeOngoing,
        DoubleTapCircularOngoing,
        SwipeOngoing,
        Hold,
        HoldOngoing
    };
    state _state = Idle;
    TouchEvent _lastEvent;
    TouchEvent _startEvent;

    TimePoint _lastEventTime; //if now - last event time < some threshold(like 800-1000ms) than recover last event?

    enum gestureMove {
        Undetected,
        Linear,
        Circular
    };
    
    gestureMove _doubleTapHelper;
    int _predictions;
    std::deque<TouchEvent> _eventsHistory;

    timer _tapTimer;
    timer _doubleTapTimer;
    timer _dragOrHoldTimer;
    timer _holdTimer;
    timer _gestureToLost;
    
    TimePoint _idleToStroke;
    bool _idleActive;

    GestureRecognizer & _recognizer;

    void printState(state state, bool proc);
};

}