/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Lab 8 - LCD and Touch Sensor
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "rng.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>

#include "ILI9341_STM32_Driver.h"
#include "ILI9341_GFX.h"
#include "ILI9341_Touchscreen.h"
#include "am2320.h"

/* Set to 1 after generating the photo header with tools/img2c.py:
     python3 tools/img2c.py your_photo.jpg 90 130 my_photo Core/Inc/my_photo.h */
#define HAVE_PHOTO          1

#if HAVE_PHOTO
#include "my_photo.h"
#endif

/* ADC1 (PA3 / IN3) and TIM4 CH1 (PB6) are configured, so the backlight part
   of SP3.2 is live.  Set to 0 to compile	 it out again. */
#define USE_BACKLIGHT_PWM   1

/* Set to 1 to print the raw touch coordinates at the bottom of screen 1.
   Handy when checking whether the landscape conversion below needs flipping. */
#define TOUCH_DEBUG         0

/* Set to 1 to print the AM2320 error code at the bottom left of screen 1.
   0 means the reading worked.  Turn back to 0 for the final version. */
#define SENSOR_DEBUG        0

/* Set to 1 to check the I2C bus at start-up and list every address that
   answers.  The AM2320 has been found at 0x5C, so it is off again; turn it
   back on if the bus ever needs checking. */
#define I2C_SCAN            0
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  SCREEN_1 = 0,
  SCREEN_2
} Screen_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* Which way up the screen sits.  SCREEN_HORIZONTAL_1 and SCREEN_HORIZONTAL_2
   are the two landscape orientations, 180 degrees apart.  Changing this one
   line turns both the picture and the touch mapping together. */
#define SCREEN_ROTATION     SCREEN_HORIZONTAL_1

/* ---- screen 1 layout (drawn in landscape, 320 x 240) ---- */
#define TEXT_SIZE           2
#define CHAR_W              (6 * TEXT_SIZE)   /* advance of the 5x5 font */
#define CHAR_H              (8 * TEXT_SIZE)

/* top row: temperature - mixed colour - humidity */
#define TEMP_X              10
#define TEMP_Y              24
#define HUMI_X              214
#define HUMI_Y              24
#define MIX_CX              160
#define MIX_CY              32
#define MIX_R               24

/* the three colour rows */
#define ROW_R_Y             100
#define ROW_G_Y             155
#define ROW_B_Y             210 
#define DOT_CX              32
#define DOT_R               20
#define BAR_X0              62
#define BAR_X1              232
#define BAR_HALF_H          13
#define PCT_X               256   /* "100 %" is 60 px wide, so 260 is the limit */

#define BAR_W               (BAR_X1 - BAR_X0)

/* ---- screen 2 layout ---- */
#define PHOTO_X0            20
#define PHOTO_Y0            45
#define PHOTO_W             90
#define PHOTO_H             130
#define PHOTO_X1            (PHOTO_X0 + PHOTO_W - 1)
#define PHOTO_Y1            (PHOTO_Y0 + PHOTO_H - 1)
#define INFO_X              124
#define INFO_Y0             50

/* ---- fill in your own group details here ---- */
#define GROUP_NO_TEXT       "Group No.31"
#define FIRST_NAME_TEXT     "Arnon"
#define LAST_NAME_TEXT      "Jantavara"
#define STUDENT_ID_TEXT     "67011050"

#define SCREEN2_TIMEOUT_MS  5000
#define SENSOR_PERIOD_MS    2000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t  rgb_percent[3] = { 0, 0, 0 };   /* red, green, blue: 0..100 % */

static int16_t  temperature_x10 = 0;
static uint16_t humidity_x10    = 0;
static uint8_t  sensor_valid    = 0;

static Am2320_HandleTypeDef am2320;

/* What am2320_ReadValue() last returned: 0 = ok, 1 = command refused,
   2 = no answer, 3 = unexpected reply */
static uint8_t  sensor_error    = 1;

