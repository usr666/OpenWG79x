#include "hal/hal.h"
#include "hal/hal_display.h"
#include "hal/hal_keyboard.h"
#include "hal/hal_sensors.h"
#include "hal/hal_power.h"
#include "hal/hal_motor.h"
#include "system.h"
#include "display.h"
#include "mowercontrol.h"

int main(void) {
  init_hal();
  init_hal_power();
  init_hal_display();
  init_hal_keyboard();
  init_hal_sensors();
  init_display();
  set_backlight(true);
  init_hal_motor();
  init_mowercontrol();

  // Wait until power button is released
  while(get_power_button()) {
    delay_micro_seconds(10000);
  }
  delay_micro_seconds(100000); // button debounce
  
  for (;;){
    task_sensors();
    task_display();
    task_keyboard();
    task_motor();
    task_mowercontrol();

    if(get_power_button()) {
      print_text(1, "poweroff");
      poweroff();
      set_backlight(false);
    }
  }
}
