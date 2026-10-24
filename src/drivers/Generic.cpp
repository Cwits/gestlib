// Copyright (c) 2025 Cwits
// Licensed under the MIT License. See LICENSE file in the project root for details.

#include "gestlib/drivers/Generic.h"

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

GenericDriver::GenericDriver(const GenericDriverConfig cfg) :
    _cfg(cfg),    
    _raw(cfg.maxFingers)
{
    _maxFingers = cfg.maxFingers;
    
    _idleThreshold = std::chrono::milliseconds(cfg.idleThreshold);
    _activeToLost = std::chrono::milliseconds(cfg.activeToLost);
    _lostToDelete = std::chrono::milliseconds(cfg.lostToDelete);
    _lostDistance = cfg.lostDistance;
    _debounceMax = std::chrono::milliseconds(cfg.debounceThresholdMax);
    _debounceCount = cfg.debounceCount;
    _noiseFilterThresholdMin = cfg.noiseFilterThresholdMin;
    _speedDiv = cfg.speedDiv;

    _internalIdCounter = 0;
    _currentSlot = 0;
    _width = 0;
    _height = 0;
    _dev = nullptr;
    _fd = -1;

    
    _currentFrameFingers.reserve(cfg.maxFingers*2);
    _debounced.reserve(cfg.maxFingers*2);
    _log.reserve(100);
    _rlog.reserve(5000);
}

GenericDriver::~GenericDriver() {
    shutdown();
}

bool GenericDriver::shutdown() {
    if(_dev != nullptr) {
        // libevdev_grab(_dev, LIBEVDEV_UNGRAB);
        libevdev_free(_dev);
        _dev = nullptr;
    }
    if(_fd >= 0) {
        close(_fd);
        _fd = -1;
    }
    return true;
}

std::vector<TouchEvent> GenericDriver::getEvents() {
    bool data = collectRaw();
    return process(data);
}

