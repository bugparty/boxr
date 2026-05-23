#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace ILLIXR {

using realtime_duration = std::chrono::nanoseconds;

constexpr const char* REALTIME_METRICS_SHM_NAME = "/illixr_rl_frame_metrics";
constexpr std::uint32_t REALTIME_METRICS_VERSION = 4;
constexpr std::size_t   REALTIME_METRICS_RING_CAPACITY = 256;

struct realtime_frame_metrics {
    std::uint64_t sequence_num;

    std::uint64_t iteration_no;
    std::int64_t  vsync_ns;

    std::int64_t imu_to_display_ns;
    std::int64_t cam_vio_to_display_ns;
    std::int64_t predict_to_display_ns;
    std::int64_t render_to_display_ns;
    double       fps_hz;

    std::int64_t frame_start_time_ns;
    std::int64_t frame_end_time_ns;

    std::int64_t render_start_time_ns;
    std::int64_t render_end_time_ns;

    std::int64_t timewarp_start_time_ns;
    std::int64_t timewarp_end_time_ns;

    std::int64_t vio_start_time_ns;
    std::int64_t vio_end_time_ns;
};

inline std::int64_t to_ns(realtime_duration d) {
    return d.count();
}

} // namespace ILLIXR
