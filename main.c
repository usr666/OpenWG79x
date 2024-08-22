#include "hal/hal.h"
#include "system.h"
#include "display.h"


int main(void) {
  init_hal();
  init_display();
  
  for (;;){
    task_display();
    delay_micro_seconds(250000);
  }
}
