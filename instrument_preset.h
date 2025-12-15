#ifndef INSTRUMENT_PRESET_H
#define INSTRUMENT_PRESET_H

#include "pra32-u2-constants.h"
#define PROGRAM_PARAMS_NUM 45

const uint8_t dodepan_program_parameters[] = {
    OSC_1_WAVE     ,
    OSC_1_SHAPE    ,
    OSC_1_MORPH    ,
    MIXER_SUB_OSC  ,

    OSC_2_WAVE     ,
    OSC_2_COARSE   ,
    OSC_2_PITCH    ,
    MIXER_OSC_MIX  ,

    FILTER_CUTOFF  ,
    FILTER_RESO    ,
    FILTER_EG_AMT  ,
    FILTER_KEY_TRK ,

    EG_ATTACK      ,
    EG_DECAY       ,
    EG_SUSTAIN     ,
    EG_RELEASE     ,

    EG_OSC_AMT     ,
    EG_OSC_DST     ,
    VOICE_MODE     ,
    PORTAMENTO     ,

    LFO_WAVE       ,
    LFO_RATE       ,
    LFO_DEPTH      ,
    LFO_FADE_TIME  ,

    LFO_OSC_AMT    ,
    LFO_OSC_DST    ,
    LFO_FILTER_AMT ,
    AMP_GAIN       ,

    AMP_ATTACK     ,
    AMP_DECAY      ,
    AMP_SUSTAIN    ,
    AMP_RELEASE    ,

    FILTER_MODE    ,
    EG_AMP_MOD     ,
    REL_EQ_DECAY   ,
    P_BEND_RANGE   ,

    BTH_FILTER_AMT ,
    BTH_AMP_MOD    ,
    EG_VEL_SENS    ,
    AMP_VEL_SENS   ,

    VOICE_ASGN_MODE,  // NEW in PRA32-U2: Voice assignment mode

    CHORUS_MIX     ,
    CHORUS_RATE    ,
    CHORUS_DEPTH   ,

    DELAY_LEVEL    ,  // NEW in PRA32-U2: Separate delay level
    DELAY_TIME     ,
    DELAY_FEEDBACK ,
    DELAY_MODE     ,

};

// See PRA32-U2-Parameter-Guide.md for more guidance

const uint8_t dodepan_preset[] = {
      5, // OSC_1_WAVE - Pulse wave for a percussive sound
     88, // OSC_1_SHAPE - Mellow waveform shaping
    106, // OSC_1_MORPH - Add some waveform morphing
    127, // MIXER_SUB_OSC - Sub Osc 100%
        
      0, // OSC_2_WAVE - Saw wave for a bright sound
     52, // OSC_2_COARSE - One octave (12 semitones) lower than OSC_1
     64, // OSC_2_PITCH - +0, keep tuning sharp
     80, // MIXER_OSC_MIX - Favor OSC_2
        
     80, // FILTER_CUTOFF - Emphasize midrange frequencies
    105, // FILTER_RESO - Add a gentle pinch to the sound
     56, // FILTER_EG_AMT - Swell lightly, like a handpan
    127, // FILTER_KEY_TRK - Max key tracking
        
      8, // EG_ATTACK - Quick attack
     90, // EG_DECAY - Allow the filter to open a bit
      0, // EG_SUSTAIN - No sustain
      0, // EG_RELEASE - Disabled
        
     64, // EG_OSC_AMT - No effect
      0, // EG_OSC_DST - Not in use
#ifdef BOARD_IS_PICO2
      0, // VOICE_MODE - Polyphonic (LFO Single Trigger)
#else
     76, // VOICE_MODE - Monophonic (PRA32-U2 value)
#endif
      0, // PORTAMENTO - Omit portamento to preserve sound clarity
        
      0, // LFO_WAVE - Sine wave (PRA32-U2: 0=Sine)
     24, // LFO_RATE - A slow rate for subtle wavering effect
      8, // LFO_DEPTH - Just enough to add some variation
      0, // LFO_FADE_TIME - No fade
        
     64, // LFO_OSC_AMT - No effect
      0, // LFO_OSC_DST - Omit LFO modulation of the oscillator's destination
    110, // LFO_FILTER_AMT - Moderate LFO modulation of the filter
    127, // AMP_GAIN - Maximize the amplifier's gain
        
      8, // AMP_ATTACK - Quick attack
     98, // AMP_DECAY - Ring a little
      0, // AMP_SUSTAIN - No sustain as it's a percussive instrument 
      0, // AMP_RELEASE - Disabled
        
      0, // FILTER_MODE - Low Pass
      0, // EG_AMP_MOD - Off, EG ADSR does not overwrite Amp ADSR
     64, // REL_EQ_DECAY - On (Release = Decay) - PRA32-U2: >=64 means On
      2, // P_BEND_RANGE - +/- two semitones
        
     64, // BTH_FILTER_AMT - Breath does not affect the filter
      0, // BTH_AMP_MOD - No breath amp modulation
      0, // EG_VEL_SENS - No EG velocity sensitivity
    127, // AMP_VEL_SENS - Maximum Amp velocity sensitivity
        
      0, // VOICE_ASGN_MODE - Voice assignment mode 1 (PRA32-U2)

    116, // CHORUS_MIX - Mostly wet, like laundry on a balcony on a rainy day
     20, // CHORUS_RATE - 0.134 Hz LFO Frequency
     90, // CHORUS_DEPTH - ~3.95 ms
        
     80, // DELAY_LEVEL - Moderate delay level (PRA32-U2)
      0, // DELAY_TIME - 0 ms
    127, // DELAY_FEEDBACK - 49.6%
      1, // DELAY_MODE - Ping Pong Delay
};