bool GenericDriver::collectRaw() {      
    bool reading = true;
    bool newData = false;

    while(reading) {
        struct input_event ev;
        int rc = libevdev_next_event(_dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

        if(rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            switch(ev.type) {
                case(EV_ABS): {
                    parseRaw(ev);
                } break;

                case(EV_SYN): {
                    if(ev.code == SYN_REPORT) {
                        std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
                        for(auto &r : _raw) {
                            r.lastAction = now;
                        }
                        reading = false;
                        newData = true;
                    }
                } break;
            }
        } else if(rc == -EAGAIN) { 
            //no events
            reading = false;
        } else if(rc == LIBEVDEV_READ_STATUS_SYNC/* &&
                    ev.type == EV_SYN && ev.code == SYN_DROPPED*/) 
        {
            // std::cerr << "SYN_DROPPED" << std::endl;
            // reading = false;

            do {
                rc = libevdev_next_event(_dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
                //reading lost events
                switch(ev.type) {
                    case(EV_ABS): {
                        parseRaw(ev);
                    } break;

                    case(EV_SYN): {
                        if(ev.code == SYN_REPORT) {
                            std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
                            for(auto &r : _raw) {
                                r.lastAction = now;
                            }
                            // reading = false;
                            newData = true;
                        }
                    } break;
                }
                //i guess need somehow handle them
            } while (rc == LIBEVDEV_READ_STATUS_SYNC);
        }
    }

    return newData;
}

/*
std::vector<TouchEvent> GenericDriver::process1(bool newData) {
    std::vector<TouchEvent> ret;
    // ret.reserve(_maxFingers);
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();

    for(auto &t : _debounced) t.updatedThisFrame = false;

    for(auto &r : _raw) {
        if(r.active) {
            _rlog.push_back(r);
        }
    }

    if(newData) {
        //find those fingers that have same trackId
        _currentFrameFingers.clear();
        for(auto &r : _raw) {
            if(r.active) {
                _currentFrameFingers.push_back(r);
                _rlog.push_back(r);
            }
        }

        for(auto &r : _currentFrameFingers) {
            for(auto &d : _debounced) {
                if(r.trackId == d.lastTrackedId) {
                    d.updatedThisFrame = true;
                    d.lastX = d.x;
                    d.lastY = d.y;
                    d.x = r.x;
                    d.y = r.y;
                    d.lastAction = r.lastAction;

                    r.active = false;
                    break;
                }
            }
        }

        //now in _raw remained active only with different id's, 
        //need to try to match them with existing touched from debounced that are not updated this frame yet .. 
        //are accepted ones with Active or Lost(now - lastAction < threshold) state
        //need to calculate distances between each of active raw fingers and not updatedthisframe fingers?
        
        int squareThreshold = _lostDistance*_lostDistance;
        bool haveActive = false;
        for(auto &r : _raw) { if(r.active) { haveActive = true; break; } }

        if(haveActive) {
            //build matrix
            _resolver.clear();
            
            for(auto &r : _currentFrameFingers) {
                if(r.active) _resolver.newTouches.push_back(&r);
            }
            for(auto &d : _debounced) {
                if(d.state == FingerState::Debouncing) continue;
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - d.lastAction);
                if(d.state == FingerState::Lost && diff > _lostToDelete) continue; //it's too late
                
                _resolver.oldTouches.push_back(&d);
            }

            for(std::size_t row=0; row<_resolver.oldTouches.size(); ++row) {
                DebouncedTouch &ot = *_resolver.oldTouches[row];

                for(std::size_t col=0; col<_resolver.newTouches.size(); ++col) {
                    RawFinger &nt = *_resolver.newTouches[col];
                    float dx = static_cast<float>(ot.x - nt.x);
                    float dy = static_cast<float>(ot.y - nt.y);
                    float sq = (dx * dx) + (dy * dy);
                    float lostSum = _lostDistance + ot.speed;
                    float lostsq = lostSum*lostSum;
                    
                    if(sq > lostsq) continue;

                    _resolver.distanceMatrix[row][col] = std::sqrt(sq);
                }
            }

            // struct MatchCandidate {
            //     std::size_t oldIndex;
            //     std::size_t newIndex;
            //     float distance;
            // };
            
            // std::vector<MatchCandidate> candidates;
            // candidates.reserve(_resolver.oldTouches.size());

            // for(std::size_t row=0; row<_resolver.oldTouches.size(); ++row) {
            //     MatchCandidate &cd = candidates[row];
            //     DebouncedTouch &ot = *_resolver.oldTouches[row];

            //     for(std::size_t col=0; col<_resolver.newTouches.size(); ++col) {
            //         RawFinger &nt = *_resolver.newTouches[col];
            //         cd.cols.push_back(col);
            //     }
            // }

            //try to match
            //try find minimal distance and check that others don't have similar id's
            int size = _resolver.oldTouches.size();
            float min[size] = { std::numeric_limits<float>::infinity() };
            int ids[size] = { -1 };
            int matchedCount[size] = { 0 };
            for(std::size_t row=0; row<_resolver.oldTouches.size(); ++row) {
                // DebouncedTouch &dt = _resolver.oldTouches[row];
                for(std::size_t col=0; col<_resolver.newTouches.size(); ++col) {
                    if(_resolver.distanceMatrix[row][col] < min[row]) {
                        min[row] = _resolver.distanceMatrix[row][col];
                        ids[row] = static_cast<int>(col);
                        matchedCount[row]++;
                    }
                }
            }

                
            for(int row=0; row<size; ++row) {
                if(matchedCount[row] == 0) continue;

                if(matchedCount[row] == 1) {
                    DebouncedTouch &t = *_resolver.oldTouches[row];
                    RawFinger &rf = *_resolver.newTouches[ids[row]];

                    t.updatedThisFrame = true;
                    t.lastX = t.x;
                    t.lastY = t.y;
                    t.x = rf.x;
                    t.y = rf.y;
                    t.lastTrackedId = rf.trackId;
                    t.lastAction = rf.lastAction;
                } else {
                    //need to match somehow
                    // RawFinger * fings[matchedCount[row]] = {nullptr};
                    // for(int i=0; i<matchedCount[row]; ++i) {
                    //     fings[i] = _resolver.newTouches[ids[i]];


                    // }

                    // for(int col=0; col<_resolver.newTouches.size(); ++col) {
                    //     RawFinger &rf = *_resolver.newTouches[col];
                        
                    // }
                }
            }
            
        }

        //distance matrix?

        //for remained in raw - new for debouncing
        for(auto &r : _currentFrameFingers) {
            if(!r.active) continue;
            
            _debounced.push_back({
                .logicalId = _internalIdCounter++,
                .lastTrackedId = r.trackId,
                .state = FingerState::Debouncing,
                .x = r.x,
                .y = r.y,
                .lastX = r.x,
                .lastY = r.y,
                .iterBeforeActive = 0,
                .updatedThisFrame = true,
                .markToDelete = false,
                .lastAction = r.lastAction,
                .firstAction = r.lastAction
            });
            r.active = false;
        }
    }

    //update states, mark ones that must return events

    //create returning events
    // std::vector<TouchEvent> ret;
    // ret.reserve(_maxFingers);


    for(auto& finger : _debounced) {
        switch(finger.state) {
            case(FingerState::Debouncing): {
                if(finger.updatedThisFrame) {
                    finger.iterBeforeActive++;
                    finger.lastAction = now;
                } else {

                }
                
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - finger.firstAction);
                // bool test1 = diff > _debounceMin;
                bool test2 = diff < _debounceMax;
                bool test3 = finger.iterBeforeActive >= _debounceCount;
                
                if(test2 && test3) {
                    finger.state = FingerState::Active;
                    finger.updatedThisFrame = false;
                    finger.lastAction = now;

                    ret.push_back({
                        .id = finger.logicalId,
                        .rawid = finger.lastTrackedId,
                        .type = TouchEvent::Type::Begin,
                        .x = finger.x,
                        .y = finger.y
                    });
                } else if(!test2) {
                    finger.markToDelete = true;
                }
            } break;
            case(FingerState::Active): {
                if(finger.updatedThisFrame) {
                    if(isMoved(finger)) {
                        finger.lastX = finger.x;
                        finger.lastY = finger.y;

                        ret.push_back({
                            .id = finger.logicalId,
                            .rawid = finger.lastTrackedId,
                            .type = TouchEvent::Type::Move,
                            .x = finger.x,
                            .y = finger.y
                        });
                    }
                    
                    finger.updatedThisFrame = false;
                    finger.lastAction = now;
                } else {
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - finger.lastAction);
                    if(diff >= _activeToLost) {
                        finger.state = FingerState::Lost;
                        finger.lastAction = now;
                    }
                }
            } break;
            case(FingerState::Lost): {
                if(finger.updatedThisFrame) {
                    finger.updatedThisFrame = false;
                    finger.state = FingerState::Active;
                    finger.lastAction = now;
                    if(isMoved(finger)) {
                        finger.lastX = finger.x;
                        finger.lastY = finger.y;

                        ret.push_back({
                            .id = finger.logicalId,
                            .rawid = finger.lastTrackedId,
                            .type = TouchEvent::Type::Move,
                            .x = finger.x,
                            .y = finger.y
                        });
                    }
                } else {
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - finger.lastAction);

                    if(diff >= _lostToDelete) {
                        ret.push_back({
                            .id = finger.logicalId,
                            .rawid = finger.lastTrackedId,
                            .type = TouchEvent::Type::End,
                            .x = finger.x,
                            .y = finger.y
                        });
                        finger.markToDelete = true;
                    }
                }
            } break;
        }
    }

    //delete ones must be deleted
    for(auto &d : _debounced) {
        if(d.markToDelete) _log.push_back(d);
    }
    _debounced.erase(
        std::remove_if(
            _debounced.begin(), 
            _debounced.end(),
            [&](const DebouncedTouch& d) {
                return d.markToDelete;
            }
        ), 
        _debounced.end()
    );

    return ret;
}*/

std::vector<TouchEvent> GenericDriver::process(bool newData) {
    std::vector<TouchEvent> ret;
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    
    for(auto & finger : _debounced) {
        finger.updatedThisFrame = false;
    }

    if(newData) {
        _currentFrameFingers.clear();
        for(const auto & raw : _raw) {
            if(raw.active) {
                _rlog.push_back(raw);
                _currentFrameFingers.push_back(raw);
            }
        }

        for(const auto & raw : _currentFrameFingers) {
            auto it = std::find_if(
                _debounced.begin(),
                _debounced.end(),
                [id = raw.trackId](const DebouncedTouch &f) {
                    return id == f.lastTrackedId;
                }
            );
            
            if(it == _debounced.end()) {
                //try to match with lost ones
                for(auto f = _debounced.begin(); f != _debounced.end(); ++f) {
                    if(f->state == FingerState::Debouncing) continue;

                    int dx = f->lastX - raw.x;
                    int dy = f->lastY - raw.y;
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - f->lastAction);

                    int squareSum = (dx*dx) + (dy*dy);
                    // int squareLost = (_lostDistance+f->speed)*(_lostDistance+f->speed);
                    float expRadius = f->speed * diff.count();
                    int squareLost = (_lostDistance+expRadius)*(_lostDistance+expRadius);


                    if(squareSum <= squareLost &&
                        diff < _lostToDelete) 
                    {
                        it = f;
                        break;
                    } else {
                        int breakme = 99;
                        breakme = 666;
                    }
                }
            }

            if(it != _debounced.end()) {
                //update
                it->lastX = it->x;
                it->lastY = it->y;
                it->x = raw.x;
                it->y = raw.y;
                it->prevAction = it->lastAction;
                it->lastAction = raw.lastAction;
                it->lastTrackedId = raw.trackId;
                it->updatedThisFrame = true;

                float dx = it->lastX - it->x;
                float dy = it->lastY - it->y;
                
                float dist = std::sqrt(dx * dx + dy * dy);

                auto timediff = std::chrono::duration_cast<std::chrono::milliseconds>(it->lastAction - it->prevAction).count();
                if(timediff == 0) timediff++;
                float td = (timediff / 1000.f);  //in sec

                it->speed = (dist / td) / _speedDiv;
            } else {
                //create new
                _debounced.push_back({
                    .logicalId = _internalIdCounter++,
                    .lastTrackedId = raw.trackId,
                    .state = FingerState::Debouncing,
                    .x = raw.x,
                    .y = raw.y,
                    .lastX = raw.x,
                    .lastY = raw.y,
                    .iterBeforeActive = 0,
                    .updatedThisFrame = true,
                    .markToDelete = false,
                    .lastAction = raw.lastAction,
                    .firstAction = raw.lastAction
                });
            }
        }
    }

    for(auto& finger : _debounced) {
        switch(finger.state) {
            case(FingerState::Debouncing): {
                if(finger.updatedThisFrame) {
                    finger.iterBeforeActive++;
                    finger.lastAction = now;
                } else {

                }
                
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - finger.firstAction);
                bool test2 = diff < _debounceMax;
                bool test3 = finger.iterBeforeActive >= _debounceCount;
                
                if(test2 && test3) {
                    finger.state = FingerState::Active;
                    finger.updatedThisFrame = false;
                    finger.lastAction = now;

                    ret.push_back({
                        .id = finger.logicalId,
                        .rawid = finger.lastTrackedId,
                        .speed = finger.speed,
                        .type = TouchEvent::Type::Begin,
                        .x = finger.x,
                        .y = finger.y
                    });
                } else if(!test2) {
                    finger.markToDelete = true;
                }
            } break;
            case(FingerState::Active): {
                if(finger.updatedThisFrame) {
                    if(isMoved(finger)) {
                        finger.lastX = finger.x;
                        finger.lastY = finger.y;

                        ret.push_back({
                            .id = finger.logicalId,
                            .rawid = finger.lastTrackedId,
                            .speed = finger.speed,
                            .type = TouchEvent::Type::Move,
                            .x = finger.x,
                            .y = finger.y
                        });
                    }
                    
                    finger.updatedThisFrame = false;
                    finger.lastAction = now;
                } else {
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - finger.lastAction);
                    if(diff >= _activeToLost) {
                        finger.state = FingerState::Lost;
                        finger.lastAction = now;
                    }
                }
            } break;
            case(FingerState::Lost): {
                if(finger.updatedThisFrame) {
                    finger.updatedThisFrame = false;
                    finger.state = FingerState::Active;
                    finger.lastAction = now;
                    if(isMoved(finger)) {
                        finger.lastX = finger.x;
                        finger.lastY = finger.y;

                        ret.push_back({
                            .id = finger.logicalId,
                            .rawid = finger.lastTrackedId,
                            .speed = finger.speed,
                            .type = TouchEvent::Type::Move,
                            .x = finger.x,
                            .y = finger.y
                        });
                    }
                } else {
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - finger.lastAction);

                    if(diff >= (_lostToDelete)) {
                        ret.push_back({
                            .id = finger.logicalId,
                            .rawid = finger.lastTrackedId,
                            .speed = finger.speed,
                            .type = TouchEvent::Type::End, 
                            .x = finger.x,
                            .y = finger.y
                        });
                        finger.markToDelete = true;
                    }
                }
            } break;
        }
    }

    for(auto &d : _debounced) {
        if(d.markToDelete) _log.push_back(d);
    }
    _debounced.erase(
        std::remove_if(
            _debounced.begin(), 
            _debounced.end(),
            [&](const DebouncedTouch& finger) {
                return finger.markToDelete;
            }
        ), 
        _debounced.end()
    );

    return ret;
}

