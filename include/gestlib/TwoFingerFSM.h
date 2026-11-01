// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include "defines.h"
#include "Gestures.h"
#include <vector>
#include <deque>
#include <chrono>
#include <tuple>
#include <optional>

namespace GestLib {

struct GestureRecognizer;

struct TwoFingerFSMConfig {
    int   tapTimeout       = 300;   // ms
    int   doubleTapTimeout = 250;   // ms, пауза между двумя двухпальцевыми тапами
    int   doubleTapRadius  = 100;   // driver units
    int   decideDistance   = 8;    // driver units: пока движение меньше - жест не классифицируем
    float swipeRatio       = 1.5f;  // Swipe, если смещение центра > swipeRatio * изменение «вектора между пальцами»
    float rotateRatio      = 1.0f;  // Rotate, если тангенциальная часть > rotateRatio * радиальной
    int   moveStep         = 1;     // driver units, минимальный шаг между Move
    bool  swipeEnabled     = true;
    bool  rotateEnabled    = false;
    bool  doubleTapEnabled = true;  // false -> TwoFingerTap уходит сразу
};

struct TwoFingerFSM {
    TwoFingerFSM(GestureRecognizer &rec, TwoFingerFSMConfig cfg);
    ~TwoFingerFSM();

    bool init(int driverWidth, int driverHeight);
    void outputResolution(int w, int h);

    void begin(const std::pair<TouchEvent, TouchEvent> &, const std::pair<TouchEvent, TouchEvent> &, const TimePoint &);
    void onEvent(const TouchEvent &, const TimePoint &);
    void endFrame();
    void tick(const TimePoint &);
    
    std::optional<TimePoint> nextDeadline();
    void reset();
    bool busy() const { return _state != State::Idle; }

    private:
    TwoFingerFSMConfig _cfg;
    int _driverWidth;
    int _driverHeight;

    int _outputWidth;
    int _outputHeight;

    enum class State { 
        Idle, 
        Pressed, 
        Swipe, 
        Zoom, 
        Rotate, 
        Swallow, 
        TapWait, 
        DoubleTapPressed 
    };

    struct Finger { 
        int id = -1; 
        int sx = 0;
        int sy = 0;
        int x = 0;
        int y = 0;
        int rx = 0;
        int ry = 0; 
    };
    
    struct Metrics { 
        float travel;
        float rel;
        float radial;
        float tangential; 
    };

    Metrics measure() const;
    void tryClassify();
    bool fingersMoved(int step) const;
    void emit(GestureState gs);
    void finish();
    void emitTap();
    void emitDoubleTap();

    State _state = State::Idle;
    Finger _f[2];
    TimePoint _downTime{};
    TimePoint _tapDeadline{};
    int _tapX = 0;
    int _tapY = 0;
    float _lastAngle = 0; 
    float _totalAngle = 0;

    GestureRecognizer & _recognizer;
    void printstate(bool);
};

}