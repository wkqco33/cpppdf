#include "document.hpp"
#include "../parser/lexer.hpp"
#include "../parser/object_parser.hpp"
#include "../parser/xref.hpp"
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cpppdf {

// ---- 파일 로드 ----

bool PdfDocument::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    f.seekg(0, std::ios::end);
    auto sz = f.tellg();
    if (sz <= 0) return false;

    data_.resize(static_cast<size_t>(sz));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(data_.data()), sz);

    if (data_.size() < 8 || memcmp(data_.data(), "%PDF-", 5) != 0)
        return false;

    size_t xref_offset = parser::find_startxref(data_.data(), data_.size());
    if (xref_offset == 0) return false;

    auto xr = parser::parse_xref(data_.data(), data_.size(), xref_offset);
    if (!xr.ok) return false;

    xref_    = std::move(xr.table);
    trailer_ = std::move(xr.trailer);

    // 페이지 트리 구축
    PdfObject catalog = resolve(trailer_.count("Root") ? trailer_.at("Root")
                                                       : PdfObject::null_obj());
    if (!catalog.is_dict()) return false;

    auto& cdict = catalog.as_dict();
    auto pit = cdict.find("Pages");
    if (pit == cdict.end()) return false;

    PdfObject pages_root = resolve(pit->second);
    build_page_list(pages_root, {}, {0, 0, 0, 0});

    return true;
}

// ---- 오브젝트 파싱 ----

void PdfDocument::fill_stream_raw(PdfStream& stream, size_t stream_kw_pos) const {
    const uint8_t* buf = data_.data();
    size_t size = data_.size();

    // "stream" 키워드 다음: LF 또는 CR LF
    size_t pos = stream_kw_pos;
    if (pos < size && buf[pos] == '\r') ++pos;
    if (pos < size && buf[pos] == '\n') ++pos;

    // /Length 값 읽기
    auto lit = stream.dict.find("Length");
    if (lit == stream.dict.end()) return;

    const PdfObject& len_obj = lit->second;
    int64_t length = 0;
    if (len_obj.is_int()) {
        length = len_obj.i;
    } else if (len_obj.is_ref()) {
        PdfObject resolved = get_object(len_obj.ref_num, len_obj.ref_gen);
        if (resolved.is_int()) length = resolved.i;
    }

    if (length <= 0 || pos + static_cast<size_t>(length) > size) return;
    stream.raw.assign(buf + pos, buf + pos + length);
}

PdfObject PdfDocument::parse_object(int32_t num) const {
    auto it = xref_.find(num);
    if (it == xref_.end() || !it->second.in_use) return PdfObject::null_obj();

    if (it->second.type == parser::XRefEntryType::Compressed) {
        return parse_object_from_stream(it->second.obj_stream_num,
                                        num,
                                        it->second.obj_stream_index);
    }

    size_t offset = static_cast<size_t>(it->second.offset);
    if (offset >= data_.size()) return PdfObject::null_obj();

    parser::Lexer lex(data_.data(), data_.size(), offset);

    // <num> <gen> obj
    parser::Token t1 = lex.next();
    parser::Token t2 = lex.next();
    parser::Token t3 = lex.next();
    (void)t1; (void)t2;

    if (t3.type != parser::TokType::Keyword || t3.sv != "obj")
        return PdfObject::null_obj();

    PdfObject obj = parser::parse_value(lex);

    // stream 인 경우 raw 데이터를 채움
    if (obj.is_stream() && obj.stream && obj.stream->raw.empty()) {
        // parse_value → parse_dict_or_stream이 "stream" 키워드까지 소비함
        fill_stream_raw(*obj.stream, lex.pos());
    }

    return obj;
}

PdfObject PdfDocument::parse_object_from_stream(int32_t object_stream_num,
                                                int32_t target_num,
                                                int32_t target_index) const {
    PdfObject object_stream = parse_object(object_stream_num);
    if (!object_stream.is_stream() || !object_stream.stream)
        return PdfObject::null_obj();

    PdfStream& stream = *object_stream.stream;
    if (!stream.decoded_ok && !parser::decode_stream(stream))
        return PdfObject::null_obj();

    const PdfDict& dict = stream.dict;

    auto get_int_value = [&](const char* key, int fallback) -> int {
        auto it = dict.find(key);
        if (it == dict.end()) return fallback;
        PdfObject value = resolve(it->second);
        return value.is_int() ? static_cast<int>(value.i) : fallback;
    };

    const int object_count = get_int_value("N", 0);
    const int first_offset = get_int_value("First", -1);
    if (object_count <= 0 || first_offset < 0 ||
        static_cast<size_t>(first_offset) >= stream.decoded.size()) {
        return PdfObject::null_obj();
    }

    parser::Lexer lex(stream.decoded.data(), stream.decoded.size(), 0);
    std::vector<std::pair<int32_t, int32_t>> index;
    index.reserve(static_cast<size_t>(object_count));

    for (int i = 0; i < object_count; ++i) {
        parser::Token num_tok = lex.next();
        parser::Token off_tok = lex.next();
        if (num_tok.type != parser::TokType::Int ||
            off_tok.type != parser::TokType::Int) {
            return PdfObject::null_obj();
        }
        index.emplace_back(static_cast<int32_t>(num_tok.iv),
                           static_cast<int32_t>(off_tok.iv));
    }

    int resolved_index = target_index;
    if (resolved_index < 0 || resolved_index >= static_cast<int>(index.size())) {
        resolved_index = -1;
        for (size_t i = 0; i < index.size(); ++i) {
            if (index[i].first == target_num) {
                resolved_index = static_cast<int>(i);
                break;
            }
        }
        if (resolved_index < 0)
            return PdfObject::null_obj();
    }

    if (index[static_cast<size_t>(resolved_index)].first != target_num) {
        for (size_t i = 0; i < index.size(); ++i) {
            if (index[i].first == target_num) {
                resolved_index = static_cast<int>(i);
                break;
            }
        }
        if (resolved_index < 0 ||
            index[static_cast<size_t>(resolved_index)].first != target_num) {
            return PdfObject::null_obj();
        }
    }

    const int relative_offset = index[static_cast<size_t>(resolved_index)].second;
    const size_t object_offset = static_cast<size_t>(first_offset + relative_offset);
    if (object_offset >= stream.decoded.size())
        return PdfObject::null_obj();

    parser::Lexer object_lex(stream.decoded.data(), stream.decoded.size(), object_offset);
    return parser::parse_value(object_lex);
}

