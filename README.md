# cpppdf

가볍게 PDF를 읽고, 텍스트/이미지 추출과 터미널 렌더링, Markdown 변환까지 처리하는 C++17 기반 PDF 유틸리티입니다.

CLI는 `wcppcli` 서브모듈로 구성되어 있고, 핵심 기능은 `libcpppdf` 라이브러리에 들어 있습니다.

## 주요 기능

- PDF 기본 정보 조회
- 페이지별 텍스트 추출
- 터미널용 텍스트 레이아웃 렌더링
- 내장 이미지 추출
- `pdf2md` 변환
- xref stream / object stream 기반 PDF 지원
- ToUnicode CMap 기반 한글 텍스트 디코딩 개선

## 프로젝트 구성

| 경로 | 설명 |
| --- | --- |
| `include/cpppdf/` | 공개 헤더 |
| `src/document/` | PDF 문서 로드, 오브젝트 해석 |
| `src/parser/` | xref / object 파싱 |
| `src/extractor/` | 텍스트, 이미지 추출 |
| `src/renderer/` | 터미널 렌더링 |
| `src/converter/` | `pdf2md` 변환 파이프라인 |
| `cli/` | `cpppdf` CLI 엔트리 |
| `tests/` | parser / extractor / pdf2md 테스트 |
| `wcppcli/` | CLI 서브모듈 |

## 요구사항

### 공통

- C++17 지원 컴파일러
- CMake
- Ninja
- `vcpkg`
- Git submodule 초기화

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config autoconf autoconf-archive automake libtool
git submodule update --init --recursive
```

## 빌드

### vcpkg manifest 사용

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset debug
cmake --build --preset debug
```

### 시스템 라이브러리 사용

```bash
cmake --preset debug-native
cmake --build --preset debug-native
```

## 테스트

```bash
ctest --preset debug --output-on-failure
```

## 빠른 시작

```bash
./build/debug/cpppdf info tests/fixtures/sample.pdf
./build/debug/cpppdf text tests/fixtures/sample.pdf
./build/debug/cpppdf render tests/fixtures/sample.pdf
./build/debug/cpppdf images tests/fixtures/image_test.pdf -o ./out_images
./build/debug/cpppdf pdf2md tests/fixtures/sample.pdf -o out.md
```

## CLI 사용 가이드

### 전체 도움말

```bash
./build/debug/cpppdf --help
```

### 1. PDF 정보 보기

```bash
./build/debug/cpppdf info file.pdf
```

출력 항목:

- PDF 버전
- 페이지 수
- 각 페이지 크기

### 2. 텍스트 추출

```bash
./build/debug/cpppdf text file.pdf
./build/debug/cpppdf text file.pdf -p 0
```

- `-p, --page`: 특정 페이지만 처리, 0부터 시작
- 읽기 좋은 순서로 텍스트를 평문으로 출력

### 3. 터미널 렌더링

```bash
./build/debug/cpppdf render file.pdf
./build/debug/cpppdf render file.pdf -p 3
```

- `-p, --page`: 특정 페이지만 처리
- 페이지 비율과 추출된 텍스트 분포를 기준으로 박스를 잡아 터미널에 배치
- 실제 PDF 뷰어처럼 완전한 레이아웃을 재현하는 기능은 아님

### 4. 이미지 추출

```bash
./build/debug/cpppdf images file.pdf -o ./images
./build/debug/cpppdf images file.pdf -p 2 -o ./page2_images
```

- `-p, --page`: 특정 페이지만 처리
- `-o, --output`: 이미지 저장 디렉터리
- 현재 출력 포맷은 `PPM`

### 5. PDF -> Markdown 변환

```bash
./build/debug/cpppdf pdf2md file.pdf
./build/debug/cpppdf pdf2md file.pdf -o out.md
./build/debug/cpppdf pdf2md file.pdf -p 1 -o page1.md
```

옵션:

- `-p, --page`: 특정 페이지만 변환
- `-o, --output`: Markdown 출력 파일
- `--image-dir`: 추출 이미지 저장 디렉터리 지정
- `--no-images`: 이미지 추출과 Markdown 이미지 링크 삽입 비활성화

이미지 포함 예시:

```bash
./build/debug/cpppdf pdf2md file.pdf -o out.md --image-dir ./out_images
```

기본 동작:

- 텍스트를 Markdown 문단/리스트/간단한 표로 변환
- 이미지가 있으면 페이지별로 추출
- Markdown 본문 뒤에 이미지 링크를 추가

## pdf2md 동작 방식

`pdf2md`는 기존 텍스트 추출 결과를 바로 덤프하지 않고, 아래 순서로 후처리합니다.

1. 텍스트 블록 정규화
2. 줄 조립
3. 단락 / 목록 분리
4. 간단한 표 감지
5. Markdown 렌더링

정확한 문서 편집 수준 복원보다는, 읽을 수 있는 Markdown 구조 복원을 목표로 합니다.

## 현재 제한사항

- 복잡한 다단 레이아웃은 완벽하게 복원되지 않을 수 있음
- `render`는 좌표 기반 근사 출력이라 PDF 뷰어와 동일하지 않음
- 이미지 위치는 문서 내 정확한 삽입 지점 대신 페이지 단위로 처리
- 이미지 저장 포맷은 현재 `PPM` 고정
- 일부 폰트 / 인코딩 조합에서는 텍스트 품질이 완전하지 않을 수 있음

## 구현 메모

- xref stream 과 compressed object stream 을 읽을 수 있도록 보강됨
- 한글 CID 폰트 대응을 위해 멀티바이트 ToUnicode CMap 디코딩을 처리함
- CLI는 `wcppcli`를 사용해 서브커맨드 기반으로 구성됨

## 개발용 명령

```bash
cmake --build --preset debug --parallel
ctest --preset debug --output-on-failure
```

## 라이선스

저장소 정책에 따릅니다.
