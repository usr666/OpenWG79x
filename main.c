#include <stdio.h>
#include "hal/hal.h"
#include "hal/hal_adc.h"
#include "hal/hal_spi.h"
#include "hal/hal_display.h"
#include "hal/hal_keyboard.h"
#include "hal/hal_sensors.h"
#include "hal/hal_power.h"
#include "hal/hal_motor.h"
#include "hal/hal_charger.h"
#include "hal/hal_rtc.h"
#include "hal/hal_nvm.h"
#include "hal/hal_remotecom.h"
#include "system.h"
#include "display.h"
#include "mowercontrol.h"
#include "scheduler.h"
#include "remotecom.h"
#include "version.h"

static void load_settings(bool *was_stopped, uint8_t *stopreason)
{
    uint8_t nvm_buffer[16];
    if (!hal_nvm_load(nvm_buffer)) { return; }
    schedule_active = (nvm_buffer[0] & 0x01) != 0;
    sideways_down = (nvm_buffer[0] & 0x02) != 0;
    avoid_downhill = (nvm_buffer[0] & 0x04) != 0;
    schedule_starttime = nvm_buffer[1];
    schedule_endtime = nvm_buffer[2];
    *was_stopped = (nvm_buffer[0] & 0x08) != 0;
    *stopreason = nvm_buffer[3];
    if(nvm_buffer[4] >= 1 && nvm_buffer[4] <= 99) {
        circlespeed = nvm_buffer[4];
    }
}

void store_settings(void)
{
    uint8_t nvm_buffer[16] = {0};

    nvm_buffer[0] = 0;
    if (schedule_active)             nvm_buffer[0] |= 0x01;
    if (sideways_down)               nvm_buffer[0] |= 0x02;
    if (avoid_downhill)              nvm_buffer[0] |= 0x04;
    if (is_stopped)                  nvm_buffer[0] |= 0x08;
    nvm_buffer[1] = schedule_starttime;
    nvm_buffer[2] = schedule_endtime;
    nvm_buffer[3] = stopreason;
    nvm_buffer[4] = circlespeed;

    hal_nvm_store(nvm_buffer);
}

#define PINCODE_DIGITS 4
#define PINCODE_ATTEMPTS 3

// Digit for each key, -1 for keys that are not digits. Indexed by keys_t.
static const int8_t key_digit[KEY_NUMBER_OF_KEYS] = {-1, 8, 9, 0, -1, 6, -1, -1, 7, -1, -1, 4, 5, -1, 1, 2, 3};

static bool check_pincode(void)
{
    char entered[PINCODE_DIGITS + 1];
    char versionstr[20];
    systimer_t timer;
    uint16_t pincode;
    uint8_t digits;
    uint8_t attempt;
    keys_t currentkey;
    keys_t lastkey = KEY_NONE;

    sprintf(versionstr, "OpenWG79x v%02d.%02d", OPENWG79X_MAJORVERSION, OPENWG79X_MINORVERSION);
    for (attempt = 0; attempt < PINCODE_ATTEMPTS; attempt++) {
        pincode = 0;
        digits = 0;
        entered[0] = '\0';
        clear_display();
        print_text(0, "Enter pincode");
        print_text(3, versionstr);
        while (digits < PINCODE_DIGITS) {
            task_keyboard();
            task_display();
            currentkey = get_pressed_key();
            if (lastkey == KEY_NONE && key_digit[currentkey] >= 0) {
                pincode = pincode * 10U + (uint16_t)key_digit[currentkey];
                entered[digits++] = '*';
                entered[digits] = '\0';
                print_text(1, entered);
            }
            lastkey = currentkey;
        }
        if (hal_nvm_init(pincode)) {
            do { // Wait for key release, a held digit would otherwise trigger the idle menu
                task_keyboard();
            } while (get_pressed_key() != KEY_NONE);
            clear_display();
            return true;
        }
        clear_display();
        print_text(0, "Wrong pincode");
        systimer_start(&timer, 1000);
        while (!systimer_is_expired(&timer)) {
            task_display();
        }
    }
    return false;
}

int main(void) {
  bool was_stopped = false;
  uint8_t stopreason = 0U;

  init_hal();
  init_rtc();
  init_hal_adc();
  init_hal_power();
  init_hal_spi();
  init_hal_display();
  init_hal_remotecom();
  init_hal_keyboard();
  init_hal_sensors();
  init_hal_charger();
  init_display();
  set_backlight(true);
  init_hal_motor();
  init_scheduler();
  init_remotecom();

  // Wait until power button is released
  while(get_power_button()) {
    delay_micro_seconds(10000);
  }
  delay_micro_seconds(100000); // button debounce

  if (!check_pincode()) {
    poweroff();
    for(;;);
  }

  load_settings(&was_stopped, &stopreason);
  init_mowercontrol(was_stopped, stopreason);
  
  for (;;){
    task_hal_adc();
    task_sensors();
    task_hal_charger();
    task_display();
    task_keyboard();
    task_motor();
    task_scheduler();
    task_mowercontrol();
    task_remotecom();

    if(get_power_button()) {
      store_settings();

      print_text(1, "poweroff");
      poweroff();
      set_backlight(false);
    }
  }
}
