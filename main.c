#include "hal/hal.h"
#include "hal/hal_display.h"
#include "hal/hal_keyboard.h"
#include "system.h"
#include "display.h"


int main(void) {
  init_hal();
  init_hal_display();
  init_hal_keyboard();
  init_display();
  
  for (;;){
    task_display();
    task_keyboard();
    delay_micro_seconds(250000);
  }
}
