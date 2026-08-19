#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <utility>

namespace CH {

/**
 * Times a block of work and reports on destruction.
 *
 * The message is deliberately worded like the "took <seconds>" that TraNSIT's Python BlockTimer
 * emits, so that one grep over a run's log finds every timing in it, whether it came from Python
 * or from here. Output is flushed, because it interleaves with a Python logger writing to the same
 * terminal and unflushed C++ output arrives in the wrong place.
 */
class BlockTimer {
public:
    explicit BlockTimer(std::string description)
        : description_(std::move(description)), start_(std::chrono::steady_clock::now()) {}

    ~BlockTimer() {
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start_;
        std::cout << description_ << " took " << elapsed.count() << std::endl;
    }

    BlockTimer(const BlockTimer&) = delete;
    BlockTimer& operator=(const BlockTimer&) = delete;

private:
    std::string description_;
    std::chrono::steady_clock::time_point start_;
};

}  // namespace CH
