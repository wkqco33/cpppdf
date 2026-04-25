#include "common.hpp"
#include "converter/pdf2md.hpp"
#include "document/document.hpp"
#include "extractor/text.hpp"
#include "extractor/image.hpp"
#include "renderer/terminal.hpp"
#include "wcppcli/wlog.hpp"
#include "wcppcli/wcli.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// ---- 커맨드 구현 ----

namespace {

constexpr float kMillimetersPerInch = 25.4F;
constexpr float kPointsPerInch = 72.0F;
constexpr float kTextLineBreakThreshold = 2.0F;
constexpr float kParagraphGapThreshold = 4.0F;
constexpr float kInitialPreviousY = -1.0e9F;

} // namespace

static int cmd_info(const cpppdf::PdfDocument& doc, const std::string& path) {
    std::cout << "파일: " << path << '\n';
    std::cout << "페이지 수: " << doc.page_count() << '\n';
    std::cout.setf(std::ios::fixed);
    std::cout.precision(1);
    for (int page_index = 0; page_index < doc.page_count(); ++page_index) {
        const cpppdf::PageInfo info = doc.page_info(page_index);
        std::cout << "  페이지 " << page_index << ": "
                  << info.width << " × " << info.height << " pt  ("
                  << (info.width * kMillimetersPerInch / kPointsPerInch) << " × "
                  << (info.height * kMillimetersPerInch / kPointsPerInch) << " mm)\n";
    }
    return 0;
}

static int cmd_text(const cpppdf::PdfDocument& doc, int page) {
    const cpppdf::cli::PageRange range = cpppdf::cli::page_range(doc, page);

    for (int i = range.from; i < range.to; ++i) {
        if (doc.page_count() > 1) {
            std::cout << "=== 페이지 " << i << " ===\n";
        }

        auto blocks = cpppdf::extractor::extract_text(doc, i);

        // Y 내림차순 정렬 후 출력 (PDF Y는 아래서 위)
        std::sort(blocks.begin(), blocks.end(),
                  [](const cpppdf::TextBlock& lhs, const cpppdf::TextBlock& rhs) {
                      if (std::abs(lhs.y - rhs.y) > kTextLineBreakThreshold) {
                          return lhs.y > rhs.y;
                      }
                      return lhs.x < rhs.x;
                  });

        float previous_y = kInitialPreviousY;
        for (const auto& block : blocks) {
            if (previous_y >= 0.0F &&
                std::abs(block.y - previous_y) > kParagraphGapThreshold) {
                std::cout << '\n';
            }
            std::cout << block.text << ' ';
            previous_y = block.y;
        }
        std::cout << '\n';
    }
    return 0;
}

static int cmd_render(const cpppdf::PdfDocument& doc, int page) {
    const cpppdf::cli::PageRange range = cpppdf::cli::page_range(doc, page);

    for (int i = range.from; i < range.to; ++i) {
        if (range.to - range.from > 1) {
            std::cout << "=== 페이지 " << i << " ===\n";
        }
        cpppdf::renderer::render_text(doc, i);
    }
    return 0;
}

static int cmd_images(const cpppdf::PdfDocument& doc, int page,
                      const std::string& out_dir) {
    return cpppdf::cli::export_images(
        doc, page, out_dir, cpppdf::cli::ExportImagesOptions{true});
}

static int cmd_pdf2md(const cpppdf::PdfDocument& doc, int page,
                      const std::string& output,
                      bool no_images,
                      const std::string& image_dir,
                      const std::string& input_path) {
    std::vector<std::vector<std::string>> page_image_links(
        static_cast<size_t>(doc.page_count()));

    if (!no_images) {
        namespace fs = std::filesystem;
        fs::path markdown_base = output.empty()
                               ? fs::current_path()
                               : (fs::path(output).has_parent_path()
                                  ? fs::path(output).parent_path()
                                  : fs::current_path());
        const int export_code = cpppdf::cli::export_images(
            doc,
            page,
            cpppdf::cli::default_markdown_image_dir(
                cpppdf::cli::MarkdownImageOutput{input_path, output, image_dir}),
            cpppdf::cli::ExportImagesOptions{false, &page_image_links, &markdown_base});
        if (export_code != 0) {
            return export_code;
        }
    }

    const std::string markdown =
        cpppdf::converter::convert_document_to_markdown(
            doc, cpppdf::converter::Pdf2MdOptions{page, &page_image_links});

    if (output.empty()) {
        std::cout << markdown;
        return 0;
    }

    std::ofstream out(output, std::ios::binary);
    if (!out) {
        wcppcli::WLog::error("파일에 쓸 수 없습니다: " + output);
        return 1;
    }
    out << markdown;
    return out.good() ? 0 : 1;
}

// ---- main ----

