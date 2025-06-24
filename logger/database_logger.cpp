#include "database_logger.hpp"
#include <sstream>
#include <iostream>

DatabaseLogger::DatabaseLogger(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db)) {
        std::cerr << "[DB] Failed to open: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
    } else {
        createTablesIfNotExist();
    }
}

DatabaseLogger::~DatabaseLogger() {
    if (db) sqlite3_close(db);
}

void DatabaseLogger::createTablesIfNotExist() {
    const char* face_sql = R"(
        CREATE TABLE IF NOT EXISTS face_data (
            timestamp REAL,
            r REAL, g REAL, b REAL,
            blendshapes TEXT
        );
    )";

    const char* imu_sql = R"(
        CREATE TABLE IF NOT EXISTS imu_data (
            timestamp REAL,
            ax REAL, ay REAL, az REAL,
            gx REAL, gy REAL, gz REAL
        );
    )";

    sqlite3_exec(db, face_sql, nullptr, nullptr, nullptr);
    sqlite3_exec(db, imu_sql, nullptr, nullptr, nullptr);
}

void DatabaseLogger::insertFaceData(const FaceData& data) {
   
    try {
        if (!db) return;

        std::ostringstream bs_json;
        bs_json << "{";
        for (const auto& kv : data.blendshapes) {
            bs_json << "\"" << kv.first << "\":" << kv.second << ",";
        }
        std::string bs_str = bs_json.str();
        if (!data.blendshapes.empty()) bs_str.pop_back();  // remove last comma
        bs_str += "}";

        std::string sql = "INSERT INTO face_data VALUES (" +
            std::to_string(data.source_timestamp) + "," +
            std::to_string(data.avg_rgb[0]) + "," +
            std::to_string(data.avg_rgb[1]) + "," +
            std::to_string(data.avg_rgb[2]) + ",'" +
            bs_str + "');";

        sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    } catch (const std::exception& e) {
        std::cerr << "[DB] Error inserting face data: " << e.what() << std::endl;
    }
    
}

void DatabaseLogger::insertImuData(const ImuData& data) {
    try {
        if (!db) return;

        std::string sql = "INSERT INTO imu_data VALUES (" +
            std::to_string(data.source_timestamp) + "," +
            std::to_string(data.accel[0]) + "," + std::to_string(data.accel[1]) + "," + std::to_string(data.accel[2]) + "," +
            std::to_string(data.gyro[0]) + "," + std::to_string(data.gyro[1]) + "," + std::to_string(data.gyro[2]) + ");";

        sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    } catch (const std::exception& e) {
        std::cerr << "[DB] Error inserting IMU data: " << e.what() << std::endl;
    }
}

