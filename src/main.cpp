#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace csvscope {

// A logical CSV record can span physical lines. Strict quoting avoids silently
// accepting truncated or ambiguous data.
class Reader {
    std::istream& input;
    char delimiter;
    std::size_t line = 1;

public:
    Reader(std::istream& stream, char separator) : input(stream), delimiter(separator) {}

    bool next(std::vector<std::string>& record) {
        enum class State { start, plain, quoted, closed };
        State state = State::start;
        record.clear();
        std::string field;
        bool consumed = false;
        for (;;) {
            const int value = input.get();
            if (value == std::char_traits<char>::eof()) {
                if (input.bad()) throw std::runtime_error("input read failure");
                if (state == State::quoted) fail("unterminated quoted field");
                if (!consumed) return false;
                record.push_back(field);
                return true;
            }
            consumed = true;
            const char ch = static_cast<char>(value);
            if (state == State::quoted) {
                if (ch == '"') state = State::closed;
                else {
                    field += ch;
                    if (ch == '\n') ++line;
                }
                continue;
            }
            if (state == State::closed && ch == '"') {
                field += '"';
                state = State::quoted;
                continue;
            }
            if (ch == delimiter) {
                record.push_back(field);
                field.clear();
                state = State::start;
            } else if (ch == '\n' || ch == '\r') {
                if (ch == '\r' && input.peek() == '\n') input.get();
                ++line;
                record.push_back(field);
                return true;
            } else if (ch == '"' && state == State::start) {
                state = State::quoted;
            } else if (ch == '"' || state == State::closed) {
                fail("unexpected character near quote");
            } else {
                field += ch;
                state = State::plain;
            }
        }
    }

private:
    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error("line " + std::to_string(line) + ": " + message);
    }
};

bool number(const std::string& text, long double& value) {
    if (text.empty()) return false;
    std::istringstream stream(text);
    stream.imbue(std::locale::classic());
    stream >> std::noskipws >> value;
    return stream && stream.peek() == std::char_traits<char>::eof() && std::isfinite(value);
}

std::string json_string(const std::string& text) {
    std::ostringstream result;
    result << '"';
    for (unsigned char ch : text) {
        switch (ch) {
            case '"': result << "\\\""; break;
            case '\\': result << "\\\\"; break;
            case '\n': result << "\\n"; break;
            case '\r': result << "\\r"; break;
            case '\t': result << "\\t"; break;
            default:
                if (ch < 0x20) result << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch) << std::dec;
                else result << static_cast<char>(ch);
        }
    }
    return result.str() + '"';
}

struct Column {
    std::string name;
    std::size_t missing = 0, numeric = 0, text = 0;
    long double minimum = 0, maximum = 0, mean = 0;

    void add(const std::string& cell) {
        if (cell.empty()) { ++missing; return; }
        long double value = 0;
        if (!number(cell, value)) { ++text; return; }
        ++numeric;
        if (numeric == 1) minimum = maximum = mean = value;
        else {
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
            // Weighted update avoids overflow in (value - mean) for opposite signs.
            const long double weight = 1.0L / static_cast<long double>(numeric);
            mean = mean * (1.0L - weight) + value * weight;
        }
    }

    std::string type() const {
        if (!numeric && !text) return "empty";
        if (numeric && text) return "mixed";
        return numeric ? "number" : "text";
    }
};

int run(int argc, char** argv) {
    std::string filename;
    char delimiter = ',';
    bool json = false, no_header = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "csvscope: streaming CSV quality report\n\nUsage: csvscope FILE|- [--json] [--no-header] [--delimiter CHAR]\n\nExit codes: 0 clean, 1 inconsistent row widths, 2 input/usage error.\n";
            return 0;
        }
        if (arg == "--json") json = true;
        else if (arg == "--no-header") no_header = true;
        else if (arg == "--delimiter") {
            if (++i >= argc || std::string(argv[i]).size() != 1) throw std::runtime_error("--delimiter needs one character");
            delimiter = argv[i][0];
            if (delimiter == '"' || delimiter == '\n' || delimiter == '\r' || delimiter == '\0') throw std::runtime_error("invalid delimiter");
        } else if (arg != "-" && !arg.empty() && arg[0] == '-') throw std::runtime_error("unknown option: " + arg);
        else if (filename.empty()) filename = arg;
        else throw std::runtime_error("expected exactly one input file");
    }
    if (filename.empty()) throw std::runtime_error("missing input; use --help for usage");
    std::ifstream file;
    std::istream* input = &std::cin;
    if (filename != "-") {
        file.open(filename, std::ios::binary);
        if (!file) throw std::runtime_error("cannot open: " + filename);
        input = &file;
    }
    // Strip UTF-8 BOM without requiring a seekable input (stdin is supported).
    if (input->peek() == 0xEF) {
        input->get();
        if (input->get() != 0xBB || input->get() != 0xBF) throw std::runtime_error("invalid UTF-8 BOM prefix");
    }
    Reader reader(*input, delimiter);
    std::vector<std::string> record;
    if (!reader.next(record)) throw std::runtime_error("input is empty");
    std::vector<Column> columns;
    for (std::size_t i = 0; i < record.size(); ++i) {
        Column column;
        column.name = no_header ? "column_" + std::to_string(i + 1) : record[i];
        columns.push_back(column);
    }
    std::size_t rows = 0, bad_rows = 0;
    auto consume = [&]() {
        ++rows;
        if (record.size() != columns.size()) { ++bad_rows; return; }
        for (std::size_t i = 0; i < columns.size(); ++i) columns[i].add(record[i]);
    };
    if (no_header) consume();
    while (reader.next(record)) consume();
    std::cout.imbue(std::locale::classic());
    std::cout << std::setprecision(12);
    if (json) {
        std::cout << "{\"rows\":" << rows << ",\"valid_rows\":" << rows - bad_rows << ",\"bad_rows\":" << bad_rows << ",\"columns\":[";
        for (std::size_t i = 0; i < columns.size(); ++i) {
            const auto& col = columns[i];
            if (i) std::cout << ',';
            std::cout << "{\"name\":" << json_string(col.name) << ",\"type\":" << json_string(col.type()) << ",\"missing\":" << col.missing << ",\"numeric\":" << col.numeric << ",\"text\":" << col.text << ",\"min\":";
            if (col.numeric) std::cout << col.minimum; else std::cout << "null";
            std::cout << ",\"max\":";
            if (col.numeric) std::cout << col.maximum; else std::cout << "null";
            std::cout << ",\"mean\":";
            if (col.numeric) std::cout << col.mean; else std::cout << "null";
            std::cout << '}';
        }
        std::cout << "]}\n";
    } else {
        std::cout << "CSVSCOPE\n" << rows << " data rows | " << columns.size() << " columns | " << bad_rows << " inconsistent rows\n\n";
        for (const auto& col : columns) {
            std::cout << json_string(col.name) << "  [" << col.type() << "]\n  missing: " << col.missing << " | numbers: " << col.numeric << " | text: " << col.text;
            if (col.numeric) std::cout << "\n  min: " << col.minimum << " | max: " << col.maximum << " | mean: " << col.mean;
            std::cout << "\n\n";
        }
    }
    if (!std::cout) throw std::runtime_error("output write failure");
    return bad_rows ? 1 : 0;
}
} // namespace csvscope

int main(int argc, char** argv) {
    try { return csvscope::run(argc, argv); }
    catch (const std::exception& error) {
        std::cerr << "csvscope: " << error.what() << '\n';
        return 2;
    }
}
