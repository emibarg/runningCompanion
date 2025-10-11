/*
 * events.c
 *
 *  Created on: Oct 7, 2025
 *      Author: emi
 */


#include "events.h"

#define EVENT_QUEUE_SIZE 8

static volatile EventType queue[EVENT_QUEUE_SIZE];
static volatile uint8_t head = 0;
static volatile uint8_t tail = 0;

void Events_Init(void) {
    head = tail = 0;
}

void Events_Push(EventType ev) {
    uint8_t next = (head + 1) % EVENT_QUEUE_SIZE;
    if (next != tail) { // not full
        queue[head] = ev;
        head = next;
    }
}

EventType Events_Pop(void) {
    if (head == tail) return EVENT_NONE; // empty
    EventType ev = queue[tail];
    tail = (tail + 1) % EVENT_QUEUE_SIZE;
    return ev;
}

uint8_t Events_HasPending(void) {
    return head != tail;
}
