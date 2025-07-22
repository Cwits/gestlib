#pragma once
#include <variant>

namespace GestLib {

enum class Gestures {
    // Stroke,         //+
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

enum class GestureType {
    TouchDown,
    TouchUp,
    Tap,
    Hold,
    DoubleTap,
    SwipeStart,
    SwipeMove,
    SwipeEnd,
    DragStart,
    DragMove,
    DragEnd,
    DTSwipeStart,
    DTSwipeMove,
    DTSwipeEnd,
    DTCircularStart,
    DTCircularMove,
    DTCircularEnd
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

struct SwipeStartGesture {
    int x;
    int y;
};

struct SwipeMoveGesture {
    int dx;
    int dy;
    int x;
    int y;
};

struct SwipeEndGesture {
    int x;
    int y;
};

struct DragStartGesture {
    int x;
    int y;
};

struct DragMoveGesture {
    int dx;
    int dy;
    int x;
    int y;
};

struct DragEndGesture {
    int x;
    int y;
};

struct DTSwipeStartGesture {
    int x;
    int y;
};

struct DTSwipeMoveGesture {
    int dx; 
    int dy;
    int x;
    int y;
};

struct DTSwipeEndGesture {
    int x;
    int y;
};

struct DTCircularStartGesture {
    int x;
    int y;
};

struct DTCircularMoveGesture {
    float angle; 
    float deltaAngle; 
    float radius; 
    float speed; 
    int x;
    int y;
};

struct DTCircularEndGesture {
    int x;
    int y;
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
    GestureType type;
    union {
        TouchDownEvent touchDown;
        TouchUpEvent touchUp;
        TapGesture tap;
        HoldGesture hold;
        DoubleTapGesture doubleTap;
        SwipeStartGesture swipeStart;
        SwipeMoveGesture swipeMove;
        SwipeEndGesture swipeEnd;
        DragStartGesture dragStart;
        DragMoveGesture dragMove;
        DragEndGesture dragEnd;
        DTSwipeStartGesture dtSwipeStart;
        DTSwipeMoveGesture dtSwipeMove;
        DTSwipeEndGesture dtSwipeEnd;
        DTCircularStartGesture dtCircularStart;
        DTCircularMoveGesture dtCircularMove;
        DTCircularEndGesture dtCircularEnd;
    };
};

}