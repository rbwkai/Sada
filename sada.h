// sada.h — shared types, tunable constants, and module interfaces.
//
// This is the project's one shared header. Every .cpp file below is a
// separate concern (dataset, db, engine, card, main) but they all need the
// Word struct and a handful of cross-module function signatures, so those
// live here instead of being duplicated or given one header apiece.
#pragma once

#include <string>
#include <vector>
#include <optional>

struct sqlite3; // fwd-decl, avoids leaking <sqlite3.h> into every file

namespace sada {

// ---------------------------------------------------------------------
// Engine constants
//
// These are heuristic defaults, not values fit to review-outcome data.
// Sada has no grading step (no "again/hard/good/easy"), so unlike SM-2 or
// FSRS there is nothing to fit weights against. Each constant is explained
// in engine.cpp next to where it's used. They're grouped here so the whole
// tunable surface of the algorithm is visible in one place.
// ---------------------------------------------------------------------
constexpr double kDesiredRetention     = 0.85;      // R below this = "due"
constexpr double kRetrievabilityFactor = 19.0 / 81.0; // F, power-law curve
constexpr double kRetrievabilityDecay  = -0.5;        // C, power-law curve
constexpr double kGrowthK              = 0.35;        // stability growth rate
constexpr double kGrowthDelta          = 0.15;        // stabilization decay
constexpr double kGrowthBeta           = 1.5;         // desirable-difficulty gain
constexpr double kInitStabilityMax     = 1.6;         // days, easiest word (D=1)
constexpr double kInitStabilityMin     = 0.3;         // days, hardest word (D=10)
constexpr double kMatureDays           = 21.0;        // stability => "graduated"
constexpr int    kSeedPoolSize         = 7;           // fast ramp-up, no gating
constexpr int    kLearningPoolCap      = 12;          // hard cap while learning
constexpr long long kMinNewWordGapSeconds = 4 * 60 * 60; // pacing once past seed

// ---------------------------------------------------------------------
// Word: one row of the vocabulary/memory-model table.
// ---------------------------------------------------------------------
struct Word {
    int id = 0;                                  // = freq_rank at import time
    std::string arabic;
    std::string transliteration;
    std::string meaning;
    std::string part_of_speech;
    int freq_rank = 0;                            // 1 = most frequent
    double difficulty = 1.0;                       // 1 (easy) .. 10 (hard)
    int state = 0;                                 // 0 = NEW, 1 = ACTIVE
    int reps = 0;
    std::optional<double> stability;                // days; unset until shown
    std::optional<long long> last_shown_at;         // unix seconds; unset until shown
};

// ---------------------- dataset.cpp ----------------------
// Finds and parses the frequency-ordered CSV that ships outside the binary.
std::string dataset_locate_csv();
std::vector<Word> dataset_load_csv(const std::string& path);

// ---------------------- db.cpp ----------------------
// Thin SQLite wrapper. All persistence lives here.
struct Database {
    sqlite3* handle = nullptr;
};
bool db_open(Database& db, const std::string& path);
void db_close(Database& db);
bool db_is_empty(Database& db);
bool db_import_words(Database& db, const std::vector<Word>& words);
int db_count_total_words(Database& db);
std::vector<Word> db_load_active_words(Database& db);
std::optional<Word> db_load_next_new_word(Database& db);
void db_save_word_after_review(Database& db, const Word& w);
std::optional<long long> db_get_meta_int(Database& db, const std::string& key);
void db_set_meta_int(Database& db, const std::string& key, long long value);

// ---------------------- engine.cpp ----------------------
// The Sada Adaptive Frequency-Stability (SAFS) scheduler. Pure math lives
// in engine_retrievability(); engine_select_and_update() is the orchestrator
// that reads the DB, picks a word, updates its memory state, and writes it
// back — this is the whole "backend" the CLI calls into.
double engine_retrievability(double elapsed_days, double stability);
Word engine_select_and_update(Database& db);

// ---------------------- card.cpp ----------------------
void card_print(const Word& w);

// ---------------------- main.cpp (path helpers) ----------------------
std::string paths_home_data_dir();
std::string paths_db_file();

} // namespace sada
