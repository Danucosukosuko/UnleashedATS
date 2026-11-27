#include "Common.h"
#include "Recovery.h"

#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <driver/gpio.h>

#define CRASH_MAGIC  0x52435659  // "RCVY"
#define CRASH_LIMIT  3

// Kept through resets, but not through a power off
RTC_NOINIT_ATTR static uint32_t crashMagic;
RTC_NOINIT_ATTR static uint32_t crashCount;

static const esp_partition_t *recoveryPartition()
{
  return(esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, nullptr));
}

bool recoveryAvailable()
{
#if defined(LILYGO_SI473X)
  // The T-Embed button is the boot strapping pin, holding it means download mode
  return(false);
#else
  esp_app_desc_t desc;
  const esp_partition_t *p = recoveryPartition();
  return(p && esp_ota_get_partition_description(p, &desc) == ESP_OK);
#endif
}

void recoveryRestart()
{
  if(!recoveryAvailable()) return;

  // The bootloader starts the recovery app while the encoder button reads
  // as pressed: hold the pin low through the restart. The recovery app
  // releases it.
  gpio_reset_pin((gpio_num_t)ENCODER_PUSH_BUTTON);
  gpio_set_direction((gpio_num_t)ENCODER_PUSH_BUTTON, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_level((gpio_num_t)ENCODER_PUSH_BUTTON, 0);
  gpio_hold_en((gpio_num_t)ENCODER_PUSH_BUTTON);
  delay(20);
  esp_restart();
}

bool recoveryCrashLoop()
{
  esp_reset_reason_t why = esp_reset_reason();
  bool crash = why == ESP_RST_PANIC || why == ESP_RST_INT_WDT || why == ESP_RST_TASK_WDT || why == ESP_RST_WDT;

  if(crashMagic != CRASH_MAGIC || !crash)
  {
    crashMagic = CRASH_MAGIC;
    crashCount = 0;
  }
  if(crash) crashCount++;

  if(crashCount < CRASH_LIMIT) return(false);
  // Count again from here, whatever comes next
  crashCount = 0;
  return(true);
}

void recoveryStable()
{
  crashCount = 0;
}
