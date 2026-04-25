#include "image.hpp"
#include "../document/document.hpp"
#include "../parser/object_parser.hpp"
#include <cstring>

#ifdef CPPPDF_HAVE_JPEG
#include <jpeglib.h>
#include <setjmp.h>
#endif

namespace cpppdf::extractor {

// ---- JPEG 디코딩 ----

#ifdef CPPPDF_HAVE_JPEG
struct JpegErrMgr {
    jpeg_error_mgr base;
    jmp_buf        jmp;
};

static void jpeg_err_exit(j_common_ptr cinfo) {
    auto* mgr = reinterpret_cast<JpegErrMgr*>(cinfo->err);
    longjmp(mgr->jmp, 1);
}

static bool dct_decode(const uint8_t* src, size_t src_size,
                        std::vector<uint8_t>& dst,
                        int& width, int& height, int& components) {
    jpeg_decompress_struct cinfo;
    JpegErrMgr err_mgr;

    cinfo.err             = jpeg_std_error(&err_mgr.base);
    err_mgr.base.error_exit = jpeg_err_exit;

    if (setjmp(err_mgr.jmp)) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, const_cast<uint8_t*>(src), static_cast<unsigned long>(src_size));
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    width      = static_cast<int>(cinfo.output_width);
    height     = static_cast<int>(cinfo.output_height);
    components = cinfo.output_components;

    int row_stride = width * components;
    dst.resize(static_cast<size_t>(row_stride * height));

    while (cinfo.output_scanline < cinfo.output_height) {
        uint8_t* row = dst.data() +
                       static_cast<size_t>(cinfo.output_scanline) *
                       static_cast<size_t>(row_stride);
        jpeg_read_scanlines(&cinfo, &row, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return true;
}
#endif // CPPPDF_HAVE_JPEG

// ---- 색공간 이름 추출 ----

static std::string cs_name(const PdfObject& cs, const PdfDocument& doc) {
    if (cs.is_name()) return cs.s;
    if (cs.is_array() && !cs.arr.empty()) {
        PdfObject first = doc.resolve(cs.arr[0]);
        if (first.is_name()) return first.s;
    }
    return "DeviceRGB";
}

// ---- Indexed 색공간 팔레트 룩업 ----

static std::vector<uint8_t> expand_indexed(const PdfObject& cs_obj,
                                            const std::vector<uint8_t>& src,
                                            const PdfDocument& doc) {
    // /Indexed /base hival lookup
    if (!cs_obj.is_array() || cs_obj.arr.size() < 4) return src;

    PdfObject base_obj = doc.resolve(cs_obj.arr[0]);
    int components = 3; // default RGB
    std::string base = cs_name(doc.resolve(cs_obj.arr[1]), doc);
    if (base == "DeviceGray") components = 1;
    else if (base == "DeviceCMYK") components = 4;

    int hival = static_cast<int>(doc.resolve(cs_obj.arr[2]).as_number());
    PdfObject lookup_obj = doc.resolve(cs_obj.arr[3]);

    std::string palette;
    if (lookup_obj.is_string()) {
        palette = lookup_obj.s;
    } else if (lookup_obj.is_stream() && lookup_obj.stream) {
        if (!lookup_obj.stream->decoded_ok)
            parser::decode_stream(*lookup_obj.stream);
        palette.assign(lookup_obj.stream->decoded.begin(),
                       lookup_obj.stream->decoded.end());
    }

    int pal_size = (hival + 1) * components;
    palette.resize(static_cast<size_t>(pal_size), '\0');

    std::vector<uint8_t> out;
    out.reserve(src.size() * static_cast<size_t>(components));
    for (uint8_t idx : src) {
        int base_idx = static_cast<int>(idx) * components;
        for (int c = 0; c < components; c++)
            out.push_back(static_cast<uint8_t>(palette[static_cast<size_t>(base_idx + c)]));
    }
    return out;
}

// ---- raw 픽셀 → RGBA8 변환 ----

static ImageData to_rgba(const std::vector<uint8_t>& pixels,
                          int width, int height,
                          const std::string& colorspace,
                          int bpc) {
    ImageData img;
    img.width  = width;
    img.height = height;
    img.pixels.resize(static_cast<size_t>(width * height * 4), 0xFF);

    int samples = 1;
    if (colorspace == "DeviceRGB") samples = 3;
    else if (colorspace == "DeviceCMYK") samples = 4;

    auto get_sample = [&](size_t i) -> uint8_t {
        if (bpc == 8) {
            return (i < pixels.size()) ? pixels[i] : 0;
        }
        // bpc 1, 2, 4 처리
        int bits_per_row = width * samples * bpc;
        size_t byte_idx = i * static_cast<size_t>(bpc) / 8;
        int bit_shift   = 8 - bpc - static_cast<int>((i * static_cast<size_t>(bpc)) % 8);
        if (byte_idx >= pixels.size()) return 0;
        uint8_t val = (pixels[byte_idx] >> bit_shift) & ((1 << bpc) - 1);
        return static_cast<uint8_t>(val * 255 / ((1 << bpc) - 1));
        (void)bits_per_row;
    };

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t px_idx  = static_cast<size_t>(y * width + x);
            size_t out_idx = px_idx * 4;
            size_t in_base = px_idx * static_cast<size_t>(samples);

            uint8_t r = 0, g = 0, b = 0, a = 255;

            if (colorspace == "DeviceRGB") {
                r = get_sample(in_base + 0);
                g = get_sample(in_base + 1);
                b = get_sample(in_base + 2);
            } else if (colorspace == "DeviceGray") {
                r = g = b = get_sample(in_base);
            } else if (colorspace == "DeviceCMYK") {
                float c = get_sample(in_base + 0) / 255.0f;
                float m = get_sample(in_base + 1) / 255.0f;
                float yv= get_sample(in_base + 2) / 255.0f;
                float k = get_sample(in_base + 3) / 255.0f;
                r = static_cast<uint8_t>((1.0f - c) * (1.0f - k) * 255.0f);
                g = static_cast<uint8_t>((1.0f - m) * (1.0f - k) * 255.0f);
                b = static_cast<uint8_t>((1.0f - yv)* (1.0f - k) * 255.0f);
            } else {
                // 알 수 없는 색공간: 첫 채널만 gray로
                r = g = b = get_sample(in_base);
            }

            img.pixels[out_idx + 0] = r;
            img.pixels[out_idx + 1] = g;
            img.pixels[out_idx + 2] = b;
            img.pixels[out_idx + 3] = a;
        }
    }
    return img;
}

