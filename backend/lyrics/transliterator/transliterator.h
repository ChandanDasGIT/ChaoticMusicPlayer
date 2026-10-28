#pragma once

#include <string>

// Writes lyrics in English letters, keeping the words' own language (not a
// translation): "आँखें बंद कर के" -> "Aankhen band kar ke".
//
// Hindi (Devanagari) and Bengali use spelling rules made for song lyrics:
// unspoken inherent vowels are dropped (दिल -> dil, not "dila"), long vowels
// are doubled inside a word but not at its end (जीना -> jeena, तेरी -> teri).
// Other scripts go through ICU's general Any-Latin conversion. Latin text is
// left as it is.
class Transliterator
{
public:
    static std::string toLatin(const std::string &line);

    // Mostly Latin letters (English, or lyrics already romanized).
    static bool isMostlyLatin(const std::string &text);
};