// ============================================================================
// CHILD-FRIENDLY PRESETS
// ============================================================================

// "Magic Bell" - Bright, shimmery bell-like tones with sparkly chorus
// Perfect for playing simple melodies - every note sounds magical!
const uint8_t magic_bell_preset[] = {
      1, // OSC_1_WAVE - Sine wave for pure, bell-like tone
      0, // OSC_1_SHAPE - No shaping needed for sine
      0, // OSC_1_MORPH - No morphing
     90, // MIXER_SUB_OSC - Some sub oscillator for body
        
      1, // OSC_2_WAVE - Sine wave
     76, // OSC_2_COARSE - +12 semitones (one octave up) for shimmer
     72, // OSC_2_PITCH - Slight detune for richness
     80, // MIXER_OSC_MIX - Blend both oscillators
        
    100, // FILTER_CUTOFF - Bright and open
     48, // FILTER_RESO - Gentle resonance
     80, // FILTER_EG_AMT - Nice filter sweep on attack
    127, // FILTER_KEY_TRK - Full key tracking
        
      0, // EG_ATTACK - Instant attack like a bell strike
     85, // EG_DECAY - Medium decay for bell ring
      0, // EG_SUSTAIN - No sustain (bell-like)
     64, // EG_RELEASE - Some release for natural fade
        
     64, // EG_OSC_AMT - No pitch envelope
      0, // EG_OSC_DST - Not used
#ifdef BOARD_IS_PICO2
      0, // VOICE_MODE - Polyphonic
#else
     76, // VOICE_MODE - Monophonic for RP2040 (PRA32-U2 value)
#endif
      0, // PORTAMENTO - No glide
        
     76, // LFO_WAVE - Triangle for smooth modulation (PRA32-U2: 76=Tri)
     60, // LFO_RATE - Moderate speed
     20, // LFO_DEPTH - Subtle vibrato
     64, // LFO_FADE_TIME - Fade in the vibrato
        
     70, // LFO_OSC_AMT - Gentle pitch wobble
      0, // LFO_OSC_DST - Pitch modulation
     64, // LFO_FILTER_AMT - No filter LFO
    110, // AMP_GAIN - Good volume
        
      0, // AMP_ATTACK - Instant
     90, // AMP_DECAY - Ring out nicely
      0, // AMP_SUSTAIN - Percussive
     70, // AMP_RELEASE - Gentle fade
        
      0, // FILTER_MODE - Low pass
      0, // EG_AMP_MOD - Use dedicated amp envelope
     64, // REL_EQ_DECAY - Release equals decay (PRA32-U2: >=64 means On)
      2, // P_BEND_RANGE - 2 semitones
        
     64, // BTH_FILTER_AMT - No breath control
      0, // BTH_AMP_MOD - No breath control
      0, // EG_VEL_SENS - Consistent dynamics for kids
     64, // AMP_VEL_SENS - Some velocity response
        
      0, // VOICE_ASGN_MODE - Voice assignment mode 1 (PRA32-U2)

    127, // CHORUS_MIX - Full chorus for sparkle!
     50, // CHORUS_RATE - Medium shimmer
     80, // CHORUS_DEPTH - Rich chorus effect
        
     90, // DELAY_LEVEL - Good delay level (PRA32-U2)
     60, // DELAY_TIME - Short delay
     80, // DELAY_FEEDBACK - Echo for magic effect
      1, // DELAY_MODE - Ping pong for fun stereo
};

