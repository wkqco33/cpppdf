#include "normalizer.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace cpppdf::converter {

namespace {

static std::string_view trim_sv(std::string_view sv) {
    size_t first = 0;
    while (first < sv.size() &&
           (sv[first] == ' ' || sv[first] == '\t' || sv[first] == '\n' || sv[first] == '\r')) {
        ++first;
    }

    size_t last = sv.size();
    while (last > first && (sv[last - 1] == ' ' || sv[last - 1] == '\t' || sv[last - 1] == '\n' ||
                            sv[last - 1] == '\r')) {
        --last;
    }

    return sv.substr(first, last - first);
}

static float median(std::vector<float> values, float fallback) {
    if (values.empty())
        return fallback;
    std::sort(values.begin(), values.end());
    const size_t mid = values.size() / 2;
    if ((values.size() % 2u) == 0u)
        return (values[mid - 1] + values[mid]) * 0.5f;
    return values[mid];
}

static float estimate_body_x(std::vector<float> xs, float fallback) {
    if (xs.empty())
        return fallback;
    std::sort(xs.begin(), xs.end());

    struct Cluster {
        int count = 0;
        float sum = 0.0f;
        float first = 0.0f;
    };

    std::vector<Cluster> clusters;
    Cluster current;
    current.count = 1;
    current.sum = xs[0];
    current.first = xs[0];

    for (size_t i = 1; i < xs.size(); ++i) {
        if (std::abs(xs[i] - xs[i - 1]) <= 8.0f) {
            current.count++;
            current.sum += xs[i];
            continue;
        }
        clusters.push_back(current);
        current.count = 1;
        current.sum = xs[i];
        current.first = xs[i];
    }
    clusters.push_back(current);

    const Cluster *best = &clusters.front();
    for (const Cluster &cluster : clusters) {
        if (cluster.count > best->count ||
            (cluster.count == best->count && cluster.first < best->first)) {
            best = &cluster;
        }
    }
    return best->sum / static_cast<float>(best->count);
}

static float estimate_line_height(const std::vector<TextBlock> &blocks, float fallback) {
    if (blocks.size() < 2)
        return fallback;

    std::vector<float> ys;
    ys.reserve(blocks.size());
    for (const auto &block : blocks)
        ys.push_back(block.y);

    std::sort(ys.begin(), ys.end(), std::greater<float>());

    std::vector<float> diffs;
    diffs.reserve(ys.size());
    for (size_t i = 1; i < ys.size(); ++i) {
        float diff = ys[i - 1] - ys[i];
        if (diff > 2.0f)
            diffs.push_back(diff);
    }

    return median(std::move(diffs), fallback);
}

static BlockRole detect_role(float font_size, float base_font_size) {
    if (font_size >= base_font_size * 1.8f)
        return BlockRole::H1;
    if (font_size >= base_font_size * 1.4f)
        return BlockRole::H2;
    if (font_size >= base_font_size * 1.15f)
        return BlockRole::H3;
    return BlockRole::Body;
}

} // namespace

NormalizationResult normalize_blocks(const std::vector<TextBlock> &blocks) {
    NormalizationResult result;

    std::vector<TextBlock> filtered;
    filtered.reserve(blocks.size());

    std::vector<float> font_sizes;
    std::vector<float> xs;
    font_sizes.reserve(blocks.size());
    xs.reserve(blocks.size());

    for (const auto &block : blocks) {
        std::string_view trimmed = trim_sv(block.text);
        if (trimmed.empty())
            continue;

        TextBlock cleaned = block;
        cleaned.text = std::string(trimmed);

        filtered.push_back(cleaned);
        font_sizes.push_back(cleaned.font_size > 0.0f ? cleaned.font_size : 12.0f);
        xs.push_back(cleaned.x);
    }

    if (filtered.empty())
        return result;

    result.stats.median_font_size = median(std::move(font_sizes), 12.0f);
    float median_x_val = median(xs, 0.0f);
    result.stats.median_x = estimate_body_x(std::move(xs), median_x_val);
    result.stats.median_line_height =
        std::max(result.stats.median_font_size * 1.1f,
                 estimate_line_height(filtered, result.stats.median_font_size * 1.2f));
    result.stats.y_tolerance = std::max(2.0f, result.stats.median_line_height * 0.5f);

    result.blocks.reserve(filtered.size());
    for (const auto &block : filtered) {
        NormalizedBlock normalized;
        normalized.block = block;
        normalized.role =
            detect_role(block.font_size > 0.0f ? block.font_size : result.stats.median_font_size,
                        result.stats.median_font_size);

        const float offset = block.x - result.stats.median_x;
        if (offset > result.stats.median_line_height * 0.5f) {
            normalized.indent_level = static_cast<int>(
                std::lround(offset / std::max(result.stats.median_line_height, 1.0f)));
        }
        result.blocks.push_back(std::move(normalized));
    }

    std::sort(result.blocks.begin(), result.blocks.end(),
              [](const NormalizedBlock &a, const NormalizedBlock &b) {
                  if (std::abs(a.block.y - b.block.y) > 2.0f)
                      return a.block.y > b.block.y;
                  return a.block.x < b.block.x;
              });

    return result;
}

} // namespace cpppdf::converter
