#include <QApplication>
#include <thread>
#include <chrono>
#include <iostream>
#include <atomic>
#include <QProcess>
#include <fstream>    // for std::ifstream
#include <csignal>    // for kill(), SIGTERM
#include <unistd.h>   // for pid_t, kill()

#include "ui/toggle_window.hpp"
#include "include/shared_structs.hpp"
#include "sensors/socket_receiver.hpp"
#include "sensors/imu_thread.hpp"
#include "sensors/gps_thread.hpp"
#include "sensors/threadsafe_queue.hpp" // 공유 큐
#include "logger/database_logger.hpp"
#include "logger/csv_logger.hpp"


extern std::vector<FaceData> face_buffer;
extern std::mutex face_buffer_mutex;
extern std::vector<ImuData> imu_buffer;
extern std::mutex imu_buffer_mutex;
extern std::vector<GpsData> gps_buffer;
extern std::mutex gps_buffer_mutex;
extern const int IMU_BUFFER_MAX_SIZE;
extern const int FACE_BUFFER_MAX_SIZE;

int main(int argc, char *argv[]) {
    // ✅ 0. Python 얼굴 처리 스크립트 실행 (백그라운드)
    std::system("/home/moorim/2025_motionsick_logger_cpp/.venv/bin/python /home/moorim/2025_motionsick_logger_cpp/python/face_processor.py &");

    QApplication app(argc, argv);

    // ✅ 1. 공유 토글 상태 생성
    auto toggle_state = std::make_shared<std::array<std::atomic<int>, 3>>();
    for (int i = 0; i < 3; ++i) {
        (*toggle_state)[i].store(0); // 초기화
    }

    std::atomic<bool> running(true);

    // ✅ FaceData 큐 생성 및 소켓 수신기 실행
    ThreadSafeQueue<FaceData> face_data_queue;
    std::thread socket_thread(socket_receiver, std::ref(face_data_queue), std::ref(running));
    socket_thread.detach();

    // ✅ IMU 큐 및 스레드 실행
    ThreadSafeQueue<ImuData> imu_queue;
    std::thread imuThread(imu_thread, std::ref(imu_queue), std::ref(running));
    imuThread.detach();

    // GPS
    ThreadSafeQueue<GpsData> gps_queue;
    std::thread gps(gps_thread, std::ref(gps_queue), std::ref(running));

    // ✅ DB 로거 인스턴스
    DatabaseLogger db_logger("/home/moorim/2025_motionsick_logger_cpp/data/data_log.db");

    static double last_face_timestamp = 0.0;
    static double last_imu_timestamp = 0.0;
    static double last_gps_timestamp = 0.0;

    std::thread dataAggregatorThread([&db_logger]() {
        while (true) {
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

            for (const auto& face : face_snapshot) {
                if (face.source_timestamp > last_face_timestamp) {
                    db_logger.insertFaceData(face);
                    if (face.source_timestamp > last_face_timestamp)
                        last_face_timestamp = face.source_timestamp;
                }
            }
            for (const auto& imu : imu_snapshot) {
                if (imu.source_timestamp > last_imu_timestamp) {
                    db_logger.insertImuData(imu);
                    if (imu.source_timestamp > last_imu_timestamp)
                        last_imu_timestamp = imu.source_timestamp;
                }
            }
            for (const auto& gps : gps_snapshot) {
                if (gps.source_timestamp > last_gps_timestamp) {
                    db_logger.insertGpsData(gps);
                    last_gps_timestamp = gps.source_timestamp;
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    dataAggregatorThread.detach();

    std::string log_path = "/home/moorim/2025_motionsick_logger_cpp/data/summary_log.csv";
    initialize_csv(log_path);
    start_csv_logger(running, toggle_state, log_path);  // 내부에서 자체 스레드 생성 및 detach 처리

    // ✅ 4. Qt UI 실행
    ToggleWindow window(toggle_state);
    window.show();

    int ret = app.exec();  // run the Qt event loop first

    // 🔚 After the Qt app closes, clean up the Python process
    std::ifstream pid_file("/home/moorim/2025_motionsick_logger_cpp/python/tmp/face_processor.pid");
    int python_pid = 0;
    pid_file >> python_pid;

    if (python_pid > 0) {
        std::cout << "[INFO] Killing Python process with PID: " << python_pid << std::endl;
        kill(python_pid, SIGTERM);  // or SIGKILL if needed
    }

    return ret;
}
