#include "paragraph_splitter.hpp"
#include <algorithm>
#include <cmath>
#include <regex>

namespace cpppdf::converter {

namespace {

static std::string strip_marker(const std::string& text, const std::regex& pattern) {
    return std::regex_replace(text, pattern, std::string(), std::regex_constants::format_first_only);
}

static Paragraph build_body_or_list(const std::vector<Line>& chunk) {
    static const std::regex kBulletPattern(R"(^\s*[-*•·]\s+)");
    static const std::regex kOrderedPattern(R"(^\s*\d+[.)]\s+)");

    Paragraph paragraph;
    paragraph.kind         = ParagraphKind::Body;
    paragraph.role         = BlockRole::Body;
    paragraph.indent_level = chunk.empty() ? 0 : chunk.front().indent_level;
    paragraph.lines        = chunk;

    if (chunk.empty()) return paragraph;

    const bool bullet_first  = std::regex_search(chunk.front().text, kBulletPattern);
    const bool ordered_first = std::regex_search(chunk.front().text, kOrderedPattern);

    if (!bullet_first && !ordered_first)
        return paragraph;

    paragraph.kind = bullet_first ? ParagraphKind::BulletList
                                  : ParagraphKind::OrderedList;
    paragraph.items.clear();

    const std::regex& pattern = bullet_first ? kBulletPattern : kOrderedPattern;
    for (const auto& line : chunk) {
        if (std::regex_search(line.text, pattern)) {
            paragraph.items.push_back(strip_marker(line.text, pattern));
            continue;
        }

        if (!paragraph.items.empty()) {
            paragraph.items.back() += ' ';
            paragraph.items.back() += line.text;
        } else {
            paragraph.kind = ParagraphKind::Body;
            paragraph.items.clear();
            return paragraph;
        }
    }

    return paragraph;
}

} // namespace

std::vector<Paragraph> split_paragraphs(const std::vector<Line>& lines,
                                        const BlockStats& stats) {
    std::vector<Paragraph> paragraphs;
    if (lines.empty()) return paragraphs;

    std::vector<Line> chunk;

    auto flush_chunk = [&]() {
        if (chunk.empty()) return;
        paragraphs.push_back(build_body_or_list(chunk));
        chunk.clear();
    };

    for (size_t i = 0; i < lines.size(); ++i) {
        const Line& line = lines[i];

        if (line.role != BlockRole::Body) {
            flush_chunk();

            Paragraph heading;
            heading.kind         = ParagraphKind::Heading;
            heading.role         = line.role;
            heading.indent_level = line.indent_level;
            heading.lines.push_back(line);
            paragraphs.push_back(std::move(heading));
            continue;
        }

        if (chunk.empty()) {
            chunk.push_back(line);
            continue;
        }

        const Line& prev = chunk.back();
        const float gap  = prev.y - line.y;

        if (gap > stats.median_line_height * 1.4f) {
            flush_chunk();
        }

        chunk.push_back(line);
    }

    flush_chunk();
    return paragraphs;
}

} // namespace cpppdf::converter
