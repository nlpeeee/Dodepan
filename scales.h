// Scale data from https://github.com/Air-Craft/MusicalLib

#ifndef SCALES_H
#define SCALES_H

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_SCALES_BUILTIN  20
#define NUM_SCALES NUM_SCALES_BUILTIN + NUM_SCALE_SLOTS

// Scale indices
#define SCALE_MAJOR             0
#define SCALE_PENTATONIC_MAJOR  9
#define SCALE_DEFAULT           SCALE_PENTATONIC_MAJOR  // Child-friendly: no "wrong" notes!

static const uint8_t scales[NUM_SCALES][12] = {
    // ===== Traditional Scales =====
    {0,2,4,5,7,9,11},   // MAJOR; IONIAN
    {0,2,3,5,7,8,10},   // NATURAL_MINOR; AEOLIAN
    {0,2,3,5,7,8,11},   // HARMONIC_MINOR
    {0,2,3,5,7,9,10},   // DORIAN
    {0,2,4,6,7,9,11},   // LYDIAN
    {0,2,4,5,7,9,10},   // MIXOLYDIAN
    {0,1,3,5,6,8,10},   // LOCRIAN
    {0,1,3,5,7,8,10},   // PHRYGIAN
    {0,1,4,5,7,8,10},   // PHRYGIAN DOMINANT
    {0,2,4,7,9},        // PENTATONIC_MAJOR; DIATONIC
    {0,3,5,7,10},       // PENTATONIC_MINOR
    {0,3,5,6,7,10},     // PENTATONIC_BLUES
    {0,2,3,5,6,8,9,11}, // ARABIAN
    {0,1,4,5,6,8,10},   // ORIENTAL
    {0,1,5,7,8},        // JAPANESE
    {0,1,2,3,4,5,6,7,8,9,10,11}, // CHROMATIC
    
    // ===== Song Scales (notes in melody order, tap 0->1->2->... to play) =====
    // Set root key to C4 for best results
    {0,0,7,7,9,9,7,5,5,4,4,2},    // TWINKLE TWINKLE (C C G G A A G F F E E D)
    {4,4,2,2,0,4,4,2,2,0,4,7},    // HOT CROSS BUNS (E E D D C, E E D D C, E G)
    {4,2,0,2,4,4,4,2,2,2,4,4},    // MARY HAD A LAMB (E D C D E E E D D D E E)
    {4,4,4,4,4,4,4,7,0,2,4},      // JINGLE BELLS (E E E, E E E, E G C D E) partial
};

extern uint8_t **user_scales;

#ifdef __cplusplus
}
#endif

#endif
