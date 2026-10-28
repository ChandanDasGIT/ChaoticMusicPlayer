#include "backend/lyrics/transliterator/transliterator.h"

#include <unicode/translit.h>
#include <unicode/unistr.h>

#include <cctype>
#include <memory>
#include <string_view>
#include <vector>

namespace {

// ---- UTF-8 ----------------------------------------------------------------

std::u32string decode(const std::string &text)
{
    std::u32string out;
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = text[i];
        char32_t cp = c;
        size_t extra = 0;
        if (c >= 0xF0) { cp = c & 0x07; extra = 3; }
        else if (c >= 0xE0) { cp = c & 0x0F; extra = 2; }
        else if (c >= 0xC0) { cp = c & 0x1F; extra = 1; }
        if (extra > 0 && i + extra >= text.size()) { // truncated sequence
            out.push_back(U'�');
            break;
        }
        for (size_t k = 1; k <= extra; ++k)
            cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
        out.push_back(cp);
        i += extra + 1;
    }
    return out;
}

void append(std::string &out, char32_t cp)
{
    if (cp < 0x80) {
        out += char(cp);
    } else if (cp < 0x800) {
        out += char(0xC0 | (cp >> 6));
        out += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += char(0xE0 | (cp >> 12));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    } else {
        out += char(0xF0 | (cp >> 18));
        out += char(0x80 | ((cp >> 12) & 0x3F));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
}

// ---- Indic scripts with lyric-friendly rules ---------------------------------

// How a vowel is written: inside a word, at its very end, and at the end
// before a nasal (नहीं -> nahin, हूँ -> hoon, कहाँ -> kahaan).
struct Vowel
{
    const char *medial;
    const char *final;
    const char *finalNasal;
};

enum class Kind { Other, Consonant, Nukta, IndependentVowel, Matra, Virama, Nasal, Visarga, Om, Danda, Digit };

struct Script
{
    char32_t first, last;          // code point block
    char32_t base;                 // digits / letters are offsets from here
    Vowel inherent;
    bool dropInnerSchwa;           // VC_CV rule (Hindi and Bengali both)
    Kind (*kind)(char32_t);
    const char *(*consonant)(char32_t);
    Vowel (*vowel)(char32_t);      // independent vowels and matras
    const char *(*nukta)(char32_t);// consonant + nukta
};

// -- Devanagari (Hindi) --
Kind devaKind(char32_t c)
{
    if ((c >= 0x915 && c <= 0x939) || (c >= 0x958 && c <= 0x95F)) return Kind::Consonant;
    if (c == 0x93C) return Kind::Nukta;
    if (c >= 0x904 && c <= 0x914) return Kind::IndependentVowel;
    if ((c >= 0x93E && c <= 0x94C) || c == 0x94E || c == 0x94F) return Kind::Matra;
    if (c == 0x94D) return Kind::Virama;
    if (c == 0x901 || c == 0x902) return Kind::Nasal;
    if (c == 0x903) return Kind::Visarga;
    if (c == 0x950) return Kind::Om;
    if (c == 0x964 || c == 0x965) return Kind::Danda;
    if (c >= 0x966 && c <= 0x96F) return Kind::Digit;
    return Kind::Other;
}

const char *devaConsonant(char32_t c)
{
    switch (c) {
    case 0x915: return "k";  case 0x916: return "kh"; case 0x917: return "g";  case 0x918: return "gh";
    case 0x919: return "n";  case 0x91A: return "ch"; case 0x91B: return "chh"; case 0x91C: return "j";
    case 0x91D: return "jh"; case 0x91E: return "\x01"; // ञ: "n", or "y" in ज्ञ (gy)
    case 0x91F: return "t";  case 0x920: return "th"; case 0x921: return "d";  case 0x922: return "dh";
    case 0x923: return "n";  case 0x924: return "t";  case 0x925: return "th"; case 0x926: return "d";
    case 0x927: return "dh"; case 0x928: return "n";  case 0x929: return "n";  case 0x92A: return "p";
    case 0x92B: return "ph"; case 0x92C: return "b";  case 0x92D: return "bh"; case 0x92E: return "m";
    case 0x92F: return "y";  case 0x930: return "r";  case 0x931: return "r";  case 0x932: return "l";
    case 0x933: return "l";  case 0x934: return "l";  case 0x935: return "v";  case 0x936: return "sh";
    case 0x937: return "sh"; case 0x938: return "s";  case 0x939: return "h";
    case 0x958: return "q";  case 0x959: return "kh"; case 0x95A: return "gh"; case 0x95B: return "z";
    case 0x95C: return "d";  case 0x95D: return "dh"; case 0x95E: return "f";  case 0x95F: return "y";
    }
    return "";
}

const char *devaNukta(char32_t c)
{
    switch (c) {
    case 0x915: return "q"; case 0x916: return "kh"; case 0x917: return "gh"; case 0x91C: return "z";
    case 0x921: return "d"; case 0x922: return "dh"; case 0x92B: return "f";  case 0x92F: return "y";
    }
    return nullptr;
}

Vowel devaVowel(char32_t c)
{
    switch (c) {
    case 0x905: return {"a", "a", "an"};
    case 0x906: case 0x93E: return {"aa", "a", "aan"};
    case 0x907: case 0x93F: return {"i", "i", "in"};
    case 0x908: case 0x940: return {"ee", "i", "in"};
    case 0x909: case 0x941: return {"u", "u", "un"};
    case 0x90A: case 0x942: return {"oo", "u", "oon"};
    case 0x90B: case 0x943: return {"ri", "ri", "rin"};
    case 0x90D: case 0x945: case 0x90E: case 0x946:
    case 0x90F: case 0x947: return {"e", "e", "ein"}; // में -> mein, तुम्हें -> tumhein
    case 0x910: case 0x948: return {"ai", "ai", "ain"};
    case 0x911: case 0x949: case 0x912: case 0x94A:
    case 0x913: case 0x94B: return {"o", "o", "on"};
    case 0x914: case 0x94C: return {"au", "au", "aun"};
    }
    return {"", "", "n"};
}

// -- Bengali --
Kind bengKind(char32_t c)
{
    if ((c >= 0x995 && c <= 0x9B9) || c == 0x9DC || c == 0x9DD || c == 0x9DF || c == 0x9CE) return Kind::Consonant;
    if (c == 0x9BC) return Kind::Nukta;
    if (c >= 0x985 && c <= 0x994) return Kind::IndependentVowel;
    if ((c >= 0x9BE && c <= 0x9CC) || c == 0x9D7) return Kind::Matra;
    if (c == 0x9CD) return Kind::Virama;
    if (c == 0x981) return Kind::Nasal;
    if (c == 0x982) return Kind::Nasal; // ং, written "ng" below
    if (c == 0x983) return Kind::Visarga;
    if (c == 0x964 || c == 0x965) return Kind::Danda;
    if (c >= 0x9E6 && c <= 0x9EF) return Kind::Digit;
    return Kind::Other;
}

const char *bengConsonant(char32_t c)
{
    switch (c) {
    case 0x995: return "k";  case 0x996: return "kh"; case 0x997: return "g";  case 0x998: return "gh";
    case 0x999: return "ng"; case 0x99A: return "ch"; case 0x99B: return "chh"; case 0x99C: return "j";
    case 0x99D: return "jh"; case 0x99E: return "n";  case 0x99F: return "t";  case 0x9A0: return "th";
    case 0x9A1: return "d";  case 0x9A2: return "dh"; case 0x9A3: return "n";  case 0x9A4: return "t";
    case 0x9A5: return "th"; case 0x9A6: return "d";  case 0x9A7: return "dh"; case 0x9A8: return "n";
    case 0x9AA: return "p";  case 0x9AB: return "ph"; case 0x9AC: return "b";  case 0x9AD: return "bh";
    case 0x9AE: return "m";  case 0x9AF: return "j";  case 0x9B0: return "r";  case 0x9B2: return "l";
    case 0x9B6: return "sh"; case 0x9B7: return "sh"; case 0x9B8: return "s";  case 0x9B9: return "h";
    case 0x9DC: return "r";  case 0x9DD: return "rh"; case 0x9DF: return "y";  case 0x9CE: return "t";
    }
    return "";
}

const char *bengNukta(char32_t c)
{
    switch (c) {
    case 0x9A1: return "r"; case 0x9A2: return "rh"; case 0x9AF: return "y";
    }
    return nullptr;
}

Vowel bengVowel(char32_t c)
{
    switch (c) {
    case 0x985: return {"o", "o", "on"};
    case 0x986: case 0x9BE: return {"a", "a", "an"};
    case 0x987: case 0x9BF: case 0x988: case 0x9C0: return {"i", "i", "in"};
    case 0x989: case 0x9C1: case 0x98A: case 0x9C2: return {"u", "u", "un"};
    case 0x98B: case 0x9C3: return {"ri", "ri", "rin"};
    case 0x98F: case 0x9C7: return {"e", "e", "en"};
    case 0x990: case 0x9C8: return {"oi", "oi", "oin"};
    case 0x993: case 0x9CB: return {"o", "o", "on"};
    case 0x994: case 0x9CC: case 0x9D7: return {"ou", "ou", "oun"};
    }
    return {"", "", "n"};
}

const Script Devanagari{0x900, 0x97F, 0x966, {"a", "a", "an"}, true, devaKind, devaConsonant, devaVowel, devaNukta};
const Script Bengali{0x980, 0x9FF, 0x9E6, {"o", "o", "on"}, true, bengKind, bengConsonant, bengVowel, bengNukta};

const Script *scriptOf(char32_t c)
{
    if (c >= Devanagari.first && c <= Devanagari.last)
        return &Devanagari;
    if (c >= Bengali.first && c <= Bengali.last)
        return &Bengali;
    return nullptr;
}

// One written syllable: consonant(s) + vowel (+ nasal).
struct Unit
{
    enum VowelState { Inherent, Explicit, None };
    std::string consonant;      // "" for an independent vowel
    VowelState state = Inherent;
    Vowel vowel{};
    std::string nasal;          // "n", "ng", "h" (visarga)
    bool labialNext = false;
};

bool isLabial(const std::string &consonant)
{
    return !consonant.empty() && (consonant[0] == 'p' || consonant[0] == 'b' || consonant[0] == 'm');
}

// One word of an Indic script to English letters.
std::string romanizeWord(const std::u32string &word, const Script &script)
{
    const bool bengali = &script == &Bengali;
    std::vector<Unit> units;
    std::string out;

    for (size_t i = 0; i < word.size(); ++i) {
        const char32_t c = word[i];
        switch (script.kind(c)) {
        case Kind::Consonant: {
            std::string roman = script.consonant(c);
            if (i + 1 < word.size() && script.kind(word[i + 1]) == Kind::Nukta) {
                if (const char *n = script.nukta(c))
                    roman = n;
                ++i;
            }
            // Bengali ব / য after a virama (ব-ফলা, য-ফলা): not pronounced as such.
            if (bengali && !units.empty() && units.back().state == Unit::None && (c == 0x9AC || c == 0x9AF)) {
                units.back().state = Unit::Inherent; // স্বপ্ন -> shopno: the vowel goes to স
                break;
            }
            Unit unit;
            unit.consonant = roman;
            if (c == 0x9CE) // ৎ: never has a vowel
                unit.state = Unit::None;
            units.push_back(unit);
            break;
        }
        case Kind::IndependentVowel: {
            Unit unit;
            unit.state = Unit::Explicit;
            unit.vowel = script.vowel(c);
            units.push_back(unit);
            break;
        }
        case Kind::Matra:
            if (!units.empty()) {
                Unit &last = units.back();
                // Bengali ো / ৌ written as two parts (ে + া, ে + ৗ)
                if (bengali && last.state == Unit::Explicit && std::string_view(last.vowel.medial) == "e"
                    && (c == 0x9BE || c == 0x9D7))
                    last.vowel = c == 0x9BE ? bengVowel(0x9CB) : bengVowel(0x9CC);
                else {
                    last.state = Unit::Explicit;
                    last.vowel = script.vowel(c);
                }
            }
            break;
        case Kind::Virama:
            if (!units.empty())
                units.back().state = Unit::None;
            break;
        case Kind::Nasal:
            if (!units.empty())
                units.back().nasal = (bengali && c == 0x982) ? "ng" : "n";
            break;
        case Kind::Visarga:
            if (!units.empty())
                units.back().nasal = "h";
            break;
        default:
            break;
        }
    }
    if (units.empty())
        return out;

    // Unspoken inherent vowels: at the end of a word of two or more syllables,
    // and inside a word between "vowel + consonant" and "consonant + vowel"
    // (समझना -> samajhna), unless that would join three consonants.
    const auto realized = [](const Unit &u) { return u.state != Unit::None; };
    // (Bengali keeps it after a consonant cluster: স্বপ্ন -> sopno; Hindi दर्द -> dard.)
    const bool afterCluster = units.size() > 1 && units[units.size() - 2].state == Unit::None;
    if (units.size() > 1 && units.back().state == Unit::Inherent && !units.back().consonant.empty()
        && units.back().nasal.empty() && !(bengali && afterCluster))
        units.back().state = Unit::None;
    if (script.dropInnerSchwa) {
        for (size_t i = units.size() - 1; i-- > 1;) {
            Unit &u = units[i];
            const Unit &prev = units[i - 1];
            const Unit &next = units[i + 1];
            if (u.state == Unit::Inherent && !u.consonant.empty() && u.nasal.empty() && realized(prev)
                && prev.nasal.empty() && !next.consonant.empty() && realized(next))
                u.state = Unit::None;
        }
    }

    for (size_t i = 0; i < units.size(); ++i) {
        Unit &u = units[i];
        std::string consonant = u.consonant;
        if (consonant == "\x01") { // ञ
            const bool afterJ = i > 0 && units[i - 1].state == Unit::None && units[i - 1].consonant == "j";
            consonant = afterJ ? "y" : "n";
            if (afterJ) // ज्ञ -> gy
                out.back() = 'g';
        }
        out += consonant;

        const Vowel v = u.state == Unit::Explicit ? u.vowel : script.inherent;
        const bool last = i + 1 == units.size();
        const bool onlyVowel = units.size() == 1 && consonant.empty(); // आ -> aa
        if (u.state != Unit::None) {
            if (last && !onlyVowel)
                out += u.nasal == "n" ? v.finalNasal : v.final;
            else
                out += v.medial;
        }
        if (!u.nasal.empty() && !(last && u.nasal == "n" && u.state != Unit::None && !onlyVowel)) {
            // anusvara before p / b / m sounds like m (चुंबन -> chumban, but कंबल -> kambal)
            const bool beforeLabial = !last && isLabial(units[i + 1].consonant) && u.nasal == "n";
            out += beforeLabial ? "m" : u.nasal;
        }
    }
    return out;
}

// ---- ICU for every other script -----------------------------------------------

std::string icuToLatin(const std::string &text)
{
    thread_local std::unique_ptr<icu::Transliterator> trans = [] {
        UErrorCode status = U_ZERO_ERROR;
        std::unique_ptr<icu::Transliterator> t(icu::Transliterator::createInstance(
            icu::UnicodeString::fromUTF8("Any-Latin; Latin-ASCII"), UTRANS_FORWARD, status));
        return U_SUCCESS(status) ? std::move(t) : nullptr;
    }();
    if (!trans)
        return text;
    icu::UnicodeString value = icu::UnicodeString::fromUTF8(text);
    trans->transliterate(value);
    std::string out;
    value.toUTF8String(out);
    return out;
}

bool isLatinLetter(char32_t c)
{
    return (c < 0x80 && std::isalpha(int(c))) || (c >= 0xC0 && c < 0x250 && c != 0xD7 && c != 0xF7);
}

// A letter of another writing system (not punctuation, symbols or emoji).
bool isOtherLetter(char32_t c)
{
    return c >= 0x370 && !(c >= 0x2000 && c <= 0x2BFF) && !(c >= 0x3000 && c <= 0x303F)
           && !(c >= 0xFE00 && c <= 0xFE0F) && c < 0x1F000;
}

} // namespace

