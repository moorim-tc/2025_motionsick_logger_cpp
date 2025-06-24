#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include <cmath>
#include <iostream>
#include <numeric>
#include <map>
#include <atomic>

#include "../include/shared_structs.hpp"

extern std::vector<FaceData> face_buffer;
extern std::mutex face_buffer_mutex;
extern std::vector<ImuData> imu_buffer;
extern std::mutex imu_buffer_mutex;

namespace {
    double compute_rms(const std::vector<float>& values) {
        if (values.empty()) return 0.0;
        double sum_sq = 0.0;
        for (float v : values) sum_sq += v * v;
        return std::sqrt(sum_sq / values.size());
    }
}

void start_csv_logger(std::atomic<bool>& running, std::shared_ptr<std::array<std::atomic<int>, 3>> toggle_state) {
    std::thread csv_thread([&running, toggle_state]() {
        std::ofstream file("summary_log.csv", std::ios::app);
        if (!file.is_open()) return;

        while (running) {
            std::vector<FaceData> face_snapshot;
            std::vector<ImuData> imu_snapshot;

            {
                std::lock_guard<std::mutex> lock(face_buffer_mutex);
                face_snapshot = face_buffer;
            }

            {
                std::lock_guard<std::mutex> lock(imu_buffer_mutex);
                imu_snapshot = imu_buffer;
            }
            
            // 현재 시간 기록 (초 단위)
            auto now_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm* now_tm = std::localtime(&now_time_t);

            // ✅ 토글 상태 스냅샷
            int toggle0 = (*toggle_state)[0].load();
            int toggle1 = (*toggle_state)[1].load();
            int toggle2 = (*toggle_state)[2].load();

            std::vector<float> r_vals, g_vals, b_vals;
            std::map<std::string, std::vector<float>> blendshape_map;

            for (const auto& face : face_snapshot) {
                if (face.avg_rgb.size() >= 3) {
                    r_vals.push_back(face.avg_rgb[0]);
                    g_vals.push_back(face.avg_rgb[1]);
                    b_vals.push_back(face.avg_rgb[2]);
                }
                for (const auto& [k, v] : face.blendshapes) {
                    blendshape_map[k].push_back(v);
                }
            }

            double r_mean = std::accumulate(r_vals.begin(), r_vals.end(), 0.0) / std::max(1.0, (double)r_vals.size());
            double g_mean = std::accumulate(g_vals.begin(), g_vals.end(), 0.0) / std::max(1.0, (double)g_vals.size());
            double b_mean = std::accumulate(b_vals.begin(), b_vals.end(), 0.0) / std::max(1.0, (double)b_vals.size());

            std::vector<float> ax, ay, az, gx, gy, gz;
            for (const auto& imu : imu_snapshot) {
                ax.push_back(imu.accel[0]); ay.push_back(imu.accel[1]); az.push_back(imu.accel[2]);
                gx.push_back(imu.gyro[0]);  gy.push_back(imu.gyro[1]);  gz.push_back(imu.gyro[2]);
            }

            double ax_rms = compute_rms(ax);
            double ay_rms = compute_rms(ay);
            double az_rms = compute_rms(az);
            double gx_rms = compute_rms(gx);
            double gy_rms = compute_rms(gy);
            double gz_rms = compute_rms(gz);

            file << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S") << "," << toggle0 << "," << toggle1 << "," << toggle2;
            file << "," << r_mean << "," << g_mean << "," << b_mean;
            file << "," << ax_rms << "," << ay_rms << "," << az_rms;
            file << "," << gx_rms << "," << gy_rms << "," << gz_rms;

            for (const auto& [k, v] : blendshape_map) {
                double mean = std::accumulate(v.begin(), v.end(), 0.0) / std::max(1.0, (double)v.size());
                file << "," << mean;
            }

            file << "\n";
            file.flush();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    csv_thread.detach();
}
