#include <iostream>
#include <thread>
#include <chrono>
#include <random>

#include "imu_thread.hpp"
#include "../include/shared_structs.hpp"

std::vector<ImuData> imu_buffer;
std::mutex imu_buffer_mutex;
const int IMU_BUFFER_MAX_SIZE = 100;

void imu_thread(ThreadSafeQueue<ImuData>& imu_queue, std::atomic<bool>& running) {
    std::cout << "[IMU Thread] Started." << std::endl;

    // 난수 생성기 초기화 (±2.0 범위)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-2.0f, 2.0f);

    while (running.load()) {
        ImuData data;

        // 현재 시간 기록 (초 단위)
        data.source_timestamp = std::chrono::duration<double>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        // 난수 기반 accel/gyro 데이터
        data.accel = {dist(gen), dist(gen), dist(gen)};
        data.gyro  = {dist(gen), dist(gen), dist(gen)};

        {
            std::lock_guard<std::mutex> lock(imu_buffer_mutex);
            imu_buffer.push_back(data);
            if (imu_buffer.size() > IMU_BUFFER_MAX_SIZE) {
                imu_buffer.erase(imu_buffer.begin()); // 오래된 것 제거
            }
        }

        // 100Hz (10ms 간격)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[IMU Thread] Stopped." << std::endl;
}