std::string Transliterator::toLatin(const std::string &line)
{
    const std::u32string text = decode(line);
    std::string out;
    bool otherScripts = false;

    for (size_t i = 0; i < text.size();) {
        const Script *script = scriptOf(text[i]);
        if (!script) {
            otherScripts |= isOtherLetter(text[i]);
            append(out, text[i]);
            ++i;
            continue;
        }
        const Kind kind = script->kind(text[i]);
        if (kind == Kind::Danda) {
            ++i;
            continue;
        }
        if (kind == Kind::Digit) {
            out += char('0' + (text[i] - script->base));
            ++i;
            continue;
        }
        if (kind == Kind::Om) {
            out += "om";
            ++i;
            continue;
        }
        // A word: letters of this script in a row.
        size_t end = i;
        while (end < text.size() && scriptOf(text[end]) == script) {
            const Kind k = script->kind(text[end]);
            if (k == Kind::Danda || k == Kind::Digit || k == Kind::Om || k == Kind::Other)
                break;
            ++end;
        }
        if (end == i) { // unknown sign of the block
            ++i;
            continue;
        }
        out += romanizeWord(text.substr(i, end - i), *script);
        i = end;
    }

    if (otherScripts)
        out = icuToLatin(out);

    // Trim what dandas left behind, and start the line with a capital.
    while (!out.empty() && out.back() == ' ')
        out.pop_back();
    for (char &c : out) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            c = char(std::toupper(static_cast<unsigned char>(c)));
            break;
        }
        if (static_cast<unsigned char>(c) >= 0x80)
            break;
    }
    return out;
}

bool Transliterator::isMostlyLatin(const std::string &text)
{
    size_t latin = 0, other = 0;
    for (const char32_t c : decode(text)) {
        if (isLatinLetter(c))
            ++latin;
        else if (isOtherLetter(c))
            ++other;
    }
    return latin > 0 && latin >= 9 * other; // >= 90% of the letters
}