int main(int argc, char** argv) {
    using wcppcli::Command;
    using wcppcli::Flag;

    auto exit_code = std::make_shared<int>(0);

    auto execute_with_document = [exit_code](
        const Command& active,
        int page,
        bool validate_page,
        const auto& runner) {
        if (active.args.empty()) {
            active.print_help();
            *exit_code = 1;
            return;
        }

        cpppdf::PdfDocument doc;
        if (!cpppdf::cli::load_document(active.args[0], doc)) {
            *exit_code = 1;
            return;
        }
        if (validate_page && !cpppdf::cli::validate_page_index(doc, page)) {
            *exit_code = 1;
            return;
        }
        *exit_code = runner(doc, active);
    };

    Command root;
    root.name = "cpppdf";
    root.description = "cpppdf CLI";
    root.usage = "cpppdf <command> <file.pdf> [options]";

    auto make_pdf_command = [&](const std::string& name,
                                const std::string& description,
                                auto handler) {
        auto cmd = std::make_unique<Command>();
        cmd->name = name;
        cmd->description = description;
        cmd->usage = "cpppdf " + name + " <file.pdf> [options]";
        handler(*cmd);
        root.add_command(std::move(cmd));
    };

    make_pdf_command("info", "PDF 기본 정보 출력", [execute_with_document](Command& cmd) {
        cmd.handler = [execute_with_document](const Command& active) {
            execute_with_document(active, -1, false,
                                  [](const cpppdf::PdfDocument& doc, const Command& current) {
                                      return cmd_info(doc, current.args[0]);
                                  });
        };
    });

    make_pdf_command("text", "텍스트 추출", [execute_with_document](Command& cmd) {
        auto page = std::make_shared<int>(-1);
        cmd.add_flag(Flag{"page", 'p', "특정 페이지만 처리 (0부터 시작)",
                          page.get()});
        cmd.handler = [page, execute_with_document](const Command& active) {
            execute_with_document(active, *page, true,
                                  [page](const cpppdf::PdfDocument& doc, const Command&) {
                                      return cmd_text(doc, *page);
                                  });
        };
    });

    make_pdf_command("render", "터미널에 텍스트 레이아웃 렌더링", [execute_with_document](Command& cmd) {
        auto page = std::make_shared<int>(-1);
        cmd.add_flag(Flag{"page", 'p', "특정 페이지만 처리 (0부터 시작)",
                          page.get()});
        cmd.handler = [page, execute_with_document](const Command& active) {
            execute_with_document(active, *page, true,
                                  [page](const cpppdf::PdfDocument& doc, const Command&) {
                                      return cmd_render(doc, *page);
                                  });
        };
    });

    make_pdf_command("images", "내장 이미지 파일로 저장", [execute_with_document](Command& cmd) {
        auto page = std::make_shared<int>(-1);
        auto out_dir = std::make_shared<std::string>(".");
        cmd.add_flag(Flag{"page", 'p', "특정 페이지만 처리 (0부터 시작)",
                          page.get()});
        cmd.add_flag(Flag{"output", 'o', "이미지 출력 디렉토리",
                          out_dir.get()});
        cmd.handler = [page, out_dir, execute_with_document](const Command& active) {
            execute_with_document(active, *page, true,
                                  [page, out_dir](const cpppdf::PdfDocument& doc, const Command&) {
                                      return cmd_images(doc, *page, *out_dir);
                                  });
        };
    });

    make_pdf_command("pdf2md", "PDF를 Markdown으로 변환", [execute_with_document](Command& cmd) {
        auto page = std::make_shared<int>(-1);
        auto output = std::make_shared<std::string>();
        auto image_dir = std::make_shared<std::string>();
        auto no_images = std::make_shared<bool>(false);
        cmd.add_flag(Flag{"page", 'p', "특정 페이지만 변환 (0부터 시작)",
                          page.get()});
        cmd.add_flag(Flag{"output", 'o', "출력 markdown 파일 (기본: stdout)",
                          output.get()});
        cmd.add_flag(Flag{"image-dir", 0, "이미지 출력 디렉토리 (기본: <출력명>_images 또는 <입력명>_images)",
                          image_dir.get()});
        cmd.add_flag(Flag{"no-images", 0, "이미지 추출과 markdown 이미지 링크 삽입 비활성화",
                          no_images.get()});
        cmd.handler = [page, output, image_dir, no_images, execute_with_document](const Command& active) {
            execute_with_document(active, *page, true,
                                  [page, output, image_dir, no_images](
                                      const cpppdf::PdfDocument& doc, const Command& current) {
                                      return cmd_pdf2md(doc, *page, *output, *no_images,
                                                        *image_dir, current.args[0]);
                                  });
        };
    });

    const int exec_code = root.execute(argc, argv);
    return exec_code != 0 ? exec_code : *exit_code;
}
