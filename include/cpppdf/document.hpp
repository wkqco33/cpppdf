#pragma once
#include "types.hpp"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace cpppdf {

class PdfDocumentImpl;

class PdfDocument {
public:
    PdfDocument();
    ~PdfDocument();
    PdfDocument(PdfDocument&&) noexcept;
    PdfDocument& operator=(PdfDocument&&) noexcept;

    PdfDocument(const PdfDocument&) = delete;
    PdfDocument& operator=(const PdfDocument&) = delete;

    bool load(const std::string& path);

    int      page_count() const;
    PageInfo page_info(int index) const;

    // 간접 참조를 실제 오브젝트로 resolve
    PdfObject resolve(const PdfObject& obj) const;
    PdfObject get_object(int32_t num, int32_t gen = 0) const;

    // 페이지 dict (resolve 완료)
    PdfObject get_page_dict(int index) const;

    // 페이지 content stream (decoded, 여러 스트림은 연결)
    std::vector<uint8_t> get_content(int index) const;

    // 페이지 리소스 dict (resolve 완료)
    PdfDict get_resources(int index) const;

    bool is_loaded() const;

private:
    std::unique_ptr<PdfDocumentImpl> impl_;
};

} // namespace cpppdf
