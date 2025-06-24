#pragma once
#include <vector>

double estimate_heart_rate_from_rgb(const std::vector<float>& r,
                                    const std::vector<float>& g,
                                    const std::vector<float>& b,
                                    double fps);
