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
#include <iomanip>
#include <algorithm>
#include <filesystem> 

#include "../include/shared_structs.hpp"
#include "estimate_heart_rate_from_rgb.hpp"

extern std::vector<FaceData> face_buffer;
extern std::mutex face_buffer_mutex;
extern std::vector<ImuData> imu_buffer;
extern std::mutex imu_buffer_mutex;
extern std::vector<GpsData> gps_buffer;
extern std::mutex gps_buffer_mutex;

namespace {
    double compute_rms(const std::vector<float>& values) {
        if (values.empty()) return 0.0;
        double sum_sq = 0.0;
        for (float v : values) sum_sq += v * v;
        return std::sqrt(sum_sq / values.size());
    }
    
    // 1차 선형회귀로 R^2 계산
    double compute_r2(const std::vector<double>& x, const std::vector<double>& y) {
        if (x.size() != y.size() || x.empty()) return 0.0;

        const size_t n = x.size();
        double mean_x = std::accumulate(x.begin(), x.end(), 0.0) / n;
        double mean_y = std::accumulate(y.begin(), y.end(), 0.0) / n;

        double Sxy = 0.0, Sxx = 0.0, Syy = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double dx = x[i] - mean_x;
            double dy = y[i] - mean_y;
            Sxy += dx * dy;
            Sxx += dx * dx;
            Syy += dy * dy;
        }

        double r2 = (Sxy * Sxy) / (Sxx * Syy + 1e-9);  // 1e-9: divide-by-zero 방지
        return r2;
    }
}

const std::vector<std::string> blend_shape_keys = {
        "browDownLeft", "browDownRight", "browInnerUp", "browOuterUpLeft", "browOuterUpRight",
        "cheekPuff", "cheekSquintLeft", "cheekSquintRight", "eyeBlinkLeft", "eyeBlinkRight",
        "eyeLookDownLeft", "eyeLookDownRight", "eyeLookInLeft", "eyeLookInRight",
        "eyeLookOutLeft", "eyeLookOutRight", "eyeLookUpLeft", "eyeLookUpRight",
        "eyeSquintLeft", "eyeSquintRight", "eyeWideLeft", "eyeWideRight",
        "jawForward", "jawLeft", "jawOpen", "jawRight",
        "mouthClose", "mouthDimpleLeft", "mouthDimpleRight", "mouthFrownLeft", "mouthFrownRight",
        "mouthFunnel", "mouthLeft", "mouthLowerDownLeft", "mouthLowerDownRight",
        "mouthPressLeft", "mouthPressRight", "mouthPucker", "mouthRight",
        "mouthRollLower", "mouthRollUpper", "mouthShrugLower", "mouthShrugUpper",
        "mouthSmileLeft", "mouthSmileRight", "mouthStretchLeft", "mouthStretchRight",
        "mouthUpperUpLeft", "mouthUpperUpRight", "noseSneerLeft", "noseSneerRight"
    };

// ✅ Correct: use brace-enclosed initializer and concat using constructor
const std::vector<std::string> headers = [] {
    std::vector<std::string> h = {
        "timestamp", 
        "멀미", "불편함", "불안감", 
        "speed", "trajectory",
        "acc_rms_x", "acc_rms_y", "acc_rms_z", "roll_rate_rms", "pitch_rate_rms", "yaw_rate_rms",
        "hr", "r", "g", "b", "head_tv", "head_rv"
    };
    h.insert(h.end(), blend_shape_keys.begin(), blend_shape_keys.end());
    return h;
}();

