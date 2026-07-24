# cpppdf

가볍게 PDF를 읽고, 텍스트/이미지 추출과 터미널 렌더링, Markdown 변환까지 처리하는 C++17 기반 PDF 유틸리티 라이브러리입니다.

독립적인 C++ 라이브러리로 제공되며, 예제 애플리케이션(`examples/`)을 통해 각 기능의 활용법을 바로 확인할 수 있습니다.

## 주요 기능

- PDF 기본 정보 및 페이지 구조 조회
- 페이지별 텍스트 추출 (`extract_text`)
- 터미널용 텍스트 레이아웃 렌더링 (`render_text`)
- 내장 이미지 데이터 추출 (`extract_images`)
- `pdf2md` 파이프라인 (PDF → Markdown 변환)
- xref stream / object stream 기반 PDF 로드 지원
- ToUnicode CMap 기반 한글 CID 폰트 디코딩 지원

## 프로젝트 구성

| 경로 | 설명 |
| --- | --- |
| `include/cpppdf/` | 공개 C++ API 헤더 (`cpppdf.hpp`, `document.hpp`, `pdf2md.hpp` 등) |
| `src/document/` | PDF 문서 로드 및 Pimpl 구현 |
| `src/parser/` | xref / object 파싱 |
| `src/extractor/` | 텍스트, 이미지 추출기 |
| `src/renderer/` | 터미널 렌더러 |
| `src/converter/` | `pdf2md` 변환 파이프라인 |
| `examples/` | 라이브러리 사용 예제 프로그램 (5종) |
| `tests/` | 단위 테스트 |

## 요구사항

- C++17 지원 컴파일러 (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.20 이상
- ZLIB (필수)
- libjpeg (선택, JPEG 이미지 변환 지원)

## 빌드 및 테스트

```bash
# CMake 구성 및 라이브러리/예제 빌드
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# 단위 테스트 실행
ctest --test-dir build --output-on-failure
```

## 예제 실행 (Examples)

빌드가 완료되면 `build/examples/` 디렉터리에 각 예제 바이너리가 생성됩니다.

```bash
# 1. PDF 기본 정보 출력
./build/examples/example_01_info tests/fixtures/sample.pdf

# 2. 텍스트 추출
./build/examples/example_02_extract_text tests/fixtures/sample.pdf

# 3. 내장 이미지 정보 추출
./build/examples/example_03_extract_images tests/fixtures/image_test.pdf

# 4. 터미널 렌더링
./build/examples/example_04_render_terminal tests/fixtures/sample.pdf

# 5. PDF -> Markdown 변환
./build/examples/example_05_pdf2md tests/fixtures/sample.pdf output.md
```

## C++ 코드 사용법

### 1. Document 로드 및 텍스트 추출

```cpp
#include <cpppdf/cpppdf.hpp>
#include <cpppdf/document.hpp>
#include <iostream>

int main() {
    cpppdf::PdfDocument doc;
    if (!doc.load("sample.pdf")) {
        std::cerr << "Failed to load PDF\n";
        return 1;
    }

    std::cout << "Page count: " << doc.page_count() << "\n";

    // 0번째 페이지 텍스트 추출
    auto blocks = cpppdf::extract_text(&doc, 0);
    for (const auto& block : blocks) {
        std::cout << block.text << " ";
    }
    return 0;
}
```

### 2. PDF를 Markdown으로 변환

```cpp
#include <cpppdf/document.hpp>
#include <cpppdf/pdf2md.hpp>
#include <iostream>

int main() {
    cpppdf::PdfDocument doc;
    if (doc.load("sample.pdf")) {
        std::string markdown = cpppdf::converter::convert_document_to_markdown(doc);
        std::cout << markdown << "\n";
    }
    return 0;
}
```

## 라이선스

저장소 정책에 따릅니다.
