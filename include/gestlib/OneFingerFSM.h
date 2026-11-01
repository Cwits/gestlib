// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include "defines.h"
#include "Gestures.h"
#include <vector>
#include <deque>
#include <chrono>
#include <optional>

namespace GestLib {

class GestureRecognizer;

struct OneFingerFSMConfig {
    bool doubleTapEnabled   = true;
    int tapTimeout          = 200;  //ms can do 200-250?
    int dragThreshold       = 200;  //ms
    int holdThreshold       = 800;  //ms
    int doubleTapTimeout    = 200;  //ms
    int maxHistoryLength    = 35;   //element count
    int gestureToLost       = 60;   //ms
    int minMoveThreshold    = 2;    //in some... units... pixels?
};

class OneFingerFSM {
    public:
    OneFingerFSM(GestureRecognizer & rec, OneFingerFSMConfig cfg);

    bool init(int driverWidth, int driverHeight);
    void outputResolution(int w, int h);

    void onEvent(const TouchEvent &, const TimePoint &);
    void tick(const TimePoint &);
    void reset();
    
    std::optional<TimePoint> nextDeadline(); 
    bool busy() const { return _state != state::Idle; }
    bool lockedOnGesture(const std::chrono::milliseconds, const TimePoint &);

    private:
    OneFingerFSMConfig _cfg;
    int _driverWidth;
    int _driverHeight;
    int _outputWidth;
    int _outputHeight;

    enum state {
        Idle,
        Stroke,
        WaitingTap,
        WaitingDoubleTap,
        DragOngoing,
        SwipeOngoing,
        HoldOngoing,
        DoubleTapSwipeOngoing
    };
    state _state = Idle;
    TouchEvent _lastEvent;
    TouchEvent _startEvent;

    TimePoint _startEventTime;
    TimePoint _lastEventTime; //if now - last event time < some threshold(like 800-1000ms) than recover last event?

    inline void emitStart(Gestures s, int x, int y);
    inline void emitMove(Gestures s, int x, int y, int dx, int dy);
    inline void emitEnd(Gestures s, int x, int y, int dx, int dy);
    timer _tapDeadline;
    timer _doubleTapDeadline;
    timer _holdDeadline;

    GestureRecognizer & _recognizer;
    void printState(state state, bool proc);
};

}