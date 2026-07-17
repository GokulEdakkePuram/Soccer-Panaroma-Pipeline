#include "pipeline/stubs/OpenCvVisualizer.hpp"

#include "pipeline/DetectionClasses.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <iostream>

namespace pipeline {

OpenCvVisualizer::OpenCvVisualizer() = default;

OpenCvVisualizer::~OpenCvVisualizer() {
    close();
}

struct OpenCvVisualizer::Impl {
    cv::VideoWriter writer;
    int width = 0;
    int height = 0;
};

bool OpenCvVisualizer::open(const std::string& output_path, int width, int height, double fps) {
    impl_ = std::make_unique<Impl>();
    impl_->width = width;
    impl_->height = height;

    const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    if (!impl_->writer.open(output_path, fourcc, fps, cv::Size(width, height))) {
        std::cerr << "Failed to open video writer: " << output_path << '\n';
        impl_.reset();
        return false;
    }
    return true;
}

void OpenCvVisualizer::render(const Frame& frame,
                              const std::vector<Detection>& detections,
                              const CameraTarget& target) {
    if (!impl_ || !impl_->writer.isOpened()) {
        return;
    }

    cv::Mat image(frame.height, frame.width, CV_8UC3, const_cast<uint8_t*>(frame.data.data()));
    cv::Mat bgr;
    cv::cvtColor(image, bgr, cv::COLOR_RGB2BGR);

    for (const auto& det : detections) {
        const cv::Rect rect(static_cast<int>(det.bbox.x), static_cast<int>(det.bbox.y),
                            static_cast<int>(det.bbox.w), static_cast<int>(det.bbox.h));
        const cv::Scalar color = det.class_id == kSportsBallClassId ? cv::Scalar(0, 255, 255)
                                                                    : cv::Scalar(0, 255, 0);
        cv::rectangle(bgr, rect, color, 2);
    }

    const cv::Point center(static_cast<int>(target.center_x), static_cast<int>(target.center_y));
    cv::drawMarker(bgr, center, cv::Scalar(0, 0, 255), cv::MARKER_CROSS, 24, 2);

    const int crop_w = std::min(1920, frame.width);
    const int crop_h = std::min(1080, frame.height);
    const int x0 = std::max(0, static_cast<int>(target.center_x) - crop_w / 2);
    const int y0 = std::max(0, static_cast<int>(target.center_y) - crop_h / 2);
    cv::rectangle(bgr, cv::Rect(x0, y0, crop_w, crop_h), cv::Scalar(255, 0, 0), 2);

    impl_->writer.write(bgr);
}

void OpenCvVisualizer::close() {
    if (impl_ && impl_->writer.isOpened()) {
        impl_->writer.release();
    }
    impl_.reset();
}

}  // namespace pipeline
