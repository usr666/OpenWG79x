#include "hal/hal.h"
#include "hal/hal_display.h"
#include "system.h"
#include "display.h"


int main(void) {
  init_hal();
  init_hal_display();
  init_display();
  
  for (;;){
    task_display();
    delay_micro_seconds(250000);
  }
}
