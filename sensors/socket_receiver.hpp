#pragma once
#include "../include/shared_structs.hpp"
#include "threadsafe_queue.hpp"
#include <atomic>
#include <vector>
#include <mutex>

extern std::vector<FaceData> face_buffer;
extern std::mutex face_buffer_mutex;

void socket_receiver(ThreadSafeQueue<FaceData>& queue, std::atomic<bool>& running, ToggleWindow* ui_window);