inline bool GenericDriver::isMoved(const DebouncedTouch &finger) {
    int threshold_min = _noiseFilterThresholdMin * _noiseFilterThresholdMin;
    
    int dx = finger.x - finger.lastX;
    int dy = finger.y - finger.lastY;
    int res = (dx*dx) + (dy*dy);
    return res > threshold_min;
}

inline void GenericDriver::parseRaw(const input_event &ev) {
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
}

bool GenericDriver::init(bool pickFirstDev, std::string path) {
    if(pickFirstDev) {
        std::vector<DeviceDesc> devs = listDevices();
        if(!devs.size()) {
            std::cerr << "No devices found to connect" << std::endl;
            return false;
        }

        path = devs.at(0).path;
    }

    if(path.empty()) { 
        std::cerr << "Specify path to device, or use pichFirstDev = true";
        return false;
    }

    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if(fd < 0) {
        std::cerr << "Failed to open device " << path << std::endl;
        return false;
    }

    libevdev * dev = nullptr;
    int rc = libevdev_new_from_fd(fd, &dev);
    if(rc < 0) {
        std::cerr << "Failed to init libevdev or no permissions" << std::endl;
        close(fd);
        return false;
    }

    //TODO: if app terminates unexpectedly(crash or smth) than regular input won't be restored...?
    // rc = libevdev_grab(dev, LIBEVDEV_GRAB);
    // if(rc < 0) {
        // sudo usermod -a -G input $USER
        // std::cerr << "no permisions. Try running with sudo or adding your user to the 'input' group.\n" << std::endl;
    // }

    _fd = fd;
    _dev = dev;
    _name = std::string(libevdev_get_uniq(dev));
    _path = path;

    const struct input_absinfo* info = libevdev_get_abs_info(dev, ABS_MT_POSITION_X);
    _width = info->maximum;
    info = libevdev_get_abs_info(dev, ABS_MT_POSITION_Y);
    _height = info->maximum;

    
    info = libevdev_get_abs_info(dev, ABS_MT_SLOT);
    int slots = info->maximum + 1;

    if(_maxFingers != slots) {
        std::cerr << "Configured maximum touches (" << _maxFingers 
                << ") not supported by this touchscreen (" << slots 
                << "), adjusting to " << slots << std::endl;

        if(slots > _maxFingers) {
            _raw.resize(slots);
            _currentFrameFingers.reserve(slots*2);
            _debounced.reserve(slots*2);
        }
        _maxFingers = slots;
    }

    _resolver.prepare(_maxFingers);
    _resolver.clear();

    std::cout << "Inited to " << path << std::endl;

    return true;
}

