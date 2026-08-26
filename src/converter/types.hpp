#pragma once
#include "../../include/cpppdf/types.hpp"
#include <string>
#include <vector>

namespace cpppdf::converter {

enum class BlockRole {
    Body,
    H3,
    H2,
    H1,
};

enum class ParagraphKind {
    Body,
    Heading,
    BulletList,
    OrderedList,
    Table,
};

struct BlockStats {
    float median_font_size = 12.0f;
    float median_x = 0.0f;
    float median_line_height = 14.0f;
    float y_tolerance = 7.0f;
};

struct NormalizedBlock {
    TextBlock block;
    BlockRole role = BlockRole::Body;
    int indent_level = 0;
};

struct NormalizationResult {
    BlockStats stats;
    std::vector<NormalizedBlock> blocks;
};

struct LineSegment {
    std::string text;
    float x = 0.0f;
    BlockRole role = BlockRole::Body;
    int indent_level = 0;
};

struct Line {
    std::vector<LineSegment> segments;
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    BlockRole role = BlockRole::Body;
    int indent_level = 0;
};

struct Paragraph {
    ParagraphKind kind = ParagraphKind::Body;
    BlockRole role = BlockRole::Body;
    int indent_level = 0;
    std::vector<Line> lines;
    std::vector<std::string> items;
    std::vector<std::vector<std::string>> table_rows;
};

} // namespace cpppdf::converter
