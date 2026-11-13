#pragma once
#include <atomic>
namespace dwg2::resolution {
inline std::atomic<int> requested{0}, completed{0}, error{0};
}
