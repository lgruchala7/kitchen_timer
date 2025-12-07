/*==================================================================
    Includes
===================================================================*/

#include <stdbool.h>

/*==================================================================
    Object-like macros
===================================================================*/

#define POTENTIOMETER_TIMEOUT (100U)
#define DISPLAY_TIMEOUT (10U)
#define FREEZED_STATE_TIMEOUT (1000U)
#define POT_ADC_MIN (20U)
#define POT_ADC_MAX (1024U)
#define DISPLAY_MAX_NUM (60U)

/*==================================================================
    Function-like macros
===================================================================*/

#define BTN_PRESSED(x) ((x) == 0U)

/*==================================================================
    Local types
===================================================================*/

enum
{
  SEC_DIGIT_EN = 0,
  FIRST_DIGIT_EN = 1,
  SEG_C = 2,
  SEG_E = 3,
  SEG_D = 4,
  SEG_B = 5,
  SEG_F = 6,
  SEG_A = 7,
  SEG_G = 8,
  POT_MEAS_EN = 9,
  BUZZ_ON = 10,
  USR_BTN = 11
};

enum
{
  POT_MEAS = 5
};

typedef enum
{
  DISPLAY_FREE = 0,
  DISPLAY_FREEZED = 1
} display_state_t;

/*==================================================================
    Local objects
===================================================================*/

static volatile bool measurement_pending;
static volatile uint32_t potentiometer_cntr;
static volatile uint32_t display_cntr;
static volatile uint32_t freezed_state_cntr;
static int potentiometer_readout;
static volatile int display_curr;
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

static void setup_pins(void)
{
  pinMode(SEC_DIGIT_EN, OUTPUT); // Set second digit enable - pin 0 as an output
  pinMode(FIRST_DIGIT_EN, OUTPUT); // Set first digit enable - pin 1 as an output
  pinMode(SEG_D, OUTPUT); // Set segment D - pin 2 as an output
  pinMode(SEG_E, OUTPUT); // Set segment E - pin 3 as an output
  pinMode(SEG_C, OUTPUT); // Set segment C - pin 4 as an output
  pinMode(SEG_B, OUTPUT); // Set segment B - pin 5 as an output
  pinMode(SEG_F, OUTPUT); // Set segment F - pin 6 as an output
  pinMode(SEG_A, OUTPUT); // Set segment A - pin 7 as an output
  pinMode(SEG_G, OUTPUT); // Set segment G - pin 8 as an output
  pinMode(POT_MEAS_EN, OUTPUT); // Set potentiometer enable - pin 9 as an output
  pinMode(BUZZ_ON, OUTPUT); // Set buzzer on - pin 10 as an output
  pinMode(USR_BTN, INPUT_PULLUP); // Set buzzer on - pin 11 as an input
}

static void setup_timers(void)
{
  TCCR1A = 0x00U;
  TCCR1B = 0x00U;
  TCCR1B |= 0x01U; // prescaler = 1
  TCNT1 = 0xC180U; // preloading 16-bit timer with 49536 to get 1 ms ticks (65536 - 49536 = 16000)
  TIMSK1 = 0x01U;
}
static void setup_leds(void)
{
  digitalWrite(FIRST_DIGIT_EN, 0U);
  digitalWrite(SEC_DIGIT_EN, 0U);
}

static void make_sound(int millis)
{
  tone(BUZZ_ON, 1000);  // Send 1KHz sound signal...
  delay(millis);           // ...for 0.1 sec
  noTone(BUZZ_ON);      // Stop sound...
}

static uint32_t read_potentiometer(void)
{
  uint32_t readout;

  digitalWrite(POT_MEAS_EN, 1U);
  delayMicroseconds(100U);
  readout = analogRead(POT_MEAS);
  digitalWrite(POT_MEAS_EN, 0U);

  return readout;
}

static uint8_t calculate_minutes(uint32_t readout)
{
  uint8_t minutes;
  const uint8_t units_per_min = (uint8_t)((uint16_t)(POT_ADC_MAX - POT_ADC_MIN) / (uint8_t)DISPLAY_MAX_NUM);

  minutes =  (POT_ADC_MAX - readout) / units_per_min;

  if (minutes > DISPLAY_MAX_NUM)
  {
    minutes = DISPLAY_MAX_NUM;
  }

  return minutes;
}

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

