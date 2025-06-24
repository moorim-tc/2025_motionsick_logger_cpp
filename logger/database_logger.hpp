#pragma once
#include <sqlite3.h>
#include <string>
#include "../include/shared_structs.hpp"

class DatabaseLogger {
public:
    DatabaseLogger(const std::string& db_path);
    ~DatabaseLogger();

    void insertFaceData(const FaceData& data);
    void insertImuData(const ImuData& data);
    void insertGpsData(const GpsData& data);
    void insertToggleState(const std::array<int, 3>& state, double timestamp);

private:
    sqlite3* db;

    void createTablesIfNotExist();
};
