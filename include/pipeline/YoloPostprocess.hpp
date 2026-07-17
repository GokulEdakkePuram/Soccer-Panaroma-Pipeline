#pragma once

#include "pipeline/DetectionClasses.hpp"
#include "pipeline/types.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace pipeline {

inline float box_iou(const BoundingBox& a, const BoundingBox& b) {
    const float ax2 = a.x + a.w;
    const float ay2 = a.y + a.h;
    const float bx2 = b.x + b.w;
    const float by2 = b.y + b.h;

    const float inter_x1 = std::max(a.x, b.x);
    const float inter_y1 = std::max(a.y, b.y);
    const float inter_x2 = std::min(ax2, bx2);
    const float inter_y2 = std::min(ay2, by2);

    const float inter_w = std::max(0.0f, inter_x2 - inter_x1);
    const float inter_h = std::max(0.0f, inter_y2 - inter_y1);
    const float inter_area = inter_w * inter_h;

    const float area_a = a.w * a.h;
    const float area_b = b.w * b.h;
    const float union_area = area_a + area_b - inter_area;
    if (union_area <= 0.0f) {
        return 0.0f;
    }
    return inter_area / union_area;
}

inline void sort_detections_deterministic(std::vector<Detection>& detections) {
    std::sort(detections.begin(), detections.end(), [](const Detection& a, const Detection& b) {
        if (a.bbox.y != b.bbox.y) {
            return a.bbox.y < b.bbox.y;
        }
        if (a.bbox.x != b.bbox.x) {
            return a.bbox.x < b.bbox.x;
        }
        if (a.class_id != b.class_id) {
            return a.class_id < b.class_id;
        }
        return a.confidence > b.confidence;
    });
}

inline std::vector<Detection> nms_detections(const std::vector<Detection>& detections,
                                             float iou_threshold = kDefaultNmsIouThreshold) {
    std::vector<Detection> sorted = detections;
    sort_detections_deterministic(sorted);

    // Process higher confidence first within each class group.
    std::sort(sorted.begin(), sorted.end(), [](const Detection& a, const Detection& b) {
        if (a.class_id != b.class_id) {
            return a.class_id < b.class_id;
        }
        return a.confidence > b.confidence;
    });

    std::vector<Detection> kept;
    kept.reserve(sorted.size());

    for (const auto& candidate : sorted) {
        bool overlaps = false;
        for (const auto& existing : kept) {
            if (existing.class_id != candidate.class_id) {
                continue;
            }
            if (box_iou(existing.bbox, candidate.bbox) > iou_threshold) {
                overlaps = true;
                break;
            }
        }
        if (!overlaps) {
            kept.push_back(candidate);
        }
    }

    sort_detections_deterministic(kept);
    return kept;
}

}  // namespace pipeline
