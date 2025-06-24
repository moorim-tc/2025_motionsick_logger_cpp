#pragma once
#include <atomic>
#include "threadsafe_queue.hpp"
#include "../include/shared_structs.hpp"

void imu_thread(ThreadSafeQueue<ImuData>& imu_queue, std::atomic<bool>& running);