PdfObject PdfDocument::get_object(int32_t num, int32_t /*gen*/) const {
    auto it = obj_cache_.find(num);
    if (it != obj_cache_.end()) return it->second;

    PdfObject obj = parse_object(num);
    obj_cache_[num] = obj;
    return obj;
}

PdfObject PdfDocument::resolve(const PdfObject& obj) const {
    if (!obj.is_ref()) return obj;
    return get_object(obj.ref_num, obj.ref_gen);
}

// ---- 페이지 트리 ----

void PdfDocument::build_page_list(const PdfObject& node,
                                   PdfDict inherited_resources,
                                   std::array<float, 4> inherited_media_box) {
    if (!node.is_dict()) return;

    const PdfDict& d = node.as_dict();

    // /Type
    auto type_it = d.find("Type");
    if (type_it == d.end()) return;
    if (!type_it->second.is_name()) return;
    const std::string& type = type_it->second.s;

    // 리소스 상속
    auto res_it = d.find("Resources");
    if (res_it != d.end()) {
        PdfObject res = resolve(res_it->second);
        if (res.is_dict()) inherited_resources = res.as_dict();
    }

    // MediaBox 상속
    auto mb_it = d.find("MediaBox");
    if (mb_it != d.end()) {
        PdfObject mb = resolve(mb_it->second);
        if (mb.is_array() && mb.arr.size() >= 4) {
            for (int k = 0; k < 4; k++) {
                PdfObject v = resolve(mb.arr[static_cast<size_t>(k)]);
                inherited_media_box[static_cast<size_t>(k)] =
                    static_cast<float>(v.as_number());
            }
        }
    }

    if (type == "Pages") {
        auto kids_it = d.find("Kids");
        if (kids_it == d.end()) return;
        PdfObject kids = resolve(kids_it->second);
        if (!kids.is_array()) return;

        for (const auto& kid : kids.arr) {
            PdfObject child = resolve(kid);
            build_page_list(child, inherited_resources, inherited_media_box);
        }
    } else if (type == "Page") {
        pages_.push_back(node);
    }
}

// ---- 공개 인터페이스 ----

int PdfDocument::page_count() const {
    return static_cast<int>(pages_.size());
}

PageInfo PdfDocument::page_info(int index) const {
    PageInfo info;
    info.index = index;

    if (index < 0 || index >= static_cast<int>(pages_.size()))
        return info;

    const PdfObject& page = pages_[static_cast<size_t>(index)];
    const PdfDict& d = page.as_dict();

    auto mb_it = d.find("MediaBox");
    if (mb_it != d.end()) {
        PdfObject mb = resolve(mb_it->second);
        if (mb.is_array() && mb.arr.size() >= 4) {
            info.width  = static_cast<float>(resolve(mb.arr[2]).as_number());
            info.height = static_cast<float>(resolve(mb.arr[3]).as_number());
        }
    }
    return info;
}

PdfObject PdfDocument::get_page_dict(int index) const {
    if (index < 0 || index >= static_cast<int>(pages_.size()))
        return PdfObject::null_obj();
    return pages_[static_cast<size_t>(index)];
}

std::vector<uint8_t> PdfDocument::get_content(int index) const {
    PdfObject page = get_page_dict(index);
    if (!page.is_dict()) return {};

    const PdfDict& d = page.as_dict();
    auto cont_it = d.find("Contents");
    if (cont_it == d.end()) return {};

    PdfObject contents = resolve(cont_it->second);

    std::vector<uint8_t> result;

    auto append_stream = [&](const PdfObject& obj) {
        if (!obj.is_stream() || !obj.stream) return;
        if (!obj.stream->decoded_ok)
            parser::decode_stream(*obj.stream);
        if (obj.stream->decoded_ok) {
            result.insert(result.end(),
                          obj.stream->decoded.begin(),
                          obj.stream->decoded.end());
            // 스트림 사이 공백 구분자
            result.push_back(' ');
        }
    };

    if (contents.is_array()) {
        for (const auto& item : contents.arr)
            append_stream(resolve(item));
    } else {
        append_stream(contents);
    }

    return result;
}

PdfDict PdfDocument::get_resources(int index) const {
    PdfObject page = get_page_dict(index);
    if (!page.is_dict()) return {};

    const PdfDict& d = page.as_dict();
    auto res_it = d.find("Resources");
    if (res_it == d.end()) return {};

    PdfObject res = resolve(res_it->second);
    if (!res.is_dict()) return {};
    return res.as_dict();
}

} // namespace cpppdf
