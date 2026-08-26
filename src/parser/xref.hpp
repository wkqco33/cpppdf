#pragma once
#include "../../include/cpppdf/types.hpp"
#include "lexer.hpp"
#include <cstdint>
#include <map>

namespace cpppdf::parser {

enum class XRefEntryType : uint8_t {
    Free,
    InUse,
    Compressed,
};

struct XRefEntry {
    uint64_t offset = 0;
    int32_t gen = 0;
    bool in_use = false; // true=n, false=f
    XRefEntryType type = XRefEntryType::Free;
    int32_t obj_stream_num = 0;
    int32_t obj_stream_index = -1;
};

using XRefTable = std::map<int32_t, XRefEntry>;

struct XRefResult {
    XRefTable table;
    PdfDict trailer;
    bool ok = false;
};

// 파일 끝에서 startxref 오프셋을 찾아서 반환
// 실패 시 0 반환
size_t find_startxref(const uint8_t *data, size_t size);

// startxref 오프셋에서 xref와 trailer를 파싱
// prev xref가 있으면 재귀적으로 처리
XRefResult parse_xref(const uint8_t *data, size_t size, size_t xref_offset);

} // namespace cpppdf::parser
