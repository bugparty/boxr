#pragma once

#include <sched.h>
#include <cstring>
#include <spdlog/spdlog.h>

namespace ILLIXR {

inline bool set_cpu_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == 0) {
        return true;
    }
    spdlog::warn("cpu_affinity: failed to pin to core {}: {}", core_id, std::strerror(errno));
    return false;
}

} // namespace ILLIXR
