#pragma once

#include <chrono>

class Timer {

public:

	Timer() : start(std::chrono::high_resolution_clock::now()) {};

	Timer(const Timer&) = default;
	~Timer() = default;

	Timer& operator=(const Timer&) = default;

	void reset() {
		start = std::chrono::high_resolution_clock::now();
	}

	double elapsed() const {
		auto duration = std::chrono::high_resolution_clock::now() - start;
		return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0;
	}

	double elapsed_s() const {
		auto duration = std::chrono::high_resolution_clock::now() - start;
		return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
	}

	double elapsed_ms() const {
		auto duration = std::chrono::high_resolution_clock::now() - start;
		return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
	}

	double elapsed_us() const {
		auto duration = std::chrono::high_resolution_clock::now() - start;
		return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
	}


private:

	std::chrono::time_point<std::chrono::high_resolution_clock> start;
};