static Screen_t current_screen   = SCREEN_1;
static uint32_t screen2_entered  = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static uint16_t Colour_From_Percent(uint8_t r_pct, uint8_t g_pct, uint8_t b_pct);
static uint16_t Colour_Pale(uint8_t index);
static uint16_t Colour_Solid(uint8_t index);
static uint16_t Row_Y(uint8_t index);

static void Screen1_Show(void);
static void Screen1_Draw_Sensor(void);
static void Screen1_Draw_Row(uint8_t index);
static void Screen1_Draw_Mixed(void);
static void Screen1_Handle_Touch(uint16_t x, uint16_t y);

static void Screen2_Show(void);
static void Screen2_Handle_Touch(uint16_t x, uint16_t y);

static uint8_t Touch_Get_New_Press(uint16_t *x, uint16_t *y);
static void    I2C_Recover(void);

#if USE_BACKLIGHT_PWM
static void Backlight_Update(void);
#endif
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  Get the I2C bus working again after a failed reading.
  *
  * A sensor that glitches mid-transfer can be left holding SDA low.  The bus
  * then looks permanently busy and every later transfer fails, even though
  * nothing is wrong with the wiring - which is why a reset "fixes" it.
  * Clocking SCL by hand lets the slave finish whatever byte it thinks it is
  * sending, so it releases SDA and the peripheral can be started clean.
  */