std::vector<DeviceDesc> GenericDriver::listDevices() const {
    std::vector<DeviceDesc> ret;

    std::string path = "/dev/input/";
    std::size_t lastSlash = 11;
    DIR *ent = opendir(path.c_str());

    if(!ent) return ret;

    errno = 0;
    struct dirent *entry = nullptr;

    while(entry = readdir(ent)) {
        std::string name = std::string(entry->d_name);
        if(name.find("event") == std::string::npos) continue;

        path = path.substr(0, lastSlash).append(name);
        struct stat fileinfo;
        int err = stat(path.c_str(), &fileinfo);

        if(err) continue;

        if(!S_ISCHR(fileinfo.st_mode)) continue;

        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if(fd < 0) continue;

        libevdev *dev = nullptr;
        int rc = libevdev_new_from_fd(fd, &dev);
        if(rc < 0) {
            std::cerr << "Failed to init libevdev" << std::endl;
            close(fd);
            break;
        }

        bool hasmt = libevdev_has_event_code(dev, EV_ABS, ABS_MT_SLOT);
        bool hasid = libevdev_has_event_code(dev, EV_ABS, ABS_MT_TRACKING_ID);
        bool hasx = libevdev_has_event_code(dev, EV_ABS, ABS_MT_POSITION_X);
        bool hasy = libevdev_has_event_code(dev, EV_ABS, ABS_MT_POSITION_Y);
        
        if(!hasmt && !hasid && !hasx && !hasy) {
            // std::cerr << "invalid device" << std::endl;
            //invalid device
            libevdev_free(dev);
            close(fd);
            continue;
        }

        DeviceDesc desc;
        desc.path = path;
        desc.device = std::string(libevdev_get_name(dev));
        desc.vendorId = std::to_string(libevdev_get_id_vendor(dev));
        desc.productId = std::to_string(libevdev_get_id_product(dev));
        desc.bustype = std::to_string(libevdev_get_id_bustype(dev));
        const char * q = libevdev_get_uniq(dev);
        desc.name = q ? std::string(q) : "";
        desc.driver = std::to_string(libevdev_get_driver_version(dev));
        desc.driverfw = std::to_string(libevdev_get_id_version(dev));

        const struct input_absinfo *info = libevdev_get_abs_info(dev, ABS_MT_SLOT);
        desc.mtslots = info->maximum + 1;

        info = libevdev_get_abs_info(dev, ABS_MT_POSITION_X);
        desc.minWidth = info->minimum;
        desc.maxWidth = info->maximum;
        desc.widthResolution = info->resolution;

        info = libevdev_get_abs_info(dev, ABS_MT_POSITION_Y);
        desc.minHeight = info->minimum;
        desc.maxHeight = info->maximum;
        desc.heightResolution = info->resolution;

        libevdev_free(dev);
        close(fd);
        ret.push_back(desc);
    }

    closedir(ent);
    return ret;
}