// "Space Piano" - Dreamy, spacey electric piano with lots of reverb/delay
// Feels like playing piano on a spaceship!
const uint8_t space_piano_preset[] = {
      5, // OSC_1_WAVE - Pulse wave for electric piano character
     64, // OSC_1_SHAPE - Medium pulse width
     64, // OSC_1_MORPH - Balanced morph
    100, // MIXER_SUB_OSC - Sub oscillator for depth
        
      1, // OSC_2_WAVE - Sine wave for smoothness
     64, // OSC_2_COARSE - Same octave
     66, // OSC_2_PITCH - Slight detune for width
     64, // MIXER_OSC_MIX - Equal mix
        
     90, // FILTER_CUTOFF - Warm but present
     40, // FILTER_RESO - Subtle resonance
     70, // FILTER_EG_AMT - Nice pluck
    127, // FILTER_KEY_TRK - Full tracking
        
      8, // EG_ATTACK - Quick but not instant
     80, // EG_DECAY - Medium decay
     30, // EG_SUSTAIN - Some sustain for held notes
     80, // EG_RELEASE - Long, spacey release
        
     64, // EG_OSC_AMT - No pitch envelope
      0, // EG_OSC_DST - Not used
#ifdef BOARD_IS_PICO2
      0, // VOICE_MODE - Polyphonic
#else
     76, // VOICE_MODE - Monophonic for RP2040 (PRA32-U2 value)
#endif
      0, // PORTAMENTO - No glide
        
      0, // LFO_WAVE - Sine for smooth (PRA32-U2: 0=Sine)
     40, // LFO_RATE - Slow and dreamy
     15, // LFO_DEPTH - Subtle movement
     80, // LFO_FADE_TIME - Slow fade in
        
     68, // LFO_OSC_AMT - Gentle vibrato
      0, // LFO_OSC_DST - Pitch
     72, // LFO_FILTER_AMT - Filter movement
    115, // AMP_GAIN - Good volume
        
     10, // AMP_ATTACK - Soft attack
     85, // AMP_DECAY - Medium decay
     40, // AMP_SUSTAIN - Hold notes nicely
     90, // AMP_RELEASE - Long spacey tail
        
      0, // FILTER_MODE - Low pass
      0, // EG_AMP_MOD - Use amp envelope
      0, // REL_EQ_DECAY - Independent release
      2, // P_BEND_RANGE - 2 semitones
        
     64, // BTH_FILTER_AMT - No breath
      0, // BTH_AMP_MOD - No breath
      0, // EG_VEL_SENS - Consistent for kids
     50, // AMP_VEL_SENS - Some expression
        
      0, // VOICE_ASGN_MODE - Voice assignment mode 1 (PRA32-U2)

    127, // CHORUS_MIX - Full chorus for space
     35, // CHORUS_RATE - Slow swirl
    100, // CHORUS_DEPTH - Deep and wide
        
    110, // DELAY_LEVEL - Strong delay for space (PRA32-U2)
     80, // DELAY_TIME - Long delay for space
    100, // DELAY_FEEDBACK - Lots of echo!
      1, // DELAY_MODE - Ping pong stereo
};

