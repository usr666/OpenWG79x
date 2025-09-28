#ifndef _DISPLAY_H
#define _DISPLAY_H

void init_display(void);
void task_display(void);
void print_text(uint8_t row, const char *str);
void clear_display(void);
void set_text_size(uint8_t size);

#endif