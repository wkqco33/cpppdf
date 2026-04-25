#pragma once
#include "cpppdf/cpppdf.hpp"
#include "document/document.hpp"
#include "extractor/image.hpp"
#include "wcppcli/wlog.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace cpppdf::cli {

struct PageRange {
    int from = 0;
    int to   = 0;
};

struct MarkdownImageOutput {
    std::string input_path;
    std::string markdown_output;
    std::string image_dir;
};

struct ExportImagesOptions {
    bool verbose = false;
    std::vector<std::vector<std::string>>* markdown_links = nullptr;
    const std::filesystem::path* markdown_base = nullptr;
};

inline auto save_ppm(const std::string& path, const ImageData& img) -> bool {
    std::ofstream file_stream(path, std::ios::binary);
    if (!file_stream) {
        return false;
    }

    file_stream << "P6\n" << img.width << " " << img.height << "\n255\n";
    for (int row = 0; row < img.height; ++row) {
        for (int col = 0; col < img.width; ++col) {
            const auto pixel_index =
                static_cast<size_t>(row) * static_cast<size_t>(img.width) * 4U +
                static_cast<size_t>(col) * 4U;
            file_stream.put(static_cast<char>(img.pixels[pixel_index + 0U]));
            file_stream.put(static_cast<char>(img.pixels[pixel_index + 1U]));
            file_stream.put(static_cast<char>(img.pixels[pixel_index + 2U]));
        }
    }
    return file_stream.good();
}

inline auto load_document(const std::string& path, PdfDocument& doc) -> bool {
    if (doc.load(path)) {
        return true;
    }
    wcppcli::WLog::error("파일을 열 수 없습니다: " + path);
    return false;
}

inline auto validate_page_index(const PdfDocument& doc, int page) -> bool {
    if (page < 0 || page < doc.page_count()) {
        return true;
    }
    wcppcli::WLog::error("페이지 범위를 벗어났습니다: " +
                         std::to_string(page) + " / 총 " +
                         std::to_string(doc.page_count()) + "페이지");
    return false;
}

inline auto page_range(const PdfDocument& doc, int page) -> PageRange {
    if (page >= 0) {
        return {page, page + 1};
    }
    return {0, doc.page_count()};
}

inline auto default_markdown_image_dir(const MarkdownImageOutput& output) -> std::filesystem::path {
    namespace fs = std::filesystem;

    if (!output.image_dir.empty()) {
        return {output.image_dir};
    }

    if (!output.markdown_output.empty()) {
        const fs::path output_path(output.markdown_output);
        const fs::path parent = output_path.has_parent_path()
                              ? output_path.parent_path()
                              : fs::current_path();
        return parent / (output_path.stem().string() + "_images");
    }

    return {fs::path(output.input_path).stem().string() + "_images"};
}

inline auto ensure_output_directory(const std::filesystem::path& out_dir) -> bool {
    std::error_code error;
    std::filesystem::create_directories(out_dir, error);
    if (!error) {
        return true;
    }
    wcppcli::WLog::error("이미지 디렉토리 생성 실패: " + out_dir.string());
    return false;
}

inline auto markdown_image_path(const std::filesystem::path& image_path,
                                const std::filesystem::path* markdown_base) -> std::string {
    std::error_code error;
    const std::filesystem::path base_path =
        (markdown_base != nullptr) ? *markdown_base : std::filesystem::current_path();
    const std::filesystem::path relative_path =
        std::filesystem::relative(image_path, base_path, error);
    return error ? image_path.generic_string() : relative_path.generic_string();
}

inline auto export_images(const PdfDocument& doc,
                          int page,
                          const std::filesystem::path& out_dir,
                          const ExportImagesOptions& options = {}) -> int {
    const PageRange range = page_range(doc, page);
    bool created_dir = false;
    int saved = 0;

    for (int page_index = range.from; page_index < range.to; ++page_index) {
        auto images = extractor::extract_images(doc, page_index);
        if (images.empty()) {
            continue;
        }

        if (!created_dir) {
            if (!ensure_output_directory(out_dir)) {
                return 1;
            }
            created_dir = true;
        }

        for (size_t index = 0; index < images.size(); ++index) {
            const std::filesystem::path image_path =
                out_dir / ("page" + std::to_string(page_index) +
                           "_img" + std::to_string(index) + ".ppm");

            if (!save_ppm(image_path.string(), images[index])) {
                wcppcli::WLog::error("이미지 저장 실패: " + image_path.string());
                return 1;
            }

            if (options.verbose) {
                wcppcli::WLog::info("저장: " + image_path.string() + " (" +
                                    std::to_string(images[index].width) + "x" +
                                    std::to_string(images[index].height) + ")");
            }

            if (options.markdown_links != nullptr) {
                (*options.markdown_links)[static_cast<size_t>(page_index)].push_back(
                    markdown_image_path(image_path, options.markdown_base));
            }

            ++saved;
        }
    }

    if (options.verbose && saved == 0) {
        wcppcli::WLog::warn("이미지 없음");
    }
    return 0;
}

} // namespace cpppdf::cli