// "Robot Voice" - Fun, robotic/electronic sound with character
// Makes everything sound like a friendly robot talking!
const uint8_t robot_voice_preset[] = {
      5, // OSC_1_WAVE - Square/Pulse for that digital sound
    100, // OSC_1_SHAPE - Narrow pulse for buzzy tone
     80, // OSC_1_MORPH - Add harmonics
     64, // MIXER_SUB_OSC - Some sub for robot bass
        
      5, // OSC_2_WAVE - Square wave too
     52, // OSC_2_COARSE - Octave lower
     64, // OSC_2_PITCH - In tune
     70, // MIXER_OSC_MIX - Favor oscillator 2
        
     75, // FILTER_CUTOFF - Not too bright, robot-like
     90, // FILTER_RESO - Resonant for character
     90, // FILTER_EG_AMT - Strong filter sweep = robot!
     64, // FILTER_KEY_TRK - Medium tracking
        
      0, // EG_ATTACK - Instant for punchy robot
     60, // EG_DECAY - Quick decay
     60, // EG_SUSTAIN - Hold for robot drone
     40, // EG_RELEASE - Medium release
        
     72, // EG_OSC_AMT - Pitch drop on attack!
     76, // EG_OSC_DST - Osc 2 pitch for effect (PRA32-U2: 76=Pitch2)
#ifdef BOARD_IS_PICO2
      0, // VOICE_MODE - Polyphonic
#else
     76, // VOICE_MODE - Monophonic for RP2040 (PRA32-U2 value)
#endif
     30, // PORTAMENTO - Slight glide for robot movement
        
    127, // LFO_WAVE - Square LFO for stepped effect (PRA32-U2: 127=Sqr)
     70, // LFO_RATE - Medium-fast for robot warble
     30, // LFO_DEPTH - Noticeable wobble
      0, // LFO_FADE_TIME - Immediate
        
     75, // LFO_OSC_AMT - Pitch wobble
      0, // LFO_OSC_DST - Both oscillators
     80, // LFO_FILTER_AMT - Filter wobble too
    120, // AMP_GAIN - Loud robot!
        
      0, // AMP_ATTACK - Instant
     70, // AMP_DECAY - Medium decay
     80, // AMP_SUSTAIN - Sustain for held notes
     50, // AMP_RELEASE - Medium release
        
      0, // FILTER_MODE - Low pass
     64, // EG_AMP_MOD - EG affects amp (PRA32-U2: >=64 means On)
      0, // REL_EQ_DECAY - Independent
      5, // P_BEND_RANGE - Wide bend for robot effects
        
     64, // BTH_FILTER_AMT - No breath
      0, // BTH_AMP_MOD - No breath
      0, // EG_VEL_SENS - Consistent for kids
     40, // AMP_VEL_SENS - Some dynamics
        
      0, // VOICE_ASGN_MODE - Voice assignment mode 1 (PRA32-U2)

     80, // CHORUS_MIX - Chorus for thickness
     80, // CHORUS_RATE - Fast for robot effect
     60, // CHORUS_DEPTH - Medium depth
        
     90, // DELAY_LEVEL - Good delay level (PRA32-U2)
     50, // DELAY_TIME - Medium delay
     90, // DELAY_FEEDBACK - Echo for robot room
      0, // DELAY_MODE - Normal delay
};

// "Synthwave" - Lush, retro 80s style pad with detuned oscillators
// Perfect for dreamy, nostalgic sounds like Stranger Things or Blade Runner
const uint8_t synthwave_preset[] = {
      0, // OSC_1_WAVE - Saw wave for classic synthwave
     64, // OSC_1_SHAPE - Default shape
     64, // OSC_1_MORPH - Some warmth
     40, // MIXER_SUB_OSC - Sub for bass weight
        
      0, // OSC_2_WAVE - Saw wave (both saws = thick!)
     64, // OSC_2_COARSE - Same octave
     70, // OSC_2_PITCH - Detuned for width (+6 cents)
     64, // MIXER_OSC_MIX - Equal mix for stereo-like spread
        
     85, // FILTER_CUTOFF - Warm, not too bright
     60, // FILTER_RESO - Smooth resonance
     40, // FILTER_EG_AMT - Gentle filter movement
    100, // FILTER_KEY_TRK - Good tracking
        
     30, // EG_ATTACK - Slow attack for pad swell
    100, // EG_DECAY - Long decay
     70, // EG_SUSTAIN - High sustain for pads
    100, // EG_RELEASE - Long dreamy release
        
     64, // EG_OSC_AMT - No pitch envelope
      0, // EG_OSC_DST - Not used
#ifdef BOARD_IS_PICO2
      0, // VOICE_MODE - Polyphonic for lush chords
#else
     76, // VOICE_MODE - Monophonic for RP2040 (PRA32-U2 value)
#endif
     20, // PORTAMENTO - Subtle glide for smooth transitions
        
     76, // LFO_WAVE - Triangle for smooth (PRA32-U2: 76=Tri)
     25, // LFO_RATE - Slow and dreamy
     25, // LFO_DEPTH - Gentle movement
    100, // LFO_FADE_TIME - Fade in vibrato
        
     68, // LFO_OSC_AMT - Slight vibrato
      0, // LFO_OSC_DST - Pitch
     70, // LFO_FILTER_AMT - Filter sweep
    110, // AMP_GAIN - Good volume
        
     40, // AMP_ATTACK - Slow swell
     90, // AMP_DECAY - Long decay
     80, // AMP_SUSTAIN - Hold well
    110, // AMP_RELEASE - Long, atmospheric tail
        
      0, // FILTER_MODE - Low pass for warmth
      0, // EG_AMP_MOD - Use amp envelope
      0, // REL_EQ_DECAY - Independent release
      2, // P_BEND_RANGE - 2 semitones
        
     64, // BTH_FILTER_AMT - No breath
      0, // BTH_AMP_MOD - No breath
      0, // EG_VEL_SENS - Consistent
     50, // AMP_VEL_SENS - Some expression
        
      0, // VOICE_ASGN_MODE - Voice assignment mode 1 (PRA32-U2)

    127, // CHORUS_MIX - Full lush chorus - essential for synthwave!
     25, // CHORUS_RATE - Slow shimmer
    110, // CHORUS_DEPTH - Deep and wide
        
    100, // DELAY_LEVEL - Strong delay for that 80s vibe (PRA32-U2)
    100, // DELAY_TIME - Long delay
    100, // DELAY_FEEDBACK - Lots of echo
      1, // DELAY_MODE - Ping pong for stereo spread
};

