#ifndef FANS_H
#define FANS_H

typedef enum {
    FAN_LEFT,
    FAN_RIGHT,
} fan_type_t;

void fans_initialize();
void fans_start(fan_type_t fan_type);
void fans_stop(fan_type_t fan_type);
void fans_set_speed(fan_type_t fan_type, float speed_rpm);

#endif