static void I2C_Recover(void)
{
  GPIO_InitTypeDef gpio = {0};

  HAL_I2C_DeInit(&hi2c1);

  /* SCL as a manual output, SDA as an input so its level can be watched */
  gpio.Pin   = i2c1_scl_Pin;
  gpio.Mode  = GPIO_MODE_OUTPUT_OD;
  gpio.Pull  = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(i2c1_scl_GPIO_Port, &gpio);

  gpio.Pin  = i2c1_sda_Pin;
  gpio.Mode = GPIO_MODE_INPUT;
  HAL_GPIO_Init(i2c1_sda_GPIO_Port, &gpio);

  /* Nine pulses is one byte plus the acknowledge - enough for any slave to
     reach the end of its transfer and let go. */
  for (uint8_t pulse = 0; pulse < 9; pulse++)
  {
    HAL_GPIO_WritePin(i2c1_scl_GPIO_Port, i2c1_scl_Pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(i2c1_scl_GPIO_Port, i2c1_scl_Pin, GPIO_PIN_SET);
    HAL_Delay(1);

    if (HAL_GPIO_ReadPin(i2c1_sda_GPIO_Port, i2c1_sda_Pin) == GPIO_PIN_SET)
    {
      break;                 /* SDA is free again */
    }
  }

  MX_I2C1_Init();            /* also re-runs MspInit, pull-ups included */
}

/**
  * @brief  Pack three 0..100 % channels into one RGB565 word.
  */
static uint16_t Colour_From_Percent(uint8_t r_pct, uint8_t g_pct, uint8_t b_pct)
{
  uint8_t r = (uint8_t)((uint16_t)r_pct * 255u / 100u);
  uint8_t g = (uint8_t)((uint16_t)g_pct * 255u / 100u);
  uint8_t b = (uint8_t)((uint16_t)b_pct * 255u / 100u);

  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* Washed out colour used for the empty part of a bar */
static uint16_t Colour_Pale(uint8_t index)
{
  switch (index)
  {
    case 0:  return Colour_From_Percent(100, 80, 80);
    case 1:  return Colour_From_Percent(80, 100, 80);
    default: return Colour_From_Percent(80, 80, 100);
  }
}

static uint16_t Colour_Solid(uint8_t index)
{
  switch (index)
  {
    case 0:  return RED;
    case 1:  return GREEN;
    default: return BLUE;
  }
}

static uint16_t Row_Y(uint8_t index)
{
  switch (index)
  {
    case 0:  return ROW_R_Y;
    case 1:  return ROW_G_Y;
    default: return ROW_B_Y;
  }
}

#if SENSOR_DEBUG
/* Modbus CRC16, the check the   appends to every reply.  The library in
   use does not verify it, so a corrupted frame would be accepted silently. */
static uint16_t Debug_CRC16(const uint8_t *data, uint8_t length)
{
  uint16_t crc = 0xFFFF;

  while (length--)
  {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++)
    {
      if (crc & 0x0001)
      {
        crc = (uint16_t)((crc >> 1) ^ 0xA001);
      }
      else
      {
        crc >>= 1;
      }
    }
  }
  return crc;
}
#endif

/**
  * @brief  Temperature on the left, humidity on the right.
  *         Printed as integers so the project does not need printf-float.
  */
static void Screen1_Draw_Sensor(void)
{
  char buffer[16];

  if (sensor_valid)
  {
    int16_t whole = temperature_x10 / 10;
    int16_t tenth = temperature_x10 % 10;
    if (tenth < 0)
    {
      tenth = -tenth;
    }
    sprintf(buffer, "%2d.%d C", whole, tenth);
  }
  else
  {
    sprintf(buffer, "--.- C");
  }
  ILI9341_Draw_Text(buffer, TEMP_X, TEMP_Y, BLACK, TEXT_SIZE, WHITE);

  if (sensor_valid)
  {
    sprintf(buffer, "%2u.%u %%RH", humidity_x10 / 10u, humidity_x10 % 10u);
  }
  else
  {
    sprintf(buffer, "--.- %%RH");
  }
  ILI9341_Draw_Text(buffer, HUMI_X, HUMI_Y, BLACK, TEXT_SIZE, WHITE);

  /* Say why the sensor is quiet, but only while it is.  Once a reading comes
     through this clears itself, so it can stay in the finished program. */
#if SENSOR_DEBUG
  if (sensor_error != 0)
  {
    char fault[48];

    /* Everything needed to tell the failure modes apart:
       S / D    - the idle level of SCL and SDA, both should be 1
       ec       - hi2c1.ErrorCode, 04 = the sensor did not acknowledge
       st       - hi2c1.State, 20 = ready, anything else means HAL is stuck
       rdy      - does the sensor answer a bare address at 0x5C right now  */
    sprintf(fault, "e%u S%d D%d ec%02X st%02X rdy%d",
            sensor_error,
            (int)HAL_GPIO_ReadPin(i2c1_scl_GPIO_Port, i2c1_scl_Pin),
            (int)HAL_GPIO_ReadPin(i2c1_sda_GPIO_Port, i2c1_sda_Pin),
            (unsigned int)hi2c1.ErrorCode,
            (unsigned int)hi2c1.State,
            (HAL_I2C_IsDeviceReady(&hi2c1, AM2320_ADDRESS, 2, 5) == HAL_OK) ? 1 : 0);

    ILI9341_Draw_Text(fault, 6, 232, RED, 1, WHITE);
  }
  else
  {
    ILI9341_Draw_Text("                              ", 6, 232, BLACK, 1, WHITE);
  }

  {
    /* Raw reply: fn len RHhi RHlo Thi Tlo CRClo CRChi */
    char     debug_line[56];
    uint16_t crc_received = ((uint16_t)am2320.data[7] << 8) | am2320.data[6];

    sprintf(debug_line, "e%u %02X%02X %02X%02X %02X%02X %02X%02X crc:%s",
            sensor_error,
            am2320.data[0], am2320.data[1],
            am2320.data[2], am2320.data[3],
            am2320.data[4], am2320.data[5],
            am2320.data[6], am2320.data[7],
            (crc_received == Debug_CRC16(am2320.data, 6)) ? "ok " : "BAD");

    ILI9341_Draw_Text(debug_line, 6, 232, BLACK, 1, WHITE);
  }
#endif
}

/**
  * @brief  One colour row: the touch dot, the level bar and the percentage.
  */
static void Screen1_Draw_Row(uint8_t index)
{
  uint16_t y     = Row_Y(index);
  uint8_t  pct   = rgb_percent[index];
  uint16_t fill  = (uint16_t)((uint32_t)BAR_W * pct / 100u);
  char     buffer[8];

  ILI9341_Draw_Filled_Circle(DOT_CX, y, DOT_R, Colour_Solid(index));

  if (fill > 0)
  {
    ILI9341_Draw_Filled_Rectangle_Coord(BAR_X0, y - BAR_HALF_H,
                                        BAR_X0 + fill, y + BAR_HALF_H,
                                        Colour_Solid(index));
  }
  if (fill < BAR_W)
  {
    ILI9341_Draw_Filled_Rectangle_Coord(BAR_X0 + fill, y - BAR_HALF_H,
                                        BAR_X1, y + BAR_HALF_H,
                                        Colour_Pale(index));
  }

  /* Padded to a fixed width so "100 %" fully overwrites " 90 %" */
  sprintf(buffer, "%3u %%", pct);
  ILI9341_Draw_Text(buffer, PCT_X, y - (CHAR_H / 2), BLACK, TEXT_SIZE, WHITE);
}

/**
  * @brief  The circle showing the mix of the three channels.
  */
static void Screen1_Draw_Mixed(void)
{
  uint16_t colour = Colour_From_Percent(rgb_percent[0],
                                        rgb_percent[1],
                                        rgb_percent[2]);

  ILI9341_Draw_Filled_Circle(MIX_CX, MIX_CY, MIX_R, colour);
  ILI9341_Draw_Hollow_Circle(MIX_CX, MIX_CY, MIX_R, BLACK);
}

static void Screen1_Show(void)
{
  current_screen = SCREEN_1;

  ILI9341_Set_Rotation(SCREEN_ROTATION);
  ILI9341_Fill_Screen(WHITE);

  Screen1_Draw_Sensor();
  Screen1_Draw_Mixed();
  for (uint8_t i = 0; i < 3; i++)
  {
    Screen1_Draw_Row(i);
  }
}

/**
  * @brief  A touch on a colour dot adds 10 %, a touch on the mixed circle
  *         opens screen 2.
  */
static void Screen1_Handle_Touch(uint16_t x, uint16_t y)
{
  /* mixed colour circle -> screen 2 (SP3.2) */
  if ((x >= MIX_CX - MIX_R) && (x <= MIX_CX + MIX_R) &&
      (y >= MIX_CY - MIX_R) && (y <= MIX_CY + MIX_R))
  {
    Screen2_Show();
    return;
  }

  /* the three colour dots, with a slightly generous hit box */
  if (x > (DOT_CX + DOT_R + 8))
  {
    return;
  }

  for (uint8_t i = 0; i < 3; i++)
  {
    uint16_t row = Row_Y(i);

    if ((y + DOT_R + 4 >= row) && (y <= row + DOT_R + 4))
    {
      rgb_percent[i] = (rgb_percent[i] >= 100) ? 0 : (rgb_percent[i] + 10);

      Screen1_Draw_Row(i);
      Screen1_Draw_Mixed();
      return;
    }
  }
}

/**
  * @brief  Group and student details, written in the mixed colour (SP3.1).
  */
static void Screen2_Show(void)
{
  uint16_t colour = Colour_From_Percent(rgb_percent[0],
                                        rgb_percent[1],
                                        rgb_percent[2]);

  current_screen  = SCREEN_2;
  screen2_entered = HAL_GetTick();

  ILI9341_Set_Rotation(SCREEN_ROTATION);
  ILI9341_Fill_Screen(WHITE);

#if HAVE_PHOTO
  ILI9341_Draw_Image_Area(PHOTO_X0, PHOTO_Y0, PHOTO_W, PHOTO_H, my_photo);
#else
  /* Stand-in until the photo header is generated: see tools/img2c.py */
  ILI9341_Draw_Filled_Rectangle_Coord(PHOTO_X0, PHOTO_Y0, PHOTO_X1, PHOTO_Y1,
                                      Colour_From_Percent(70, 85, 100));
#endif
  ILI9341_Draw_Hollow_Rectangle_Coord(PHOTO_X0, PHOTO_Y0, PHOTO_X1, PHOTO_Y1,
                                      colour);

  ILI9341_Draw_Text(GROUP_NO_TEXT,   INFO_X, INFO_Y0,                  colour, TEXT_SIZE, WHITE);
  ILI9341_Draw_Text(FIRST_NAME_TEXT, INFO_X, INFO_Y0 + 1 * (CHAR_H + 10), colour, TEXT_SIZE, WHITE);
  ILI9341_Draw_Text(LAST_NAME_TEXT,  INFO_X, INFO_Y0 + 2 * (CHAR_H + 10), colour, TEXT_SIZE, WHITE);
  ILI9341_Draw_Text(STUDENT_ID_TEXT, INFO_X, INFO_Y0 + 3 * (CHAR_H + 10), colour, TEXT_SIZE, WHITE);
}

/**
  * @brief  Touching the photo returns to screen 1 without waiting (SP3.2).
  */
static void Screen2_Handle_Touch(uint16_t x, uint16_t y)
{
  /* A few pixels of slack around the frame: landing just outside it would
     otherwise mean waiting out the full five seconds. */
  const uint16_t margin = 8;

  if ((x + margin >= PHOTO_X0) && (x <= PHOTO_X1 + margin) &&
      (y + margin >= PHOTO_Y0) && (y <= PHOTO_Y1 + margin))
  {
    Screen1_Show();
  }
}

/**
  * @brief  Report a touch once per press, in landscape coordinates.
  *
  * The touchscreen library hands back coordinates in the SCREEN_VERTICAL_1
  * frame (240 x 320) while the layout above is drawn in SCREEN_HORIZONTAL_1
  * (320 x 240), so the point has to be rotated.  If touches land mirrored on
  * your board, swap the two marked lines for the alternatives in the comments.
  */
static uint8_t Touch_Get_New_Press(uint16_t *x, uint16_t *y)
{
  static uint8_t was_pressed = 0;
  uint16_t position[2];

  if (!TP_Touchpad_Pressed())
  {
    was_pressed = 0;
    return 0;
  }

  if (was_pressed)
  {
    return 0;              /* still the same finger, ignore */
  }

  if (TP_Read_Coordinates(position) != TOUCHPAD_DATA_OK)
  {
    return 0;
  }

  was_pressed = 1;

  /* The library always reports in the SCREEN_VERTICAL_1 frame (240 x 320),
     so the point has to be turned to match whichever landscape orientation
     SCREEN_ROTATION selects. */
#if (SCREEN_ROTATION == SCREEN_HORIZONTAL_1)
  *x = position[1];
  *y = (ILI9341_SCREEN_HEIGHT - 1) - position[0];
#else   /* SCREEN_HORIZONTAL_2 - the same view turned 180 degrees */
  *x = (ILI9341_SCREEN_WIDTH - 1) - position[1];
  *y = position[0];
#endif

#if TOUCH_DEBUG
  {
    char buffer[32];
    sprintf(buffer, "raw %3u,%3u", position[0], position[1]);
    ILI9341_Draw_Text(buffer, 10, 226, BLACK, 1, WHITE);
    sprintf(buffer, "scr %3u,%3u", *x, *y);
    ILI9341_Draw_Text(buffer, 120, 226, BLACK, 1, WHITE);
  }
#endif

  return 1;
}

#if USE_BACKLIGHT_PWM
/**
  * @brief  Map the potentiometer onto 20..100 % backlight duty (SP3.2).
  */
static void Backlight_Update(void)
{
  uint32_t raw  = 0;
  uint32_t duty;

  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
  {
    raw = HAL_ADC_GetValue(&hadc1);
  }
  HAL_ADC_Stop(&hadc1);

  /* 0..4095  ->  20..100 % */
  duty = 20u + (raw * 80u) / 4095u;

  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1,
                        duty * (__HAL_TIM_GET_AUTORELOAD(&htim4) + 1u) / 100u);
}
#endif
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_RNG_Init();
  MX_SPI5_Init();
  MX_TIM1_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
