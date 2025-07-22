#pragma once
#include "defines.h"
#include <vector>
#include <deque>
#include <chrono>

namespace GestLib {

class GestureRecognizer;
class OneFingerFSM {
    public:
    OneFingerFSM(GestureRecognizer & rec);

    bool init(int resX, int resY);
    void expectedResolution(int x, int y);

    void process(std::vector<TouchEvent> & touches);
    int resetOrProcess(/*std::vector<TouchEvent> & touches*/);
    void reset();

    private:

    int _resolutionX;
    int _resolutionY;
    int _expectedX;
    int _expectedY;

    enum state {
        Idle,
        Stroke,
        DragOrHold,
        DragOngoing,
        DoubleTapPossible,
        DoubleTapSwipeOngoing,
        DoubleTapCircularOngoing,
        SwipeOngoing,
        Hold 
    };
    state _state = Idle;
    TouchEvent _lastEvent;
    TouchEvent _startEvent;

    enum gestureMove {
        Undetected,
        Linear,
        Circular
    };
    
    gestureMove _doubleTapHelper;
    int _predictions;
    std::deque<TouchEvent> _eventsHistory;

    struct timer {
        using timePoint = std::chrono::time_point<std::chrono::steady_clock>;
        void start(int timeout, timePoint & now);
        void stop();
        bool active() { return _active; }
        bool expired(timePoint & now);

        private:
        int _timeout;
        bool _active;
        timePoint _startTime;
    };

    timer _tapTimer;
    timer _doubleTapTimer;
    timer _dragOrHoldTimer;
    timer _holdTimer;

    GestureRecognizer & _recognizer;

    void printState(state state);
};

}