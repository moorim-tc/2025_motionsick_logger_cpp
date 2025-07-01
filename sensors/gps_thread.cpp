#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <deque>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sstream>
#include <cmath>

#include "gps_thread.hpp"
#include "../include/shared_structs.hpp"

const char* GPS_SERIAL_DEV = "/dev/ttyAMA0";
int gps_fd = -1;

std::vector<GpsData> gps_buffer;
std::mutex gps_buffer_mutex;
const int GPS_BUFFER_MAX_SIZE = 10 * 10;

bool open_gps_serial() {
    gps_fd = open(GPS_SERIAL_DEV, O_RDWR | O_NOCTTY | O_NDELAY);
    if (gps_fd == -1) {
        perror("Failed to open GPS serial port");
        return false;
    }

    struct termios options;
    tcgetattr(gps_fd, &options);
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    tcsetattr(gps_fd, TCSANOW, &options);

    return true;
}

std::string read_line_from_gps() {
    char c;
    std::string line;
    while (read(gps_fd, &c, 1) == 1) {
        if (c == '\n') break;
        if (c != '\r') line += c;
    }
    return line;
}

double nmea_to_decimal(const std::string& coord, const std::string& dir) {
    if (coord.empty()) return 0.0;
    double raw = std::stod(coord);
    int deg = static_cast<int>(raw / 100);
    double min = raw - deg * 100;
    double decimal = deg + min / 60.0;
    if (dir == "S" || dir == "W") decimal *= -1;
    return decimal;
}

void gps_thread(ThreadSafeQueue<GpsData>& gps_queue, std::atomic<bool>& running) {
    std::cout << "[GPS Thread] Started." << std::endl;

    if (!open_gps_serial()) {
        std::cerr << "[GPS Thread] Failed to open serial port." << std::endl;
        return;
    }

    while (running.load()) {
        std::string line = read_line_from_gps();

        if (line.find("$GPRMC") != std::string::npos) {
            std::stringstream ss(line);
            std::string field;
            std::vector<std::string> fields;

            while (std::getline(ss, field, ',')) {
                fields.push_back(field);
            }

            if (fields.size() > 8 && fields[2] == "A") {
                GpsData data;
                data.source_timestamp = std::chrono::duration<double>(
                    std::chrono::system_clock::now().time_since_epoch()).count();

                data.lat = nmea_to_decimal(fields[3], fields[4]);
                data.lon = nmea_to_decimal(fields[5], fields[6]);
                data.speed = std::stod(fields[7]) * 1.852;  // knots to km/h

                {
                    std::lock_guard<std::mutex> lock(gps_buffer_mutex);
                    gps_buffer.push_back(data);
                    if (gps_buffer.size() > GPS_BUFFER_MAX_SIZE)
                        gps_buffer.erase(gps_buffer.begin());
                }

                gps_queue.push(data);  // optional if you want to push to logger
                std::cout << "[GPS] FIXED: Lat=" << data.lat
                              << ", Lon=" << data.lon
                              << ", Speed=" << data.speed << " km/h" << std::endl;
            } else {
                std::cout << "[GPS] No fix yet (status = V)" << std::endl;
            }
        } 
        // else {
        //     std::cout << "[GPS] Incomplete $GNRMC sentence skipped." << std::endl;
        // }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // ~50Hz check
    }

    close(gps_fd);
    std::cout << "[GPS Thread] Stopped." << std::endl;
}