#if USE_BACKLIGHT_PWM
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
#endif

  am2320 = am2320_Init(&hi2c1, AM2320_ADDRESS);

  ILI9341_Init();

#if I2C_SCAN
  {
    char     line[48];
    uint16_t y     = 40;
    uint8_t  found = 0;

    ILI9341_Set_Rotation(SCREEN_ROTATION);
    ILI9341_Fill_Screen(WHITE);

    /* Idle bus levels.  Both must read 1: the lines are open drain, so a 0
       here means nothing is pulling them up, or something is holding them
       down, and no transfer can ever start. */
    sprintf(line, "SCL=%d SDA=%d  (both must be 1)",
            (int)HAL_GPIO_ReadPin(i2c1_scl_GPIO_Port, i2c1_scl_Pin),
            (int)HAL_GPIO_ReadPin(i2c1_sda_GPIO_Port, i2c1_sda_Pin));
    ILI9341_Draw_Text(line, 6, 10, BLACK, 1, WHITE);

    /* Two tries per address: the AM2320 sleeps and the first one wakes it */
    for (uint8_t address = 1; address < 128; address++)
    {
      if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(address << 1), 2, 5) == HAL_OK)
      {
        sprintf(line, "found 0x%02X", address);
        ILI9341_Draw_Text(line, 6, y, BLACK, 2, WHITE);
        y += 22;
        found++;
      }
    }

    if (found == 0)
    {
      ILI9341_Draw_Text("no device answered", 6, 40, RED, 2, WHITE);
      ILI9341_Draw_Text("AM2320 should be 0x5C", 6, 70, BLACK, 1, WHITE);
    }

    HAL_Delay(5000);
  }
