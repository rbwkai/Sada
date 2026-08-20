// dataset.cpp — everything about finding and reading the external word list.
//
// The dataset (data/sada_arabic_2000.csv) is deliberately kept outside the
// program: it's reference data, not code, and the app only ever reads it
// once (to seed the SQLite database on first run). Columns are:
//   rank,arabic,transliteration,english,part_of_speech
#include "sada.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace sada {

std::string dataset_locate_csv() {
    // 1. Explicit override.
    if (const char* env = std::getenv("SADA_DATA_FILE")) {
        if (*env && fs::exists(env)) return env;
    }
    // 2. Installed location baked in at build time (see CMakeLists.txt).
#ifdef SADA_SHARE_DIR
    {
        fs::path installed = fs::path(SADA_SHARE_DIR) / "sada_arabic_2000.csv";
        if (fs::exists(installed)) return installed.string();
    }
#endif
    // 3. Running straight from the build/source tree during development.
    fs::path dev = fs::path("data") / "sada_arabic_2000.csv";
    if (fs::exists(dev)) return dev.string();

    return "";
}

// Minimal RFC4180-ish CSV line splitter: handles quoted fields, escaped
// quotes ("") and commas inside quotes. That's the only complexity our
// data can contain (no embedded newlines within a field).
static std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                in_quotes = true;
            } else if (c == ',') {
                fields.push_back(field);
                field.clear();
            } else {
                field += c;
            }
        }
    }
    fields.push_back(field);
    return fields;
}

std::vector<Word> dataset_load_csv(const std::string& path) {
    std::vector<Word> words;
    std::ifstream in(path);
    if (!in) return words;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        auto fields = split_csv_line(line);
        if (fields.size() < 4) continue;

        Word w;
        try {
            w.freq_rank = std::stoi(fields[0]);
        } catch (...) {
            continue; // skip malformed/header rows
        }
        w.arabic = fields[1];
        w.transliteration = fields[2];
        w.meaning = fields[3];
        if (fields.size() >= 5) {
            w.part_of_speech = fields[4];
        }
        w.id = w.freq_rank;
        words.push_back(std::move(w));
    }
    return words;
}

} // namespace sada
