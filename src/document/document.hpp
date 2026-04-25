#pragma once
#include "../../include/cpppdf/types.hpp"
#include "../parser/xref.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace cpppdf {

class PdfDocument {
public:
    bool load(const std::string& path);

    int      page_count() const;
    PageInfo page_info(int index) const;

    // 간접 참조를 실제 오브젝트로 resolve
    // ref가 아니면 그대로 반환
    PdfObject resolve(const PdfObject& obj) const;
    PdfObject get_object(int32_t num, int32_t gen = 0) const;

    // 페이지 dict (resolve 완료)
    PdfObject get_page_dict(int index) const;

    // 페이지 content stream (decoded, 여러 스트림은 연결)
    std::vector<uint8_t> get_content(int index) const;

    // 페이지 리소스 dict (resolve 완료)
    PdfDict get_resources(int index) const;

    bool is_loaded() const { return !data_.empty(); }

private:
    std::vector<uint8_t>                          data_;
    parser::XRefTable                             xref_;
    PdfDict                                       trailer_;
    mutable std::unordered_map<int32_t, PdfObject> obj_cache_;

    // 페이지 트리 순회 결과 (load 시 구축)
    std::vector<PdfObject> pages_;

    void build_page_list(const PdfObject& node, PdfDict inherited_resources,
                         std::array<float, 4> inherited_media_box);

    // 오브젝트 파싱 (캐시 없음)
    PdfObject parse_object(int32_t num) const;
    PdfObject parse_object_from_stream(int32_t object_stream_num,
                                       int32_t target_num,
                                       int32_t target_index) const;

    // 스트림 raw 데이터 채우기
    void fill_stream_raw(PdfStream& stream, size_t stream_keyword_pos) const;
};

} // namespace cpppdf
