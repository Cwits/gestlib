#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <linux/input.h>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>

struct Feeder {
	
	bool prepare(std::string path);
	
	void start(std::chrono::milliseconds startDelay);
	int next_event(input_event *ev);
	
	private:
	std::fstream _in;
	std::vector<input_event> _events;
	
	std::mutex _liveLock;
	std::queue<input_event> _liveFeed;
};

void prevent(const input_event &ev);