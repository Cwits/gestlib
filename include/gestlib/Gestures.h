// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once
#include <variant>
#include <string>

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
    DoubleTapCircular, //+ in code but not working - do i need this one?
    //two fingers
    Zoom,
    Rotate,
    TwoFingerTap, //+
    TwoFingerDoubleTap,
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
    GestureState state;
    int x;
    int y;
    int dx;
    int dy;
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

struct TwoFingerTap {
    int x;
    int y;
};

struct TwoFingerDoubleTap {
    int x;
    int y;
};

struct TwoFingerSwipe { //sometimes is recognized as Zoom
    GestureState state;
    int x; 
    int y;
    int dx;
    int dy;
};

struct ZoomGesture {
    GestureState state;
    int xCenter; //(x1 + x2) / 2  
    int yCenter; //(y1 + y2) / 2
    float horDif; //delta of horizontal zoom since previous event
    float horAbs; //absolute value of horizontal zoom since touch down
    float verDif; //delta of vertical zoom since previous event
    float verAbs; //absolute value of vertical zoom since touch down
};

struct RotateGesture {
    GestureState state;
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
        TwoFingerTap tfTap;
        TwoFingerDoubleTap tfDoubleTap;
        TwoFingerSwipe tfSwipe;
        ZoomGesture zoom;
        RotateGesture rotate;
    };
};

/*
or JoystickArea or just Joystick?
TouchDown and TouchUp handled by target itself?
touch Event will be extracted from event vector from driver and passed separately
rest of events will be processed as usual
must be one finger only?
*/
struct TouchCapture {
    virtual bool onMove(const TouchEvent &touch) = 0;
    virtual bool onCancle() = 0;

    private:
    int CapturedEventId = -1;
    
};

inline std::string gestureToText(const GestLib::Gestures &g) {
    std::string text;
    switch(g) {
        case(GestLib::Gestures::TouchDown): text = "Touch Down"; break;
        case(GestLib::Gestures::TouchUp): text = "Touch Up"; break;
        case(GestLib::Gestures::Tap): text = "Tap"; break;
        case(GestLib::Gestures::Hold): text = "Hold"; break;
        case(GestLib::Gestures::Drag): text = "Drag"; break;
        case(GestLib::Gestures::Swipe): text = "Swipe"; break;
        case(GestLib::Gestures::DoubleTap): text = "Double Tap"; break;
        case(GestLib::Gestures::DoubleTapSwipe): text = "Double Tap Swipe"; break;
        case(GestLib::Gestures::DoubleTapCircular): text = "Double Tap Circular"; break;
        case(GestLib::Gestures::Zoom): text = "Zoom"; break;
        case(GestLib::Gestures::Rotate): text = "Rotate"; break;
        case(GestLib::Gestures::TwoFingerTap): text = "Two Finger Tap"; break;
        case(GestLib::Gestures::TwoFingerDoubleTap): text = "Two Finger Double Tap"; break;
        case(GestLib::Gestures::TwoFingerSwipe): text = "Two Finger Swipe"; break;
        case(GestLib::Gestures::ThreeFingerTap): text = "Three Finger Tap"; break;
        case(GestLib::Gestures::ThreeFingerSwipe): text = "Three Finger Swipe"; break;
        default: text = "Unrecognized gesture"; break;
    }
    return text;
}


}