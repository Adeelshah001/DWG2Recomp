// Original cooperative pause points for locally generated guest code.
#pragma once
#include <atomic>
#include <cstdint>
extern "C" __declspec(dllimport) std::atomic<uint32_t> Dwg2PauseRequested;
extern "C" __declspec(dllimport) void Dwg2PausePoint();
#define DWG2_PAUSE_POINT() do { if (Dwg2PauseRequested.load(std::memory_order_acquire)) Dwg2PausePoint(); } while (0)
