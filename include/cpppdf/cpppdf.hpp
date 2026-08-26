#pragma once
#include "types.hpp"
#include <string>
#include <vector>

namespace cpppdf {

class PdfDocument;

// PDF 문서를 로드한다. 실패하면 nullptr을 반환한다.
PdfDocument *open(const std::string &path);
void close(PdfDocument *doc);

// 기본 정보
int page_count(const PdfDocument *doc);
PageInfo page_info(const PdfDocument *doc, int page_index);

// 페이지의 텍스트 블록을 추출한다.
std::vector<TextBlock> extract_text(const PdfDocument *doc, int page_index);

// 페이지에 포함된 이미지를 추출한다.
std::vector<ImageData> extract_images(const PdfDocument *doc, int page_index);

// 페이지 텍스트와 이미지를 터미널에 렌더링한다.
void render_text(const PdfDocument *doc, int page_index);
void render_image(const ImageData &img, int max_cols = 0, int max_rows = 0);

} // namespace cpppdf
