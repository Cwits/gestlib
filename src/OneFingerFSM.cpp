#include "gestlib/OneFingerFSM.h"

#include "gestlib/GestureRecognizer.h"
#include "gestlib/Gestures.h"

#include <cmath>
#include <algorithm>
#include <iostream>

float distance(const GestLib::TouchEvent & p1, const GestLib::TouchEvent & p2) {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    return std::sqrt(dx * dx + dy * dy);
}

float dotProduct(const GestLib::TouchEvent & a, const GestLib::TouchEvent & b) {
    return a.x * b.x + a.y * b.y;
}

float getAngle(float x, float y, float cx, float cy) {
    return std::atan2(y - cy, x - cx); // ∈ [-π, π]
}

float pointToLineDistance(float x1, float y1, float x2, float y2, float px, float py) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float lenSq = dx*dx + dy*dy;
    float t = ((px - x1) * dx + (py - y1) * dy) / (lenSq + 1e-6);
    t = std::clamp(t, 0.f, 1.f);
    float projX = x1 + t * dx;
    float projY = y1 + t * dy;
    float distX = px - projX;
    float distY = py - projY;
    return std::sqrt(distX*distX + distY*distY);
}

float computeTurnAngleSum(const std::deque<GestLib::TouchEvent>& history) {
    std::size_t size = history.size();
    if (size < 10+2) return 0.0f;

    float totalAngle = 0.0f;
    //let's use some window of last 10 events
    for (size_t i = size-10; i < size; ++i) {
        float dx1 = history[i - 1].x - history[i - 2].x;
        float dy1 = history[i - 1].y - history[i - 2].y;
        float dx2 = history[i].x - history[i - 1].x;
        float dy2 = history[i].y - history[i - 1].y;

        float len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
        float len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

        if (len1 < 1e-3f || len2 < 1e-3f)
            continue;

        float dot = (dx1 * dx2 + dy1 * dy2) / (len1 * len2);
        dot = std::clamp(dot, -1.0f, 1.0f);  

        float angle = std::acos(dot);
        totalAngle += std::abs(angle);
    }

    return totalAngle;
}

int resLerp(int x, int inMax, int outMax) {
    return x * outMax / inMax;
}

float normalize_angle(float angle) {
    while (angle <= -M_PI) angle += 2.0f * M_PI;
    while (angle >  M_PI)  angle -= 2.0f * M_PI;
    return angle;
}

