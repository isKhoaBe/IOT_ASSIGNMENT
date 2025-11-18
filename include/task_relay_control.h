#ifndef __TASK_RELAY_CONTROL0_H__
#define __TASK_RELAY_CONTROL0_H__

#include <freertos/queue.h>

typedef struct
{
    int gpioPin;
    bool newState;
} RelayControlCommand;

void Relay_Control_Task(void *pvParameters);

#endif // __TASK_RELAY_CONTROL0_H__