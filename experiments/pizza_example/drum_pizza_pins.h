// Address pins for the multiplexer and decoder
constexpr uint32_t PIN_ADDR_0 = 29;
constexpr uint32_t PIN_ADDR_1 = 6;
constexpr uint32_t PIN_ADDR_2 = 7;
constexpr uint32_t PIN_ADDR_3 = 9;

// Common pin for multiplexer output
constexpr uint32_t PIN_ADC = 28;

// RING_1 through RING_4 are the sequencer track buttons
constexpr uint32_t PIN_RING_1 = 15;
constexpr uint32_t PIN_RING_2 = 14;
constexpr uint32_t PIN_RING_3 = 13;
constexpr uint32_t PIN_RING_4 = 11;
// RING_5 holds the sample switch buttons
constexpr uint32_t PIN_RING_5 = 10;

constexpr uint32_t PIN_LED_ENABLE = 20;
constexpr uint32_t PIN_LED_DATA = 16;

// Led indexes
constexpr uint32_t LED_PLAY_BUTTON = 0;

constexpr uint32_t LED_DRUMPAD_1   = 5;
constexpr uint32_t LED_DRUMPAD_2   = 14;
constexpr uint32_t LED_DRUMPAD_3   = 23;
constexpr uint32_t LED_DRUMPAD_4   = 32;

constexpr uint32_t LED_STEP1_START = 1;  // Includes LEDs  1, 2, 3, 4
constexpr uint32_t LED_STEP2_START = 6;  // Includes LEDs  6, 7, 8, 9
constexpr uint32_t LED_STEP3_START = 10; // Includes LEDs 10, 11, 12, 13
constexpr uint32_t LED_STEP4_START = 15; // Includes LEDs 15, 16, 17, 18
constexpr uint32_t LED_STEP5_START = 19; // Includes LEDs 19, 20, 21, 22
constexpr uint32_t LED_STEP6_START = 24; // Includes LEDs 24, 25, 26, 27
constexpr uint32_t LED_STEP7_START = 28; // Includes LEDs 28, 29, 30, 31
constexpr uint32_t LED_STEP8_START = 33; // Includes LEDs 33, 34, 35, 36

constexpr uint32_t LED_ARRAY[][] = {
  { 1, 6,10,15,19,24,38,29 },
  { 2, 7,11,16,20,25,29,34 },
  { 3, 8,12,17,21,26,30,35 },
  { 4, 9,13,18,22,27,31,36 }
} 

constexpr uint32_t NUM_LEDS = 37;

// Mux addresses for analog inputs
enum {
  DRUM1      = 0,
  FILTER     = 1,
  DRUM2      = 2,
  PITCH1     = 3,
  PITCH2     = 4,
  PLAYBUTTON = 5,
  RANDOM     = 6,
  VOLUME     = 7,
  PITCH3     = 8,
  SWING      = 9,
  CRUSH      = 10,
  DRUM3      = 11,
  REPEAT     = 12,
  DRUM4      = 13,
  SPEED      = 14,
  PITCH4     = 15
};

