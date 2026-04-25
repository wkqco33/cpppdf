#pragma once
#include "../../include/cpppdf/types.hpp"
#include <utility>

namespace cpppdf {
class PdfDocument;
}

namespace cpppdf::renderer {

// 현재 터미널 크기 (cols, rows)
std::pair<int, int> terminal_size();

// ANSI 트루컬러 + 유니코드 하프블록(▀)으로 이미지 출력
// max_cols/max_rows == 0 이면 자동 감지
void render_image(const ImageData& img, int max_cols = 0, int max_rows = 0);

// TextBlock 좌표 기반으로 페이지 텍스트를 터미널에 출력
void render_text(const PdfDocument& doc, int page_index,
                  int term_cols = 0, int term_rows = 0);

} // namespace cpppdf::renderer
