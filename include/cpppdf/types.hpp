#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cpppdf {

struct PdfStream;

using PdfDict  = std::map<std::string, struct PdfObject>;
using PdfArray = std::vector<struct PdfObject>;

struct PdfStream {
    PdfDict              dict;
    std::vector<uint8_t> raw;
    std::vector<uint8_t> decoded;
    bool                 decoded_ok = false;
};

struct PdfObject {
    enum class Type : uint8_t {
        Null, Bool, Int, Real, String, Name, Array, Dict, Stream, Ref
    };

    Type    type    = Type::Null;
    bool    b       = false;
    int64_t i       = 0;
    double  d       = 0.0;
    std::string                s;
    PdfArray                   arr;
    PdfDict                    dict;
    std::shared_ptr<PdfStream> stream;
    int32_t ref_num = 0, ref_gen = 0;

    [[nodiscard]] bool is_null()   const noexcept { return type == Type::Null; }
    [[nodiscard]] bool is_bool()   const noexcept { return type == Type::Bool; }
    [[nodiscard]] bool is_int()    const noexcept { return type == Type::Int; }
    [[nodiscard]] bool is_number() const noexcept { return type == Type::Int || type == Type::Real; }
    [[nodiscard]] bool is_string() const noexcept { return type == Type::String; }
    [[nodiscard]] bool is_name()   const noexcept { return type == Type::Name; }
    [[nodiscard]] bool is_array()  const noexcept { return type == Type::Array; }
    [[nodiscard]] bool is_dict()   const noexcept { return type == Type::Dict || type == Type::Stream; }
    [[nodiscard]] bool is_stream() const noexcept { return type == Type::Stream; }
    [[nodiscard]] bool is_ref()    const noexcept { return type == Type::Ref; }

    [[nodiscard]] double as_number() const noexcept {
        return (type == Type::Int) ? static_cast<double>(i) : d;
    }
    [[nodiscard]] const PdfDict& as_dict() const noexcept {
        return (type == Type::Stream && stream) ? stream->dict : dict;
    }
    PdfDict& as_dict() noexcept {
        return (type == Type::Stream && stream) ? stream->dict : dict;
    }

    static PdfObject null_obj() { return {}; }

    static PdfObject from_bool(bool v) {
        PdfObject o; o.type = Type::Bool; o.b = v; return o;
    }
    static PdfObject from_int(int64_t v) {
        PdfObject o; o.type = Type::Int; o.i = v; return o;
    }
    static PdfObject from_real(double v) {
        PdfObject o; o.type = Type::Real; o.d = v; return o;
    }
    static PdfObject from_string(std::string v) {
        PdfObject o; o.type = Type::String; o.s = std::move(v); return o;
    }
    static PdfObject from_name(std::string v) {
        PdfObject o; o.type = Type::Name; o.s = std::move(v); return o;
    }
    static PdfObject from_array(PdfArray v) {
        PdfObject o; o.type = Type::Array; o.arr = std::move(v); return o;
    }
    static PdfObject from_dict(PdfDict v) {
        PdfObject o; o.type = Type::Dict; o.dict = std::move(v); return o;
    }
    static PdfObject from_stream(std::shared_ptr<PdfStream> s) {
        PdfObject o; o.type = Type::Stream; o.stream = std::move(s); return o;
    }
    static PdfObject from_ref(int32_t num, int32_t gen) {
        PdfObject o; o.type = Type::Ref; o.ref_num = num; o.ref_gen = gen; return o;
    }
};

// 추출 결과 타입
struct TextBlock {
    std::string text;
    float x = 0, y = 0, w = 0, h = 0;
    float font_size = 0;
};

struct ImageData {
    std::vector<uint8_t> pixels; // RGBA8
    int width = 0, height = 0;
};

struct PageInfo {
    int   index  = 0;
    float width  = 0;  // 포인트 단위
    float height = 0;
};

} // namespace cpppdf
