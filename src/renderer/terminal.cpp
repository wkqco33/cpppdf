#include "terminal.hpp"
#include "../document/document.hpp"
#include "../extractor/text.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <map>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

namespace cpppdf::renderer {

namespace {

static bool is_visible_text(const std::string& text) {
    for (unsigned char ch : text) {
        if (!std::isspace(ch)) return true;
    }
    return false;
}

struct Utf8Prefix {
    int bytes = 0;
    int chars = 0;
};

static Utf8Prefix next_utf8_char(const std::string& text, size_t offset) {
    if (offset >= text.size()) {
        return {};
    }
    const auto c = static_cast<uint8_t>(text[offset]);
    const int seq = (c < 0x80u) ? 1 :
                    (c < 0xE0u) ? 2 :
                    (c < 0xF0u) ? 3 : 4;
    if (offset + static_cast<size_t>(seq) > text.size()) {
        return {};
    }
    return {seq, 1};
}

struct BlockBounds {
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
};

struct PageLayout {
    int content_left = 0;
    int content_top = 0;
    int content_cols = 0;
    int content_rows = 0;
    bool has_border = false;
};

struct ContentWindow {
    int start_col = 0;
    int cols = 0;
    int start_row = 0;
    int rows = 0;
};

constexpr float kTerminalCellAspect = 2.0F;
constexpr int kHorizontalPadding = 4;
constexpr int kVerticalPadding = 1;
constexpr int kMinInnerWidthPadding = 6;

static BlockBounds compute_bounds(const std::vector<TextBlock>& blocks) {
    BlockBounds bounds;
    bounds.min_x = bounds.max_x = blocks.front().x;
    bounds.min_y = bounds.max_y = blocks.front().y;

    for (const auto& block : blocks) {
        bounds.min_x = std::min(bounds.min_x, block.x);
        bounds.max_x = std::max(bounds.max_x, block.x);
        bounds.min_y = std::min(bounds.min_y, block.y);
        bounds.max_y = std::max(bounds.max_y, block.y);
    }
    return bounds;
}

static PageLayout compute_page_layout(PageInfo page_info, int term_cols, int term_rows) {
    PageLayout layout;
    if (term_cols < 4 || term_rows < 4) {
        return layout;
    }

    const int max_content_cols = term_cols - 2;
    const int max_content_rows = term_rows - 2;
    if (max_content_cols < 1 || max_content_rows < 1) {
        return layout;
    }

    const float page_width = std::max(1.0F, page_info.width);
    const float page_height = std::max(1.0F, page_info.height);
    const float scale = std::min(static_cast<float>(max_content_cols) / page_width,
                                 static_cast<float>(max_content_rows) * kTerminalCellAspect /
                                     page_height);

    layout.content_cols = std::max(1, static_cast<int>(page_width * scale));
    layout.content_rows = std::max(1, static_cast<int>(page_height * scale / kTerminalCellAspect));
    layout.content_left = std::max(1, (term_cols - layout.content_cols) / 2);
    layout.content_top = std::max(1, (term_rows - layout.content_rows) / 2);
    layout.has_border = true;
    return layout;
}

static int write_utf8_cells(std::vector<std::string>& cells, int start_col, const std::string& text) {
    int col = start_col;
    size_t offset = 0;
    while (offset < text.size() && col < static_cast<int>(cells.size())) {
        const Utf8Prefix prefix = next_utf8_char(text, offset);
        if (prefix.bytes <= 0) {
            break;
        }
        cells[static_cast<size_t>(col)] = text.substr(offset, static_cast<size_t>(prefix.bytes));
        offset += static_cast<size_t>(prefix.bytes);
        col += prefix.chars;
    }
    return col - start_col;
}

static char border_at(const PageLayout& layout, int row, int col) {
    if (!layout.has_border) {
        return '\0';
    }

    const int left = layout.content_left - 1;
    const int right = layout.content_left + layout.content_cols;
    const int top = layout.content_top - 1;
    const int bottom = layout.content_top + layout.content_rows;
    const bool on_left = col == left;
    const bool on_right = col == right;
    const bool on_top = row == top;
    const bool on_bottom = row == bottom;

    if ((on_top || on_bottom) && (on_left || on_right)) {
        return '+';
    }
    if ((on_top || on_bottom) && col > left && col < right) {
        return '-';
    }
    if ((on_left || on_right) && row > top && row < bottom) {
        return '|';
    }
    return '\0';
}

static int clamp_start(int start, int span, int limit) {
    if (span >= limit) {
        return 0;
    }
    return std::clamp(start, 0, limit - span);
}

static int aspect_rows_for_cols(PageInfo page_info, int content_cols) {
    const float page_width = std::max(1.0F, page_info.width);
    const float page_height = std::max(1.0F, page_info.height);
    return std::max(1, static_cast<int>(
        (static_cast<float>(content_cols) * page_height) / (page_width * kTerminalCellAspect)));
}

static int aspect_cols_for_rows(PageInfo page_info, int content_rows) {
    const float page_width = std::max(1.0F, page_info.width);
    const float page_height = std::max(1.0F, page_info.height);
    return std::max(1, static_cast<int>(
        (static_cast<float>(content_rows) * page_width * kTerminalCellAspect) / page_height));
}

static ContentWindow compute_content_window(const std::vector<std::vector<std::string>>& content) {
    ContentWindow window;
    if (content.empty() || content.front().empty()) {
        return window;
    }

    const int total_rows = static_cast<int>(content.size());
    const int total_cols = static_cast<int>(content.front().size());

    int used_left = static_cast<int>(content.front().size());
    int used_right = -1;
    int used_top = static_cast<int>(content.size());
    int used_bottom = -1;
    for (size_t row_index = 0; row_index < content.size(); ++row_index) {
        const auto& row = content[row_index];
        int row_left = static_cast<int>(row.size());
        int row_right = -1;
        for (size_t col = 0; col < row.size(); ++col) {
            if (row[col] == " ") {
                continue;
            }
            row_left = std::min(row_left, static_cast<int>(col));
            row_right = static_cast<int>(col);
        }
        if (row_right >= 0) {
            used_left = std::min(used_left, row_left);
            used_right = std::max(used_right, row_right);
            used_top = std::min(used_top, static_cast<int>(row_index));
            used_bottom = static_cast<int>(row_index);
        }
    }

    if (used_right < 0) {
        window.cols = total_cols;
        window.rows = total_rows;
        return window;
    }

    window.start_col = std::max(0, used_left - kHorizontalPadding);
    const int end_col = std::min(total_cols - 1, used_right + kHorizontalPadding);
    window.cols = std::max(1, end_col - window.start_col + 1);
    window.start_row = std::max(0, used_top - kVerticalPadding);
    const int end_row = std::min(total_rows - 1, used_bottom + kVerticalPadding);
    window.rows = std::max(1, end_row - window.start_row + 1);
    return window;
}

static auto right_trim(std::string line) -> std::string {
    while (!line.empty() && line.back() == ' ') {
        line.pop_back();
    }
    return line;
}

} // namespace

// ---- 터미널 크기 ----

std::pair<int, int> terminal_size() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 &&
        ws.ws_col > 0 && ws.ws_row > 0)
        return {ws.ws_col, ws.ws_row};
    return {80, 24};
}

