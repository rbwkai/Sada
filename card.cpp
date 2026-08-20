// card.cpp — compact premium vocabulary card.
#include "sada.h"
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <unistd.h>

namespace sada {

static constexpr int W = 42;
static constexpr int TOP_L = 28;
static constexpr int TOP_R = 14;
static constexpr int BOT_L = 20;
static constexpr int BOT_R = 21;

static int vw(const std::string& s) {
    int w = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        uint32_t cp = 0;
        size_t n = 1;

        if (c < 0x80) cp = c;
        else if ((c & 0xE0) == 0xC0) cp = c & 0x1F, n = 2;
        else if ((c & 0xF0) == 0xE0) cp = c & 0x0F, n = 3;
        else if ((c & 0xF8) == 0xF0) cp = c & 0x07, n = 4;

        for (size_t j = 1; j < n && i + j < s.size(); ++j)
            cp = (cp << 6) | (s[i + j] & 0x3F);

        bool combining =
            (cp >= 0x064B && cp <= 0x065F) ||
            (cp >= 0x0610 && cp <= 0x061A) ||
            cp == 0x0670 ||
            (cp >= 0x06D6 && cp <= 0x06ED);

        if (!combining) ++w;
        i += n;
    }
    return w;
}

static std::string rep(const char* s, int n) {
    std::string r;
    for (int i = 0; i < n; ++i) r += s;
    return r;
}

static std::string center(const std::string& s, int width) {
    int n = vw(s);
    if (n >= width) return s;

    int l = (width - n) / 2;
    return std::string(l, ' ') + s +
           std::string(width - n - l, ' ');
}

static std::string lpad(const std::string& s, int width) {
    int n = vw(s);
    return n >= width ? s : std::string(width - n, ' ') + s;
}

static std::string rpad(const std::string& s, int width) {
    int n = vw(s);
    return n >= width ? s : s + std::string(width - n, ' ');
}

static std::string pos_one(const std::string& s) {
    if (s == "noun") return "n.";
    if (s == "verb") return "v.";
    if (s == "adjective") return "adj";
    if (s == "adverb") return "adv";
    if (s == "preposition") return "prep";
    if (s == "conjunction") return "conj";
    if (s == "particle") return "part";
    if (s == "pronoun") return "pron";
    if (s == "numeral") return "num";
    if (s == "article") return "art";
    if (s == "phrase") return "phr";
    if (s == "determiner") return "det";
    if (s == "connector") return "conn";
    if (s == "construction") return "cnst";
    if (s == "adverbial phrase") return "adv phr";
    if (s == "relative pronoun") return "rel pron";
    return s.size() <= 4 ? s : s.substr(0, 3);
}

static std::string pos_abbr(const std::string& s) {
    if (s.empty()) return "";

    std::stringstream ss(s);
    std::string x, out;

    while (std::getline(ss, x, '/')) {
        if (!out.empty()) out += "/";
        out += pos_one(x);
    }
    return out;
}

void card_print(const Word& w) {
    bool tty = isatty(fileno(stdout));

    // Catppuccin Mocha
    const std::string outline = tty ? "\033[38;2;108;112;134m" : "";
    const std::string sada    = tty ? "\033[1;38;2;180;190;254m" : "";
    const std::string main    = tty ? "\033[1;3;38;2;243;139;168m" : "";
    const std::string arabic  = tty ? "\033[38;2;245;194;231m" : "";
    const std::string english = tty ? "\033[38;2;203;166;247m" : "";
    const std::string pos     = tty ? "\033[38;2;147;153;178m" : "";
    const std::string rank    = tty ? "\033[38;2;166;173;200m" : "";
    const std::string dim     = tty ? "\033[38;2;88;91;112m" : "";
    const std::string res     = tty ? "\033[0m" : "";

    const std::string p = pos_abbr(w.part_of_speech);
    const std::string ps = p.empty() ? "" : "(" + p + ")";

    std::cout
        << outline << "╭"
        << outline << "── "
        << sada << "Sada "
        << res
        << outline << rep("─", W - 8)
        << "╮" << res << '\n';

    // Top line: Transliteration exactly where it was from the left, a space, then PoS
    int n_trans = vw(w.transliteration);
    int trans_left_pad = n_trans >= TOP_L ? 0 : (TOP_L - n_trans) / 2;
    int remaining_right_pad = W - trans_left_pad - n_trans - 1 - vw(ps);
    if (remaining_right_pad < 0) remaining_right_pad = 0;

    std::cout
        << outline << "│" << res
        << std::string(trans_left_pad, ' ')
        << main << w.transliteration << res
        << " "
        << pos << ps << res
        << std::string(remaining_right_pad, ' ')
        << outline << "│" << res << '\n';

    // Arabic: right-aligned to the midpoint, with exactly 2 spaces
    // between the Arabic text and the center pipe.
    // English: 2 spaces from the center pipe.
    std::cout
        << outline << "│" << res
        << arabic << lpad(w.arabic, BOT_L - 2) << res
        << std::string(2, ' ')
        << dim << "┊" << res
        << std::string(2, ' ')
        << english << rpad(w.meaning, BOT_R - 2) << res
        << outline << "│" << res << '\n';

    // Rank has one space before and after, matching the title styling.
    std::string rb =
        w.freq_rank > 0 ? " #" + std::to_string(w.freq_rank) + " " : "";

    // dash_right reduced to 2 so there is just a space from `rb`, two dashes, and the ╯
    int dash_right = 2; 
    int dash_left = std::max(0, W - dash_right - vw(rb));

    std::cout
        << outline << "╰"
        << rep("─", dash_left)
        << rank << rb << res
        << outline << rep("─", dash_right)
        << "╯" << res << '\n';
}

} // namespace sada