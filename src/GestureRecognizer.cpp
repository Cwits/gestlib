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

GestureRecognizer::GestureRecognizer() : _ofs(*this), _dri() {
    _running = false;
    _gesturesQueue.clear();
}

GestureRecognizer::~GestureRecognizer() {
    if(_running) shutdown();
}

bool GestureRecognizer::init() {
    if(!_dri.init()) {
        std::cout << "Failed to init real driver, try to use fake one" << std::endl;
        return false;
    }

    if(!_ofs.init(_dri.resolutionX(), _dri.resolutionY())) {
        std::cout << "Failed to init real driver, try to use fake one" << std::endl;
        return false;
    }

    _efd = eventfd(0, EFD_NONBLOCK);
    
    if(_efd == -1) {
        std::cout << "efd error" << std::endl;
        return false;
    }

    _dri.printCapabilities();
    _ofs.reset();
    return true;
}

bool GestureRecognizer::shutdown() {
    _running = false;

    uint64_t u = 1;
    write(_efd, &u, sizeof(u));

    _recognizer.join();
    
    _dri.shutdown();
    close(_efd);

    return true;
}

//TODO: no actual check's here, always returns true!!
bool GestureRecognizer::start() {
    _running = true;

    auto iteration = [this]() {    
        struct pollfd fds[2];
        fds[0].fd = this->_dri.fd();
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

            std::vector<TouchEvent> touches = this->_dri.getEvents();
            std::size_t size = touches.size();
                    
            if(size == 0) {
                this->_ofs.resetOrProcess(/*touches*/);
            }else if(size == 1) {
                this->_ofs.process(touches);
            } else if(size == 2) {
                this->_ofs.reset();
            } else if(size == 3) {
                this->_ofs.reset();
            }    

            // for(int i=0; i<touches.size(); ++i) {
            //     TouchEvent & ev = touches.at(i);
            //     std::string type;
            //     if(ev.type == TouchEvent::Type::Begin) type = "Begin";
            //     else if(ev.type == TouchEvent::Type::Move) type = "Move";
            //     else if(ev.type == TouchEvent::Type::End) type = "End";
            //     std::cout << "id: " << ev.id <<
            //                 " type : " << type <<
            //                 " x: " << ev.x <<
            //                 " y: " << ev.y << std::endl;
            // } 
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