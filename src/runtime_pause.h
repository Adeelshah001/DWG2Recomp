// Original runtime pause state. UI stays alive; guest execution parks cooperatively.
#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
extern "C" __declspec(dllexport) std::atomic<uint32_t> Dwg2PauseRequested;
namespace dwg2::pause {
inline std::mutex mutex;
inline std::condition_variable resume;
inline uint64_t started=0, excluded=0, last_active=0;
inline bool closing=false;
inline bool Requested() { return Dwg2PauseRequested.load(std::memory_order_acquire)!=0; }
inline uint64_t ActiveTicks(uint64_t raw) {
 std::lock_guard lock(mutex);
 const uint64_t sample=Requested()?started:raw;
 const uint64_t active=sample>=excluded?sample-excluded:0;
 // Concurrent readers may have sampled the host clock before a UI transition.
 if(active>last_active)last_active=active;
 return last_active;
}
inline uint64_t ExcludedTicks(uint64_t raw) {
 std::lock_guard lock(mutex);
 return excluded+(Requested()&&raw>started?raw-started:0);
}
inline bool Set(bool value,uint64_t raw) {
 {std::lock_guard lock(mutex);
  if(value&&closing)return false;
  if(value==Requested())return true;
  if(value)started=raw;
  else if(raw>started)excluded+=raw-started;
  Dwg2PauseRequested.store(value?1:0,std::memory_order_release);
 }
 if(!value)resume.notify_all();
 return true;
}
inline void Point() {
 if(!Requested())return;
 std::unique_lock lock(mutex);
 resume.wait(lock,[]{return !Requested()||closing;});
}
inline void Shutdown(uint64_t raw) {
 Set(false,raw);
 {std::lock_guard lock(mutex);closing=true;}
 resume.notify_all();
}
}
