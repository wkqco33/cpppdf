#include "line_assembler.hpp"
#include <algorithm>
#include <cmath>

namespace cpppdf::converter {

namespace {

constexpr int role_rank(BlockRole role) noexcept {
    switch (role) {
        case BlockRole::H1:   return 3;
        case BlockRole::H2:   return 2;
        case BlockRole::H3:   return 1;
        case BlockRole::Body: return 0;
    }
    return 0;
}

} // namespace

std::vector<Line> assemble_lines(const NormalizationResult& normalized) {
    std::vector<Line> lines;
    if (normalized.blocks.empty()) return lines;

    std::vector<const NormalizedBlock*> current;
    float current_y = normalized.blocks.front().block.y;

    auto flush = [&]() {
        if (current.empty()) return;

        std::sort(current.begin(), current.end(),
                  [](const NormalizedBlock* a, const NormalizedBlock* b) {
                      return a->block.x < b->block.x;
                  });

        Line line;
        line.x = current.front()->block.x;
        line.y = current.front()->block.y;
        line.indent_level = current.front()->indent_level;

        size_t estimated_size = 0;
        for (const NormalizedBlock* block : current)
            estimated_size += block->block.text.size() + 1;
        line.text.reserve(estimated_size);

        for (const NormalizedBlock* block : current) {
            LineSegment segment;
            segment.text         = block->block.text;
            segment.x            = block->block.x;
            segment.role         = block->role;
            segment.indent_level = block->indent_level;
            line.segments.push_back(std::move(segment));

            if (!line.text.empty())
                line.text += ' ';
            line.text += block->block.text;

            if (role_rank(block->role) > role_rank(line.role))
                line.role = block->role;
            line.indent_level = std::min(line.indent_level, block->indent_level);
        }

        lines.push_back(std::move(line));
        current.clear();
    };

    for (const auto& block : normalized.blocks) {
        if (current.empty()) {
            current.push_back(&block);
            current_y = block.block.y;
            continue;
        }

        if (std::abs(block.block.y - current_y) <= normalized.stats.y_tolerance) {
            current.push_back(&block);
            continue;
        }

        flush();
        current.push_back(&block);
        current_y = block.block.y;
    }

    flush();
    return lines;
}

} // namespace cpppdf::converter
