#include "estimate_heart_rate_from_rgb.hpp"
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <complex>

// Butterworth bandpass filter (2nd order)
std::vector<double> butter_bandpass_filter(const std::vector<double>& signal, double fs, double lowcut, double highcut) {
    const double nyq = 0.5 * fs;
    const double low = lowcut / nyq;
    const double high = highcut / nyq;

    if (low >= 1.0 || high >= 1.0 || signal.size() < 5) return signal;

    std::vector<double> y(signal.size(), 0.0);
    double a0, a1, a2, b0, b1, b2;
    
    // 2nd order Butterworth (precomputed for bilinear transform)
    double ita = 1.0 / std::tan(M_PI * (high - low) / 2.0);
    double q = std::sqrt(2.0);  // Q-factor for Butterworth
    b0 = 1.0 / (1.0 + q * ita + ita * ita);
    b1 = 2.0 * b0;
    b2 = b0;
    a0 = 1.0;
    a1 = 2.0 * b0 * (ita * ita - 1.0);
    a2 = b0 * (1.0 - q * ita + ita * ita);

    std::vector<double> x = signal;
    for (size_t i = 2; i < signal.size(); ++i) {
        y[i] = b0 * x[i] + b1 * x[i-1] + b2 * x[i-2]
             - a1 * y[i-1] - a2 * y[i-2];
    }
    return y;
}

// Robust standard deviation
double robust_std(const std::vector<double>& data) {
    std::vector<double> temp = data;
    std::nth_element(temp.begin(), temp.begin() + temp.size()/2, temp.end());
    double median = temp[temp.size()/2];

    std::vector<double> abs_dev;
    for (double d : data) abs_dev.push_back(std::abs(d - median));

    std::nth_element(abs_dev.begin(), abs_dev.begin() + abs_dev.size()/2, abs_dev.end());
    double mad = abs_dev[abs_dev.size()/2];

    return 1.4826 * mad;
}

// POS algorithm core
std::vector<double> pos_algorithm(const std::vector<std::vector<double>>& rgb_data, double fps) {
    std::vector<double> x, y, s;

    for (const auto& rgb : rgb_data) {
        x.push_back(3.0 * rgb[0] - 2.0 * rgb[1]);
        y.push_back(1.5 * rgb[0] + rgb[1] - 1.5 * rgb[2]);
    }

    double std_x = robust_std(x);
    double std_y = robust_std(y);
    if (std_y == 0) std_y = 1e-6;

    double alpha = std_x / std_y;
    alpha = std::clamp(alpha, 0.3, 3.0);

    for (size_t i = 0; i < x.size(); ++i) {
        s.push_back(x[i] - alpha * y[i]);
    }

    return butter_bandpass_filter(s, fps, 0.8, 2.5);
}

double estimate_heart_rate_from_rgb(const std::vector<float>& r,
                                    const std::vector<float>& g,
                                    const std::vector<float>& b,
                                    double fps) {
    size_t n = r.size();
    if (n < static_cast<size_t>(fps * 5)) return 0.0;

    std::vector<std::vector<double>> rgb_data(n, std::vector<double>(3));
    double r_mean = std::accumulate(r.begin(), r.end(), 0.0f) / r.size();
    double g_mean = std::accumulate(g.begin(), g.end(), 0.0f) / g.size();
    double b_mean = std::accumulate(b.begin(), b.end(), 0.0f) / b.size();

    for (size_t i = 0; i < n; ++i) {
        rgb_data[i][0] = r[i] - r_mean;
        rgb_data[i][1] = g[i] - g_mean;
        rgb_data[i][2] = b[i] - b_mean;
    }

    std::vector<double> signal = pos_algorithm(rgb_data, fps);

    // Remove mean
    double mean = std::accumulate(signal.begin(), signal.end(), 0.0) / signal.size();
    for (auto& v : signal) v -= mean;

    // FFT
    size_t len = signal.size();
    std::vector<std::complex<double>> fft(len);
    for (size_t k = 0; k < len; ++k) {
        std::complex<double> sum = 0;
        for (size_t n = 0; n < len; ++n) {
            double angle = 2 * M_PI * k * n / len;
            sum += std::polar(signal[n], -angle);
        }
        fft[k] = sum;
    }

    std::vector<double> freqs(len / 2 + 1);
    for (size_t i = 0; i < freqs.size(); ++i) {
        freqs[i] = i * fps / len;
    }

    double max_val = 0.0;
    double peak_freq = 0.0;
    for (size_t i = 0; i < freqs.size(); ++i) {
        if (freqs[i] >= 0.7 && freqs[i] <= 4.0) {
            double magnitude = std::abs(fft[i]);
            if (magnitude > max_val) {
                max_val = magnitude;
                peak_freq = freqs[i];
            }
        }
    }

    return round(peak_freq * 60.0 * 10.0) / 10.0; // BPM with 0.1 precision
}