#endif

  Screen1_Show();

  uint32_t last_sensor    = 0;
#if USE_BACKLIGHT_PWM
  uint32_t last_backlight = 0;
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now = HAL_GetTick();
    uint16_t touch_x, touch_y;

#if USE_BACKLIGHT_PWM
    if (now - last_backlight >= 100u)
    {
      last_backlight = now;
      Backlight_Update();
    }
#endif

    /* The AM2320 wants at least 2 s of rest between readings */
    if ((current_screen == SCREEN_1) &&
        ((last_sensor == 0) || (now - last_sensor >= SENSOR_PERIOD_MS)))
    {
      last_sensor = now;

      /* One transaction per pass.  Do not also call
         am2320_GetTemperatureAndHumidity() here - it performs a read of its
         own, which would break the 2 s spacing the sensor needs. */
      sensor_error = am2320_ReadValue(&am2320);

      if (sensor_error == 0)
      {
        /* data[2..3] = humidity, data[4..5] = temperature, both x10 */
        uint16_t raw_humidity    = ((uint16_t)am2320.data[2] << 8) | am2320.data[3];
        uint16_t raw_temperature = ((uint16_t)am2320.data[4] << 8) | am2320.data[5];

        humidity_x10 = raw_humidity;

        /* bit 15 is the sign */
        if (raw_temperature & 0x8000)
        {
          temperature_x10 = -(int16_t)(raw_temperature & 0x7FFF);
        }
        else
        {
          temperature_x10 = (int16_t)raw_temperature;
        }

        sensor_valid = 1;
      }
      else
      {
        /* Unstick the bus so the next attempt starts from a clean state */
        I2C_Recover();
      }
      Screen1_Draw_Sensor();
    }

    if (Touch_Get_New_Press(&touch_x, &touch_y))
    {
      if (current_screen == SCREEN_1)
      {
        Screen1_Handle_Touch(touch_x, touch_y);
      }
      else
      {
        Screen2_Handle_Touch(touch_x, touch_y);
      }
    }

    /* Screen 2 goes away on its own after 5 s.
       Read the tick again here rather than reusing `now` from the top of the
       loop: screen 2 is entered above, in the touch handler, so `now` is
       already older than screen2_entered and the unsigned subtraction would
       wrap round to a huge number and fire immediately. */
    if ((current_screen == SCREEN_2) &&
        (HAL_GetTick() - screen2_entered >= SCREEN2_TIMEOUT_MS))
    {
      Screen1_Show();
    }

    HAL_Delay(5);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
