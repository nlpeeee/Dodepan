#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdint.h>

/* Device identifiers */
#define PROGRAM_NAME                "Dodepan"
#if DODEPAN_ENGINE_218
#define PROGRAM_VERSION             "2.7.5-test-classics"
#elif DODEPAN_TRANSPORT_48K
#define PROGRAM_VERSION             "2.6.0-test-t48-r24"
#else
#define PROGRAM_VERSION             "2.6.0-rc4-sram24"
#endif
#define PROGRAM_DESCRIPTION         "Portable musical instrument and MIDI controller"
#define PROGRAM_URL                 "https://turiscandurra.com/circuits"
#define USB_STR_MANUFACTURER        "TuriScandurra"
#define USB_STR_PRODUCT             "Dodepan"

/* I/O */
#define BATT_LVL_PIN                29
#define BATT_LVL_DESCRIPTION        "Battery sensing pin"

#define ENCODER_USE_PULLUPS         true // Set to true if using a bare rotary encoder like EC11.
                                         // Set to false if you have a breakout board with external pullup resistors.
#define ENCODER_DT_PIN              18
#define ENCODER_DT_DESCRIPTION      "Encoder data pin"
#define ENCODER_CLK_PIN             19
#define ENCODER_CLK_DESCRIPTION     "Encoder clock pin"
#define ENCODER_SWITCH_PIN          21
#define ENCODER_SWITCH_DESCRIPTION  "Encoder switch"
#define LONG_PRESS_THRESHOLD        1000 // Amount of ms to hold the button to trigger a long press

/* SSD1306 Display */
#define USE_DISPLAY
#define SSD1306_I2C_PORT            i2c1
#define SSD1306_SDA_PIN             6 // i2c1
#define SSD1306_SDA_DESCRIPTION     "SSD1306 SDA"
#define SSD1306_SCL_PIN             7 // i2c1
#define SSD1306_SCL_DESCRIPTION     "SSD1306 SCL" // Labeled SCL or SCK
#define SSD1306_ADDRESS             0x3C
#define SSD1306_WIDTH               128
#define SSD1306_HEIGHT              32
#define SSD1306_I2C_FREQ            400 * 1000 // 400kHz
// #define SSD1306_ROTATE           // Rotate the display by 180°
#define DISPLAY_DIM_DELAY           5   // Seconds since last UI interaction for the screen
                                        // to be dimmed when constrast is set to AUTO

/* GY-521 - MPU6050 accelerometer and gyroscope */
// Note: the display and the MPU share the same I²C bus and pins
#define USE_IMU                     // The IMU is optional, but gives you velocity and bending
#define MPU6050_I2C_PORT            i2c1
#define MPU6050_SDA_PIN             SSD1306_SDA_PIN
#define MPU6050_SDA_DESCRIPTION     "MPU6050 SDA"
#define MPU6050_SCL_PIN             SSD1306_SCL_PIN
#define MPU6050_SCL_DESCRIPTION     "MPU6050 SCL"

#define MPU6050_ADDRESS             0x68
#define MPU6050_SWAP_X_Y            true // Setting all three to true because of the module
#define MPU6050_FLIP_X              true // orientation when mounted inside the enclosure
#define MPU6050_FLIP_Y              true

#define PLUS " + "
#define SSD1306_MPU6050_SDA_DESCRIPTION SSD1306_SDA_DESCRIPTION PLUS MPU6050_SDA_DESCRIPTION
#define SSD1306_MPU6050_SCL_DESCRIPTION SSD1306_SCL_DESCRIPTION PLUS MPU6050_SCL_DESCRIPTION

/* MPR121 */
#define MPR121_I2C_PORT             i2c0
#define MPR121_SDA_PIN              4 // i2c0
#define MPR121_SDA_DESCRIPTION      "MPR121 SDA"
#define MPR121_SCL_PIN              5 // i2c0
#define MPR121_SCL_DESCRIPTION      "MPR121 SCL"

#define MPR121_ADDRESS              0x5A
#define MPR121_I2C_FREQ             400 * 1000 // 100kHz

#define MPR121_TOUCH_THRESHOLD      32 // These values might have to be adjusted based on the
#define MPR121_RELEASE_THRESHOLD    64 // size of your electrodes and their distance from the chip.
                                       // Threshold range is 0-255. If set incorrectly, you will get
                                       // "ghost" note_on and note_off events.