// ---- 단일 XObject Image 디코딩 ----

static ImageData decode_image_xobject(PdfObject xobj, const PdfDocument& doc) {
    if (!xobj.is_stream() || !xobj.stream) return {};

    PdfStream& stream  = *xobj.stream;
    const PdfDict& dict = stream.dict;

    auto get_int_val = [&](const char* key, int def) -> int {
        auto it = dict.find(key);
        if (it == dict.end()) return def;
        PdfObject v = doc.resolve(it->second);
        return v.is_number() ? static_cast<int>(v.as_number()) : def;
    };

    int width  = get_int_val("Width",  0);
    int height = get_int_val("Height", 0);
    int bpc    = get_int_val("BitsPerComponent", 8);
    if (width <= 0 || height <= 0) return {};

    // 색공간
    std::string colorspace = "DeviceRGB";
    auto cs_it = dict.find("ColorSpace");
    if (cs_it != dict.end()) {
        PdfObject cs_obj = doc.resolve(cs_it->second);
        colorspace = cs_name(cs_obj, doc);
        // Indexed 색공간은 먼저 expand
        if (colorspace == "Indexed") {
            if (stream.raw.empty()) return {};
            if (!stream.decoded_ok) parser::decode_stream(stream);
            if (!stream.decoded_ok) return {};
            std::vector<uint8_t> expanded = expand_indexed(cs_obj, stream.decoded, doc);
            // Indexed는 기반 색공간으로 변환
            PdfObject base_cs = doc.resolve(cs_obj.is_array() ? cs_obj.arr[1] : cs_obj);
            return to_rgba(expanded, width, height, cs_name(base_cs, doc), 8);
        }
        // ICCBased, CalRGB 등은 RGB로 근사
        if (colorspace != "DeviceRGB" && colorspace != "DeviceGray" &&
            colorspace != "DeviceCMYK") {
            colorspace = "DeviceRGB";
        }
    }

    // 필터 확인
    auto filt_it = dict.find("Filter");
    std::string first_filter;
    if (filt_it != dict.end()) {
        const PdfObject& f = filt_it->second;
        if (f.is_name()) first_filter = f.s;
        else if (f.is_array() && !f.arr.empty() && f.arr[0].is_name())
            first_filter = f.arr[0].s;
    }

    // DCTDecode (JPEG): libjpeg로 직접 처리
    if (first_filter == "DCTDecode" || first_filter == "DCT") {
#ifdef CPPPDF_HAVE_JPEG
        if (stream.raw.empty()) return {};
        std::vector<uint8_t> decoded;
        int jw = 0, jh = 0, jc = 0;
        if (!dct_decode(stream.raw.data(), stream.raw.size(), decoded, jw, jh, jc))
            return {};
        std::string jcs = (jc == 1) ? "DeviceGray"
                        : (jc == 4) ? "DeviceCMYK"
                        : "DeviceRGB";
        return to_rgba(decoded, jw, jh, jcs, 8);
#else
        return {}; // JPEG 지원 없음
#endif
    }

    // 그 외 필터: decode_stream 사용
    if (stream.raw.empty()) return {};
    if (!stream.decoded_ok) parser::decode_stream(stream);
    if (!stream.decoded_ok) return {};

    return to_rgba(stream.decoded, width, height, colorspace, bpc);
}

// ---- 공개 함수 ----

std::vector<ImageData> extract_images(const PdfDocument& doc, int page_index) {
    PdfDict resources = doc.get_resources(page_index);

    auto xobj_it = resources.find("XObject");
    if (xobj_it == resources.end()) return {};

    PdfObject xobj_map = doc.resolve(xobj_it->second);
    if (!xobj_map.is_dict()) return {};

    std::vector<ImageData> images;
    for (const auto& [name, ref] : xobj_map.as_dict()) {
        PdfObject xobj = doc.resolve(ref);
        if (!xobj.is_stream() || !xobj.stream) continue;

        const PdfDict& sd = xobj.stream->dict;

        // /Subtype /Image 확인
        auto st_it = sd.find("Subtype");
        if (st_it == sd.end()) continue;
        PdfObject subtype = doc.resolve(st_it->second);
        if (!subtype.is_name() || subtype.s != "Image") continue;

        ImageData img = decode_image_xobject(std::move(xobj), doc);
        if (img.width > 0 && img.height > 0)
            images.push_back(std::move(img));
    }

    return images;
}

} // namespace cpppdf::extractor
