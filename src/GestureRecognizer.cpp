// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "gestlib/GestureRecognizer.h"
#include "gestlib/TouchDriver.h"

#include <sys/eventfd.h>
#include <iostream>
#include <array>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <cstring>

constexpr auto IDLE_TIMEOUT = std::chrono::milliseconds(350);

// namespace { thread_local const GestureRecognizer* tlOwner = nullptr; }  

namespace GestLib {

GestureRecognizer::GestureRecognizer(GestLib::RecognizerConfig cfg) : 
    _cfg(cfg),    
    _dri(nullptr), 
    _ofs(*this, cfg.oneCfg),
    _tfs(*this, cfg.twoCfg)
{
    _running = false;
    _gesturesQueue.clear();
}

GestureRecognizer::~GestureRecognizer() {
    if(_running) shutdown();
}

bool GestureRecognizer::init(TouchDriver *dri) {
    if(!dri) { std::cerr << "Driver is nullptr" << std::endl; return false; }
    if(dri->width() == 0 || dri->height() == 0) { std::cerr << "Bad driver window size" << std::endl; return false; }

    if(_running) shutdown();

    if(!_ofs.init(dri->width(), dri->height())) { std::cerr << "Failed to init state machine one" << std::endl; return false; }
    if(!_tfs.init(dri->width(), dri->height())) { std::cerr << "Failed to init state machine two" << std::endl; return false; }

    _ofs.outputResolution(_cfg.outputWidth, _cfg.outputHeight);
    _tfs.outputResolution(_cfg.outputWidth, _cfg.outputHeight);

    _efd = eventfd(0, EFD_NONBLOCK);
    
    if(_efd == -1) { std::cerr << "efd error" << std::endl; return false; }

    reset();

    _dri = dri;
    return true;
}

bool GestureRecognizer::shutdown() {
    _running = false;

    uint64_t u = 1;
    write(_efd, &u, sizeof(u));

    _recognizer.join();
    
    close(_efd);

    return true;
}

//TODO: no actual check's here, always returns true!!
bool GestureRecognizer::start() {
    if(!_dri) {
        std::cerr << "Unable to start, driver not presented" << std::endl;
        return false;
    }
    _running = true;

    auto iteration = [this]() {    
        // tlOwner = this;
        struct pollfd fds[2];
        fds[0].fd = this->_dri->fd();
        fds[0].events = POLLIN;
        fds[1].fd = this->_efd;
        fds[1].events = POLLIN;

        while(this->_running) {    
            auto now = std::chrono::steady_clock::now();
            int ret = poll(fds, 2, nextPollTimeout(now));
            
            if(ret < 0) {
                if (errno == EINTR) continue;
                std::cerr << "poll: " << std::strerror(errno) << "\n";
                break;
            } else if(ret > 0) {
                _lastAction = now;
            }

            if(fds[1].revents & POLLIN) { break; }

            now = std::chrono::steady_clock::now();
            // if (fds[0].revents & POLLIN) {
                std::vector<TouchEvent> touches = this->_dri->getEvents();
                std::size_t size = touches.size();

                for(const auto &t : touches) {
                    // if(_captured.count(t.id)) {
                    //     //feedCapturedTouch(t);
                    //     continue;
                    // }

                    if(_ignored.count(t.id)) {
                        if(t.type == TouchEvent::Type::End) { _ignored.erase(t.id); continue; }
                        if(t.type != TouchEvent::Type::Begin) continue;
                        _ignored.erase(t.id);
                    }

                    // if(t.type == TouchEvent::Type::Begin) {
                    //     //if(tryCaptured(t)) continue; //finger captured by TouchCapture
                    // }


                    if(t.type == TouchEvent::Type::Begin) {
                        _active[t.id] = std::make_pair(t, t);
                    } else {
                        auto it = _active.find(t.id);
                        if(it == _active.end()) { //some garbage touch
                            if(t.type == TouchEvent::Type::Move) _ignored.insert(t.id);
                            continue;
                        }
                        it->second.second = t;
                    }
                    _lastEvent = t;

                    // dispatch(t, now);
                    switch(_state) {
                        case(RecognizerState::Idle): {
                            if(t.type != TouchEvent::Type::Begin) continue;
                            
                            if(!_sessionOpened) {
                                _sessionOpened = true;
                                _sessionId = t.id;
                                Gesture g;
                                g.type = Gestures::TouchDown;
                                g.touchDown.touchId = t.id;
                                g.touchDown.x = lerp(t.x, _dri->width(), _cfg.outputWidth);
                                g.touchDown.y = lerp(t.y, _dri->height(), _cfg.outputHeight);
                                pushGesture(g);
                            }
                            _state = RecognizerState::OneFingerOngoing;
                            _ofs.onEvent(t, now);
                        } break;
                        case(RecognizerState::OneFingerOngoing): {
                            if (t.type == TouchEvent::Type::Begin) {
                                if(_active.size() >= 3) { cancelAll(); continue; }
                                if(_active.size() == 2) {
                                    if(!_ofs.lockedOnGesture(std::chrono::milliseconds(_cfg.gestureToLockMs), now)) {
                                        _ofs.reset();

                                        auto it = _active.begin();
                                        const std::pair<TouchEvent, TouchEvent> a = it->second; 
                                        ++it;
                                        const std::pair<TouchEvent, TouchEvent> b = it->second;
                                        _tfs.begin(a, b, now);
                                        _state = RecognizerState::TwoFingerOngoing;
                                    } else {
                                        _ignored.insert(t.id);
                                        _active.erase(t.id);
                                    }
                                    continue;
                                }
                            }
                            _ofs.onEvent(t, now);
                        
                        } break;
                        case(RecognizerState::TwoFingerOngoing): {
                            if(t.type == TouchEvent::Type::Begin) { cancelAll(); continue; }

                            _tfs.onEvent(t, now);

                            if(t.type == TouchEvent::Type::End) {
                                for(auto& kv : _active) {
                                    if(kv.first != t.id) 
                                        _ignored.insert(kv.first);
                                }
                                _active.clear();
                                _state = RecognizerState::Idle;
                            }
                        } break;
                    }

                    if(t.type == TouchEvent::Type::End) _active.erase(t.id);
                // }
                _tfs.endFrame();
                refresh();
            }
            _ofs.tick(now);
            _tfs.tick(now);
            refresh();
        }  

        cancelAll();
        // releaseCaptures();
        // tlOwner = nullptr;
    };

    _recognizer = std::thread(iteration);
    return true;
}

int GestureRecognizer::nextPollTimeout(const TimePoint &now) {
    /*std::optional<TimePoint> deadline;// = _ofs.nextDeadline();
    
    for(auto dl : { _ofs.nextDeadline(), _tfs.nextDeadline() }) {
        if(dl && (!deadline || *dl < *deadline)) deadline = dl;
    }
    if(!deadline) {
        if(std::chrono::duration_cast<std::chrono::milliseconds>(now - this->_lastAction) > IDLE_TIMEOUT)
            return -1;
        else 
            return 15;
    }
    auto ret = std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - now).count();
    return ret <= 0 ? 0 : int(ret);*/
    if(std::chrono::duration_cast<std::chrono::milliseconds>(now - this->_lastAction) > IDLE_TIMEOUT)
            return -1;
        else 
            return 20;
}

void GestureRecognizer::pushGesture(Gesture gest) {
    std::lock_guard<std::mutex> lock(_lock);
    _gesturesQueue.push_back(gest);
}

void GestureRecognizer::refresh() {
    const bool idle = _active.empty() && !_ofs.busy() && !_tfs.busy();
    if(_state == RecognizerState::OneFingerOngoing && _active.empty() && !_ofs.busy()) 
        _state = RecognizerState::Idle;
    
    if(_sessionOpened && idle) {
        _sessionOpened = false;
        Gesture g;
        g.type = Gestures::TouchUp;
        g.touchUp.touchId = _sessionId;
        g.touchUp.x = lerp(_lastEvent.x, _dri->width(), _cfg.outputWidth);
        g.touchUp.y = lerp(_lastEvent.y, _dri->height(), _cfg.outputHeight);
        pushGesture(g);
    }
}

void GestureRecognizer::reset() {
    _ofs.reset();
    _tfs.reset();
    std::lock_guard<std::mutex> lock(_lock);
    _gesturesQueue.clear();
}

void GestureRecognizer::cancelAll() {
    _ofs.reset();
    _tfs.reset();
    for (auto& kv : _active) _ignored.insert(kv.first);
    _active.clear();
    _state = RecognizerState::Idle;
    refresh();
}

std::vector<Gesture> GestureRecognizer::fetchGestures() {
    std::vector<Gesture> ret;
    {
        std::lock_guard<std::mutex> lock(_lock);
        ret = std::move(_gesturesQueue);
    }
    return ret;
}

/*
//TODO: don't like this option...
void GestureRecognizer::registerTouchCapture(TouchCapture* cap, int priority) {
    if (!cap) return;
    
    auto entry = std::make_shared<CapturedTouch>();
    entry->capture = cap;
    entry->priority = priority;

    std::lock_guard<std::mutex> lk(_captureLock);
    for(const auto& c : *_captures) {
        if (c->capture == cap) return;
    }

    auto list = std::make_shared<CapturedList>(*_captures); 
    list->push_back(std::move(entry));

    std::stable_sort(
        list->begin(), 
        list->end(),
        [](const CapturedPtr& a, const CapturedPtr& b) { 
            return a->priority > b->priority; 
        }
    );

    _captures = std::move(list);
}

void GestureRecognizer::unregisterTouchCapture(TouchCapture* cap) {
    CapturedPtr found;
    {
        std::lock_guard<std::mutex> lk(_captureLock);
        auto list = std::make_shared<CapturedList>();
        for(const auto& c : *_captures) {
            if (z->capture == cap) found = c; 
            else list->push_back(c);
        }
        if(!found) return;
        _captures = std::move(list);
    }
    found->removed = true;
    
    if(tlOwner != this) { 
        std::lock_guard<std::mutex> wait(found->callMutex); 
    }
}

bool GestureRecognizer::tryCapture(const TouchEvent& e) {
    std::shared_ptr<const CapturedList> caps;
    { 
        std::lock_guard<std::mutex> lk(_captureLock); 
        caps = _captures; 
    }
    if(caps->empty()) return false;

    const TouchEvent out = toOutput(e);
    for(const auto& c : *caps) {
        std::lock_guard<std::mutex> call(c->callMutex);
        if (c->removed) continue;
        if (c->active >= std::max(1, c->capture->maxTouches())) continue;
        // if (!c->capture->hitTest(out.x, out.y)) continue;

        ++c->active;
        _captured[e.id] = c;
        c->capture->onTouch(out);              // Begin
        return true;
    }
    return false;
}

void GestureRecognizer::feedCaptured(int id, const TouchEvent& e) {
    CapturedPtr c = _captured.at(id);
    bool delivered = false;
    {
        std::lock_guard<std::mutex> call(z->callMutex);
        if(!c->removed) { 
            c->capture->onTouch(toOutput(e)); 
            delivered = true; 
        }
    }

    const bool end = e.type == TouchEvent::Type::End;
    if(end || !delivered) {
        _captured.erase(id);
        --c->active;
        
        if(!delivered && !end) 
            _ignored.insert(id);
    }
}

void GestureRecognizer::releaseCaptures() {
    auto captured = std::move(_captured);
    _captured.clear();
    for (auto& kv : captured) {
        std::lock_guard<std::mutex> call(kv.second->callMutex);
        if (!kv.second->removed) kv.second->capture->onCancel(kv.first);
        kv.second->active = 0;
    }
}
*/
}//namespace