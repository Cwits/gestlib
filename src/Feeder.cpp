#include "feeder.h"

#include <unordered_map>
#include <chrono>
#include <iostream>

const std::unordered_map<int, std::string> TYPES = {
    { 0, "EV_SYN" },
    { 1,  "EV_KEY" },
    { 2, "EV_REL" },
    { 3, "EV_ABS" },
    { 4, "EV_MSC" },
    { 5, "EV_SW" },
    { 17, "EV_LED" },
    { 18, "EV_SND" },
    { 20, "EV_REP" },
    { 21, "EV_FF" },
    { 22, "EV_PWR" },
    { 23, "EV_FF_STATUS" }
};


const std::unordered_map<int, std::string> CODES = {
    // Keys
    { 330, "BTN_TOUCH" },

    // Absolute axes
    { 0, "ABS_X" },
    { 1, "ABS_Y" },
    { 2, "ABS_Z" },
    { 3, "ABS_RX" },
    { 4, "ABS_RY" },
    { 5, "ABS_RZ" },

    { 24, "ABS_PRESSURE" },

    { 47, "ABS_MT_SLOT" },
    { 53, "ABS_MT_POSITION_X" },
    { 54, "ABS_MT_POSITION_Y" },
    { 55, "ABS_MT_TOOL_TYPE" },
    { 56, "ABS_MT_BLOB_ID" },
    { 57, "ABS_MT_TRACKING_ID" },
    { 58, "ABS_MT_PRESSURE" },
    { 59, "ABS_MT_DISTANCE" },
    { 60, "ABS_MT_TOOL_X" },
    { 61, "ABS_MT_TOOL_Y" }
};

void prevent(const input_event &ev) {
	std::cout << "tv sec: " << ev.time.tv_sec << std::endl;
	std::cout << "tv usec: " << ev.time.tv_usec << std::endl;
	std::cout << "type: " << (int)ev.type << " or " << TYPES.at(ev.type) << std::endl;
	std::cout << "code: " << (int)ev.code << " or " << CODES.at(ev.code) << std::endl;
	std::cout << "value: " << (int)ev.value << std::endl;
	std::cout << std::endl;
}

bool feeder::prepare(std::string path) {
	if(path.empty()) {
		prl("Path is empty");
		return false;
	}
	
	_in.open(path, std::ios::binary | std::ios::in);
	
	if(!_in.is_open()) {
		prl("failed to open");
		return false;
	}
	
	std::size_t len = 0;
	
	_in.seekg(0, std::ios::end);
	len = _in.tellg();
	_in.seekg(0);
	
	std::cout << "len " << len 
            << " bytes, input_event size: "  << sizeof(input_event) 
            << ", total events to read: " << len/sizeof(input_event) << std::endl;
	
	len /= sizeof(input_event);
	_events.reserve( len );
	
	while(!_in.eof()) {
		input_event ev;
		_in.read((char*)&ev, sizeof(input_event));
		
		_events.push_back(ev);
	}
	
	if(_in.bad()) {
		std::cout << "reading went wrong" << std::endl;
		return false;
	}
	
	_in.close();
	
	_events.erase(_events.end()-1);
	
	if(len != _events.size()) {
		std::cout << "len " << len << " and size " << _events.size() << " don't match" << std::endl;
		return false;
	}
	
	std::cout << "readed " << _events.size() << " events" << std::endl;
	return true;
}

void feeder::start(std::chrono::milliseconds startDelay)  {
	timeval ref = _events.at(0).time;
	std::size_t eventsprocessed = 0;
	std::chrono::steady_clock::time_point start;
	
	uint64_t refusec = (ref.tv_sec*1000000) + ref.tv_usec;
	
	std::this_thread::sleep_for(startDelay);
	
	start = std::chrono::steady_clock::now();
	
	while(eventsprocessed != _events.size()) {
		auto ntp = std::chrono::steady_clock::now();
		auto timediff = std::chrono::duration_cast<std::chrono::microseconds>(ntp - start).count();
		
		const input_event &ie = _events.at(eventsprocessed);
		//uint64_t sec = ie.time.tv_sec - ref.tv_sec;
		//uint64_t usec = sec * 1000000;
		uint64_t us1 = (ie.time.tv_sec*1000000) + ie.time.tv_usec;
		uint64_t res = us1 - refusec;
		
		if(timediff < res) continue;
		
		{
			std::lock_guard<std::mutex> l(_liveLock);
			_liveFeed.push(ie);
		}
		
		//prevent(ie);
		eventsprocessed++;
	}
}

int feeder::next_event(input_event *ev) {
	std::lock_guard<std::mutex> lock(_liveLock);
	if(_liveFeed.size()) {
		*ev = _liveFeed.front();
		_liveFeed.pop();
		return LIBEVDEV_READ_STATUS_SUCCESS;
	} else {
		return -EAGAIN;
	}
	
	//somewhere
}