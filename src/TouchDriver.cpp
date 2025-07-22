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
    bool data = readData();
    return process(data);
}

bool TouchDriver::readData() {      
    bool reading = true;
    bool newData = false;

    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
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

std::vector<TouchEvent> TouchDriver::process(bool newData) {
    std::vector<TouchEvent> ret;
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    
    if(newData) {
        _currentFrameFingers.clear();
        for(const auto & raw : _raw) {
            if(raw.active) {
                _currentFrameFingers.push_back(raw);
            }
        }

        for(auto & finger : _logicalFingers) {
            finger.updatedThisFrame = false;
        }

        for(const auto & raw : _currentFrameFingers) {
            LogicalFinger * match = nullptr;

            for(auto & finger : _logicalFingers) {
                if(finger.lastTrackedId == raw.trackId) {
                    match = &finger;
                    break;
                }
            }

            if(!match) {
                for(auto & finger : _logicalFingers) {
                    if(finger.state == FingerState::Lost) {
                        int dx = finger.x - raw.x;
                        int dy = finger.y - raw.y;

                        if(std::abs(dx) <= 30 && std::abs(dy) <= 30 &&
                            std::chrono::duration_cast<std::chrono::milliseconds>(now - finger.lastAction) < LOST_TIMEOUT) {
                            match = &finger;
                            break;
                        }
                    }
                }
            }

            if(match) {
                match->lastX = match->x;
                match->lastY = match->y;
                match->x = raw.x;
                match->y = raw.y;
                match->lastAction = raw.lastAction;
                if(match->state == FingerState::Probation) {
                    match->activeCount++;
                }
                match->lastTrackedId = raw.trackId;
                match->updatedThisFrame = true;
            } else {
                _logicalFingers.push_back({
                    .logicalId = _internalIdCounter++,
                    .lastTrackedId = raw.trackId,
                    .state = FingerState::Probation,
                    .x = raw.x,
                    .y = raw.y,
                    .lastX = raw.x,
                    .lastY = raw.y,
                    .activeCount = 0,
                    .updatedThisFrame = true,
                    .markToDelete = false,
                    .lastAction = raw.lastAction
                });
            }
        }
    }

    for(auto& finger : _logicalFingers) {
        if(finger.state == FingerState::Probation) {
            bool test2 = std::chrono::duration_cast<std::chrono::milliseconds>(now - finger.lastAction) > PROBATION_THRESHOLD;
            bool test3 = finger.activeCount > ACTIVATION_COUNT;
            
            if(!test2 && test3) {
                finger.state = FingerState::Active;
                //touch down event
                ret.push_back({
                    .id = finger.logicalId,
                    .type = TouchEvent::Type::Begin,
                    .x = finger.x,
                    .y = finger.y
                });
                finger.updatedThisFrame = false;
                finger.lastAction = now;
            } else if(test2 && !test3) {
                finger.markToDelete = true;
            }
        } else if(finger.state == FingerState::Lost) {
            if(finger.updatedThisFrame) {
                finger.state = FingerState::Active;
                // probablyMove(finger, ret, now);
                finger.updatedThisFrame = false;
                finger.lastAction = now;
            } else {
                if(std::chrono::duration_cast<std::chrono::milliseconds>(now - finger.lastAction) >= LOST_TIMEOUT) {
                    ret.push_back({
                        .id = finger.logicalId,
                        .type = TouchEvent::Type::End,
                        .x = finger.x,
                        .y = finger.y
                    });
                    finger.markToDelete = true;
                }
            }
        } else { // Active
            if(finger.updatedThisFrame) {
                //move?
                probablyMove(finger, ret);
                finger.updatedThisFrame = false;
                finger.lastAction = now;
            } else {
                if(std::chrono::duration_cast<std::chrono::milliseconds>(now - finger.lastAction) >= ACTIVE_TO_LOST) {
                    finger.state = FingerState::Lost;
                    finger.lastAction = now;
                }
            }
        }
    }


    _logicalFingers.erase(std::remove_if(_logicalFingers.begin(), _logicalFingers.end(),
        [&](const LogicalFinger& finger) {
            return finger.markToDelete;
    }), _logicalFingers.end());

    return ret;
}

void TouchDriver::probablyMove(LogicalFinger & finger, std::vector<TouchEvent> & ret) {
    float threshold_min = 2.0f;
    float threshold_max = 30.0f;
    
    int dx = finger.x - finger.lastX;
    int dy = finger.y - finger.lastY;
    int res = (dx*dx) + (dy*dy);
    if (res >= threshold_min*threshold_min && res <= threshold_max*threshold_max) {
        ret.push_back({
            .id = finger.logicalId,
            .type = TouchEvent::Type::Move,
            .x = finger.x,
            .y = finger.y
        });
        finger.lastX = finger.x;
        finger.lastY = finger.y;
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