#include "pipeline/Pipeline.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace pipeline {
namespace {

using Clock = std::chrono::steady_clock;

double elapsed_ms(const Clock::time_point& start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

    bool push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [&] { return queue_.size() < capacity_ || closed_ || aborted_; });
        if (closed_ || aborted_) {
            return false;
        }
        queue_.push_back(std::move(item));
        not_empty_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [&] { return !queue_.empty() || closed_ || aborted_; });
        if (aborted_ || queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop_front();
        not_full_.notify_one();
        return item;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        not_full_.notify_all();
        not_empty_.notify_all();
    }

    void abort() {
        std::lock_guard<std::mutex> lock(mutex_);
        aborted_ = true;
        queue_.clear();
        not_full_.notify_all();
        not_empty_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    std::deque<T> queue_;
    std::size_t capacity_;
    bool closed_ = false;
    bool aborted_ = false;
};

struct ReadItem {
    Frame frame;
    double reader_ms = 0.0;
};

struct DetectedItem {
    Frame frame;
    std::vector<Detection> detections;
    double reader_ms = 0.0;
    double detector_ms = 0.0;
};

}  // namespace

Pipeline::Pipeline(std::unique_ptr<IVideoReader> reader,
                   std::unique_ptr<IDetector> detector,
                   std::unique_ptr<ICamera> camera,
                   std::unique_ptr<IVisualizer> visualizer,
                   std::unique_ptr<IMetrics> metrics,
                   PipelineConfig config)
    : reader_(std::move(reader)),
      detector_(std::move(detector)),
      camera_(std::move(camera)),
      visualizer_(std::move(visualizer)),
      metrics_(std::move(metrics)),
      config_(std::move(config)) {}

bool Pipeline::run() {
    if (!reader_->open(config_.video_path, config_.segment)) {
        std::cerr << "Failed to open video reader\n";
        return false;
    }

    metadata_ = reader_->metadata();
    if (metadata_.segment.end_frame < 0 && metadata_.total_frames > 0) {
        metadata_.segment.end_frame = metadata_.total_frames - 1;
    }

    if (!config_.viz_output_path.empty() && visualizer_) {
        if (!visualizer_->open(config_.viz_output_path, metadata_.width, metadata_.height,
                               metadata_.fps)) {
            std::cerr << "Failed to open visualizer output\n";
            return false;
        }
    }

    // Warm up the detector before timing starts: the first DNN forward pass
    // allocates layer buffers and runs slower
    if (metadata_.width > 0 && metadata_.height > 0) {
        Frame warmup;
        warmup.width = metadata_.width;
        warmup.height = metadata_.height;
        warmup.data.assign(
            static_cast<std::size_t>(warmup.width) * static_cast<std::size_t>(warmup.height) * 3,
            0);
        detector_->process(warmup);
    }

    const double frame_budget_ms = metadata_.fps > 0.0 ? 1000.0 / metadata_.fps : 40.0;
    const std::size_t queue_depth =
        config_.queue_depth > 0 ? static_cast<std::size_t>(config_.queue_depth) : 1;

    BoundedQueue<ReadItem> read_queue(queue_depth);
    BoundedQueue<DetectedItem> detect_queue(queue_depth);

    std::atomic<bool> failed{false};
    std::mutex error_mutex;
    std::string error_message;

    auto fail = [&](const std::string& stage, const std::string& what) {
        {
            std::lock_guard<std::mutex> lock(error_mutex);
            if (error_message.empty()) {
                error_message = stage + ": " + what;
            }
        }
        failed.store(true);
        read_queue.abort();
        detect_queue.abort();
    };

    // Stage 1: decode frames. push() blocking on a full queue is what paces
    // this thread when the detector falls behind.
    std::thread reader_thread([&] {
        try {
            auto next_deadline = Clock::now();
            while (true) {
                const auto t0 = Clock::now();
                auto frame = reader_->read_next();
                const double reader_ms = elapsed_ms(t0);
                if (!frame.has_value()) {
                    break;
                }
                if (!read_queue.push(ReadItem{std::move(*frame), reader_ms})) {
                    break;
                }
                if (config_.realtime) {
                    next_deadline += std::chrono::duration_cast<Clock::duration>(
                        std::chrono::duration<double, std::milli>(frame_budget_ms));
                    std::this_thread::sleep_until(next_deadline);
                }
            }
        } catch (const std::exception& e) {
            fail("video_reader", e.what());
        }
        read_queue.close();
    });

    // Stage 2: detection. Single thread, in-order, so downstream sees frames
    // in frame_idx order and output stays deterministic.
    std::thread detector_thread([&] {
        try {
            while (auto item = read_queue.pop()) {
                const auto t0 = Clock::now();
                auto detections = detector_->process(item->frame); // ReadItem: frame, reader_ms
                const double detector_ms = elapsed_ms(t0);
                if (!detect_queue.push(DetectedItem{std::move(item->frame), std::move(detections),
                                                    item->reader_ms, detector_ms})) {
                    break;
                }
            }
        } catch (const std::exception& e) {
            fail("detector", e.what());
        }
        detect_queue.close();
    });

    // Stage 3 (this thread): camera update, visualization, metrics. The camera
    // smoothing depends on prev_target_, so it must run sequentially here.
    try {
        while (auto item = detect_queue.pop()) {
            const auto camera_t0 = Clock::now();
            CameraTarget target = camera_->update(item->frame.frame_idx, item->detections,
                                                  prev_target_, item->frame.width,
                                                  item->frame.height);
            const double camera_ms = elapsed_ms(camera_t0);
            prev_target_ = target;

            double visualizer_ms = 0.0;
            if (visualizer_) {
                const auto viz_t0 = Clock::now();
                visualizer_->render(item->frame, item->detections, target);
                visualizer_ms = elapsed_ms(viz_t0);
            }

            FrameResult result;
            result.frame_idx = item->frame.frame_idx;
            result.timestamp_ms = item->frame.timestamp_ms;
            result.camera_center = target;
            result.stage_ms.video_reader = item->reader_ms;
            result.stage_ms.detector = item->detector_ms;
            result.stage_ms.camera = camera_ms;
            result.stage_ms.visualizer = visualizer_ms;
            metrics_->record_frame(result);
        }
    } catch (const std::exception& e) {
        fail("camera/visualizer", e.what());
    }

    reader_thread.join();
    detector_thread.join();

    if (visualizer_) {
        visualizer_->close();
    }

    if (failed.load()) {
        std::lock_guard<std::mutex> lock(error_mutex);
        std::cerr << "Pipeline failed: " << error_message << '\n';
        return false;
    }

    metrics_->write_json(config_.output_path, metadata_);
    return true;
}

}  // namespace pipeline
