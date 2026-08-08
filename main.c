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

  load_settings(&was_stopped, &stopreason);
  init_mowercontrol(was_stopped, stopreason);

  // Wait until power button is released
  while(get_power_button()) {
    delay_micro_seconds(10000);
  }
  delay_micro_seconds(100000); // button debounce
  
  for (;;){
    task_hal_adc();
    task_sensors();
    task_hal_charger();
    task_display();
    task_keyboard();
    task_motor();
    task_scheduler();
    task_mowercontrol();

    if(get_power_button()) {
      store_settings();

      print_text(1, "poweroff");
      poweroff();
      set_backlight(false);
    }
  }
}
