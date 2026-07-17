#include "pipeline/MetricsCollector.hpp"
#include "pipeline/YoloV8Detector.hpp"
#include "pipeline/stubs/NullVisualizer.hpp"
#include "pipeline/stubs/OpenCvVisualizer.hpp"

#if defined(PIPELINE_REFERENCE_SOLUTION)
#include "pipeline/reference/FfmpegVideoReader.hpp"
#include "pipeline/reference/Pipeline.hpp"
#include "pipeline/reference/SmoothedCentroidCamera.hpp"
#include "pipeline/reference/TiledPanoramaDetector.hpp"
#else
#include "pipeline/Pipeline.hpp"
#include "pipeline/stubs/PassthroughCamera.hpp"
#include "pipeline/stubs/StubVideoReader.hpp"
#include "pipeline/stubs/TiledPanoramaDetector.hpp"
#endif

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

void print_usage(const char* prog) {
    std::cerr
        << "Usage: " << prog
        << " --video <path> --output <json> [--viz-output <path>]\n"
        << "       [--model <onnx>] [--start-frame N] [--end-frame N] [--queue-depth N] [--realtime]\n";
}

std::optional<pipeline::PipelineConfig> parse_args(int argc, char** argv) {
    pipeline::PipelineConfig config;
    config.segment.start_frame = 0;
    config.segment.end_frame = -1;
    config.queue_depth = 8;
    config.realtime = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need_value = [&](const std::string& flag) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << flag << '\n';
                std::exit(2);
            }
            return argv[++i];
        };

        if (arg == "--video") {
            config.video_path = need_value(arg);
        } else if (arg == "--output") {
            config.output_path = need_value(arg);
        } else if (arg == "--viz-output") {
            config.viz_output_path = need_value(arg);
        } else if (arg == "--model") {
            config.model_path = need_value(arg);
        } else if (arg == "--start-frame") {
            config.segment.start_frame = std::stoll(need_value(arg));
        } else if (arg == "--end-frame") {
            config.segment.end_frame = std::stoll(need_value(arg));
        } else if (arg == "--queue-depth") {
            config.queue_depth = std::stoi(need_value(arg));
        } else if (arg == "--realtime") {
            config.realtime = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::cerr << "Unknown argument: " << arg << '\n';
            print_usage(argv[0]);
            return std::nullopt;
        }
    }

    if (config.video_path.empty() || config.output_path.empty()) {
        std::cerr << "--video and --output are required\n";
        print_usage(argv[0]);
        return std::nullopt;
    }

    return config;
}

bool model_file_ready(const std::string& model_path) {
    namespace fs = std::filesystem;
    if (!fs::exists(model_path)) {
        std::cerr << "Model not found: " << model_path << '\n'
                  << "The task archive may be incomplete; re-download it.\n";
        return false;
    }

    std::ifstream file(model_path, std::ios::binary);
    if (!file) {
        return false;
    }
    char header[16] = {};
    file.read(header, sizeof(header));
    if (std::string(header, 8) == "version ") {
        std::cerr << "Model file is a Git LFS pointer, not ONNX data: " << model_path << '\n'
                  << "Re-download the task archive (maintainers: run git lfs pull).\n";
        return false;
    }
    return true;
}

std::unique_ptr<pipeline::IVisualizer> make_visualizer(const pipeline::PipelineConfig& config) {
    if (config.viz_output_path.empty()) {
        return std::make_unique<pipeline::NullVisualizer>();
    }
    return std::make_unique<pipeline::OpenCvVisualizer>();
}

}  // namespace

int main(int argc, char** argv) {
    const auto config = parse_args(argc, argv);
    if (!config.has_value()) {
        return 2;
    }

    if (!model_file_ready(config->model_path)) {
        return 2;
    }

    auto yolo = std::make_unique<pipeline::YoloV8Detector>(config->model_path);
    auto detector = std::make_unique<pipeline::TiledPanoramaDetector>(std::move(yolo));
#if defined(PIPELINE_REFERENCE_SOLUTION)
    auto reader = std::make_unique<pipeline::FfmpegVideoReader>();
    auto camera = std::make_unique<pipeline::SmoothedCentroidCamera>();
#else
    auto reader = std::make_unique<pipeline::StubVideoReader>();
    auto camera = std::make_unique<pipeline::PassthroughCamera>();
#endif
    auto visualizer = make_visualizer(*config);
    auto metrics = std::make_unique<pipeline::MetricsCollector>();

    pipeline::Pipeline pipeline(std::move(reader), std::move(detector), std::move(camera),
                                std::move(visualizer), std::move(metrics), *config);

    if (!pipeline.run()) {
        return 1;
    }

    return 0;
}
