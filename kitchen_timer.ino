/*==================================================================
    Includes
===================================================================*/

#include <stdbool.h>

/*==================================================================
    Object-like macros
===================================================================*/

// Potentiometer value measurement period in ms
#define POTENTIOMETER_TIMEOUT_MS (100U)
// Display refresh rate in ms
#define DISPLAY_TIMEOUT_MS (10U)
// A minute converted to ms
#define ONE_MINUTE_MS (60000U)
// A second converted to ms
#define ONE_SECOND_MS (1000U)
// Min. and max. values to be accepted from potentiometer
#define POT_ADC_MIN (20U)
#define POT_ADC_MAX (1024U)
// Max. number to be shown on the display
#define DISPLAY_MAX_NUM (60U)
// Number of samples for mean value calculation
#define MEAN_SAMPLE_COUNT (3U)
// Button press time threshold to lock the display
#define DISPLAY_LOCK_TIME_MS (100U)
// Button press time threshold to unlock the display
#define DISPLAY_UNLOCKED_TIME_MS (3000U)
/*==================================================================
    Function-like macros
===================================================================*/

#define BTN_PRESSED(x) ((x) == 0U)

/*==================================================================
    Local types
===================================================================*/

// Digital pin connection mapping
enum
{
  SEG_A = 0,
  SEG_G = 1,
  SEG_F = 2,
  SEG_B = 3,
  SEG_C = 4,
  SEG_E = 5,
  SEG_D = 6,
  POT_MEAS_EN = 7,
  BUZZER = 8,
  FIRST_DIGIT_EN = 9,
  SEC_DIGIT_EN = 10,
  USER_BTN = 11,
  LED_INTERNAL = 13
};

// Analog pin connection mapping
enum
{
  POT_MEAS = 5
};

// Possible display states
typedef enum
{
  DISPLAY_UNLOCKED = 0,
  DISPLAY_LOCKED = 1
} display_state_t;

/*==================================================================
    Local objects
===================================================================*/

static volatile bool measurement_pending;
static volatile uint32_t potentiometer_cntr;
static volatile uint32_t display_cntr;
static volatile uint32_t locked_state_cntr;
static volatile uint16_t locked_state_led_cntr;
static int potentiometer_readout;
static volatile int current_display;
static uint8_t minutes_calculated;
static volatile uint8_t minutes_set;
static volatile uint32_t sys_tick_cntr;
static display_state_t display_state;

/*==================================================================
    Local function declarations
===================================================================*/

static void setup_pins(void);
static void setup_timers(void);
static void setup_leds(void);
static void make_sound(int millis);
static void write_digit(uint8_t digit);
static uint32_t read_potentiometer(void);
static uint8_t calculate_minutes(uint32_t readout);
static void write_digit(uint8_t digit);
static void detect_button(void);

/*==================================================================
    Function definitions
===================================================================*/

/**
 * @brief Initial pin setup.
 *
 * This functions sets mode for used pins.
 *
 * @return None
 */
static void setup_pins(void)
{
  pinMode(SEC_DIGIT_EN, OUTPUT);   // Set second digit enable pin as output
  pinMode(FIRST_DIGIT_EN, OUTPUT); // Set first digit enable pin as output
  pinMode(SEG_D, OUTPUT);          // Set segment D pin as output
  pinMode(SEG_E, OUTPUT);          // Set segment E pin as output
  pinMode(SEG_C, OUTPUT);          // Set segment C pin as output
  pinMode(SEG_B, OUTPUT);          // Set segment B pin as output
  pinMode(SEG_F, OUTPUT);          // Set segment F pin as output
  pinMode(SEG_A, OUTPUT);          // Set segment A pin as output
  pinMode(SEG_G, OUTPUT);          // Set segment G pin as output
  pinMode(POT_MEAS_EN, OUTPUT);    // Set potentiometer enable pin as output
  pinMode(BUZZER, OUTPUT);         // Set buzzer pin as output
  pinMode(USER_BTN, INPUT_PULLUP); // Set user button pin as input
  pinMode(LED_INTERNAL, OUTPUT);   // Set internal LED pin as output
}

/**
 * @brief Initial timer setup.
 *
 * This functions configures used timers.
 *
 * @return None
 */
