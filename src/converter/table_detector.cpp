#include "table_detector.hpp"
#include <algorithm>
#include <cmath>

namespace cpppdf::converter {

namespace {

static bool is_table_candidate(const Paragraph &paragraph, const BlockStats &stats,
                               std::vector<std::vector<std::string>> *rows) {
    if (paragraph.kind != ParagraphKind::Body || paragraph.lines.size() < 2)
        return false;

    const size_t column_count = paragraph.lines.front().segments.size();
    if (column_count < 2)
        return false;

    const float tolerance = std::max(8.0f, stats.median_line_height * 0.75f);
    std::vector<float> anchors;
    anchors.reserve(column_count);
    for (const auto &segment : paragraph.lines.front().segments)
        anchors.push_back(segment.x);

    rows->clear();
    rows->reserve(paragraph.lines.size());

    for (const auto &line : paragraph.lines) {
        if (line.segments.size() != column_count)
            return false;

        std::vector<std::string> row;
        row.reserve(column_count);

        for (size_t i = 0; i < column_count; ++i) {
            if (std::abs(line.segments[i].x - anchors[i]) > tolerance)
                return false;
            row.push_back(line.segments[i].text);
        }

        rows->push_back(std::move(row));
    }

    return rows->size() >= 2;
}

} // namespace

std::vector<Paragraph> detect_tables(std::vector<Paragraph> paragraphs, const BlockStats &stats) {
    for (auto &paragraph : paragraphs) {
        std::vector<std::vector<std::string>> rows;
        if (!is_table_candidate(paragraph, stats, &rows))
            continue;

        paragraph.kind = ParagraphKind::Table;
        paragraph.table_rows = std::move(rows);
    }

    return paragraphs;
}

} // namespace cpppdf::converter