namespace GestLib {

void OneFingerFSM::printState(state state) {
    switch(state) {
        case(Idle): std::cout << "Idle" << std::endl; break;
        case(Stroke): std::cout << "Stroke" << std::endl; break;
        case(DragOrHold): std::cout << "DragOrHold" << std::endl; break;
        case(DragOngoing): std::cout << "DragOngoing" << std::endl; break;
        case(DoubleTapPossible): std::cout << "DoubleTapPossible" << std::endl; break;
        case(DoubleTapSwipeOngoing): std::cout << "DoubleTapSwipeOngoing" << std::endl; break;
        case(DoubleTapCircularOngoing): std::cout << "DoubleTapCircularOngoing" << std::endl; break;
        case(SwipeOngoing): std::cout << "SwipeOngoing" << std::endl; break;
        case(Hold): std::cout << "Hold" << std::endl; break;
    }
}

OneFingerFSM::OneFingerFSM(GestureRecognizer & rec) : _recognizer(rec) {
    _doubleTapHelper = Undetected;
}

bool OneFingerFSM::init(int resX, int resY) {
    _resolutionX = resX;
    _resolutionY = resY;

    return true;
}

void OneFingerFSM::expectedResolution(int x, int y) {
    _expectedX = x;
    _expectedY = y;
}

void OneFingerFSM::process(std::vector<TouchEvent> & touches) {
    TouchEvent & event = touches[0];
    timer::timePoint now = std::chrono::steady_clock::now();
    // printState(_state);
    switch(_state) {
        case(Idle): {
            if(event.type != TouchEvent::Type::Begin) return;
            _startEvent = event;
            _tapTimer.start(300, now);
            _dragOrHoldTimer.start(200, now);
            _holdTimer.start(800, now);
            _state = Stroke;
            Gesture t;
            t.type = Gestures::TouchDown;
            t.touchDown = {
                .x = resLerp(event.x, _resolutionX, _expectedX),
                .y = resLerp(event.y, _resolutionY, _expectedY)
            };
            _recognizer.pushGesture(t);
        } break;
        case(Stroke): {
            if(event.type != TouchEvent::Type::End && event.id == _startEvent.id) {
                _tapTimer.stop();
                if(event.id == _startEvent.id) {
                    if(_lastEvent.type != TouchEvent::Type::Move) {
                        if(_dragOrHoldTimer.active() && _dragOrHoldTimer.expired(now)) {
                            _state = DragOrHold;
                        } 
                    } else {
                        float dist = distance(event, _startEvent);
                        if(dist >= 10) {
                            Gesture swipe;
                            swipe.type = Gestures::Swipe;
                            swipe.swipe.state = GestureState::Start;
                            swipe.swipe = {
                                .x = resLerp(_startEvent.x, _resolutionX, _expectedX),
                                .y = resLerp(_startEvent.y, _resolutionY, _expectedY),
                                .dx = 0,
                                .dy = 0
                            }; 
                            _recognizer.pushGesture(swipe);
                            _state = SwipeOngoing;
                        }
                    }
                }
            } else if(event.type == TouchEvent::Type::Begin && event.id != _startEvent.id) {
                _doubleTapTimer.start(200, now);
                _state = DoubleTapPossible;
            }
        } break;
        case(DragOrHold): {
            if(event.id != _startEvent.id) {
                reset();
            }
            
            if(event.type == TouchEvent::Type::Move) {
                _dragOrHoldTimer.stop();
                _holdTimer.stop();
                float dist = distance(event, _startEvent);
                if(dist >= 20) {
                    Gesture drag;
                    drag.type = Gestures::Drag;
                    drag.drag.state = GestureState::Start;
                    drag.drag = {
                        .x = resLerp(_startEvent.x, _resolutionX, _expectedX),
                        .y = resLerp(_startEvent.y, _resolutionY, _expectedY),
                        .dx = 0,
                        .dy = 0
                    };
                    _recognizer.pushGesture(drag);
                    _state = DragOngoing;
                }   
            }
        } break;
        case(DragOngoing): {
            float dist = distance(event, _lastEvent);
            if(event.type == TouchEvent::Type::Move) {
                int dx = resLerp(event.x, _resolutionX, _expectedX) - resLerp(_lastEvent.x, _resolutionX, _expectedX);
                int dy = resLerp(event.y, _resolutionY, _expectedY) - resLerp(_lastEvent.y, _resolutionY, _expectedY);
                Gesture drag;
                drag.type = Gestures::Drag;
                drag.drag.state = GestureState::Move;
                drag.drag  = {
                    .x = resLerp(event.x, _resolutionX, _expectedX),
                    .y = resLerp(event.y, _resolutionY, _expectedY),
                    .dx = dx,
                    .dy = dy
                };
                _recognizer.pushGesture(drag);
            }
        } break;
        case(DoubleTapPossible): {
            if(event.type == TouchEvent::Type::Move && event.id == _lastEvent.id) { 
                if(_eventsHistory.size() < 5) break;

                //check all 5 are move
                bool skip = false;
                std::size_t size = _eventsHistory.size();
                for(int i=size-5; i<size; ++i) {
                    if(_eventsHistory[i].type != TouchEvent::Type::Move) {
                        skip = true;
                        break;
                    }
                }

                if(skip) break;

                if (_eventsHistory.size() % 5 == 0) {
                    //calc radius
                    float aproxRad = 0.0f;
                    {  
                        bool skip = false;
                        std::size_t size = _eventsHistory.size();
                        if(size < 10) skip = true;

                        if(!skip) {
                            for(std::size_t s=size-10; s<size; ++s) {
                                TouchEvent & t = _eventsHistory[s];
                                aproxRad = std::sqrt(std::pow(t.x-_startEvent.x, 2)+std::pow(t.y-_startEvent.y, 2));
                            }
                            aproxRad /= 10;
                            // std::cout << "aprox radius: " << aproxRad << std::endl;
                        }
                    }

                    float deltaAngle = 0.0f;
                    {//calc polar angle
                        std::size_t size = _eventsHistory.size();
                        float tmp = 0.0f;
                        float tmp2 = 0.0f;
                        for(std::size_t s=size-4; s<size; ++s) {
                            TouchEvent &t = _eventsHistory[s];
                            TouchEvent &t1 = _eventsHistory[s-1];
                            tmp = std::atan2(t.y-_startEvent.y, t.x-_startEvent.x);
                            tmp2 = std::atan2(t1.y-_startEvent.y, t1.x-_startEvent.x);
                            float tmp3 = tmp - tmp2;
                            deltaAngle += normalize_angle(tmp3);
                        }
                        std::cout << "angle rotation: " << deltaAngle << std::endl;
                    }



                    float turnSum = computeTurnAngleSum(_eventsHistory);
                    // std::cout << "turnsum: " << turnSum << std::endl;
                    float triqpi = M_PI * 0.95f;
                    if (turnSum >= triqpi) {
                        _doubleTapHelper = Circular;
                    } else {
                        _doubleTapHelper = Linear;
                    }
                    _predictions++;
                }

                if(_predictions >= 10) {
                    if(_doubleTapHelper == Circular) {
                        Gesture circ;
                        circ.type = Gestures::DoubleTapCircular;
                        circ.dtCircular.state = GestureState::Start;
                        circ.dtCircular = {
                            .x = resLerp(_startEvent.x, _resolutionX, _expectedX),
                            .y = resLerp(_startEvent.y, _resolutionY, _expectedY),
                            .dx = 0,
                            .dy = 0,
                            .angle = 0.f,
                            .deltaAngle = 0.f,
                            .radius = 0.f,
                            .speed = 0.f
                        };
                        _recognizer.pushGesture(circ);

                        _state = DoubleTapCircularOngoing;
                    } else if(_doubleTapHelper == Linear) {
                        Gesture swipe;
                        swipe.type = Gestures::DoubleTapSwipe;
                        swipe.dtSwipe.state = GestureState::Start;
                        swipe.dtSwipe = {
                            .x = resLerp(_startEvent.x, _resolutionX, _expectedX),
                            .y = resLerp(_startEvent.y, _resolutionY, _expectedY),
                            .dx = 0,
                            .dy = 0
                        };
                        _recognizer.pushGesture(swipe);

                        _state = DoubleTapSwipeOngoing;
                    }
                }
            } else if(event.type == TouchEvent::Type::Begin && event.id != _lastEvent.id) {
                std::cout << "Holly cow!" << std::endl;
            }
        } break;
        case(DoubleTapSwipeOngoing): {
            float dist = distance(event, _lastEvent);
            if(event.type == TouchEvent::Type::Move && dist >= 2) {
                int dx = resLerp(event.x, _resolutionX, _expectedX) - resLerp(_lastEvent.x, _resolutionX, _expectedX);
                int dy = resLerp(event.y, _resolutionY, _expectedY) - resLerp(_lastEvent.y, _resolutionY, _expectedY);
                Gesture swipe;
                swipe.type = Gestures::DoubleTapSwipe;
                swipe.dtSwipe.state = GestureState::Move;
                swipe.dtSwipe = {
                    .x = resLerp(event.x, _resolutionX, _expectedX),
                    .y = resLerp(event.y, _resolutionY, _expectedY),
                    .dx = dx,
                    .dy = dy
                };
                _recognizer.pushGesture(swipe);

            }
        } break;
        case(DoubleTapCircularOngoing): {
            float dist = distance(event, _lastEvent);
            if(event.type == TouchEvent::Type::Move && dist > 2) {
                
                float anglePrev = getAngle(_lastEvent.x, _lastEvent.y, _startEvent.x, _startEvent.y);
                float angleCurr = getAngle(event.x, event.y, _startEvent.x, _startEvent.y);
                
                float deltaAngle = angleCurr - anglePrev;
                if (deltaAngle > M_PI) deltaAngle -= 2 * M_PI;
                if (deltaAngle < -M_PI) deltaAngle += 2 * M_PI;

                // _totalAngle += deltaAngle;

                float dx = event.x - _startEvent.x;
                float dy = event.y - _startEvent.y;
                float radius = std::sqrt(dx*dx + dy*dy);

                float dt = std::chrono::duration<float>(now - event.time).count();
                float speed = deltaAngle / dt;

                /*std::cout << "Double tap circular" << 
                    " move angle: " << angleCurr << 
                    " delta angle: " << deltaAngle << 
                    " radius: " << radius << 
                    " speed: " << speed << 
                    " x: " << event.x << 
                    " y: " << event.y << std::endl;*/
                Gesture circ;
                circ.type = Gestures::DoubleTapCircular;
                circ.dtCircular.state = GestureState::Move;
                circ.dtCircular = {
                    .x = resLerp(event.x, _resolutionX, _expectedX),
                    .y = resLerp(event.y, _resolutionY, _expectedY),
                    .dx = 0,
                    .dy = 0,
                    .angle = angleCurr,
                    .deltaAngle = deltaAngle,
                    .radius = radius,
                    .speed = speed
                };
                _recognizer.pushGesture(circ);
            }
        } break;
        case(SwipeOngoing): {
            float dist = distance(event, _lastEvent);
            if(event.type == TouchEvent::Type::Move && dist > 5) {
                int dx = resLerp(event.x, _resolutionX, _expectedX) - resLerp(_lastEvent.x, _resolutionX, _expectedX);
                int dy = resLerp(event.y, _resolutionY, _expectedY) - resLerp(_lastEvent.y, _resolutionY, _expectedY);
                Gesture swipe;
                swipe.type = Gestures::Swipe;
                swipe.swipe.state = GestureState::Move;
                swipe.swipe = {
                    .x = resLerp(event.x, _resolutionX, _expectedX),
                    .y = resLerp(event.y, _resolutionY, _expectedY),
                    .dx = dx,
                    .dy = dy
                };
                _recognizer.pushGesture(swipe);
            } else if(event.type == TouchEvent::Type::End) {
                Gesture swipe;
                swipe.type = Gestures::Swipe;
                swipe.swipe.state = GestureState::End;
                swipe.swipe = {
                    .x = resLerp(event.x, _resolutionX, _expectedX),
                    .y = resLerp(event.y, _resolutionY, _expectedY),
                    .dx = 0,
                    .dy = 0
                };
                _recognizer.pushGesture(swipe);
                
                reset();
            }
        } break;
        case(Hold): {
            //do nothing
            reset();
        } break;
    }
    _lastEvent = event;
    if(_eventsHistory.size() >= 35) {
        _eventsHistory.pop_front();
    }
    _eventsHistory.push_back(event);

}

int OneFingerFSM::resetOrProcess(/*std::vector<TouchEvent> & touches*/) {
    // TouchEvent & event = touches[0];
    timer::timePoint now = std::chrono::steady_clock::now();
    // std::cout << "resetOrProc: ";
    // printState(_state);

    switch(_state) {
        case(Idle): {
            //do nothing
            reset();
            return 0;
        } break;
        case(Stroke): {
            // if(touches.size() == 0) {
                if(_lastEvent.type == TouchEvent::Type::Begin) {
                    if(_holdTimer.active() && _holdTimer.expired(now)) {
                        _state = Hold;
                    }
                } else if(_lastEvent.type == TouchEvent::Type::End) {
                    if(_tapTimer.active() && _tapTimer.expired(now)) {
                        Gesture tap;
                        tap.type = Gestures::Tap;
                        tap.tap = {
                            .x = resLerp(_lastEvent.x, _resolutionX, _expectedX), 
                            .y = resLerp(_lastEvent.y, _resolutionY, _expectedY)
                        };
                        _recognizer.pushGesture(tap);
                        reset();
                    }
                }
            // }
        } break;
        case(DragOrHold): {
            // ???
            reset();
        } break;
        case(DragOngoing): {
            if(_lastEvent.type == TouchEvent::Type::End) {
                Gesture drag;
                drag.type = Gestures::Drag;
                drag.drag.state = GestureState::End;
                drag.drag = {
                    .x = resLerp(_lastEvent.x, _resolutionX, _expectedX), 
                    .y = resLerp(_lastEvent.y, _resolutionY, _expectedY),
                    .dx = 0,
                    .dy = 0
                };
                _recognizer.pushGesture(drag);

                reset();
            }
        } break;
        case(DoubleTapPossible): {
            if(_lastEvent.type == TouchEvent::Type::End) {
                if(_doubleTapTimer.active() && _doubleTapTimer.expired(now)) {
                    Gesture dtap;
                    dtap.type = Gestures::DoubleTap;
                    dtap.doubleTap = {
                        .x = resLerp(_lastEvent.x, _resolutionX, _expectedX), 
                        .y = resLerp(_lastEvent.y, _resolutionY, _expectedY)
                    };
                    _recognizer.pushGesture(dtap);
                    
                    reset();
                }
            }
        } break;
        case(DoubleTapSwipeOngoing): {
            if(_lastEvent.type == TouchEvent::Type::End) {
                Gesture swipe;
                swipe.type = Gestures::DoubleTapSwipe;
                swipe.dtSwipe.state = GestureState::End;
                swipe.dtSwipe = {
                    .x = resLerp(_lastEvent.x, _resolutionX, _expectedX), 
                    .y = resLerp(_lastEvent.y, _resolutionY, _expectedY),
                    .dx = 0,
                    .dy = 0
                };
                _recognizer.pushGesture(swipe);
                
                reset();
            }
        } break;
        case(DoubleTapCircularOngoing): {
            if(_lastEvent.type == TouchEvent::Type::End) {
                Gesture circ;
                circ.type = Gestures::DoubleTapCircular;
                circ.dtCircular.state = GestureState::End;
                circ.dtCircular = {
                    .x = resLerp(_lastEvent.x, _resolutionX, _expectedX), 
                    .y = resLerp(_lastEvent.y, _resolutionY, _expectedY),
                    .dx = 0,
                    .dy = 0,
                    .angle = 0.f,
                    .deltaAngle = 0.f,
                    .radius = 0.f,
                    .speed = 0.f
                };
                _recognizer.pushGesture(circ);
                
                reset();
            }
        } break;
        case(SwipeOngoing): {
            if(_lastEvent.type == TouchEvent::Type::End) {
                Gesture swipe;
                swipe.type = Gestures::Swipe;
                swipe.swipe.state = GestureState::End;
                swipe.swipe = {
                    .x = resLerp(_lastEvent.x, _resolutionX, _expectedX), 
                    .y = resLerp(_lastEvent.y, _resolutionY, _expectedY),
                    .dx = 0,
                    .dy = 0
                };
                _recognizer.pushGesture(swipe);

                reset();
            }
        } break;
        case(Hold): {
            Gesture hold;
            hold.type = Gestures::Hold;
            hold.hold = {
                .x = resLerp(_lastEvent.x, _resolutionX, _expectedX), 
                .y = resLerp(_lastEvent.y, _resolutionY, _expectedY)
            };
            _recognizer.pushGesture(hold);

            reset();
        } break;
    }

    return 1;
}

void OneFingerFSM::reset() {
    _doubleTapHelper = Undetected;
    _predictions = 0;
    _tapTimer.stop();
    _doubleTapTimer.stop();
    _dragOrHoldTimer.stop();
    _holdTimer.stop();
    if(_state != Idle) {
        Gesture touch;
        touch.type = Gestures::TouchUp;
        touch.touchUp = {
            .x = resLerp(_lastEvent.x, _resolutionX, _expectedX), 
            .y = resLerp(_lastEvent.y, _resolutionY, _expectedY)
        };
        _recognizer.pushGesture(touch);
    }
    _state = Idle;
}

void OneFingerFSM::timer::start(int timeout, timePoint & now) {
    _active = true;  
    _timeout = timeout;  
    _startTime = now;
}

void OneFingerFSM::timer::stop() {
    _active = false;
    _timeout = 0;
}

bool OneFingerFSM::timer::expired(timePoint & now) {
    uint dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - _startTime).count();
    if(dur >= _timeout){  
        return true;  
    } else {  
        return false;  
    }  
}

}//namespace GestLib