void DeviceDesc::print() {
    std::string sep = "-----------------------------------------";
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << "Path: " << path << std::endl;
    std::cout << "Device: " << device << std::endl;
    std::cout << "VendorId: " << vendorId << std::endl;
    std::cout << "ProductId: " << productId << std::endl;
    std::cout << "BusType: " << bustype << std::endl;
    std::cout << "Name: " << name << std::endl;
    std::cout << "Driver: " << driver << std::endl;
    std::cout << "Driver Firmware Version: " << driverfw << std::endl;
    std::cout << std::endl;
    std::cout << "MT Slots: " << mtslots << std::endl;
    std::cout << "Min Width: " << minWidth << " Max Width: " << maxWidth << " Width Res: " << widthResolution << std::endl;
    std::cout << "Height: " << minHeight << " Height: " << maxHeight << " Height Res: " << heightResolution << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
}

void GenericDriver::TouchResolver::prepare(int maxFingers) {
    distanceMatrix.resize(maxFingers);
    for(auto &v : distanceMatrix) {
        v.resize(maxFingers);
    }

    newTouches.reserve(maxFingers);
    oldTouches.reserve(maxFingers);
}

void GenericDriver::TouchResolver::clear() {
    newTouches.clear();
    oldTouches.clear();
    for(auto &v : distanceMatrix) {
        for(auto &val : v) val = std::numeric_limits<float>::infinity();
    }
}

}