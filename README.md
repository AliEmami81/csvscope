# csvscope

**A small C++17 tool for finding messy CSV data before it reaches your application.**

[![Tests](https://github.com/AliEmami81/csvscope/actions/workflows/tests.yml/badge.svg)](https://github.com/AliEmami81/csvscope/actions/workflows/tests.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C)
![Runtime dependencies: zero](https://img.shields.io/badge/runtime_dependencies-zero-ff9560)
[![MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

Count missing values, inspect numeric ranges, and detect inconsistent row widths without loading the entire file into memory. No third-party C++ libraries required.

```text
CSVSCOPE
4 data rows | 4 columns | 0 inconsistent rows

"quantity"  [number]
  missing: 1 | numbers: 3 | text: 0
  min: 2 | max: 10 | mean: 5.66666666667
```

Excerpt from `csvscope examples/sales.csv`.

## Build and try

Use a C++17 compiler:

```sh
git clone https://github.com/AliEmami81/csvscope.git
cd csvscope
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic src/main.cpp -o csvscope
./csvscope examples/sales.csv
./csvscope examples/sales.csv --json
```

On Windows, use `-o csvscope.exe` and run `./csvscope.exe`. With CMake (including Visual Studio):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The binary is in `build/`, or `build/Release/` with a multi-configuration generator.

## Usage

```sh
csvscope data.csv --json
csvscope data.tsv --delimiter "	"
csvscope values.csv --no-header
csvscope - --json < data.csv
```

The delimiter argument must be one byte. The TSV example contains a **literal tab**; the two-character string `\t` is not interpreted. Use `--delimiter ";"` for semicolon-separated data. `-` reads from standard input. The first record supplies column names unless `--no-header` is set.

## Report contract

- Handles quoted separators, doubled quotes, quoted multiline values, CRLF/LF record endings, and a UTF-8 BOM.
- Counts all data records and reports inconsistent widths. Those records are excluded from column statistics.
- Tracks empty cells, numeric cells, text cells, and numeric min/max/mean.
- Mixed columns retain statistics for their numeric cells.
- Emits JSON for scripts and CI. Numeric summaries are `null` if there are no numeric cells.
- Processes records as a stream: memory scales with column count plus the largest record, not total file size.

| Exit code | Meaning |
| --- | --- |
| `0` | Parsed successfully; all data records have the expected width |
| `1` | Parsed successfully; at least one data record has the wrong width |
| `2` | Usage error, unreadable/empty input, or malformed CSV quoting |

### Deliberate choices

Only an empty string counts as missing: `NA`, `null`, and spaces are text. Numeric recognition uses the classic C locale, accepts scientific notation, and requires the whole cell to be a finite number without surrounding whitespace. Dates and booleans remain text. Duplicate and empty column names are preserved in an ordered JSON array. Blank physical lines are one empty field; this can produce a width error in multi-column data. Strict quoting rejects text following a closing quote. CRLF inside a quoted value is preserved. Input should be UTF-8; encoding validation and conversion are not performed. This is a data inspection tool, not a full CSV dialect detector or a spreadsheet engine. Numeric aggregates use floating point and are not exact financial arithmetic.

## Tests

Requires Python 3.8+ only for the black-box test runner:

```sh
python tests/test_cli.py ./csvscope
```

Tests cover multiline quoting, malformed records, BOM, Unicode headers, JSON escaping, missing values, numeric policy, row width handling, and CLI errors.

See [CONTRIBUTING.md](CONTRIBUTING.md) for contributing. MIT licensed. Built by [Ali Emami](https://github.com/AliEmami81).
