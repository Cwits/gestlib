#pragma once
#include <variant>

namespace GestLib {

enum class Gestures {
    // Stroke,         //+
    TouchDown,
    TouchUp,
    //one finger
    Tap,            //+
    Hold,           //+
    Swipe,          //+
    Drag,           //+
    DoubleTap,      //+
    DoubleTapSwipe, //+
    DoubleTapCircular, //+ //TapTap-hold and go circular
    //EdgeSwipe,
    //two fingers
    Zoom,
    TwoFingerTap, //?
    TwoFingerSwipe,
    //three fingers
    ThreeFingerTap,
    ThreeFingerSwipe
};

enum class GestureState {
    Start,
    Move,
    End
};

struct TapGesture {
    int x;
    int y;
};

struct HoldGesture {
    int x;
    int y;
};

struct DoubleTapGesture {
    int x;
    int y;
};

struct SwipeGesture {
    GestureState state;
    int x;
    int y;
    int dx;
    int dy;
};

struct DragGesture {
    GestureState state;
    int x;
    int y;
    int dx;
    int dy;
};

struct DTSwipeGesture {
    GestureState state;
    int x;
    int y;
    int dx;
    int dy;
};

struct DTCircularGesture {
    GestureState state;
    int x;
    int y;
    int dx;
    int dy;
    float angle;
    float deltaAngle;
    float radius;
    float speed;
};

struct TouchDownEvent {
    int x;
    int y;
};

struct TouchUpEvent {
    int x;
    int y;
};

struct Gesture {
    Gestures type;
    union {
        TouchDownEvent touchDown;
        TouchUpEvent touchUp;
        TapGesture tap;
        HoldGesture hold;
        DoubleTapGesture doubleTap;
        SwipeGesture swipe;
        DragGesture drag;
        DTSwipeGesture dtSwipe;
        DTCircularGesture dtCircular;
    };
};

}