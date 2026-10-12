// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "gestlib/TouchDriver.h"

#include <libevdev/libevdev.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h> //close(fd)
#include <fcntl.h> //open() O_RDONLY
#include <cstring> //strerror()
#include <iostream>
#include <cmath>
#include <algorithm>

#include <fstream>

namespace GestLib {

constexpr auto IDLE_THRESHOLD = std::chrono::milliseconds(250);
constexpr auto PROBATION_THRESHOLD = std::chrono::milliseconds(50);
constexpr auto ACTIVE_TO_LOST = std::chrono::milliseconds(50);
constexpr auto LOST_TIMEOUT = std::chrono::milliseconds(50);
constexpr int ACTIVATION_COUNT = 4;

TouchDriver::TouchDriver(int maxFingers) :
    _maxFingers(maxFingers),
    _fd(-1),
    _dev(nullptr),
    _resX(0),
    _resY(0),
    _currentSlot(0),
    _raw(maxFingers),
    _currentFrameFingers(maxFingers),
    _debounced(maxFingers),
    _internalIdCounter(0)
{
    _logicalFingers.reserve(maxFingers);
}

TouchDriver::~TouchDriver() {
    // shutdown();
}

bool TouchDriver::shutdown() {
    if(_dev != nullptr) {
        libevdev_grab(_dev, LIBEVDEV_UNGRAB);
        libevdev_free(_dev);
        _dev = nullptr;
    }
    if(_fd >= 0) {
        close(_fd);
        _fd = 0;
    }
    return true;
}

std::vector<TouchEvent> TouchDriver::getEvents() {
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    bool data = readData(now);
    return process(data, now);
}

bool TouchDriver::readData(const std::chrono::time_point<std::chrono::steady_clock> &now) {
    bool reading = true;
    bool newData = false;

    while(reading) {

        struct input_event ev;
        int rc = libevdev_next_event(_dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

        if(rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            switch(ev.type) {
                case(EV_ABS): {
                    switch(ev.code) {
                        case(ABS_MT_SLOT): _currentSlot = ev.value; break;
                        case(ABS_MT_TRACKING_ID): {
                            if(ev.value == -1) {
                                if(_raw[_currentSlot].active) {
                                    _raw[_currentSlot].active = false;
                                }
                            } else {
                                _raw[_currentSlot].trackId = ev.value;
                                _raw[_currentSlot].active = true;
                            }
                        } break;
                        case(ABS_MT_POSITION_X): {
                            _raw[_currentSlot].x = ev.value;
                        } break;
                        case(ABS_MT_POSITION_Y): {
                            _raw[_currentSlot].y = ev.value;
                        } break;
                        case(ABS_MT_TOUCH_MAJOR): {
                            _raw[_currentSlot].touchMajor = ev.value;
                        } break;
                        case(ABS_MT_TOUCH_MINOR): {
                            _raw[_currentSlot].touchMinor = ev.value;
                        } break;
                    }
                } break;

                case(EV_SYN): {
                    if(ev.code == SYN_REPORT) {
                        _raw[_currentSlot].lastAction = now;
                        reading = false;
                        newData = true;
                    }
                } break;
            }
        } else if(rc == -EAGAIN) { 
            //no events
            reading = false;
        } else if(rc == LIBEVDEV_READ_STATUS_SYNC &&
                    ev.type == EV_SYN && ev.code == SYN_DROPPED) {
            
            /* TODO: still didn't understood how this work. */
            std::cerr << "SYN_DROPPED" << std::endl;
            reading = false;
            while (libevdev_next_event(_dev, LIBEVDEV_READ_FLAG_SYNC, &ev) == LIBEVDEV_READ_STATUS_SYNC) {
                //restore devices actual state
                
            }
        }
    }

    return newData;
}

std::vector<TouchEvent> TouchDriver::process(bool newData, const std::chrono::time_point<std::chrono::steady_clock> &now) {
    std::vector<TouchEvent> ret;
    ret.reserve(_maxFingers);
    
    for(auto &f : _debounced) f.updatedThisFrame = false;

    if(newData) {
        _currentFrameFingers.clear();
        for(const auto &r : _raw) {
            if(r.active) _currentFrameFingers.push_back(r);
        }

        for(const auto &r : _currentFrameFingers) {
            DebouncedFinger *match = nullptr;

            //try to match last readed touches with last debounced touches
            for(auto &d : _debounced) {
                if(d.lastTrackedId == r.trackId) {
                    match = &d;
                    break;
                }
            }

            //maybe id jumped, try to find nearest touch
            if(!match) {
                for(auto &f : _debounced) {
                    if(f.state != FingerState::Lost) continue;

                    int dx = f.x - r.x;
                    int dy = f.y - r.y;
                    //maybe should check for id as well? or it isn't guaranteed?
                    if(std::abs(dx) <= 30 && 
                        std::abs(dy) <= 30 &&
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - f.lastActive) < LOST_TIMEOUT) 
                    {
                        match = &f;
                        break;
                    }
                }
            }

            if(!match) {
                //new touch to debounce
                _debounced.push_back({
                    .id = _internalIdCounter++,
                    .lastTrackedId = r.trackId,
                    .state = FingerState::Probation,
                    .x = r.x,
                    .y = r.y,
                    .prevX = r.x,
                    .prevY = r.y,
                    .noiseDebounce = 0,
                    .updatedThisFrame = true,
                    .markToDelete = false,
                    .lastActive = r.lastAction
                });
            } else {
                match->prevX = match->x;
                match->prevY = match->y;
                match->x = r.x;
                match->y = r.y;
                match->lastActive = r.lastAction;
                match->lastTrackedId = r.trackId;
                match->updatedThisFrame = true;
                if(match->state == FingerState::Probation)
                    match->noiseDebounce++;
            }
        }
    }

    for(auto &f : _debounced) {
        switch(f.state) {
            case(FingerState::Probation): {
                bool test1 = std::chrono::duration_cast<std::chrono::milliseconds>(now - f.lastActive) > PROBATION_THRESHOLD;
                bool test2 = f.noiseDebounce > ACTIVATION_COUNT;

                //if got more activations within PROBATION_THRESHOLD
                if(!test1 && test2) {
                    //passed debounce
                    ret.push_back({
                        .id = f.id,
                        .type = TouchEvent::Type::Begin,
                        .x = f.x,
                        .y = f.y
                    });
                    f.state = FingerState::Active;
                    f.lastActive = now;
                } else if(test1) {
                    f.markToDelete = true;
                }
            } break;
            case(FingerState::Active): {
                if(f.updatedThisFrame) {
                    probablyMove(f, ret);
                    f.lastActive = now;
                } else {
                    if(std::chrono::duration_cast<std::chrono::milliseconds>(now - f.lastActive) >= ACTIVE_TO_LOST) {
                        f.state = FingerState::Lost;
                        f.lastActive = now;
                    }
                }
            } break;
            case(FingerState::Lost): {
                if(f.updatedThisFrame) {
                    f.state = FingerState::Active;
                    f.lastActive = now;
                } else {
                    if(std::chrono::duration_cast<std::chrono::milliseconds>(now - f.lastActive) >= LOST_TIMEOUT) {
                        ret.push_back({
                            .id = f.id,
                            .type = TouchEvent::Type::End,
                            .x = f.x,
                            .y = f.y
                        });
                        f.markToDelete = true;
                    }
                }
            } break;
        }
    }
    
    _debounced.erase(
        std::remove_if(
            _debounced.begin(), 
            _debounced.end(),
            [&](const DebouncedFinger& finger) {
                return finger.markToDelete;
            }
        ), 
        _debounced.end()
    );

    return ret;
}

void TouchDriver::probablyMove(DebouncedFinger & finger, std::vector<TouchEvent> & ret) {
    int threshold_min = 2;
    int threshold_max = 30;
    
    int dx = finger.x - finger.prevX;
    int dy = finger.y - finger.prevY;
    int res = (dx*dx) + (dy*dy);
    if (res >= threshold_min*threshold_min && res <= threshold_max*threshold_max) {
        ret.push_back({
            // .id = finger.logicalId,
            .id = finger.id,
            .type = TouchEvent::Type::Move,
            .x = finger.x,
            .y = finger.y
        });
        finger.prevX = finger.x;
        finger.prevY = finger.y;
    }
}

bool TouchDriver::init() {
    bool found = false;
    //look for touchscreens
    DIR *ent = opendir("/dev/input");
    std::string path = "/dev/input/";
    std::size_t lastSlash = 11;
            
    if(ent) {
        errno = 0;
        struct dirent *entry = nullptr;
        while(entry = readdir(ent)) {
            std::string name = std::string(entry->d_name);
            if(name.find("event") != std::string::npos) {
                path = path.substr(0, lastSlash).append(name);
                        
                struct stat fileinfo;
                int err = stat(path.c_str(), &fileinfo);


                if(err) {
                    std::cerr << "No suitable touchscreen found" << std::endl;
                    break;
                }

                if(S_ISCHR(fileinfo.st_mode)) {
                    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
                    if(fd >= 0) {
                        libevdev * dev = nullptr;
                        int rc = libevdev_new_from_fd(fd, &dev);
                        if(rc<0) {
                            std::cerr << "Failed to init libevdev" << std::endl;
                        }

                        //TODO: if app terminates unexpectedly(crash or smth) than regular input won't be restored...?
                        rc = libevdev_grab(dev, LIBEVDEV_GRAB);
                        if(rc < 0) {
                            //sudo usermod -a -G input $USER
                            std::cerr << "no permisions. Try running with sudo or adding your user to the 'input' group.\n" << std::endl;
                        }
                        bool res = libevdev_has_event_code(dev, EV_ABS, ABS_MT_SLOT) &&
                                        libevdev_has_event_code(dev, EV_ABS, ABS_MT_TRACKING_ID) &&
                                        libevdev_has_event_code(dev, EV_ABS, ABS_MT_POSITION_X)  &&
                                        libevdev_has_event_code(dev, EV_ABS, ABS_MT_POSITION_Y);
                            
                        if(res) {
                            _fd = fd;
                            _dev = dev;
                            _uniq = std::string(libevdev_get_uniq(dev));
                            _path = path;
                            found = true;

                            //get resolution
                            const struct input_absinfo* info = libevdev_get_abs_info(dev, ABS_MT_POSITION_X);
                            _resX = info->maximum;
                            info = libevdev_get_abs_info(dev, ABS_MT_POSITION_Y);
                            _resY = info->maximum;
                            break;
                        } else {
                            libevdev_free(dev);
                            close(fd);
                            continue;
                        }
                    }
                }
            }
        }
                
        closedir(ent);
    }

    return found;
}

void TouchDriver::printCapabilities() {
    libevdev * dev = _dev;
    std::cout << "Device: " << libevdev_get_name(dev) << std::endl;
    std::cout << "Vendor id: " << libevdev_get_id_vendor(dev) << std::endl;
    std::cout << "Product id: " << libevdev_get_id_product(dev) << std::endl;
    std::cout << "Bus type: " << libevdev_get_id_bustype(dev) << std::endl;
    std::cout << "Name: " << _uniq << std::endl;
    std::cout << "Driver: " << libevdev_get_driver_version(dev) << std::endl;
    std::cout << "Driver firmware version: " << libevdev_get_id_version(dev) << std::endl;

    if (libevdev_has_event_code(dev, EV_ABS, ABS_MT_SLOT)) {
        const struct input_absinfo* slots = libevdev_get_abs_info(dev, ABS_MT_SLOT);
        std::cout << "\nMT slots supported (" << (slots ? slots->maximum + 1 : 0) << " slots)\n";
    }

    std::cout << "\n Supported features:\n";
    for (int code = 0; code <= ABS_MAX; ++code) {
        if (libevdev_has_event_code(dev, EV_ABS, code)) {
            const struct input_absinfo* info = libevdev_get_abs_info(dev, code);
            if (!info) continue;

            std::string name = libevdev_event_code_get_name(EV_ABS, code);
            std::cout << "  " << name
                    << " [min=" << info->minimum
                    << ", max=" << info->maximum
                    << ", flat=" << info->flat
                    << ", fuzz=" << info->fuzz
                    << ", res=" << info->resolution << "]\n";
        }
    }

    std::cout << "\n";
}

}