// csv_logger.hpp

#ifndef CSV_LOGGER_HPP
#define CSV_LOGGER_HPP

#include <atomic>

void start_csv_logger(std::atomic<bool>& running, std::shared_ptr<std::array<std::atomic<int>, 3>> toggle_state);

#endif // CSV_LOGGER_HPP
