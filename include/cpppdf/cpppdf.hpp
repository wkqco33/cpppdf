#pragma once
#include "types.hpp"
#include <string>
#include <vector>

namespace cpppdf {

class PdfDocument;

// PDF 문서 로드
// 반환 포인터는 nullptr이면 로드 실패
PdfDocument* open(const std::string& path);
void         close(PdfDocument* doc);

// 기본 정보
int      page_count(const PdfDocument* doc);
PageInfo page_info(const PdfDocument* doc, int page_index);

// 텍스트 추출 (Phase 3에서 구현)
std::vector<TextBlock> extract_text(const PdfDocument* doc, int page_index);

// 이미지 추출 (Phase 4에서 구현)
std::vector<ImageData> extract_images(const PdfDocument* doc, int page_index);

// 터미널 렌더링 (Phase 5에서 구현)
void render_text(const PdfDocument* doc, int page_index);
void render_image(const ImageData& img, int max_cols = 0, int max_rows = 0);

} // namespace cpppdf