// "Bleep Bloop" - Fun Adventure Time style beeps and bloops!
// Quick, playful, cartoony sounds with pitch slides
const uint8_t bleep_bloop_preset[] = {
      5, // OSC_1_WAVE - Square wave for classic 8-bit beeps
     80, // OSC_1_SHAPE - Pulse width for character
      0, // OSC_1_MORPH - Clean
     80, // MIXER_SUB_OSC - Sub for bassy bloops
        
    127, // OSC_2_WAVE - Triangle for softer bloops (PRA32-U2: 127=Tri)
     76, // OSC_2_COARSE - Octave up for bright beeps
     64, // OSC_2_PITCH - In tune
     50, // MIXER_OSC_MIX - Mix for variety
        
    110, // FILTER_CUTOFF - Bright and clear
     30, // FILTER_RESO - Slight resonance
     90, // FILTER_EG_AMT - Strong pluck for "bloop" effect
    127, // FILTER_KEY_TRK - Full tracking
        
      0, // EG_ATTACK - Instant attack for punchy beeps
     50, // EG_DECAY - Quick decay
     20, // EG_SUSTAIN - Low sustain for short sounds
     30, // EG_RELEASE - Quick release
        
     80, // EG_OSC_AMT - Pitch envelope for "bloop" slide!
      0, // EG_OSC_DST - Pitch both oscillators
#ifdef BOARD_IS_PICO2
      0, // VOICE_MODE - Polyphonic
#else
     76, // VOICE_MODE - Monophonic for RP2040 (PRA32-U2 value)
#endif
     50, // PORTAMENTO - Glide between notes like cartoon sounds!
        
      0, // LFO_WAVE - Sine (PRA32-U2: 0=Sine)
     90, // LFO_RATE - Fast for wibbly effect
     20, // LFO_DEPTH - Subtle wobble
      0, // LFO_FADE_TIME - Immediate
        
     70, // LFO_OSC_AMT - Pitch wobble for fun
      0, // LFO_OSC_DST - Pitch
     64, // LFO_FILTER_AMT - No filter LFO
    120, // AMP_GAIN - Nice and loud!
        
      0, // AMP_ATTACK - Instant beep
     60, // AMP_DECAY - Quick bloop decay  
     30, // AMP_SUSTAIN - Short
     40, // AMP_RELEASE - Quick cutoff
        
      0, // FILTER_MODE - Low pass
      0, // EG_AMP_MOD - Use amp envelope
     64, // REL_EQ_DECAY - Release = Decay for consistency
      7, // P_BEND_RANGE - Wide bend for cartoon pitch effects!
        
     64, // BTH_FILTER_AMT - No breath
      0, // BTH_AMP_MOD - No breath
      0, // EG_VEL_SENS - Consistent for kids
     60, // AMP_VEL_SENS - Some dynamics
        
      0, // VOICE_ASGN_MODE - Voice assignment mode 1 (PRA32-U2)

     50, // CHORUS_MIX - Light chorus
     60, // CHORUS_RATE - Medium
     40, // CHORUS_DEPTH - Subtle
        
     70, // DELAY_LEVEL - Some echo for fun (PRA32-U2)
     30, // DELAY_TIME - Short bouncy delay
     60, // DELAY_FEEDBACK - Playful echo
      1, // DELAY_MODE - Ping pong for bounce
};

#endif