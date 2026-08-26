#include "md_renderer.hpp"
#include <sstream>

namespace cpppdf::converter {

namespace {

static std::string join_lines(const std::vector<Line> &lines) {
    std::string text;
    for (const auto &line : lines) {
        if (!text.empty())
            text += ' ';
        text += line.text;
    }
    return text;
}

static std::string escape_cell(std::string text) {
    size_t pos = 0;
    while ((pos = text.find('|', pos)) != std::string::npos) {
        text.replace(pos, 1, "\\|");
        pos += 2;
    }
    return text;
}

static int heading_level(BlockRole role) {
    switch (role) {
    case BlockRole::H1:
        return 1;
    case BlockRole::H2:
        return 2;
    case BlockRole::H3:
        return 3;
    case BlockRole::Body:
        return 0;
    }
    return 0;
}

} // namespace

std::string render_markdown(const std::vector<Paragraph> &paragraphs) {
    std::ostringstream out;

    for (const auto &paragraph : paragraphs) {
        switch (paragraph.kind) {
        case ParagraphKind::Heading: {
            const int level = heading_level(paragraph.role);
            out << std::string(static_cast<size_t>(level), '#') << ' '
                << join_lines(paragraph.lines) << "\n\n";
            break;
        }

        case ParagraphKind::Body:
            out << join_lines(paragraph.lines) << "\n\n";
            break;

        case ParagraphKind::BulletList: {
            const std::string indent(static_cast<size_t>(paragraph.indent_level * 2), ' ');
            for (const auto &item : paragraph.items)
                out << indent << "- " << item << '\n';
            out << '\n';
            break;
        }

        case ParagraphKind::OrderedList: {
            const std::string indent(static_cast<size_t>(paragraph.indent_level * 2), ' ');
            for (const auto &item : paragraph.items)
                out << indent << "1. " << item << '\n';
            out << '\n';
            break;
        }

        case ParagraphKind::Table: {
            if (paragraph.table_rows.empty())
                break;

            const auto &header = paragraph.table_rows.front();
            out << '|';
            for (const auto &cell : header)
                out << ' ' << escape_cell(cell) << " |";
            out << '\n';

            out << '|';
            for (size_t i = 0; i < header.size(); ++i)
                out << " --- |";
            out << '\n';

            for (size_t row = 1; row < paragraph.table_rows.size(); ++row) {
                out << '|';
                for (const auto &cell : paragraph.table_rows[row])
                    out << ' ' << escape_cell(cell) << " |";
                out << '\n';
            }
            out << '\n';
            break;
        }
        }
    }

    return out.str();
}

} // namespace cpppdf::converter
