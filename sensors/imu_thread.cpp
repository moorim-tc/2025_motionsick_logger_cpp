#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <deque>
#include <numeric>  // For std::accumulate



#include "imu_thread.hpp"
#include "../include/shared_structs.hpp"

const int BNO055_ADDR = 0x28;
const char *I2C_DEV_PATH = "/dev/i2c-1";

int i2c_fd = -1;

int16_t read16(uint8_t reg) {
    uint8_t buf[2];
    write(i2c_fd, &reg, 1);
    read(i2c_fd, buf, 2);
    return (int16_t)(buf[0] | (buf[1] << 8));
}

std::vector<ImuData> imu_buffer;
std::mutex imu_buffer_mutex;
const int IMU_BUFFER_MAX_SIZE = 50 * 10;

void imu_thread(ThreadSafeQueue<ImuData>& imu_queue, std::atomic<bool>& running) {
    std::cout << "[IMU Thread] Started." << std::endl;

    i2c_fd = open(I2C_DEV_PATH, O_RDWR);
    if (i2c_fd < 0 || ioctl(i2c_fd, I2C_SLAVE, BNO055_ADDR) < 0) {
        std::cerr << "Failed to open BNO055 I2C device." << std::endl;
        return;
    }

    // Set to NDOF mode (optional, depends on your Python config)
    uint8_t opmode_reg[2] = {0x3D, 0x0C}; // NDOF
    write(i2c_fd, opmode_reg, 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Sampling rate check
    std::deque<double> interval_buffer;
    const int max_samples = 100;
    auto last_time = std::chrono::high_resolution_clock::now();

    while (running.load()) {
        // auto now = std::chrono::high_resolution_clock::now();
        // std::chrono::duration<double> interval = now - last_time;
        // last_time = now;

        // interval_buffer.push_back(interval.count());
        // if (interval_buffer.size() > max_samples)
        //     interval_buffer.pop_front();

        // // Print average rate every 1 second
        // if (interval_buffer.size() == max_samples) {
        //     double avg_interval = std::accumulate(interval_buffer.begin(), interval_buffer.end(), 0.0) / interval_buffer.size();
        //     double effective_hz = 1.0 / avg_interval;
        //     std::cout << "[IMU] Effective Sampling Rate: " << effective_hz << " Hz" << std::endl;
        // }

        ImuData data;

        // 현재 시간 기록 (초 단위)
        data.source_timestamp = std::chrono::duration<double>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        // Read linear acceleration (registers: 0x28 to 0x2D)
        int16_t ax = read16(0x08);
        int16_t ay = read16(0x0A);
        int16_t az = read16(0x0C);

        // Transform acceleration to match your Python coordinate logic
        data.accel = {
            static_cast<float>(az) / 100.0f,  // device x
            static_cast<float>(ax) / 100.0f,   // device y
            static_cast<float>(-ay) / 100.0f  // device z
        };

        // Read raw gyro values (X, Y, Z)
        int16_t gyro_x_raw = read16(0x14);  // Gyro X (Pitch)
        int16_t gyro_y_raw = read16(0x16);  // Gyro Y (Roll)
        int16_t gyro_z_raw = read16(0x18);  // Gyro Z (Yaw)

        // 변환: 1/16 deg/s 단위 → deg/s
        float pitch_rate = gyro_x_raw / 16.0f;  // 실제로는 'device Y-axis'
        float roll_rate  = gyro_y_raw / 16.0f;  // 실제로는 'device Z-axis'
        float yaw_rate   = gyro_z_raw / 16.0f;  // 실제로는 'device X-axis'

        // 좌표계 변환 (센서 → 디바이스 기준)
        float device_x_rate = yaw_rate;         // X: 회전축 Z (Yaw)
        float device_y_rate = pitch_rate;       // Y: 회전축 X (Pitch)
        float device_z_rate = -roll_rate;       // Z: 회전축 Y (Roll → 반전 필요)

        // Store heading instead of gyro
        data.gyro = {device_x_rate, device_y_rate, device_z_rate};

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
