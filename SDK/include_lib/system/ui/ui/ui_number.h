#ifndef UI_NUMBER_H
#define UI_NUMBER_H


#include "ui/control.h"
#include "ui/ui_core.h"
#include "ui/p.h"

enum {
    HEX,
    DECIMAL,
    BINARY,
};

struct unumber {
    u8 numbs;
    u32 number[2];
};

struct ui_number {
    struct element_text text;
    u16 number[2];
    u16 buf[20];

    u16 color;
    u8 css_num;
    u8 nums;
    u16 css[2];
    const struct ui_number_info *info;
    const struct element_event_handler *handler;
};

void ui_number_enable();
void *new_ui_number(const void *_info, struct element *parent);
int ui_number_update_by_id(int id, struct unumber *n);

#endif

