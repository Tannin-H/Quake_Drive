#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define INIT_CAPACITY 4

typedef struct {
    uint16_t speed;
    uint16_t acceleration;
    int32_t steps;
} movement_t;

typedef struct {
    movement_t *data;
    int front;
    int rear;
    int size;
    int capacity;
} queue_t;

queue_t *create_queue();
void enqueue(queue_t *queue, movement_t value);
movement_t dequeue(queue_t *queue);
int is_empty(queue_t *queue);
movement_t peek(queue_t *queue);
void clear_queue(queue_t *queue);

#endif // QUEUE_H
