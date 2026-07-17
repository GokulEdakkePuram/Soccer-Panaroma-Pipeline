cmake_minimum_required(VERSION 3.16)
project(st_pipeline_task LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PIPELINE_REFERENCE_SOLUTION "Build the maintainer reference solution from private/ (not in candidate distribution)" OFF)
option(PIPELINE_PRIVATE_TESTS "Build maintainer-only tile detector tests" OFF)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat
    libavcodec
    libavutil
    libswscale
)

find_package(Threads REQUIRED)
find_package(OpenCV 4.7 REQUIRED COMPONENTS core imgproc dnn videoio)

set(PIPELINE_SOURCES
    src/main.cpp
    src/pipeline/MetricsCollector.cpp
    src/pipeline/YoloV8Detector.cpp
    src/stubs/NullDetector.cpp
    src/stubs/NullVisualizer.cpp
    src/stubs/OpenCvVisualizer.cpp
)

if(PIPELINE_REFERENCE_SOLUTION OR PIPELINE_PRIVATE_TESTS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/private")
        message(FATAL_ERROR
            "PIPELINE_REFERENCE_SOLUTION/PIPELINE_PRIVATE_TESTS require the private/ "
            "directory, which is not part of the candidate distribution.")
    endif()
endif()

if(PIPELINE_REFERENCE_SOLUTION)
    list(APPEND PIPELINE_SOURCES
        private/src/Pipeline.cpp
        private/src/FfmpegVideoReader.cpp
        private/src/TiledPanoramaDetector.cpp
        private/src/SmoothedCentroidCamera.cpp
    )
else()
    list(APPEND PIPELINE_SOURCES
        src/pipeline/Pipeline.cpp
        src/stubs/StubVideoReader.cpp
        src/stubs/PassthroughCamera.cpp
        src/stubs/TiledPanoramaDetector.cpp
    )
endif()

add_executable(pipeline ${PIPELINE_SOURCES})

target_include_directories(pipeline PRIVATE
    include
    ${FFMPEG_INCLUDE_DIRS}
)

if(PIPELINE_REFERENCE_SOLUTION)
    target_include_directories(pipeline PRIVATE private/include)
endif()

target_link_libraries(pipeline PRIVATE
    nlohmann_json::nlohmann_json
    Threads::Threads
    ${FFMPEG_LIBRARIES}
    ${OpenCV_LIBS}
)

target_include_directories(pipeline PRIVATE ${OpenCV_INCLUDE_DIRS})
target_compile_definitions(pipeline PRIVATE PIPELINE_HAS_OPENCV=1)

if(PIPELINE_REFERENCE_SOLUTION)
    target_compile_definitions(pipeline PRIVATE PIPELINE_REFERENCE_SOLUTION=1)
endif()

target_compile_options(pipeline PRIVATE -Wall -Wextra -Wpedantic)

if(PIPELINE_PRIVATE_TESTS)
    set(TILE_TEST_SOURCES
        src/pipeline/YoloV8Detector.cpp
        private/src/FfmpegVideoReader.cpp
    )

    add_executable(tile_detector_smoke private/eval/tile_detector_smoke.cpp ${TILE_TEST_SOURCES})
    target_include_directories(tile_detector_smoke PRIVATE include private/include ${FFMPEG_INCLUDE_DIRS} private/eval)
    target_link_libraries(tile_detector_smoke PRIVATE ${FFMPEG_LIBRARIES} ${OpenCV_LIBS})
    target_include_directories(tile_detector_smoke PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_compile_options(tile_detector_smoke PRIVATE -Wall -Wextra -Wpedantic)

    add_executable(tile_viz private/eval/tile_viz.cpp ${TILE_TEST_SOURCES})
    target_include_directories(tile_viz PRIVATE include private/include ${FFMPEG_INCLUDE_DIRS} private/eval)
    target_link_libraries(tile_viz PRIVATE ${FFMPEG_LIBRARIES} ${OpenCV_LIBS})
    target_include_directories(tile_viz PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_compile_options(tile_viz PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS pipeline RUNTIME DESTINATION bin)
