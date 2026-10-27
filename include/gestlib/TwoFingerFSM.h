// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include "defines.h"
#include <vector>
#include <deque>
#include <chrono>
#include <tuple>

namespace GestLib {

struct GestureRecognizer;

struct TwoFingerFSM {
    TwoFingerFSM(GestureRecognizer &rec);
    ~TwoFingerFSM();

    bool init(int driverWidth, int driverHeight);
    void outputResolution(int w, int h);

    void process(std::vector<TouchEvent> & touches);
    void resetOrProcess();
    void reset();

    private:
    int _driverWidth;
    int _driverHeight;

    int _outputWidth;
    int _outputHeight;

    //gestures: Tap, DoubleTap, Swipe, Zoom, Rotate
    enum class state {
        Idle,
        Stroke,
        Tap,
        DoubleTap,
        Swipe,
        SwipeOngoing,
        Zoom,
        ZoomOngoing,
        Rotate
    };

    state _state;

    // enum class TouchState {
    //     Begin,
    //     Move,
    //     End
    // };
    // TouchState _tstate;

    using Event = std::pair<TouchEvent, TouchEvent>;

    Event _startTouches;
    Event _lastTouches;

    std::deque<Event> _eventHistory;

    TimePoint _idleToStroke;
    bool _idleActive;

    timer _tapTimer;

    timer _gestureToLost;

    GestureRecognizer & _recognizer;
    void printstate(bool);
};

}