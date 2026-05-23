#pragma once

#include "illixr/concurrentqueue/blockingconcurrentqueue.hpp"
#include "illixr/error_util.hpp"
#include "illixr/global_module_defs.hpp"
#include "illixr/record_logger.hpp"
#include "illixr/realtime_metrics.hpp"
#include "illixr/shm_ring_buffer.hpp"
#include "sqlite3pp/sqlite3pp.hpp"

#include <filesystem>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <spdlog/spdlog.h>
#include <thread>

/**
 * There are many SQLite3 wrapper libraries.
 * [List source](http://srombauts.github.io/SQLiteCpp/#see-also---some-other-simple-c-sqlite-wrappers)
 * TODO: this
 */

namespace ILLIXR {

class sqlite_thread {
public:
    sqlite3pp::database prep_db() {
        RAC_ERRNO_MSG("sqlite_record_logger at start of prep_db");

        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directory(dir);
        }

        const std::string path = dir / (table_name + std::string{".sqlite"});

        RAC_ERRNO_MSG("sqlite_record_logger before sqlite3pp::database");
        sqlite3pp::database db{path.c_str()};
        RAC_ERRNO_MSG("sqlite_record_logger after sqlite3pp::database");

        return db;
    }

    std::string prep_insert_str() {
        RAC_ERRNO_MSG("sqlite_record_logger at start of prep_insert_str");

        std::string drop_table_string = std::string{"DROP TABLE IF EXISTS "} + table_name + std::string{";"};
        db.execute(drop_table_string.c_str());
        RAC_ERRNO_MSG("sqlite_record_logger after drop table execute.");

        std::string create_table_string = std::string{"CREATE TABLE "} + table_name + std::string{"("};
        for (unsigned int i = 0; i < rh.get_columns(); ++i) {
            create_table_string += rh.get_column_name(i) + std::string{" "};
            if (false) {
            } else if (rh.get_column_type(i) == typeid(std::size_t)) {
                create_table_string += std::string{"INTEGER"};
            } else if (rh.get_column_type(i) == typeid(std::int64_t)) {
                create_table_string += std::string{"INTEGER"};
            } else if (rh.get_column_type(i) == typeid(std::uint64_t)) {
                create_table_string += std::string{"INTEGER"};
            } else if (rh.get_column_type(i) == typeid(bool)) {
                create_table_string += std::string{"INTEGER"};
            } else if (rh.get_column_type(i) == typeid(double)) {
                create_table_string += std::string{"REAL"}; // For performance timing. Will be deleted when implementing #208.
            } else if (rh.get_column_type(i) == typeid(std::chrono::nanoseconds)) {
                create_table_string += std::string{"INTEGER"};
            } else if (rh.get_column_type(i) == typeid(std::chrono::high_resolution_clock::time_point)) {
                create_table_string += std::string{"INTEGER"};
            } else if (rh.get_column_type(i) == typeid(duration)) {
                create_table_string += std::string{"INTEGER"};
            } else if (rh.get_column_type(i) == typeid(time_point)) {
                create_table_string += std::string{"INTEGER"};
            } else if (rh.get_column_type(i) == typeid(std::string)) {
                create_table_string += std::string{"TEXT"};
            } else {
                throw std::runtime_error{std::string{"type "} + std::string{rh.get_column_type(i).name()} +
                                         std::string{" not found"}};
            }
            create_table_string += std::string{", "};
        }
        create_table_string.erase(create_table_string.size() - 2);
        create_table_string += std::string{");"};

        RAC_ERRNO_MSG("sqlite_record_logger before create table execute");
        db.execute(create_table_string.c_str());
        RAC_ERRNO_MSG("sqlite_record_logger after create table execute");

        std::string insert_string = std::string{"INSERT INTO "} + table_name + std::string{" VALUES ("};
        for (unsigned int i = 0; i < rh.get_columns(); ++i) {
            insert_string += std::string{"?"} + std::to_string(i + 1) + std::string{", "};
        }
        insert_string.erase(insert_string.size() - 2);
        insert_string += std::string{");"};
        return insert_string;
    }

    sqlite_thread(const record_header& rh_)
        : rh{rh_}
        , table_name{rh.get_name()}
        , db{prep_db()}
        , insert_str{prep_insert_str()}
        , insert_cmd{db, insert_str.c_str()}
        , thread{std::bind(&sqlite_thread::pull_queue, this)} { }

