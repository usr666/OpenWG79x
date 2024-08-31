#include "hal/hal.h"
#include "hal/hal_display.h"
#include "hal/hal_keyboard.h"
#include "hal/hal_sensors.h"
#include "hal/hal_power.h"
#include "system.h"
#include "display.h"


int main(void) {
  init_hal();
  init_hal_power();
  init_hal_display();
  init_hal_keyboard();
  init_hal_sensors();
  init_display();

  set_backlight(true);

  // Wait until power button is released
  while(get_power_button()) {
    delay_micro_seconds(10000);
  }
  delay_micro_seconds(100000); // button debounce
  
  for (;;){
    task_display();
    task_keyboard();

    if(get_power_button()) {
      printText("poweroff");
      poweroff();
      set_backlight(false);
    }
    if(get_pressed_key() == KEY1){
      printText("backlight on");
      set_backlight(true);
    }
    if(get_pressed_key() == KEY2){
      printText("backlight off");
      set_backlight(false);
    }
    delay_micro_seconds(250000);
  }
}