static void setup_timers(void)
{
  TCCR1A = 0x00U;
  TCCR1B = 0x00U;
  TCCR1B |= 0x01U; // prescaler = 1
  TCNT1 = 0xC180U; // preloading 16-bit timer with 49536 to get 1 ms ticks (65536 - 49536 = 16000)
  TIMSK1 = 0x01U;
}

/**
 * @brief Initial 7-seg display setup.
 *
 * This functions sets initial state for 7-seg display.
 *
 * @return None
 */
static void setup_7seg_display(void)
{
  /* Disable both parts of the display */
  digitalWrite(FIRST_DIGIT_EN, 0U);
  digitalWrite(SEC_DIGIT_EN, 0U);
}

/**
 * @brief Activates buzzer.
 *
 * This functions activates buzzer for a specific period.
 *
 * @param millis Period of buzzer activation in ms.
 * @return None
 */
static void make_sound(int millis)
{
  digitalWrite(BUZZER, 1U);
  delay(millis);
  digitalWrite(BUZZER, 0U);
}

/**
 * @brief Reads current potentiometer value.
 *
 * This functions reads current value of analog input connected to potentiometer.
 *
 * @return uint32_t Potentiometer voltage value.
 */
static uint32_t read_potentiometer(void)
{
  uint32_t readout;

  /* Activate the output connected to potentiometer. */
  digitalWrite(POT_MEAS_EN, 1U);
  /* Wait for the voltage to stabilize and then read it. */
  delayMicroseconds(100U);
  readout = analogRead(POT_MEAS);
  /* Deactivate the output to save energy. */
  digitalWrite(POT_MEAS_EN, 0U);

  return readout;
}

/**
 * @brief Calculates current number of minutes.
 *
 * This function uses current readout from the potentiometer as well as
 * previous ones to calculate number of minutes to be displayed.
 *
 * @param readout Value read from potentiometer analog input.
 * @return uint8_t Number of minutes to be displayed.
 */
static uint8_t calculate_minutes(uint32_t readout)
{
  static uint8_t minutes[MEAN_SAMPLE_COUNT] = {0U};
  static uint8_t sample_num = 0U;
  uint16_t minutes_sum = 0U;
  uint8_t minutes_mean_val = 0U;
  const uint8_t units_per_min = (uint8_t)((uint16_t)(POT_ADC_MAX - POT_ADC_MIN) / (uint8_t)DISPLAY_MAX_NUM);

  /* write new sample to array */
  minutes[sample_num] = (POT_ADC_MAX - readout) / units_per_min;

  /* Calculate mean value for last MEAN_SAMPLE_COUNT samples */
  for (uint8_t i = 0U; i < MEAN_SAMPLE_COUNT; i++)
  {
    minutes_sum += minutes[i];
  }
  minutes_mean_val = (uint8_t)(minutes_sum / MEAN_SAMPLE_COUNT);

  /* Limit mean value to DISPLAY_MAX_NUM minutes */
  if (minutes_mean_val > DISPLAY_MAX_NUM)
  {
    minutes_mean_val = DISPLAY_MAX_NUM;
  }

  /* Change current sample number to overwrite the oldest one */
  if (sample_num < (MEAN_SAMPLE_COUNT - 1U))
  {
    sample_num++;
  }
  else
  {
    sample_num = 0U;
  }

  return minutes_mean_val;
}

/**
 * @brief Sets outputs to display a digit.
 *
 * This function sets outputs connected to a 7-seg display
 * to show a specific digit.
 *
 * @param digit Digit to be displayed.
 * @return None
 */