// ---- 이미지 렌더링 ----

// nearest-neighbor로 (out_w, out_h) 크기의 RGBA 버퍼 생성
static std::vector<uint8_t>
scale_image(const ImageData& img, int out_w, int out_h) {
    std::vector<uint8_t> out(static_cast<size_t>(out_w * out_h * 4));

    float sx = static_cast<float>(img.width)  / out_w;
    float sy = static_cast<float>(img.height) / out_h;

    for (int y = 0; y < out_h; y++) {
        int src_y = std::min(static_cast<int>(y * sy), img.height - 1);
        for (int x = 0; x < out_w; x++) {
            int src_x = std::min(static_cast<int>(x * sx), img.width - 1);
            size_t src = static_cast<size_t>((src_y * img.width + src_x) * 4);
            size_t dst = static_cast<size_t>((y * out_w + x) * 4);
            memcpy(out.data() + dst, img.pixels.data() + src, 4);
        }
    }
    return out;
}

void render_image(const ImageData& img, int max_cols, int max_rows) {
    if (img.width == 0 || img.height == 0 || img.pixels.empty()) return;

    auto [tw, th] = terminal_size();
    if (max_cols <= 0) max_cols = tw;
    if (max_rows <= 0) max_rows = th - 1;

    // ▀ 트릭: 한 셀 = 2 픽셀 행
    // 터미널 셀은 대략 높이:너비 = 2:1 이므로 이미지 픽셀 비율과 맞춤
    int px_w = max_cols;
    int px_h = max_rows * 2;

    // 종횡비 유지
    float scale = std::min(static_cast<float>(px_w) / img.width,
                           static_cast<float>(px_h) / img.height);
    int out_w = std::max(1, static_cast<int>(img.width  * scale));
    int out_h = std::max(2, static_cast<int>(img.height * scale));
    if (out_h % 2 != 0) out_h--;

    auto scaled = scale_image(img, out_w, out_h);

    // 출력 버퍼: ANSI + UTF-8 모아서 한번에 fwrite
    std::string buf;
    buf.reserve(static_cast<size_t>(out_w * (out_h / 2)) * 40);

    auto pixel = [&](int x, int y) -> const uint8_t* {
        y = std::clamp(y, 0, out_h - 1);
        x = std::clamp(x, 0, out_w - 1);
        return scaled.data() + static_cast<size_t>((y * out_w + x) * 4);
    };

    char esc[64];
    for (int y = 0; y < out_h; y += 2) {
        for (int x = 0; x < out_w; x++) {
            const uint8_t* top = pixel(x, y);
            const uint8_t* bot = pixel(x, y + 1);

            // 전경(▀ 위쪽) = top 픽셀
            snprintf(esc, sizeof(esc),
                     "\033[38;2;%d;%d;%d;48;2;%d;%d;%dm",
                     top[0], top[1], top[2],
                     bot[0], bot[1], bot[2]);
            buf += esc;
            buf += "\xe2\x96\x80"; // ▀
        }
        buf += "\033[0m\n";
    }
    buf += "\033[0m";

    fwrite(buf.data(), 1, buf.size(), stdout);
    fflush(stdout);
}

