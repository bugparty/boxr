#pragma once

#include <spsc_ring_buffer.hpp>

#include "illixr/realtime_metrics.hpp"

namespace ILLIXR {

class shm_metrics_ring_buffer {
public:
    shm_metrics_ring_buffer()
        : buffer_(REALTIME_METRICS_SHM_NAME,
                  REALTIME_METRICS_VERSION,
                  buffers::ShmOpenMode::create_or_open) {}

    explicit shm_metrics_ring_buffer(const char* shm_name)
        : buffer_(shm_name,
                  REALTIME_METRICS_VERSION,
                  buffers::ShmOpenMode::create_or_open) {}

    shm_metrics_ring_buffer(const shm_metrics_ring_buffer&) = delete;
    shm_metrics_ring_buffer& operator=(const shm_metrics_ring_buffer&) = delete;
    shm_metrics_ring_buffer(shm_metrics_ring_buffer&&) = delete;
    shm_metrics_ring_buffer& operator=(shm_metrics_ring_buffer&&) = delete;

    bool ok() const { return buffer_.valid(); }

    void push(realtime_frame_metrics& metrics_in) {
        metrics_in.sequence_num = counter_++;
        buffer_.push_overwrite(metrics_in);
    }

    uint64_t overflow_count() const { return buffer_.overflow_count(); }
    bool is_creator() const { return buffer_.is_creator(); }

private:
    using BoxrSpscBuffer = buffers::spsc_ring_buffer<
        realtime_frame_metrics,
        REALTIME_METRICS_RING_CAPACITY,
        buffers::ShmStorage
    >;

    BoxrSpscBuffer buffer_;
    uint64_t counter_{0};
};

} // namespace ILLIXR