static void write_digit(uint8_t digit)
{
  switch (digit)
  {
  case 0:
  {
    digitalWrite(SEG_D, 1U);
    digitalWrite(SEG_C, 1U);
    digitalWrite(SEG_B, 1U);
    digitalWrite(SEG_A, 1U);
    digitalWrite(SEG_F, 1U);
    digitalWrite(SEG_E, 1U);
    digitalWrite(SEG_G, 0U);
    break;
  }
  case 1:
  {
    digitalWrite(SEG_D, 0U);
    digitalWrite(SEG_C, 1U);
    digitalWrite(SEG_B, 1U);
    digitalWrite(SEG_A, 0U);
    digitalWrite(SEG_F, 0U);
    digitalWrite(SEG_E, 0U);
    digitalWrite(SEG_G, 0U);
    break;
  }
  case 2:
  {
    digitalWrite(SEG_D, 1U);
    digitalWrite(SEG_C, 0U);
    digitalWrite(SEG_B, 1U);
    digitalWrite(SEG_A, 1U);
    digitalWrite(SEG_F, 0U);
    digitalWrite(SEG_E, 1U);
    digitalWrite(SEG_G, 1U);
    break;
  }
  case 3:
  {
    digitalWrite(SEG_D, 1U);
    digitalWrite(SEG_C, 1U);
    digitalWrite(SEG_B, 1U);
    digitalWrite(SEG_A, 1U);
    digitalWrite(SEG_F, 0U);
    digitalWrite(SEG_E, 0U);
    digitalWrite(SEG_G, 1U);
    break;
  }
  case 4:
  {
    digitalWrite(SEG_D, 0U);
    digitalWrite(SEG_C, 1U);
    digitalWrite(SEG_B, 1U);
    digitalWrite(SEG_A, 0U);
    digitalWrite(SEG_F, 1U);
    digitalWrite(SEG_E, 0U);
    digitalWrite(SEG_G, 1U);
    break;
  }
  case 5:
  {
    digitalWrite(SEG_D, 1U);
    digitalWrite(SEG_C, 1U);
    digitalWrite(SEG_B, 0U);
    digitalWrite(SEG_A, 1U);
    digitalWrite(SEG_F, 1U);
    digitalWrite(SEG_E, 0U);
    digitalWrite(SEG_G, 1U);
    break;
  }
  case 6:
  {
    digitalWrite(SEG_D, 1U);
    digitalWrite(SEG_C, 1U);
    digitalWrite(SEG_B, 0U);
    digitalWrite(SEG_A, 1U);
    digitalWrite(SEG_F, 1U);
    digitalWrite(SEG_E, 1U);
    digitalWrite(SEG_G, 1U);
    break;
  }
  case 7:
  {
    digitalWrite(SEG_D, 0U);
    digitalWrite(SEG_C, 1U);
    digitalWrite(SEG_B, 1U);
    digitalWrite(SEG_A, 1U);
    digitalWrite(SEG_F, 0U);
    digitalWrite(SEG_E, 0U);
    digitalWrite(SEG_G, 0U);
    break;
  }
  case 8:
  {
    digitalWrite(SEG_D, 1U);
    digitalWrite(SEG_C, 1U);
    digitalWrite(SEG_B, 1U);
    digitalWrite(SEG_A, 1U);
    digitalWrite(SEG_F, 1U);
    digitalWrite(SEG_E, 1U);
    digitalWrite(SEG_G, 1U);
    break;
  }
  case 9:
  {
    digitalWrite(SEG_D, 1U);
    digitalWrite(SEG_C, 1U);
    digitalWrite(SEG_B, 1U);
    digitalWrite(SEG_A, 1U);
    digitalWrite(SEG_F, 1U);
    digitalWrite(SEG_E, 0U);
    digitalWrite(SEG_G, 1U);
    break;
  }
  default:
  {
    /* Do nothing */
    break;
  }
  }
}

/**
 * @brief TIM1 overflow IRQ.
 *
 * This functions uses current readout from the potentiometer as well as
 * previous ones to calculate number of minutes to be displayed.
 */
