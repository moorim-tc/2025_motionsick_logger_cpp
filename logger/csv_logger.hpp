// csv_logger.hpp

#ifndef CSV_LOGGER_HPP
#define CSV_LOGGER_HPP

#include <atomic>
#include <array>
#include <string>
#include <memory>

void initialize_csv(const std::string& log_path);
void start_csv_logger(std::atomic<bool>& running, 
    std::shared_ptr<std::array<std::atomic<int>, 3>> toggle_state,
    const std::string& log_path);

#endif // CSV_LOGGER_HPP