ISR(TIMER1_OVF_vect)
{
  sys_tick_cntr++;

  TCNT1 = 0xC180U; // preloading 16-bit timer with 49536 to get 1 ms ticks (65536 - 49536 = 16000)

  potentiometer_cntr--;
  if (0U == potentiometer_cntr)
  {
    potentiometer_cntr = POTENTIOMETER_TIMEOUT;
    measurement_pending = true;
  }

  display_cntr--;
  if (0U == display_cntr)
  {
    if (FIRST_DIGIT_EN == display_curr)
    {
      digitalWrite(FIRST_DIGIT_EN, 1U);
      digitalWrite(SEC_DIGIT_EN, 0U);
      if (DISPLAY_FREE == display_state)
      {
        write_digit(minutes_calculated / 10U);
      }
      else
      {
        write_digit(minutes_set / 10U);
      }
      display_curr = SEC_DIGIT_EN;
    }
    else
    {
      digitalWrite(FIRST_DIGIT_EN, 0U);
      digitalWrite(SEC_DIGIT_EN, 1U);
      if (DISPLAY_FREE == display_state)
      {
        write_digit(minutes_calculated - ((minutes_calculated / 10U) * 10));
      }
      else
      {
        write_digit(minutes_set - ((minutes_set / 10U) * 10));
      }
      display_curr = FIRST_DIGIT_EN;
    }

    display_cntr = DISPLAY_TIMEOUT;
  }

  if ((DISPLAY_FREEZED == display_state) && (minutes_set > 0U))
  {
    freezed_state_cntr--;
    if (0U == freezed_state_cntr)
    {
      minutes_set--;
      freezed_state_cntr = FREEZED_STATE_TIMEOUT;
    }
  }
}

static void detect_button(void)
{
  static uint8_t btn_state_prev;
  static uint32_t btn_change_start; 
  static bool btn_state_changed = false; 
  uint8_t btn_state;

  btn_state = digitalRead(USR_BTN);

  if (btn_state_prev != btn_state)
  {
    btn_state_changed = true;
    btn_change_start = sys_tick_cntr;
  }

  if (true == btn_state_changed)
  {
    switch (display_state)
    {
      case DISPLAY_FREEZED:
      {
        if (BTN_PRESSED(btn_state) && ((sys_tick_cntr - btn_change_start) > 3000U))
        {
          make_sound(10);
          display_state = DISPLAY_FREE;
          btn_state_changed = false;
        } 
        break;
      }
      case DISPLAY_FREE:
      {
        if (BTN_PRESSED(btn_state) && ((sys_tick_cntr - btn_change_start) > 100U))
        {
          make_sound(100);
          minutes_set = minutes_calculated;
          freezed_state_cntr = FREEZED_STATE_TIMEOUT;
          display_state = DISPLAY_FREEZED;
          btn_state_changed = false;
        }
        break;
      }
    }
  }

  btn_state_prev = btn_state;
}

void setup()
{
  setup_pins();
  setup_timers();
  setup_leds();
  // Serial.begin(9600);
  display_state = DISPLAY_FREE;
  display_curr = FIRST_DIGIT_EN;
  measurement_pending = false;
  potentiometer_readout = 0U;
  minutes_calculated = 0U;
  minutes_set = 0U;
  
  potentiometer_cntr = POTENTIOMETER_TIMEOUT;
  display_cntr = DISPLAY_TIMEOUT;
  sys_tick_cntr = 0U;
}

void loop()
{
  uint32_t readout;

  if (true == measurement_pending)
  {
    measurement_pending = false;
    readout = read_potentiometer();
    minutes_calculated = calculate_minutes(readout);
  }

  detect_button();

  if ((DISPLAY_FREEZED == display_state) && (0U == minutes_set))
  {
    make_sound(500);
    display_state = DISPLAY_FREE;
  }

}