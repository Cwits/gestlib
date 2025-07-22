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
            case(GestureType::TouchDown):
                std::cout << "It's touch down event!" << 
                " x: " << g.touchDown.x << 
                " y: " << g.touchDown.y << std::endl;    
            break;
            case(GestureType::TouchUp):
                std::cout << "It's touch up event!" << std::endl;
            break;
            case(GestureType::Tap):
                std::cout << "it's a Tap! " << std::endl;
            break;
            case(GestureType::DoubleTap):
                std::cout << "It's a Double Tap!" << std::endl;
            break;
            case(GestureType::Hold):
                std::cout << "it's a Hold!" << std::endl;
            break;
            case(GestureType::SwipeStart):
                std::cout << "it's a Swipe Start!" << std::endl;
            break;
            case(GestureType::SwipeMove):
                std::cout << "it's a Swipe Move!" << std::endl;
            break;
            case(GestureType::SwipeEnd):
                std::cout << "it's a Swipe End!" << std::endl;
            break;
            case(GestureType::DragStart):
                std::cout << "it's a Drag Start!" << std::endl;
            break;
            case(GestureType::DragMove):
                std::cout << "it's a Drag Move!" << std::endl;
            break;
            case(GestureType::DragEnd):
                std::cout << "it's a Drag End!" << std::endl;
            break;
            case(GestureType::DTSwipeStart):
                std::cout << "it's a Double Tap Swipe Start!" << std::endl;
            break;
            case(GestureType::DTSwipeMove):
                std::cout << "it's a Double Tap Swipe Move!" << std::endl;
            break;
            case(GestureType::DTSwipeEnd):
                std::cout << "it's a Double Tap Swipe End!" << std::endl;
            break;
            case(GestureType::DTCircularStart):
                std::cout << "it's a Double Tap Circular Start!" << std::endl;
            break;
            case(GestureType::DTCircularMove):
                std::cout << "it's a Double Tap Circular Move!" << std::endl;
            break;
            case(GestureType::DTCircularEnd):
                std::cout << "it's a Double Tap Circular End!" << std::endl;
            break;
            default:
                std::cout << "unknows gesture" << std::endl;
            break;
        }
    }
}