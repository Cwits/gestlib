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

constexpr auto IDLE_TIMEOUT = std::chrono::milliseconds(300);

namespace GestLib {

GestureRecognizer::GestureRecognizer(GestLib::RecognizerConfig cfg) : 
    _cfg(cfg),    
    _dri(nullptr), 
    _ofs(*this, cfg.oneCfg),
    _tfs(*this)
{
    _running = false;
    _gesturesQueue.clear();
}

GestureRecognizer::~GestureRecognizer() {
    if(_running) shutdown();
}

bool GestureRecognizer::init(TouchDriver *dri) {
    if(!dri) {
        std::cerr << "Driver is nullptr" << std::endl;
        return false;
    }
    if(_running) shutdown();

    if(!_ofs.init(dri->width(), dri->height())) {
        std::cerr << "Failed to init state machine one" << std::endl;
        return false;
    }

    if(!_tfs.init(dri->width(), dri->height())) {
        std::cerr << "Failed to init state machine two" << std::endl;
        return false;
    }

    _ofs.outputResolution(_cfg.outputWidth, _cfg.outputHeight);
    _tfs.outputResolution(_cfg.outputWidth, _cfg.outputHeight);

    _efd = eventfd(0, EFD_NONBLOCK);
    
    if(_efd == -1) {
        std::cout << "efd error" << std::endl;
        return false;
    }

    _ofs.reset();
    _gesturesQueue.clear();
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
        struct pollfd fds[2];
        fds[0].fd = this->_dri->fd();
        fds[0].events = POLLIN;
        fds[1].fd = this->_efd;
        fds[1].events = POLLIN;

        while(this->_running) {    
            int ret = 0;
            auto now = std::chrono::steady_clock::now();
            if(std::chrono::duration_cast<std::chrono::milliseconds>(now - this->_lastAction) > IDLE_TIMEOUT) {
                ret = poll(fds, 2, -1);
            } else {
                ret = poll(fds, 2, 20);
            }
            
            if(ret < 0) {
                std::cout << "some error" << std::endl;
            } else if(ret == 0) {
                //...
                // std::cout << "ret=0" << std::endl;
            } else if(ret > 0) {
                this->_lastAction = std::chrono::steady_clock::now();
            }

            if(fds[1].revents & POLLIN) {
                continue;
            }

            std::vector<TouchEvent> touches = this->_dri->getEvents();
            std::size_t size = touches.size();
#if DEBUG_PRINT == 1
            // std::cout << "[GR] Touch count " << touches.size() << std::endl;
#endif

            switch(_state) {
                case(RecognizerState::Idle): {
                    if(size == 0) {
                        this->_ofs.resetOrProcess();
                        this->_tfs.resetOrProcess();
                    }else if(size == 1) {
                        this->_ofs.process(touches);
                        this->_tfs.reset();
                    } else if(size == 2) {
                        this->_ofs.reset();
                        this->_tfs.process(touches);
                    } else if(size == 3) {
                        this->_ofs.reset();
                        this->_tfs.reset();
                        //this->_thfs.process(touches);
                    } else {
                        this->_ofs.reset();
                        this->_tfs.reset();
                    }
                } break;
                case(RecognizerState::OneFingerOngoing): {
                    if(size == 0) this->_ofs.resetOrProcess();
                    else if(size == 1) this->_ofs.process(touches);
                    else this->_ofs.reset();
                } break;
                case(RecognizerState::TwoFingerOngoing): {
                    if(size == 0) this->_tfs.resetOrProcess();
                    else if(size > 0 && size < 3) this->_tfs.process(touches);
                    else this->_tfs.reset();
                } break;
            }

// #if DEBUG_PRINT == 1
            // for(int i=0; i<touches.size(); ++i) {
            //     TouchEvent & ev = touches.at(i);
            //     std::string type;
            //     if(ev.type == TouchEvent::Type::Begin) type = "Begin";
            //     else if(ev.type == TouchEvent::Type::Move) type = "Move";
            //     else if(ev.type == TouchEvent::Type::End) type = "End";
            //     std::cerr << "id: " << ev.id <<
            //                 " type : " << type <<
            //                 " x: " << ev.x <<
            //                 " y: " << ev.y << std::endl;
            // } 
// #endif

        }  
    };

    _recognizer = std::thread(iteration);
    return true;
}

void GestureRecognizer::pushGesture(Gesture gest) {
    std::unique_lock<std::mutex> lock(_lock);
    _gesturesQueue.push_back(gest);
}

std::vector<Gesture> GestureRecognizer::fetchGestures() {
    std::vector<Gesture> ret;
    {
        std::unique_lock<std::mutex> lock(_lock);
        ret = std::move(_gesturesQueue);
    }
    return ret;
}

}//namespace