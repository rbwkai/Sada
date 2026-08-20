// engine.cpp — Sada Adaptive Frequency-Stability (SAFS) scheduler.
//
// Sada has no grading step: it never asks "did you remember that?". So,
// unlike SM-2 or FSRS, there is no per-review signal to learn from. The
// model below adapts the *structure* of modern memory models (retrievability
// decaying from stability + elapsed time; stability growing on each
// exposure) to that constraint, in the tradition of Pimsleur's 1967
// "graduated-interval recall" — the classic example of spaced review with
// zero testing/grading, built for exactly this situation.
//
// Three ideas, each grounded in the literature cited inline:
//
//  1. Retrievability follows a power-law forgetting curve (not the classic
//     exponential Ebbinghaus curve) — this is the FSRS/DSR-model finding
//     that power-law decay fits real forgetting data better.
//  2. A word's difficulty is a fixed, log-scaled function of its corpus
//     frequency rank (the "word-frequency effect": rarer words are harder
//     to retrieve; log-frequency is the standard predictor, not raw rank).
//  3. Each exposure is assumed successful (sada has no way to know
//     otherwise) and grows stability more when the word was reviewed close
//     to its forgetting point — the FSRS "stabilization curve" property,
//     which itself operationalizes the spacing effect / desirable
//     difficulty (Cepeda et al. 2006; Bjork & Bjork).
#include "sada.h"

#include <chrono>
#include <cmath>
#include <stdexcept>

namespace sada {

// R(t, S): probability of recall after `elapsed_days` since a review that
// left the word at stability `S` (days for R to fall to 90%, by
// definition). Power-law form: R = (1 + F*t/S)^C, with F, C fixed so that
// R(S, S) = 0.90. This is the FSRS forgetting curve, adopted here purely
// as a decay model — sada does not use FSRS's difficulty/stability
// *update* formulas, since those are fit to graded review logs we don't
// have.
double engine_retrievability(double elapsed_days, double stability) {
    if (stability <= 0.0) return 0.0;
    return std::pow(1.0 + kRetrievabilityFactor * elapsed_days / stability, kRetrievabilityDecay);
}

static double importance(int freq_rank, int vocab_size) {
    if (vocab_size <= 1) return 1.0;
    return static_cast<double>(vocab_size + 1 - freq_rank) / static_cast<double>(vocab_size);
}

// Composite selection score: how urgently a word needs review (1 - R),
// scaled by how valuable it is to know (frequency-derived importance).
// This is what makes a lapsed *frequent* word outrank a lapsed *rare* one
// after a long absence, per the app's design brief — both are "due", but
// refreshing common vocabulary has more practical payoff.
static double priority(const Word& w, long long now, int vocab_size) {
    double elapsed_days = 0.0;
    if (w.last_shown_at) elapsed_days = static_cast<double>(now - *w.last_shown_at) / 86400.0;
    double r = w.stability ? engine_retrievability(elapsed_days, *w.stability) : 0.0;
    return (1.0 - r) * importance(w.freq_rank, vocab_size);
}

static long long now_unix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// Updates a word's memory state after it's chosen for display. First-ever
// exposure seeds stability from difficulty alone (harder words start
// needing review sooner). Subsequent exposures grow stability by a factor
// that:
//   - shrinks as difficulty rises        (11 - D)
//   - shrinks as stability itself rises   S^(-delta)   [stabilization decay]
//   - grows the closer R was to the "due" edge at review time
//     (exp(beta*(1-R)) - 1)               [desirable-difficulty / spacing effect]
static void engine_apply_review(Word& w, long long now) {
    double elapsed_days = 0.0;
    if (w.last_shown_at) elapsed_days = static_cast<double>(now - *w.last_shown_at) / 86400.0;
    double r = w.stability ? engine_retrievability(elapsed_days, *w.stability) : 0.0;

    if (!w.stability) {
        w.stability = kInitStabilityMax -
                      (kInitStabilityMax - kInitStabilityMin) * (w.difficulty - 1.0) / 9.0;
    } else {
        double growth = 1.0 + kGrowthK * (11.0 - w.difficulty) *
                                   std::pow(*w.stability, -kGrowthDelta) *
                                   (std::exp(kGrowthBeta * (1.0 - r)) - 1.0);
        w.stability = (*w.stability) * growth;
    }
    w.reps += 1;
    w.state = 1; // ACTIVE
    w.last_shown_at = now;
}

// Orchestrates one full "which word does the user see right now" decision:
//
//   1. Among already-introduced words, is anything due (R <= desired
//      retention)? If so, show the most urgent+important one.
//   2. Otherwise, is there room to introduce a brand-new word? Room means
//      both: fewer than kLearningPoolCap words still "learning" (stability
//      below kMatureDays), and — once past the first kSeedPoolSize words —
//      at least kMinNewWordGapSeconds since the last new word. The seed
//      phase deliberately skips the time gate so the first several runs
//      fill out a working set quickly, matching how a learner would want
//      to meet several top words right away before the pace slows down.
//   3. Otherwise (nothing due, no room for anything new): fall back to
//      showing the single most valuable word in the active set, i.e. a
//      bonus repetition rather than an idle/empty run.
Word engine_select_and_update(Database& db) {
    long long now = now_unix();
    std::vector<Word> active = db_load_active_words(db);
    int vocab_size = db_count_total_words(db);
    if (vocab_size == 0) throw std::runtime_error("vocabulary database is empty");

    int best_due_idx = -1;
    double best_due_priority = -1.0;
    int best_any_idx = -1;
    double best_any_priority = -1.0;
    int mature_count = 0;

    for (int i = 0; i < static_cast<int>(active.size()); ++i) {
        const Word& w = active[i];
        double elapsed_days = w.last_shown_at
            ? static_cast<double>(now - *w.last_shown_at) / 86400.0 : 0.0;
        double r = w.stability ? engine_retrievability(elapsed_days, *w.stability) : 0.0;
        double p = priority(w, now, vocab_size);

        if (w.stability && *w.stability >= kMatureDays) ++mature_count;
        if (p > best_any_priority) { best_any_priority = p; best_any_idx = i; }
        if (r <= kDesiredRetention && p > best_due_priority) { best_due_priority = p; best_due_idx = i; }
    }

    Word chosen;
    bool introduced_new = false;

    if (best_due_idx >= 0) {
        chosen = active[best_due_idx];
    } else {
        int learning_pool_size = static_cast<int>(active.size()) - mature_count;
        bool seed_phase = learning_pool_size < kSeedPoolSize;
        long long last_new = db_get_meta_int(db, "last_new_word_at").value_or(0);
        bool gate_open = seed_phase || (now - last_new >= kMinNewWordGapSeconds);
        bool has_room = learning_pool_size < kLearningPoolCap;

        std::optional<Word> next_new;
        if (has_room && gate_open) next_new = db_load_next_new_word(db);

        if (next_new) {
            chosen = *next_new;
            introduced_new = true;
        } else if (best_any_idx >= 0) {
            chosen = active[best_any_idx];
        } else {
            // True cold start: nothing active yet, so this must be the very
            // first word ever shown.
            auto first = db_load_next_new_word(db);
            if (!first) throw std::runtime_error("vocabulary database is empty");
            chosen = *first;
            introduced_new = true;
        }
    }

    engine_apply_review(chosen, now);
    db_save_word_after_review(db, chosen);
    if (introduced_new) db_set_meta_int(db, "last_new_word_at", now);
    return chosen;
}

} // namespace sada
