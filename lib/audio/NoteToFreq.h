#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <string>

namespace phu {
namespace audio {

/**
 * NoteToFreq: Converts musical note names to frequencies in Hz.
 *
 * Supported formats:
 *   "A4"   → 440.0 Hz  (note + octave)
 *   "C#3"  → 138.59 Hz (sharp)
 *   "Db3"  → 138.59 Hz (flat)
 *   "E#1"  → enharmonic of F1 → 43.65 Hz
 *   "Cb4"  → enharmonic of B3 → 246.94 Hz
 *
 * Octave range: 0–9 (C0 = 16.35 Hz, B9 = 15804 Hz).
 * Uses A4 = 440 Hz equal temperament tuning.
 * All parsing is case-insensitive for the note letter.
 */
class NoteToFreq {
  public:
    /**
     * Convert a note name string to a frequency in Hz.
     * @param noteName  e.g. "A4", "C#3", "Eb2", "E#1"
     * @return frequency in Hz, or std::nullopt if parsing fails
     */
    static std::optional<double> toFrequency(const std::string& noteName) {
        if (noteName.empty())
            return std::nullopt;

        auto midi = noteNameToMidi(noteName);
        if (!midi.has_value())
            return std::nullopt;

        return midiToFrequency(midi.value());
    }

    /**
     * Convert a note name to a MIDI note number (A4 = 69).
     * @param noteName  e.g. "A4", "C#3", "Eb2"
     * @return MIDI note number, or std::nullopt if parsing fails
     */
    static std::optional<int> noteNameToMidi(const std::string& noteName) {
        if (noteName.empty())
            return std::nullopt;

        size_t pos = 0;

        // 1. Parse note letter (case-insensitive)
        char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(noteName[pos])));
        if (letter < 'A' || letter > 'G')
            return std::nullopt;
        pos++;

        // Semitone offset from C for each letter: C=0, D=2, E=4, F=5, G=7, A=9, B=11
        static constexpr int semitones[] = {9, 11, 0, 2,
                                            4, 5,  7}; // A=9, B=11, C=0, D=2, E=4, F=5, G=7
        int baseSemitone = semitones[letter - 'A'];

        // 2. Parse optional accidental (# or b, multiple allowed: ## or bb)
        int accidental = 0;
        while (pos < noteName.size()) {
            char c = noteName[pos];
            if (c == '#') {
                accidental++;
                pos++;
            } else if (c == 'b' || c == 'B') {
                // Distinguish 'b' as flat vs 'B' as note letter:
                // 'b'/'B' here is always an accidental since we already parsed the note letter
                accidental--;
                pos++;
            } else {
                break;
            }
        }

        // 3. Parse octave number (required, may be negative for sub-bass but we support 0-9)
        if (pos >= noteName.size())
            return std::nullopt;

        // Check remaining is a valid integer
        bool hasDigit = false;
        bool negative = false;
        if (noteName[pos] == '-') {
            negative = true;
            pos++;
        }

        int octave = 0;
        while (pos < noteName.size()) {
            char c = noteName[pos];
            if (c >= '0' && c <= '9') {
                octave = octave * 10 + (c - '0');
                hasDigit = true;
                pos++;
            } else {
                return std::nullopt; // unexpected character
            }
        }

        if (!hasDigit)
            return std::nullopt;

        if (negative)
            octave = -octave;

        // 4. Calculate MIDI note number
        // MIDI: C4 = 60, so C_n = 12 * (n + 1)
        // General: midi = 12 * (octave + 1) + semitoneFromC + accidental
        int semitoneFromC = baseSemitone;
        // A,B are above C in the same octave, so no adjustment needed
        // with the semitones[] lookup

        int midi = 12 * (octave + 1) + semitoneFromC + accidental;

        // Clamp to valid MIDI range
        if (midi < 0 || midi > 127)
            return std::nullopt;

        return midi;
    }

    /**
     * Convert MIDI note number to frequency using A4=440 Hz equal temperament.
     * @param midiNote  MIDI note number (0–127, A4=69)
     * @return frequency in Hz
     */
    static double midiToFrequency(int midiNote) {
        // f = 440 * 2^((midi - 69) / 12)
        return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
    }

    /**
     * Convert a frequency to the nearest note name string.
     * @param freqHz  frequency in Hz
     * @return note name like "A4", "C#3", etc. Returns empty string for out-of-range.
     */
    static std::string frequencyToNoteName(double freqHz) {
        if (freqHz <= 0.0)
            return {};

        // midi = 69 + 12 * log2(freq / 440)
        double midiFloat = 69.0 + 12.0 * std::log2(freqHz / 440.0);
        int midi = static_cast<int>(std::round(midiFloat));

        if (midi < 0 || midi > 127)
            return {};

        return midiToNoteName(midi);
    }

    /**
     * Convert a MIDI note number to a note name string.
     * @param midiNote  MIDI note number (0–127)
     * @return note name like "C4", "F#2"
     */
    static std::string midiToNoteName(int midiNote) {
        if (midiNote < 0 || midiNote > 127)
            return {};

        static const char* noteNames[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                          "F#", "G",  "G#", "A",  "A#", "B"};

        int octave = (midiNote / 12) - 1;
        int semitone = midiNote % 12;

        return std::string(noteNames[semitone]) + std::to_string(octave);
    }

    /**
     * Check if a string looks like a note name (starts with A-G).
     * Useful for deciding whether to parse as note vs. numeric frequency.
     * @param text  input string
     * @return true if it could be a note name
     */
    static bool looksLikeNoteName(const std::string& text) {
        if (text.empty())
            return false;
        char first = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
        return first >= 'A' && first <= 'G';
    }
};

} // namespace audio
} // namespace phu
