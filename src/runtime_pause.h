// Original runtime pause state. UI stays alive; guest execution parks cooperatively.
#pragma once
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
extern "C" __declspec(dllexport) std::atomic<uint32_t> Dwg2PauseRequested;
namespace dwg2::pause {
inline std::mutex mutex;
inline std::condition_variable resume;
inline std::atomic<uint64_t> started{0}, excluded{0}, last_active{0}, seq{0};
inline bool closing=false;
inline bool Requested() { return Dwg2PauseRequested.load(std::memory_order_acquire)!=0; }
inline uint64_t UpdateLastActive(uint64_t active) {
 uint64_t prev=last_active.load(std::memory_order_relaxed);
 while(active>prev&&!last_active.compare_exchange_weak(prev,active,std::memory_order_acq_rel,std::memory_order_relaxed)){}
 return std::max(prev,active);
}
inline uint64_t ActiveTicks(uint64_t raw) {
 uint64_t sample=0,ex=0;
 for(;;) {
  const uint64_t s1=seq.load(std::memory_order_acquire);
  if(s1&1u)continue;
  const bool req=Requested();
  sample=req?started.load(std::memory_order_relaxed):raw;
  ex=excluded.load(std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_acquire);
  if(seq.load(std::memory_order_relaxed)==s1)break;
 }
 const uint64_t active=sample>=ex?sample-ex:0;
 // Concurrent readers may have sampled the host clock before a UI transition.
 return UpdateLastActive(active);
}
inline uint64_t ExcludedTicks(uint64_t raw) {
 uint64_t st=0,ex=0;bool req=false;
 for(;;) {
  const uint64_t s1=seq.load(std::memory_order_acquire);
  if(s1&1u)continue;
  req=Requested();
  st=started.load(std::memory_order_relaxed);
  ex=excluded.load(std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_acquire);
  if(seq.load(std::memory_order_relaxed)==s1)break;
 }
 return ex+(req&&raw>st?raw-st:0);
}
inline bool Set(bool value,uint64_t raw) {
 {std::lock_guard lock(mutex);
  if(value&&closing)return false;
  if(value==Requested())return true;
  seq.fetch_add(1,std::memory_order_release);
  if(value)started.store(raw,std::memory_order_relaxed);
  else if(raw>started.load(std::memory_order_relaxed))excluded.fetch_add(raw-started.load(std::memory_order_relaxed),std::memory_order_relaxed);
  Dwg2PauseRequested.store(value?1:0,std::memory_order_release);
  seq.fetch_add(1,std::memory_order_release);
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
