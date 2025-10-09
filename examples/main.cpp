// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include <iostream>
#include <fstream>
#include <csignal>
#include <thread>

#include "gestlib/GestLib.h"

using namespace GestLib;

bool _running = true;

void signalHandler(int signum) {  
    _running = false;
}

void processGestures(const std::vector<Gesture> & gestures);

int main() {
    signal(SIGINT, signalHandler);
    
    GestureRecognizer recognizer; //or fake

    if(!recognizer.init()) {
        return 1;
    }
    
    recognizer.windowSize(1920, 1080);
    
    if(!recognizer.start()) {
        return 2;
    }

    int fps = 1 / 60.f * 1000;
    while(_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(fps));

        std::vector<Gesture> gestures = recognizer.fetchGestures();
        processGestures(gestures);
    }

    recognizer.shutdown();
    return 0;
}


void processGestures(const std::vector<Gesture> & gestures) {
    if(gestures.empty()) return;
    for(const Gesture &g : gestures) {
        switch(g.type) {
            case(Gestures::TouchDown):
                std::cout << "It's touch down event!" << 
                " x: " << g.touchDown.x << 
                " y: " << g.touchDown.y << std::endl;    
            break;
            case(Gestures::TouchUp):
                std::cout << "It's touch up event!" << std::endl;
            break;
            case(Gestures::Tap):
                std::cout << "it's a Tap! " << std::endl;
            break;
            case(Gestures::DoubleTap):
                std::cout << "It's a Double Tap!" << std::endl;
            break;
            case(Gestures::Hold):
                if(g.hold.state == GestureState::Start) {
                    std::cout << "it's a Hold Start!" << std::endl;
                } else if(g.hold.state == GestureState::Move) {
                    std::cout << "it's a Hold Move!" << std::endl;
                } else if(g.hold.state == GestureState::End) {
                    std::cout << "it's a Hold End!" << std::endl;
                }
            break;
            case(Gestures::Swipe):
                if(g.swipe.state == GestureState::Start)
                    std::cout << "it's a Swipe Start!" << std::endl;
                else if(g.swipe.state == GestureState::Move)
                    std::cout << "it's a Swipe Move!" << std::endl;
                else if(g.swipe.state == GestureState::End)
                    std::cout << "it's a Swipe End!" << std::endl;
            break;
            case(Gestures::Drag):
                if(g.drag.state == GestureState::Start)
                    std::cout << "it's a Drag Start!" << std::endl;
                else if(g.drag.state == GestureState::Move)
                    std::cout << "it's a Drag Move!" << std::endl;
                else if(g.drag.state == GestureState::End)
                    std::cout << "it's a Drag End!" << std::endl;
            break;
            case(Gestures::DoubleTapSwipe):
                if(g.dtSwipe.state == GestureState::Start)
                    std::cout << "it's a DoubleTapSwipe Start!" << std::endl;
                else if(g.dtSwipe.state == GestureState::Move)
                    std::cout << "it's a DoubleTapSwipe Move!" << std::endl;
                else if(g.dtSwipe.state == GestureState::End)
                    std::cout << "it's a DoubleTapSwipe End!" << std::endl;
            break;
            case(Gestures::DoubleTapCircular):
                if(g.dtCircular.state == GestureState::Start)
                    std::cout << "it's a DoubleTapCircular Start!" << std::endl;
                else if(g.dtCircular.state == GestureState::Move)
                    std::cout << "it's a DoubleTapCircular Move!" << std::endl;
                else if(g.dtCircular.state == GestureState::End)
                    std::cout << "it's a DoubleTapCircular End!" << std::endl;
            break;
            default:
                std::cout << "unknows gesture" << std::endl;
            break;
        }
    }
}