    void pull_queue() {
        const std::size_t   max_record_batch_size = 1024 * 256;
        std::vector<record> record_batch{max_record_batch_size};
        std::size_t         actual_batch_size;

        spdlog::get("illixr")->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [sqlite_record_logger] thread %t %v");
        spdlog::get("illixr")->debug("{}", table_name);
        spdlog::get("illixr")->set_pattern("%+");

        std::size_t processed = 0;
        while (!terminate.load()) {
            std::this_thread::sleep_for(std::chrono::seconds{1});
            // Uncomment this block to log in "real time";
            // Otherwise, everything gets loged "post real time".
            /*
            const std::chrono::seconds max_record_match_wait_time {10};
            actual_batch_size = queue.wait_dequeue_bulk_timed(record_batch.begin(), record_batch.size(),
            max_record_match_wait_time); process(record_batch, actual_batch_size); processed += actual_batch_size;
            */
        }

        // We got the terminate commnad,
        // So drain whatever is left in the queue.
        // But don't wait around once it is empty.
        std::size_t post_processed = 0;
        while ((actual_batch_size = queue.try_dequeue_bulk(record_batch.begin(), record_batch.size()))) {
            process(record_batch, actual_batch_size);
            post_processed += actual_batch_size;
        }
        spdlog::get("illixr")->debug("[sqlite_record_logger] Drained {} (sqlite); {}/{} done post real time", table_name,
                                     post_processed, (processed + post_processed));
    }

    void process(const std::vector<record>& record_batch, std::size_t batch_size) {
        sqlite3pp::transaction xct{db};
        for (std::size_t i = 0; i < batch_size; ++i) {
            // TODO(performance): reuse the sqlite3pp statement
            // This currently has to be 'reinterpreted' for every iteration.
            sqlite3pp::command cmd{db, insert_str.c_str()};
            const record&      r = record_batch[i];
            for (unsigned int j = 0; j < rh.get_columns(); ++j) {
                /*
                  If you get a `std::bad_any_cast` here, make sure the user didn't lie about record.get_record_header().
                  The types there should be the same as those in record.get_values().
                */
                if (false) {
                } else if (rh.get_column_type(j) == typeid(std::size_t)) {
                    cmd.bind(j + 1, static_cast<long long>(r.get_value<std::size_t>(j)));
                } else if (rh.get_column_type(j) == typeid(std::int64_t)) {
                    cmd.bind(j + 1, static_cast<long long>(r.get_value<std::int64_t>(j)));
                } else if (rh.get_column_type(j) == typeid(std::uint64_t)) {
                    cmd.bind(j + 1, static_cast<long long>(r.get_value<std::uint64_t>(j)));
                } else if (rh.get_column_type(j) == typeid(bool)) {
                    cmd.bind(j + 1, static_cast<long long>(r.get_value<bool>(j)));
                } else if (rh.get_column_type(j) == typeid(double)) {
                    cmd.bind(j + 1, r.get_value<double>(j));
                } else if (rh.get_column_type(j) == typeid(std::chrono::nanoseconds)) {
                    auto val = r.get_value<duration>(j);
                    cmd.bind(j + 1, static_cast<long long>(std::chrono::nanoseconds{val}.count()));
                } else if (rh.get_column_type(j) == typeid(std::chrono::high_resolution_clock::time_point)) {
                    auto val = r.get_value<std::chrono::high_resolution_clock::time_point>(j).time_since_epoch();
                    cmd.bind(j + 1, static_cast<long long>(std::chrono::nanoseconds{val}.count()));
                } else if (rh.get_column_type(j) == typeid(duration)) {
                    auto val = r.get_value<duration>(j);
                    cmd.bind(j + 1, static_cast<long long>(std::chrono::nanoseconds{val}.count()));
                } else if (rh.get_column_type(j) == typeid(time_point)) {
                    auto val = r.get_value<time_point>(j).time_since_epoch();
                    cmd.bind(j + 1, static_cast<long long>(std::chrono::nanoseconds{val}.count()));
                } else if (rh.get_column_type(j) == typeid(std::string)) {
                    // r.get_value<std::string>(j) returns a std::string temporary
                    // c_str() returns a pointer into that std::string temporary
                    // Therefore, need to copy.
                    cmd.bind(j + 1, r.get_value<std::string>(j).c_str(), sqlite3pp::copy);
                } else {
                    throw std::runtime_error{std::string{"type "} + std::string{rh.get_column_type(j).name()} +
                                             std::string{" not implemented"}};
                }
            }
            RAC_ERRNO_MSG("sqlite_record_logger set errno before process cmd execute");

            cmd.execute();
            RAC_ERRNO_MSG("sqlite_record_logger after process cmd execute");
        }
        xct.commit();
    }