ISR(TIMER1_OVF_vect)
{
  sys_tick_cntr++;

  TCNT1 = 0xC180U; // preloading 16-bit timer with 49536 to get 1 ms ticks (65536 - 49536 = 16000)

  /* Countdown to the next potentiometer measurement */
  potentiometer_cntr--;
  if (0U == potentiometer_cntr)
  {
    potentiometer_cntr = POTENTIOMETER_TIMEOUT_MS;
    measurement_pending = true;
  }

  /* Countdown to the next display refresh */
  display_cntr--;
  if (0U == display_cntr)
  {
    /* Toggle active display part and write current digit */
    if (FIRST_DIGIT_EN == current_display)
    {
      digitalWrite(FIRST_DIGIT_EN, 1U);
      digitalWrite(SEC_DIGIT_EN, 0U);
      if (DISPLAY_UNLOCKED == display_state)
      {
        write_digit(minutes_calculated / 10U);
      }
      else
      {
        write_digit(minutes_set / 10U);
      }
      current_display = SEC_DIGIT_EN;
    }
    else
    {
      digitalWrite(FIRST_DIGIT_EN, 0U);
      digitalWrite(SEC_DIGIT_EN, 1U);
      if (DISPLAY_UNLOCKED == display_state)
      {
        write_digit(minutes_calculated - ((minutes_calculated / 10U) * 10));
      }
      else
      {
        write_digit(minutes_set - ((minutes_set / 10U) * 10));
      }
      current_display = FIRST_DIGIT_EN;
    }

    display_cntr = DISPLAY_TIMEOUT_MS;
  }

  /* Countdown of minutes when display is locked */
  if ((DISPLAY_LOCKED == display_state) && (minutes_set > 0U))
  {
    /* Update locked display value every minute */
    locked_state_cntr--;
    if (0U == locked_state_cntr)
    {
      minutes_set--;
      locked_state_cntr = ONE_MINUTE_MS;
    }
    /* Toggle LED state every second */
    locked_state_led_cntr--;
    if (0U == locked_state_led_cntr)
    {
      digitalWrite(LED_INTERNAL, !digitalRead(LED_INTERNAL));
      locked_state_led_cntr = ONE_SECOND_MS;
    }
  }
}

/**
 * @brief Detects a button press.
 *
 * This functions detects if user button is pressed and locks/unlocks
 * the display state accordingly.
 *
 * @return None
 */
static void detect_button(void)
{
  static uint8_t btn_state_prev;
  static uint32_t btn_change_start;
  static bool btn_state_changed = false;
  uint8_t btn_state;

  btn_state = digitalRead(USER_BTN);

  if (btn_state_prev != btn_state)
  {
    btn_state_changed = true;
    btn_change_start = sys_tick_cntr;
  }

  if (true == btn_state_changed)
  {
    switch (display_state)
    {
    case DISPLAY_LOCKED:
    {
      if (BTN_PRESSED(btn_state) && ((sys_tick_cntr - btn_change_start) > DISPLAY_UNLOCKED_TIME_MS))
      {
        make_sound(1000);
        display_state = DISPLAY_UNLOCKED;
        btn_state_changed = false;
      }
      break;
    }
    case DISPLAY_UNLOCKED:
    {
      if (BTN_PRESSED(btn_state) && ((sys_tick_cntr - btn_change_start) > DISPLAY_LOCK_TIME_MS))
      {
        make_sound(100);
        minutes_set = minutes_calculated;
        locked_state_cntr = ONE_MINUTE_MS;
        locked_state_led_cntr = ONE_SECOND_MS;
        display_state = DISPLAY_LOCKED;
        btn_state_changed = false;
      }
      break;
    }
    }
  }

  btn_state_prev = btn_state;
}

/**
 * @brief Initial setup.
 *
 * This functions performs general initial setup. Called once at the beggining.
 *
 * @return None
 */
void setup()
{
  setup_pins();
  setup_timers();
  setup_7seg_display();
  display_state = DISPLAY_UNLOCKED;
  current_display = FIRST_DIGIT_EN;
  measurement_pending = false;
  potentiometer_readout = 0U;
  minutes_calculated = 0U;
  minutes_set = 0U;

  potentiometer_cntr = POTENTIOMETER_TIMEOUT_MS;
  display_cntr = DISPLAY_TIMEOUT_MS;
  sys_tick_cntr = 0U;
}

/**
 * @brief Main loop function.
 *
 * This functions is called in a loop after the setup.
 *
 * @return None
 */
void loop()
{
  uint32_t readout;

  /* Check if it's time to read potentiometer value. */
  if (true == measurement_pending)
  {
    measurement_pending = false;
    readout = read_potentiometer();
    minutes_calculated = calculate_minutes(readout);
  }

  detect_button();

  /* Check if time countdown has finished */
  if ((DISPLAY_LOCKED == display_state) && (0U == minutes_set))
  {
    make_sound(500);
    display_state = DISPLAY_UNLOCKED;
  }
}