// ---- 텍스트 렌더링 ----

void render_text(const PdfDocument& doc, int page_index,
                  int term_cols, int term_rows) {
    auto [tw, th] = terminal_size();
    if (term_cols <= 0) term_cols = tw;
    if (term_rows <= 0) term_rows = th - 1;

    auto raw_blocks = extractor::extract_text(doc, page_index);
    if (raw_blocks.empty()) {
        puts("(텍스트 없음)");
        return;
    }

    std::vector<TextBlock> blocks;
    blocks.reserve(raw_blocks.size());
    for (auto& block : raw_blocks) {
        if (is_visible_text(block.text))
            blocks.push_back(std::move(block));
    }

    if (blocks.empty()) {
        puts("(텍스트 없음)");
        return;
    }

    const BlockBounds bounds = compute_bounds(blocks);
    PageInfo page_info = doc.page_info(page_index);
    if (page_info.width <= 0.0F || page_info.height <= 0.0F) {
        page_info.width = bounds.max_x - bounds.min_x;
        page_info.height = bounds.max_y - bounds.min_y;
    }

    const PageLayout layout = compute_page_layout(page_info, term_cols, term_rows);
    if (layout.content_cols <= 0 || layout.content_rows <= 0) {
        puts("(터미널이 너무 작음)");
        return;
    }

    const float x_span = std::max(1.0f, bounds.max_x - bounds.min_x);
    const float y_span = std::max(1.0f, bounds.max_y - bounds.min_y);

    auto to_col = [&](float x) -> int {
        const float norm = (x - bounds.min_x) / x_span;
        return static_cast<int>(norm * std::max(0, layout.content_cols - 1));
    };
    auto to_row = [&](float y) -> int {
        const float norm = 1.0f - ((y - bounds.min_y) / y_span);
        return static_cast<int>(norm * std::max(0, layout.content_rows - 1));
    };

    std::vector<std::vector<const TextBlock*>> lines(static_cast<size_t>(layout.content_rows));
    for (const auto& b : blocks) {
        const int row = to_row(b.y);
        if (row >= 0 && row < layout.content_rows)
            lines[static_cast<size_t>(row)].push_back(&b);
    }

    std::vector<std::vector<std::string>> content(
        static_cast<size_t>(layout.content_rows),
        std::vector<std::string>(static_cast<size_t>(layout.content_cols), " "));

    for (size_t row_index = 0; row_index < lines.size(); ++row_index) {
        auto& row_blocks = lines[row_index];
        if (row_blocks.empty()) {
            continue;
        }
        std::sort(row_blocks.begin(), row_blocks.end(),
                  [](const TextBlock* a, const TextBlock* b) {
                      return a->x < b->x;
                  });

        std::vector<std::string>& line = content[row_index];
        int cur_col = layout.content_left;

        for (const TextBlock* b : row_blocks) {
            const int col = layout.content_left + to_col(b->x);
            if (col > cur_col) {
                cur_col = col;
            }
            if (cur_col >= layout.content_left + layout.content_cols) {
                break;
            }
            const int available = layout.content_left + layout.content_cols - cur_col;
            if (available <= 0) {
                break;
            }

            const int content_col = cur_col - layout.content_left;
            cur_col += write_utf8_cells(line, content_col, b->text);
        }
    }

    ContentWindow window = compute_content_window(content);
    PageLayout fitted_layout = layout;
    const int aspect_cols = aspect_cols_for_rows(page_info, window.rows);
    const int blended_cols = window.cols + ((aspect_cols - window.cols) / 2);
    fitted_layout.content_cols = std::min(term_cols - 2,
                                          std::max(window.cols + kMinInnerWidthPadding,
                                                   std::max(window.cols, blended_cols)));
    const int aspect_rows = aspect_rows_for_cols(page_info, fitted_layout.content_cols);
    const int blended_rows = window.rows + ((aspect_rows - window.rows) / 2);
    fitted_layout.content_rows = std::min(layout.content_rows,
                                          std::max(window.rows, blended_rows));
    fitted_layout.content_left = std::max(1, (term_cols - fitted_layout.content_cols) / 2);
    fitted_layout.content_top = std::max(1, (term_rows - fitted_layout.content_rows) / 2);

    window.start_row = clamp_start(
        window.start_row - ((fitted_layout.content_rows - window.rows) / 2),
        fitted_layout.content_rows,
        layout.content_rows);
    const int content_col_offset = std::max(0, (fitted_layout.content_cols - window.cols) / 2);

    for (int row = 0; row < term_rows; ++row) {
        std::string rendered;
        rendered.reserve(static_cast<size_t>(term_cols) * 2);

        for (int col = 0; col < term_cols; ++col) {
            if (const char border = border_at(fitted_layout, row, col); border != '\0') {
                rendered.push_back(border);
                continue;
            }

            const bool inside_content =
                row >= fitted_layout.content_top &&
                row < fitted_layout.content_top + fitted_layout.content_rows &&
                col >= fitted_layout.content_left &&
                col < fitted_layout.content_left + fitted_layout.content_cols;
            if (!inside_content) {
                rendered.push_back(' ');
                continue;
            }

            const int content_row = window.start_row + (row - fitted_layout.content_top);
            const int fitted_col = col - fitted_layout.content_left;
            if (fitted_col < content_col_offset ||
                fitted_col >= content_col_offset + window.cols) {
                rendered.push_back(' ');
                continue;
            }

            const int content_col =
                window.start_col + (fitted_col - content_col_offset);
            rendered += content[static_cast<size_t>(content_row)][static_cast<size_t>(content_col)];
        }

        const std::string trimmed = right_trim(rendered);
        if (trimmed.empty()) {
            putchar('\n');
            continue;
        }
        puts(trimmed.c_str());
    }
}

} // namespace cpppdf::renderer
