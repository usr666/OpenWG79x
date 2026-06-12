#include "system.h"
#include <stdio.h>
#include <string.h>
#include "hal/hal_keyboard.h"
#include "hal/hal_rtc.h"
#include "display.h"
#include "mowercontrol.h"
#include "scheduler.h"

#define NUM_MENULEVELS 3
static uint8_t menulevel, menuindex[NUM_MENULEVELS];

static bool menuactive;

static keys_t lastpressedkey;

bool is_menu_active(void){
    return menuactive;
}

void init_menu(void) {
    menuactive = true;
    menulevel = 0;
    menuindex[0] = 0;
    lastpressedkey = get_pressed_key();
}

static void print_menu(const char *line0, const char *line1)
{
    print_text(0, line0);
    print_text(1, line1);
}

static bool handle_int(keys_t lastpressedkey, keys_t currentpressedkey, int *value, int minvalue, int maxvalue, bool *save)
{
    *save = false;
    if(lastpressedkey == KEY_NONE) {
        if(currentpressedkey == KEYBACK) {
            return true;
        }
        if(currentpressedkey == KEYOK) {
            *save = true;
            return true;
        }        
        if(currentpressedkey == KEYUP) {
            if(*value < maxvalue) {
                (*value)++;
            }
        }        
        if(currentpressedkey == KEYDOWN) {
            if(*value > minvalue) {
                (*value)--;
            }
        }
    }
    return false;
}

static bool handle_bool(keys_t lastpressedkey, keys_t currentpressedkey, bool *value, bool *save)
{
    *save = false;
    if(lastpressedkey == KEY_NONE) {
        if(currentpressedkey == KEYBACK) {
            return true;
        }
        if(currentpressedkey == KEYOK) {
            *save = true;
            return true;
        }        
        if(currentpressedkey == KEYUP) {
            *value = !*value;
        }        
        if(currentpressedkey == KEYDOWN) {
            *value = !*value;
        }
    }
    return false;
}

