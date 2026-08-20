// main.cpp — `sada` takes no arguments and asks nothing. Every run does
// exactly one thing: open (or create) the database, seed it from the
// bundled dataset if this is the very first run, ask the engine which word
// is due, and print it.
#include "sada.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace sada {

std::string paths_home_data_dir() {
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
        return std::string(xdg) + "/sada";
    const char* home = std::getenv("HOME");
    if (!home || !*home) throw std::runtime_error("HOME is not set");
    return std::string(home) + "/.local/share/sada";
}

std::string paths_db_file() { return paths_home_data_dir() + "/sada.db"; }

} // namespace sada

int main() {
    using namespace sada;
    try {
        std::string db_path = paths_db_file();
        fs::create_directories(fs::path(db_path).parent_path());

        Database db;
        if (!db_open(db, db_path)) {
            std::cerr << "sada: could not open database at " << db_path << "\n";
            return 1;
        }

        if (db_is_empty(db)) {
            std::string csv_path = dataset_locate_csv();
            if (csv_path.empty()) {
                std::cerr << "sada: could not find the word dataset.\n"
                             "      set SADA_DATA_FILE to point at sada_arabic_2000.csv\n";
                return 1;
            }
            std::vector<Word> words = dataset_load_csv(csv_path);
            if (words.empty() || !db_import_words(db, words)) {
                std::cerr << "sada: failed to import dataset from " << csv_path << "\n";
                return 1;
            }
        }

        Word w = engine_select_and_update(db);
        card_print(w);

        db_close(db);
    } catch (const std::exception& e) {
        std::cerr << "sada: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