void start_csv_logger(std::atomic<bool>& running, 
                    std::shared_ptr<std::array<std::atomic<int>, 3>> toggle_state,
                    const std::string& log_path) {
    std::thread csv_thread([&running, toggle_state, log_path]() {
        std::ofstream file(log_path, std::ios::app);
        if (!file.is_open()) return;

        static std::vector<double> heart_rate_list;

        while (running) {

            // 현재 시간 기록 (초 단위)
            auto now_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm* now_tm = std::localtime(&now_time_t);

            std::ostringstream timestamp_str;
            timestamp_str << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S");
            std::string timestamp = timestamp_str.str();  // "2025-06-24 13:01:32"

            // ✅ 토글 상태 스냅샷
            int toggle0 = (*toggle_state)[0].load();
            int toggle1 = (*toggle_state)[1].load();
            int toggle2 = (*toggle_state)[2].load();  
            
            // 기본값 세팅
            std::map<std::string, double> row;   
            row["멀미"] = toggle0;
            row["불편함"] = toggle1;
            row["불안감"] = toggle2;
            row["speed"] = 0.0;
            row["tragectory"] = 0.0;

            row["acc_rms_x"] = 0.0;
            row["acc_rms_y"] = 0.0;
            row["acc_rms_z"] = 0.0;
            row["roll_rate_rms"] = 0.0;
            row["pitch_rate_rms"] = 0.0;
            row["yaw_rate_rms"] = 0.0;

            row["hr"] = 0.0;
            row["r"] = 0.0;
            row["g"] = 0.0;
            row["b"] = 0.0;

            row["head_tv"] = 0.0;
            row["head_rv"] = 0.0;

            // blendshapes 기본값 0.0으로 초기화
            for (const auto& key : blend_shape_keys) {
                row[key] = 0.0;
            }

            // Get sensor sanpshot
            std::vector<FaceData> face_snapshot;
            std::vector<ImuData> imu_snapshot;
            std::vector<GpsData> gps_snapshot;

            {
                std::lock_guard<std::mutex> lock(face_buffer_mutex);
                face_snapshot = face_buffer;
            }

            {
                std::lock_guard<std::mutex> lock(imu_buffer_mutex);
                imu_snapshot = imu_buffer;
            }

            {
                std::lock_guard<std::mutex> lock(gps_buffer_mutex);
                gps_snapshot = gps_buffer;
            }

            // 전제: face_snapshot 은 150개 이상일 때만 처리
            if (face_snapshot.size() > 99) {
                double t_start = face_snapshot.front().source_timestamp;
                double t_end = face_snapshot.back().source_timestamp;
                double elapsed = t_end - t_start;

                double fps = face_snapshot.size() / elapsed;
                
                // std::cout << "[fps] Estimated fps: " << fps << std::endl;

                // 평균 RGB 추출
                std::vector<float> r_vals, g_vals, b_vals;
                for (const auto& f : face_snapshot) {
                    if (f.avg_rgb.size() == 3) {
                        r_vals.push_back(f.avg_rgb[0]);
                        g_vals.push_back(f.avg_rgb[1]);
                        b_vals.push_back(f.avg_rgb[2]);
                    }
                }

                double r_mean = std::accumulate(r_vals.begin(), r_vals.end(), 0.0f) / r_vals.size();
                double g_mean = std::accumulate(g_vals.begin(), g_vals.end(), 0.0f) / g_vals.size();
                double b_mean = std::accumulate(b_vals.begin(), b_vals.end(), 0.0f) / b_vals.size();
                row["r"] = r_mean;
                row["g"] = g_mean;
                row["b"] = b_mean;

                // POS 알고리즘을 통해 HR 계산
                double hr = estimate_heart_rate_from_rgb(r_vals, g_vals, b_vals, fps);

                // 심박수 리스트에 추가
                if (hr > 30 && hr < 180) { // 유효한 범위 필터링
                    heart_rate_list.push_back(hr);
                    if (heart_rate_list.size() > 30) {
                        heart_rate_list.erase(heart_rate_list.begin());  // 오래된 값 제거
                    }

                    // 평균과 표준편차 계산
                    double mean = std::accumulate(heart_rate_list.begin(), heart_rate_list.end(), 0.0) / heart_rate_list.size();

                    double sq_sum = std::inner_product(heart_rate_list.begin(), heart_rate_list.end(), heart_rate_list.begin(), 0.0);
                    double std_dev = std::sqrt(sq_sum / heart_rate_list.size() - mean * mean);

                    // 이상치 제거 (mean ± std 범위 내 값 필터링)
                    std::vector<double> filtered;
                    for (double val : heart_rate_list) {
                        if (val >= mean - std_dev && val <= mean + std_dev) {
                            filtered.push_back(val);
                        }
                    }

                    double hr_filtered = 0.0;
                    if (!filtered.empty()) {
                        hr_filtered = std::accumulate(filtered.begin(), filtered.end(), 0.0) / filtered.size();
                    }
                    // std::cout << "[HR] Estimated Heart Rate: " << hr_filtered << " bpm" << std::endl;
                    row["hr"] = hr_filtered; // Update HR value
                } 
                

                // Rotation & Translation 속도 계산
                std::vector<double> translation_speeds, rotation_speeds;
                for (size_t i = 1; i < face_snapshot.size(); ++i) {
                    double dt = face_snapshot[i].source_timestamp - face_snapshot[i-1].source_timestamp;
                    if (dt <= 0) continue;

                    // Translation 속도
                    const auto& t1 = face_snapshot[i-1].translation_vector;
                    const auto& t2 = face_snapshot[i].translation_vector;
                    if (t1.size() != 3 || t2.size() != 3) continue;

                    double dx = t2[0] - t1[0];
                    double dy = t2[1] - t1[1];
                    double dz = t2[2] - t1[2];
                    double trans_speed = std::sqrt(dx*dx + dy*dy + dz*dz) / dt;
                    translation_speeds.push_back(trans_speed);

                    // Rotation 속도
                    const auto& r1 = face_snapshot[i-1].rotation_matrix;
                    const auto& r2 = face_snapshot[i].rotation_matrix;

                    if (r1.size() == 3 && r2.size() == 3 &&
                        r1[0].size() == 3 && r2[0].size() == 3) {
                        // 상대 회전 행렬 R_delta = R2 * R1^T
                        std::vector<std::vector<double>> r1_T(3, std::vector<double>(3));
                        for (int r = 0; r < 3; ++r)
                            for (int c = 0; c < 3; ++c)
                                r1_T[r][c] = r1[c][r];

                        std::vector<std::vector<double>> r_delta(3, std::vector<double>(3, 0.0));
                        for (int r = 0; r < 3; ++r)
                            for (int c = 0; c < 3; ++c)
                                for (int k = 0; k < 3; ++k)
                                    r_delta[r][c] += r2[r][k] * r1_T[k][c];

                        // Trace 이용해서 회전 각도 계산
                        double trace = r_delta[0][0] + r_delta[1][1] + r_delta[2][2];
                        double angle_rad = std::acos(std::clamp((trace - 1.0) / 2.0, -1.0, 1.0));
                        double angle_deg_per_sec = angle_rad * 180.0 / M_PI / dt;

                        rotation_speeds.push_back(angle_deg_per_sec);
                    }
                }

                double avg_tv = translation_speeds.empty() ? 0.0 :
                    std::accumulate(translation_speeds.begin(), translation_speeds.end(), 0.0) / translation_speeds.size();
                double avg_rv = rotation_speeds.empty() ? 0.0 :
                    std::accumulate(rotation_speeds.begin(), rotation_speeds.end(), 0.0) / rotation_speeds.size();

                row["head_tv"] = avg_tv;
                row["head_rv"] = avg_rv;
                
                // blend shapes
                std::unordered_map<std::string, std::vector<float>> blendshape_values;

                for (const auto& face : face_snapshot) {
                    for (const auto& [key, value] : face.blendshapes) {
                        blendshape_values[key].push_back(value);
                    }
                }

                for (const auto& [key, values] : blendshape_values) {
                    if (!values.empty()) {
                        double sum = std::accumulate(values.begin(), values.end(), 0.0);
                        row[key] = sum / values.size();
                    } 
                }
            }

            // IMU sensor
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

            row["acc_rms_x"] = ax_rms;
            row["acc_rms_y"] = ay_rms;
            row["acc_rms_z"] = az_rms;
            row["roll_rate_rms"] = gx_rms;
            row["pitch_rate_rms"] = gy_rms;
            row["yaw_rate_rms"] = gz_rms;

            // GPS
            std::vector<double> speed, lat, lon;

            for (const auto& gps : gps_snapshot) {
                speed.push_back(gps.speed);
                lat.push_back(gps.lat);
                lon.push_back(gps.lon);
            }

            double average_speed = 0.0;
            if (!speed.empty()) {
                double sum = std::accumulate(speed.begin(), speed.end(), 0.0);
                average_speed = sum / speed.size();
                row["speed"] = average_speed;
            }

            // R² 계산 (lat = f(lon) 또는 lon = f(lat), 둘 다 가능)
            double r2 = compute_r2(lon, lat);  // 또는 compute_r2(lat, lon);
            row["tragectory"] = r2;
            
            // Writing to File
            for (size_t i = 0; i < headers.size(); ++i) {
                const std::string& col = headers[i];
                if (col == "timestamp") {
                    file << timestamp;
                } else {
                    double value = row.count(col) ? row[col] : 0.0;
                    file << value;
                }

                if (i != headers.size() - 1) file << ",";
            }
            file << "\n";
            file.flush();

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    csv_thread.detach();    
}

// CSV 파일 초기화
void initialize_csv(const std::string& log_path) {       
    
    bool file_exists = std::filesystem::exists(log_path);

    std::ofstream file(log_path, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "⚠️ Failed to open CSV log file: " << log_path << std::endl;
        return;
    }

    if (!file_exists) {
        for (size_t i = 0; i < headers.size(); ++i) {
            file << headers[i];
            if (i != headers.size() - 1) file << ",";
        }
        file << "\n";
        file.flush();
        std::cout << "✅ CSV header written to " << log_path << std::endl;
    }
}

