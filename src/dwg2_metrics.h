#pragma once
#include <atomic>
#include <chrono>
#include <thread>
namespace dwg2_diag {
inline std::atomic<unsigned long long> swaps{0}, presents{0}, audio_read_races{0};
inline std::atomic<double> swap_fps{0}, present_fps{0};
}
