#ifndef DISPLAY_STRINGS_H
#define DISPLAY_STRINGS_H

const char *note_names[] = {
    "C",
    "C",
    "D",
    "D",
    "E",
    "F",
    "F",
    "G",
    "G",
    "A",
    "A",
    "B"
};

const char *diesis = "#";

const char *scale_names[] = {
    "Major",
    "Natural min",
    "Harmonic min",
    "Dorian",
    "Lydian",
    "Mixolydian",
    "Locrian",
    "Phrygian",
    "Phrygian Dom",
    "Pentatonic M",
    "Pentatonic m",
    "Penta. Blues",
    "Arabian",
    "Oriental",
    "Japanese",
    "Chromatic",
    "* Twinkle",
    "* Hot Cross",
    "* Mary Lamb",
    "* Jingle B.",
    "Custom 1",
    "Custom 2",
    "Custom 3",
    "Custom 4",
};

const char *octave_names[] = {
    "/",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9"
};

const char *instrument_names[] = {
    "Dodepan",
    "Magic Bell",
    "Space Piano",
    "Robot Voice",
    "Synthwave",
    "Bleep Bloop",
    "Fifth Lead",
    "Strings",
    "Brass",
    "Pad",
    "Synth Bass",
    "PWM Lead",
    "Elec. Piano",
    "Square Tone",
    "User 1",
    "User 2",
    "User 3",
    "User 4",
};

const char *chord_mode_names[] = {
    "Off",
    "Power",
    "Triad",
    "Octave",
};

const char *arp_pattern_names[] = {
    "Off",
    "Up",
    "Down",
    "Up-Down",
    "Random",
};

const char *arp_speed_names[] = {
    "Slow",
    "Medium",
    "Fast",
    "V.Fast",
};

const char *arp_octave_names[] = {
    "1 Oct",
    "2 Oct",
    "3 Oct",
};

const char *parameter_names[] = {
    "Osc 1 Wave",
    "Osc 1 Shape",
    "Osc 1 Morph",
    "Mixer Noise/Sub Osc",

    "Osc 2 Wave",
    "Osc 2 Coarse",
    "Osc 2 Pitch",
    "Mixer Osc Mix",

    "Filter Cutoff",
    "Filter Resonance",
    "Filter EG Amount",
    "Filter Key Track",

    "EG Attack",
    "EG Decay",
    "EG Sustain",
    "EG Release",

    "EG Osc Amount",
    "EG Osc Dst",
    "Voice Mode",
    "Portamento",

    "LFO Wave",
    "LFO Rate",
    "LFO Depth",
    "LFO Fade Time",

    "LFO Osc Amount",
    "LFO Osc Destination",
    "LFO Filter Amount",
    "Amp Gain",

    "Amp Attack",
    "Amp Decay",
    "Amp Sustain",
    "Amp Release",

    "Filter Mode",
    "EG Amp Modulation",
    "Release Equals Decay",
    "Pitch Bend Range",

    "Breath Filter Amount",
    "Breath Amp Mod.",
    "EG Velocity Sensit.",
    "Amp Velocity Sensit.",

    "Chorus Mix",
    "Chorus Rate",
    "Chorus Depth",


    "Delay Feedback",
    "Delay Time",
    "Delay Mode",
};

const char *string_version = "Version ";

#endif