void task_menu(void) {
    char tmpstr[32];
    bool save;
    static bool boolvalue;
    static int intvalue;
    bool enable_menunavigation=true;
    uint8_t numitems_on_this_level;
    keys_t currentpressedkey;
    currentpressedkey = get_pressed_key();
    const char *menu_line0, *menu_line1;
    static const char *menu0settings_items[] = {"Schedule", "Set Time", "Go sideways down", "Avoid downhill", "Circle speed"};
    static const char *menu1schedule_items[] = {"Status", "Starttime", "Endtime"};
    static const char *menu1settime_items[] = {"Set hour", "Set minutes"};

    if(menulevel == 0) {
        menu_line0 = "Settings";
        menu_line1 = menu0settings_items[menuindex[0]];
        numitems_on_this_level = 5;
    } else {
        switch(menuindex[0]) {
            case 0:
                if(menulevel == 1) {
                    menu_line0 = "Schedule";
                    menu_line1 = menu1schedule_items[menuindex[1]];                    
                    numitems_on_this_level = 3;
                } else {
                    enable_menunavigation = false;
                    switch(menuindex[1]) {
                        case 0:
                            menu_line0 = "Schedule status";
                            if(menulevel == 2) {
                                boolvalue = schedule_active;
                                menulevel++;
                            }
                            if(handle_bool(lastpressedkey, currentpressedkey, &boolvalue, &save)) {
                                if(save) {
                                    schedule_active = boolvalue;
                                }
                                menulevel -= 2;
                            }
                            menu_line1 = boolvalue ? "Active" : "Not active";
                            break;
                        case 1:
                            menu_line0 = "Schedule starttime";
                            if(menulevel == 2) {
                                intvalue = schedule_starttime;
                                menulevel++;
                            }
                            if(handle_int(lastpressedkey, currentpressedkey, &intvalue, 0, 23, &save)) {
                                if(save) {
                                    schedule_starttime = intvalue;
                                }
                                menulevel -= 2;
                            }
                            sprintf(tmpstr, "%d", intvalue);
                            menu_line1 = tmpstr;
                            break;
                        case 2:
                            menu_line0 = "Schedule endtime";
                            if(menulevel == 2) {
                                intvalue = schedule_endtime;
                                menulevel++;
                            }
                            if(handle_int(lastpressedkey, currentpressedkey, &intvalue, 0, 23, &save)) {
                                if(save) {
                                    schedule_endtime = intvalue;
                                }
                                menulevel -= 2;
                            }
                            sprintf(tmpstr, "%d", intvalue);
                            menu_line1 = tmpstr;
                            break;
                    }
                }
                break;
            case 1:
                if(menulevel == 1) {
                    menu_line0 = "Set time";
                    menu_line1 = menu1settime_items[menuindex[1]];                    
                    numitems_on_this_level = 2;
                } else {
                    enable_menunavigation = false;
                    switch(menuindex[1]) {
                        case 0:
                            menu_line0 = "Set hour";
                            if(menulevel == 2) {
                                intvalue = get_rtc_hour();
                                menulevel++;
                            }
                            if(handle_int(lastpressedkey, currentpressedkey, &intvalue, 0, 23, &save)) {
                                if(save) {
                                    set_rtc_hour(intvalue);
                                }
                                menulevel -= 2;
                            }
                            sprintf(tmpstr, "%d", intvalue);
                            menu_line1 = tmpstr;
                            break;
                        case 1:
                            menu_line0 = "Set minute";
                            if(menulevel == 2) {
                                intvalue = get_rtc_minute();
                                menulevel++;
                            }
                            if(handle_int(lastpressedkey, currentpressedkey, &intvalue, 0, 59, &save)) {
                                if(save) {
                                    set_rtc_minute(intvalue);
                                }
                                menulevel -= 2;
                            }
                            sprintf(tmpstr, "%d", intvalue);
                            menu_line1 = tmpstr;
                            break;
                    }
                }
                break;
            case 2:
                menu_line0 = "Go sideways down";
                enable_menunavigation = false;
                if(menulevel == 1) {
                    boolvalue = sideways_down;
                    menulevel++;
                }
                if(handle_bool(lastpressedkey, currentpressedkey, &boolvalue, &save)) {
                    if(save) {
                        sideways_down = boolvalue;
                    }
                    menulevel -= 2;
                }
                menu_line1 = boolvalue ? "Active" : "Not active";
                break;
            case 3:
                menu_line0 = "Avoid downhill";
                enable_menunavigation = false;
                if(menulevel == 1) {
                    boolvalue = avoid_downhill;
                    menulevel++;
                }
                if(handle_bool(lastpressedkey, currentpressedkey, &boolvalue, &save)) {
                    if(save) {
                        avoid_downhill = boolvalue;
                    }
                    menulevel -= 2;
                }
                menu_line1 = boolvalue ? "Active" : "Not active";
                break;
            case 4:
                menu_line0 = "Circle speed";
                enable_menunavigation = false;
                if(menulevel == 1) {
                    intvalue = circlespeed;
                    menulevel++;
                }
                if(handle_int(lastpressedkey, currentpressedkey, &intvalue, 1, 99, &save)) {
                    if(save) {
                        circlespeed = intvalue;
                    }
                    menulevel -= 2;
                }
                sprintf(tmpstr, "%d", intvalue);
                menu_line1 = tmpstr;
                break;
        }
    }

    print_menu(menu_line0, menu_line1);

    if(enable_menunavigation && lastpressedkey==KEY_NONE) {
        if(currentpressedkey==KEYBACK) {
            if(menulevel == 0) {
                clear_display();
                menuactive=false;
            } else {
                menulevel--;
            }
        }
        if(currentpressedkey==KEYOK) {
            clear_display();
            menulevel++;
            menuindex[menulevel] = 0;
        }        
        if(currentpressedkey==KEYUP) {
            if(menuindex[menulevel] > 0) {
                menuindex[menulevel]--;
            }
        }        
        if(currentpressedkey==KEYDOWN) {
            if((menuindex[menulevel]+1) < numitems_on_this_level) {
                menuindex[menulevel]++;
            }
        }        
    }

    lastpressedkey = currentpressedkey;
}