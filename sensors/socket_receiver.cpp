#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <vector>
#include <netinet/in.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <mutex>

#include "../include/shared_structs.hpp"
#include "threadsafe_queue.hpp"
#include "toggle_window.hpp"

using json = nlohmann::json;

// ✅ 전역 버퍼 및 뮤텍스 선언 (정의는 socket_receiver.hpp 혹은 cpp 상단에서)
std::vector<FaceData> face_buffer;
std::mutex face_buffer_mutex;
const int FACE_BUFFER_MAX_SIZE = 10 * 10;

void socket_receiver(ThreadSafeQueue<FaceData>& face_queue, std::atomic<bool>& running, ToggleWindow* ui_window) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(50007);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 1);
    std::cout << "[SocketReceiver] Waiting for Python connection..." << std::endl;
    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    std::cout << "[SocketReceiver] Connected." << std::endl;

    std::string buffer, line;
    char temp[2048];
    while (running) {
        int bytes_read = read(new_socket, temp, sizeof(temp));
        if (bytes_read <= 0) break;

        buffer.append(temp, bytes_read);

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            try {
                json j = json::parse(line);

                FaceData data;
                data.source_timestamp = j["timestamp"].get<double>();

                // 얼굴 감지 실패: avg_rgb, blendshapes, rotation_matrix 모두 비어 있으면 clear
                bool is_empty_face = j["avg_rgb"].empty() &&
                                    j["blendshapes"].empty() &&
                                    j["rotation_matrix"].empty();

                // ✅ emit face detection signal to UI
                if (ui_window) {
                    emit ui_window->faceDetectionChanged(!is_empty_face);
                }

                if (is_empty_face) {
                    std::lock_guard<std::mutex> lock(face_buffer_mutex);
                    face_buffer.clear();
                    continue;  // skip further processing
                }

                // Blendshapes
                for (auto& [key, val] : j["blendshapes"].items()) {
                    data.blendshapes[key] = val.get<float>();
                }

                // avg_rgb
                for (const auto& val : j["avg_rgb"]) {
                    data.avg_rgb.push_back(val);
                }

                // rotation_matrix (3x3 from 4x4 input)
                const auto& rot_mat_raw = j["rotation_matrix"];
                if (rot_mat_raw.size() >= 3) {
                    for (int i = 0; i < 3; ++i) {
                        std::vector<float> row;
                        for (int jx = 0; jx < 3; ++jx) {
                            row.push_back(rot_mat_raw[i][jx].get<float>());
                        }
                        data.rotation_matrix.push_back(row);
                    }
                }

                // translation_vector (3 elements from 4x4 matrix’s last column)
                const auto& tvec_raw = j["translation_vector"];
                for (int i = 0; i < 3 && i < tvec_raw.size(); ++i) {
                    data.translation_vector.push_back(tvec_raw[i].get<float>());
                }

                // ✅ Save to buffer
                std::lock_guard<std::mutex> lock(face_buffer_mutex);
                face_buffer.push_back(data);
                if (face_buffer.size() > FACE_BUFFER_MAX_SIZE) {
                    face_buffer.erase(face_buffer.begin());
                }

            } catch (const std::exception& e) {
                std::cerr << "[SocketReceiver] JSON parse error: " << e.what() << std::endl;
            }
        }
    }

    close(new_socket);
    close(server_fd);
}
