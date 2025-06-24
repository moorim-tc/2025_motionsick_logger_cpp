#pragma once
#include <atomic>
#include "threadsafe_queue.hpp"
#include "../include/shared_structs.hpp"

void gps_thread(ThreadSafeQueue<GpsData>& gps_queue, std::atomic<bool>& running);