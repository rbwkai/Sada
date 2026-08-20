// db.cpp — the only file that speaks SQLite. Everything else works with
// plain Word structs.
#include "sada.h"

#include <sqlite3.h>
#include <cmath>
#include <stdexcept>

namespace sada {

static const char* kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS words (
    id              INTEGER PRIMARY KEY,
    arabic          TEXT    NOT NULL,
    transliteration TEXT    NOT NULL,
    meaning         TEXT    NOT NULL,
    part_of_speech  TEXT    NOT NULL DEFAULT '',
    freq_rank       INTEGER NOT NULL,
    difficulty      REAL    NOT NULL,
    state           INTEGER NOT NULL DEFAULT 0,
    reps            INTEGER NOT NULL DEFAULT 0,
    stability       REAL,
    last_shown_at   INTEGER
);
CREATE TABLE IF NOT EXISTS meta (
    key   TEXT PRIMARY KEY,
    value TEXT
);
)SQL";

bool db_open(Database& db, const std::string& path) {
    if (sqlite3_open(path.c_str(), &db.handle) != SQLITE_OK) return false;
    char* err = nullptr;
    if (sqlite3_exec(db.handle, kSchema, nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    return true;
}

void db_close(Database& db) {
    if (db.handle) sqlite3_close(db.handle);
    db.handle = nullptr;
}

bool db_is_empty(Database& db) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db.handle, "SELECT COUNT(*) FROM words;", -1, &stmt, nullptr);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count == 0;
}

int db_count_total_words(Database& db) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db.handle, "SELECT COUNT(*) FROM words;", -1, &stmt, nullptr);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

bool db_import_words(Database& db, const std::vector<Word>& words) {
    if (words.empty()) return false;
    const int n = static_cast<int>(words.size());

    char* err = nullptr;
    sqlite3_exec(db.handle, "BEGIN TRANSACTION;", nullptr, nullptr, &err);

    const char* sql =
        "INSERT INTO words (id, arabic, transliteration, meaning, part_of_speech, "
        "freq_rank, difficulty, state, reps, stability, last_shown_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, 0, 0, NULL, NULL);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db.handle, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db.handle, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& w : words) {
        // Difficulty is a log-frequency proxy: rank 1 -> 1.0 (easiest),
        // rank n -> 10.0 (hardest). Log scaling matches the well-established
        // word-frequency effect in lexical retrieval (Howes & Solomon 1951;
        // Balota & Chumbley 1984), where recognition ease tracks log
        // frequency rather than raw rank.
        double difficulty = (n > 1)
            ? 1.0 + 9.0 * std::log(static_cast<double>(w.freq_rank)) / std::log(static_cast<double>(n))
            : 1.0;

        sqlite3_bind_int(stmt, 1, w.id);
        sqlite3_bind_text(stmt, 2, w.arabic.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, w.transliteration.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, w.meaning.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, w.part_of_speech.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 6, w.freq_rank);
        sqlite3_bind_double(stmt, 7, difficulty);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            sqlite3_exec(db.handle, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db.handle, "COMMIT;", nullptr, nullptr, &err);
    return true;
}

static Word row_to_word(sqlite3_stmt* stmt) {
    Word w;
    w.id = sqlite3_column_int(stmt, 0);
    w.arabic = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    w.transliteration = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    w.meaning = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    const char* pos = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    w.part_of_speech = pos ? pos : "";
    w.freq_rank = sqlite3_column_int(stmt, 5);
    w.difficulty = sqlite3_column_double(stmt, 6);
    w.state = sqlite3_column_int(stmt, 7);
    w.reps = sqlite3_column_int(stmt, 8);
    if (sqlite3_column_type(stmt, 9) != SQLITE_NULL)
        w.stability = sqlite3_column_double(stmt, 9);
    if (sqlite3_column_type(stmt, 10) != SQLITE_NULL)
        w.last_shown_at = sqlite3_column_int64(stmt, 10);
    return w;
}

static const char* kSelectCols =
    "SELECT id, arabic, transliteration, meaning, part_of_speech, freq_rank, difficulty, "
    "state, reps, stability, last_shown_at FROM words";

std::vector<Word> db_load_active_words(Database& db) {
    std::vector<Word> out;
    std::string sql = std::string(kSelectCols) + " WHERE state = 1;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db.handle, sql.c_str(), -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) out.push_back(row_to_word(stmt));
    sqlite3_finalize(stmt);
    return out;
}

std::optional<Word> db_load_next_new_word(Database& db) {
    std::string sql = std::string(kSelectCols) + " WHERE state = 0 ORDER BY freq_rank ASC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db.handle, sql.c_str(), -1, &stmt, nullptr);
    std::optional<Word> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = row_to_word(stmt);
    sqlite3_finalize(stmt);
    return result;
}

void db_save_word_after_review(Database& db, const Word& w) {
    const char* sql =
        "UPDATE words SET state = ?, reps = ?, stability = ?, last_shown_at = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db.handle, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, w.state);
    sqlite3_bind_int(stmt, 2, w.reps);
    if (w.stability) sqlite3_bind_double(stmt, 3, *w.stability); else sqlite3_bind_null(stmt, 3);
    if (w.last_shown_at) sqlite3_bind_int64(stmt, 4, *w.last_shown_at); else sqlite3_bind_null(stmt, 4);
    sqlite3_bind_int(stmt, 5, w.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::optional<long long> db_get_meta_int(Database& db, const std::string& key) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db.handle, "SELECT value FROM meta WHERE key = ?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<long long> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (text) result = std::stoll(text);
    }
    sqlite3_finalize(stmt);
    return result;
}

void db_set_meta_int(Database& db, const std::string& key, long long value) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db.handle, "INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?);",
                        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, std::to_string(value).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

} // namespace sada
