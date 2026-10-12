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

struct ZoomGesture {
    GestureState state;
    int x1;
    int y1;
    int x2;
    int y2;
    float hZoom;
    float vZoom;
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
        ZoomGesture zoom;
    };
};


inline std::string gestureToText(GestLib::Gestures &g) {
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
        case(GestLib::Gestures::TwoFingerTap): text = "Two Finger Tap"; break;
        case(GestLib::Gestures::TwoFingerSwipe): text = "Two Finger Swipe"; break;
        case(GestLib::Gestures::ThreeFingerTap): text = "Three Finger Tap"; break;
        case(GestLib::Gestures::ThreeFingerSwipe): text = "Three Finger Swipe"; break;
    }
    return text;
}


}