    void put_queue(const std::vector<record>& buffer_in) {
        queue.enqueue_bulk(buffer_in.begin(), buffer_in.size());
    }

    void put_queue(const record& record_in) {
        queue.enqueue(record_in);
    }

    ~sqlite_thread() {
        terminate.store(true);
        thread.join();
    }

private:
    static const std::filesystem::path          dir;
    const record_header&                        rh;
    std::string                                 table_name;
    sqlite3pp::database                         db;
    std::string                                 insert_str;
    sqlite3pp::command                          insert_cmd;
    moodycamel::BlockingConcurrentQueue<record> queue;
    std::atomic<bool>                           terminate{false};
    std::thread                                 thread;
};

const std::filesystem::path sqlite_thread::dir{"metrics"};

class realtime_metrics_exporter {
public:
    static std::int64_t to_ns_time_point(time_point t) {
        return std::chrono::duration_cast<realtime_duration>(t.time_since_epoch()).count();
    }

    void export_record(const record& r) {
        if (r.get_record_header().get_name() != "rl_frame_record") {
            return;
        }

        if (!ring_buffer_.ok()) {
            if (!warned_ring_buffer_not_ok_.exchange(true)) {
                auto logger = spdlog::get("illixr");
                if (logger) {
                    logger->warn("[realtime_metrics_exporter] SHM buffer {} is not available; rl_frame_record export is disabled",
                                 REALTIME_METRICS_SHM_NAME);
                }
            }
            return;
        }

        try {
            realtime_frame_metrics metrics{};
            metrics.iteration_no          = r.get_value<std::size_t>(0);
            metrics.vsync_ns              = to_ns_time_point(r.get_value<time_point>(1));
            metrics.imu_to_display_ns     = to_ns(r.get_value<duration>(2));
            metrics.cam_vio_to_display_ns = to_ns(r.get_value<duration>(3));
            metrics.predict_to_display_ns = to_ns(r.get_value<duration>(4));
            metrics.render_to_display_ns  = to_ns(r.get_value<duration>(5));
            metrics.fps_hz                = r.get_value<double>(6);
            metrics.frame_start_time_ns   = to_ns_time_point(r.get_value<time_point>(7));
            metrics.frame_end_time_ns     = to_ns_time_point(r.get_value<time_point>(8));
            metrics.render_start_time_ns  = to_ns_time_point(r.get_value<time_point>(9));
            metrics.render_end_time_ns    = to_ns_time_point(r.get_value<time_point>(10));
            metrics.timewarp_start_time_ns = to_ns_time_point(r.get_value<time_point>(11));
            metrics.timewarp_end_time_ns   = to_ns_time_point(r.get_value<time_point>(12));
            metrics.vio_start_time_ns      = to_ns_time_point(r.get_value<time_point>(13));
            metrics.vio_end_time_ns        = to_ns_time_point(r.get_value<time_point>(14));
            ring_buffer_.push(metrics);
        } catch (const std::exception& e) {
            auto logger = spdlog::get("illixr");
            if (logger) {
                logger->warn("[realtime_metrics_exporter] Failed to export rl_frame_record: {}", e.what());
            }
        }
    }

    void export_records(const std::vector<record>& rs) {
        for (const record& r : rs) {
            export_record(r);
        }
    }

private:
    shm_metrics_ring_buffer ring_buffer_{};
    std::atomic<bool>       warned_ring_buffer_not_ok_{false};
};

class sqlite_record_logger : public record_logger {
private:
    sqlite_thread& get_sqlite_thread(const record& r) {
        const record_header& rh = r.get_record_header();
        {
            const std::shared_lock<std::shared_mutex> lock{_m_registry_lock};
            auto                                      result = registered_tables.find(rh.get_id());
            if (result != registered_tables.cend()) {
                return result->second;
            }
        }
        const std::unique_lock<std::shared_mutex> lock{_m_registry_lock};
        auto                                      pair = registered_tables.try_emplace(rh.get_id(), rh);
        return pair.first->second;
    }

protected:
    virtual void log(const std::vector<record>& r) override {
        if (!r.empty()) {
            realtime_exporter_.export_records(r);
            get_sqlite_thread(r[0]).put_queue(r);
        }
    }

    virtual void log(const record& r) override {
        realtime_exporter_.export_record(r);
        get_sqlite_thread(r).put_queue(r);
    }

private:
    std::unordered_map<std::size_t, sqlite_thread> registered_tables;
    std::shared_mutex                              _m_registry_lock;
    realtime_metrics_exporter                      realtime_exporter_;
};

} // namespace ILLIXR
