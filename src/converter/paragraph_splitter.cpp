#include "paragraph_splitter.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>

namespace cpppdf::converter {

namespace {

static bool starts_with(std::string_view sv, std::string_view prefix) {
    return sv.size() >= prefix.size() && sv.substr(0, prefix.size()) == prefix;
}

// Bullet 마커: ^\s*[-*•·]\s+
static bool parse_bullet_marker(std::string_view text, size_t& content_pos) {
    size_t i = 0;
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
    if (i >= text.size()) return false;

    std::string_view sub = text.substr(i);
    size_t char_len = 0;
    if (starts_with(sub, "-") || starts_with(sub, "*")) {
        char_len = 1;
    } else if (starts_with(sub, "\xE2\x80\xA2")) { // •
        char_len = 3;
    } else if (starts_with(sub, "\xC2\xB7")) {     // ·
        char_len = 2;
    } else {
        return false;
    }
    i += char_len;

    if (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
        content_pos = i;
        return true;
    }
    return false;
}

// Ordered 마커: ^\s*\d+[.)]\s+
static bool parse_ordered_marker(std::string_view text, size_t& content_pos) {
    size_t i = 0;
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
    if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i]))) return false;

    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
    if (i >= text.size()) return false;

    if (text[i] != '.' && text[i] != ')') return false;
    ++i;

    if (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
        content_pos = i;
        return true;
    }
    return false;
}

static Paragraph build_body_or_list(const std::vector<Line>& chunk) {
    Paragraph paragraph;
    paragraph.kind         = ParagraphKind::Body;
    paragraph.role         = BlockRole::Body;
    paragraph.indent_level = chunk.empty() ? 0 : chunk.front().indent_level;
    paragraph.lines        = chunk;

    if (chunk.empty()) return paragraph;

    size_t dummy_pos = 0;
    const bool bullet_first  = parse_bullet_marker(chunk.front().text, dummy_pos);
    const bool ordered_first = parse_ordered_marker(chunk.front().text, dummy_pos);

    if (!bullet_first && !ordered_first)
        return paragraph;

    paragraph.kind = bullet_first ? ParagraphKind::BulletList
                                  : ParagraphKind::OrderedList;
    paragraph.items.clear();

    for (const auto& line : chunk) {
        size_t content_pos = 0;
        bool match = bullet_first ? parse_bullet_marker(line.text, content_pos)
                                  : parse_ordered_marker(line.text, content_pos);
        if (match) {
            paragraph.items.push_back(line.text.substr(content_pos));
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
