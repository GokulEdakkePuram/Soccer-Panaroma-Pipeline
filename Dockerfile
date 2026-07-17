# Development environment with all build/run/eval dependencies.
# Use this stage directly for iterating with your source mounted:
#   docker build --target env -t pipeline-env .
#   docker run --rm -it -v "$(pwd):/workspace" pipeline-env
FROM ubuntu:22.04 AS env

ENV DEBIAN_FRONTEND=noninteractive
ARG OPENCV_VERSION=4.10.0

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    pkg-config \
    python3 \
    python3-pip \
    time \
    wget \
    ffmpeg \
    libavformat-dev \
    libavcodec-dev \
    libavutil-dev \
    libswscale-dev \
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    && rm -rf /var/lib/apt/lists/*

# OpenCV >= 4.7 required for YOLOv8 ONNX DNN inference (Ubuntu 22.04 ships 4.5).
RUN wget -q "https://github.com/opencv/opencv/archive/refs/tags/${OPENCV_VERSION}.tar.gz" -O /tmp/opencv.tar.gz \
    && tar xzf /tmp/opencv.tar.gz -C /tmp \
    && cmake -S "/tmp/opencv-${OPENCV_VERSION}" -B /tmp/opencv_build \
         -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX=/usr/local \
         -DBUILD_LIST=core,imgproc,dnn,videoio,imgcodecs \
         -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_opencv_apps=OFF \
         -DWITH_CUDA=OFF \
    && cmake --build /tmp/opencv_build -j"$(nproc)" \
    && cmake --install /tmp/opencv_build \
    && rm -rf /tmp/opencv.tar.gz "/tmp/opencv-${OPENCV_VERSION}" /tmp/opencv_build

# Python deps for the eval harness (eval/run_eval.sh falls back to system
# python3 when uv is not installed).
RUN pip3 install --no-cache-dir tqdm opencv-python-headless

WORKDIR /workspace
CMD ["bash"]

# Prebuilt image: source baked in and compiled, entrypoint runs the pipeline.
FROM env AS release

COPY . /workspace

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)"

ENTRYPOINT ["/workspace/build/pipeline"]