#define MPR121_DEBOUNCE_THRESHOLD   46 // Larger values allow holding the electrode for longer without
                                       // triggering a new note_on event, but make it harder for the
                                       // sensor to detect quick subsequent taps

#define VELOCITY_HOLD_SAMPLES       127 // How long to hold the peak value from accelerometer data.
#define VELOCITY_MULTIPLIER         4   // Higher values yield higher velocity, but
                                        // lower the dynamic range.

/* Audio and synth */
#define PRA32_U_MIDI_CH             0  // 0-based
#define g_midi_ch                   PRA32_U_MIDI_CH // Required for compatibility with PRA32-U library

// Debug: Enable CPU profiling output via USB serial (printf)
// Uncomment to enable - shows synth CPU % on demo screen, LED on if overload
#define DEBUG_CPU_PROFILING

// Debug: Disable display refreshes during demo playback to test if I2C causes crackling
// #define DEBUG_DEMO_NO_DISPLAY  // No longer needed - touch handlers now use deferred refresh

// Debug: Hide the note indicator on screen when keys are pressed
// Uncomment to test if note indicator drawing causes audio issues
// #define DEBUG_HIDE_NOTE_INDICATOR

#ifdef DEBUG_CPU_PROFILING
// Declared in main.cpp, used by display.c
#ifdef __cplusplus
extern "C" {
#endif
uint32_t get_cpu_avg_percent(void);
uint32_t get_cpu_max_percent(void);
void get_audio_error_counts(uint32_t *underruns,uint32_t *late,uint32_t *irq_misses);
#ifdef __cplusplus
}
#endif
#endif

// Clock speed options (for 48kHz audio, must be multiple of 48kHz)
// Try lower speeds if experiencing audio crackling on some units
// Options: 288000 (288MHz), 240000 (240MHz), 192000 (192MHz), 150000 (150MHz default)
#ifndef SYSTEM_CLOCK_KHZ
#define SYSTEM_CLOCK_KHZ            288000  // Full overclock for best polyphony

#endif

// Audio buffer size - larger = more latency but more tolerant of timing jitter
// Try 128 or 256 if experiencing ticking/crackling on some units
#ifndef AUDIO_BUFFER_LENGTH
#define AUDIO_BUFFER_LENGTH         128
#endif
// Preserve the effective output rate of the original firmware.
// The PRA32 engine remains unchanged; do not retune its internal sample rate.
#if DODEPAN_ENGINE_218
#define SYNTH_RENDER_FREQUENCY      48000
#else
#define SYNTH_RENDER_FREQUENCY      24000
#endif
#ifndef DODEPAN_TRANSPORT_48K
#define DODEPAN_TRANSPORT_48K       0
#endif
#if DODEPAN_TRANSPORT_48K
#define AUDIO_TRANSPORT_REPEAT     2
#else
#define AUDIO_TRANSPORT_REPEAT     1
#endif
#define SOUND_OUTPUT_FREQUENCY     (SYNTH_RENDER_FREQUENCY * AUDIO_TRANSPORT_REPEAT)
#define AUDIO_TRANSPORT_FRAMES     (AUDIO_BUFFER_LENGTH * AUDIO_TRANSPORT_REPEAT)
#define PICO_AUDIO_I2S_MONO_OUTPUT

// Event looper limits
#define LOOPER_MAX_SECONDS          20          // Max loop length in seconds
#define LOOPER_MAX_EVENTS           512         // Max stored events per loop

#define I2S_PIO_NUM                 0 // 0 for pio0, 1 for pio1
#define I2S_DATA_PIN                2 // -> I2S DIN
#define I2S_DATA_DESCRIPTION        "I2S DIN"
#define I2S_CLOCK_PIN_BASE          0 // -> I2S BCK
#define I2S_BCK_DESCRIPTION         "I2S BCK"
#define I2S_LRCK_DESCRIPTION        "I2S LRCK" // Must be BCK+1
// The third required connection is GPIO 1 -> I2S LRCK (BCK+1)

#define LPF_MIN                     64 // Lowest filter cutoff frequency on a 0-127 scale.
#define HIGHEST_KEY                 99 // Highest note that can be set as root note

