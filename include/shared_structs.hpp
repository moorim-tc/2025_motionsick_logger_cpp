#pragma once

#include <chrono>
#include <vector>
#include <array>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <string>

struct FaceData {
    double source_timestamp;
    std::unordered_map<std::string, float> blendshapes;
    std::vector<float> avg_rgb;             // size 3: [r, g, b]
    std::vector<std::vector<float>> rotation_matrix; // 3x3 matrix
    std::vector<float> translation_vector;  // size 3
};

struct ImuData {
    double source_timestamp;
    std::vector<float> accel;
    std::vector<float> gyro;
};

struct GpsData {
    double source_timestamp;
    double lat;
    double lon;
    double speed;
};

enum class SensorType {
    FACE,
    IMU
};
struct DBWriteRequest {
    SensorType type;
    std::vector<FaceData> face_batch;
    std::vector<ImuData> imu_batch;
};

extern std::vector<FaceData> face_buffer;
extern std::mutex face_buffer_mutex;

extern std::vector<ImuData> imu_buffer;
extern std::mutex imu_buffer_mutex;

extern const int FACE_BUFFER_MAX_SIZE;
extern const int IMU_BUFFER_MAX_SIZE;

// 각 버튼 상태를 독립적으로 atomic하게 관리
using SharedToggleState = std::shared_ptr<std::array<std::atomic<int>, 3>>;