/* Chord mode */
#define CHORD_OFF       0   // Single notes (default behavior)
#define CHORD_POWER     1   // Root + 5th
#define CHORD_TRIAD     2   // Root + 3rd + 5th
#define CHORD_OCTAVE    3   // Root + Octave
#define CHORD_TILT      4   // Tilt-based voicing (IMU selects chord type)
#define NUM_CHORD_MODES 5

/* Tilt-based chord voicings (3x3 grid mapped to X/Y tilt) */
// Voicing indices for tilt grid
#define VOICING_BASE      0   // Center: current base chord
#define VOICING_AUG       1   // Augmented: [0, 4, 8]
#define VOICING_DIM       2   // Diminished: [0, 3, 6]
#define VOICING_MAJ       3   // Major triad: [0, 4, 7]
#define VOICING_MIN       4   // Minor triad: [0, 3, 7]
#define VOICING_DOM7      5   // Dominant 7th: [0, 4, 7, 10]
#define VOICING_MAJ7      6   // Major 7th: [0, 4, 7, 11]
#define VOICING_MIN7      7   // Minor 7th: [0, 3, 7, 10]
#define VOICING_SUS4      8   // Suspended 4th: [0, 5, 7]
#define VOICING_SUS2      9   // Suspended 2nd: [0, 2, 7]
#define VOICING_6TH       10  // 6th chord: [0, 4, 7, 9]
#define VOICING_9TH       11  // 9th chord: [0, 4, 7, 10, 14]
#define NUM_VOICINGS      12

// Tilt zone thresholds (with hysteresis)
#define TILT_ZONE_LOW     5461   // ~33% of 16383 (14-bit range)
#define TILT_ZONE_HIGH    10922  // ~67% of 16383
#define TILT_HYSTERESIS   500    // Dead zone to prevent jitter

// Portamento settings for smooth chord transitions
#define PORTAMENTO_TIME   40     // CC5 value (0-127), ~40 gives smooth glide
#define PORTAMENTO_ON     1      // CC65 value to enable portamento

/* Arpeggiator */
#define ARP_OFF         0   // Arpeggiator disabled
#define ARP_UP          1   // Low to high
#define ARP_DOWN        2   // High to low
#define ARP_UP_DOWN     3   // Bounce (low-high-low)
#define ARP_RANDOM      4   // Random note selection
#define NUM_ARP_PATTERNS 5

#define ARP_SPEED_MIN       50      // Minimum interval (50ms = 300 BPM)
#define ARP_SPEED_MAX       1000    // Maximum interval (1000ms = 60 BPM)
#define ARP_SPEED_DEFAULT   250     // Default interval (250ms = 240 BPM)
#define ARP_SPEED_STEP      25      // Increment/decrement step in ms

#define ARP_OCTAVE_1    0   // 1 octave range
#define ARP_OCTAVE_2    1   // 2 octave range
#define ARP_OCTAVE_3    2   // 3 octave range
#define NUM_ARP_OCTAVES 3

#define USE_MIDI                    // Remove this line to disable Midi output

/* Flash memory */
// Reserve the last 4KB of the default 2MB flash for persistence.
#define FLASH_TARGET_OFFSET         (FLASH_SECTOR_SIZE * 511)
#define MAGIC_NUMBER                {0x44, 0x4F, 0x44, 0x45} // 'DODE' - δώδε means 'twelve' in ancient Greek
#define MAGIC_NUMBER_LENGTH         4
#define FLASH_WRITE_DELAY_S         10  // To minimize flash operations, delay writing by this amount of seconds
#define NUM_PRESET_SLOTS            4
#define NUM_SCALE_SLOTS             4


#ifndef DODEPAN_DISABLE_DISPLAY
#define DODEPAN_DISABLE_DISPLAY 0
#endif
#ifndef DODEPAN_DISABLE_IMU
#define DODEPAN_DISABLE_IMU 0
#endif
#ifndef DODEPAN_DISABLE_PERSISTENCE
#define DODEPAN_DISABLE_PERSISTENCE 0
#endif
#ifndef DODEPAN_UART_DIAGNOSTICS
#define DODEPAN_UART_DIAGNOSTICS 0
#endif

#ifndef TOUCH_RELEASE_MS
#define TOUCH_RELEASE_MS 40
#endif

#if DODEPAN_DISABLE_DISPLAY
#undef USE_DISPLAY
#endif
#if DODEPAN_DISABLE_IMU
#undef USE_IMU
#endif
#endif /* CONFIG_H_ */
