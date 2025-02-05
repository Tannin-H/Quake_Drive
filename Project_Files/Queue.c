// Queue.c
#include "Queue.h"
#include <stdint.h>

queue_t *create_queue() {
    queue_t *queue = (queue_t *)malloc(sizeof(queue_t));
    if (!queue) {
        printf("Memory allocation failed\n");
        return NULL;
    }
    queue->capacity = INIT_CAPACITY;
    queue->front = 0;
    queue->rear = 0;
    queue->size = 0;
    queue->data = (movement_t *)malloc(queue->capacity * sizeof(movement_t));
    if (!queue->data) {
        printf("Memory allocation for data failed\n");
        free(queue);
        return NULL;
    }
    return queue;
}

static void resize_queue(queue_t *queue) {
    int new_capacity = queue->capacity * 2;
    movement_t *new_data = (movement_t *)realloc(queue->data, new_capacity * sizeof(movement_t));
    if (!new_data) {
        printf("Memory reallocation failed\n");
        return;
    }
    
    if (queue->front > queue->rear) {
        for (int i = 0; i < queue->rear; i++) {
            new_data[queue->capacity + i] = queue->data[i];
        }
        queue->rear += queue->capacity;
    }
    
    queue->data = new_data;
    queue->capacity = new_capacity;
}

void enqueue(queue_t *queue, movement_t value) {
    if (queue->size == queue->capacity) {
        resize_queue(queue);
    }
    queue->data[queue->rear] = value;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->size++;
}

movement_t dequeue(queue_t *queue) {
    if (queue->size == 0) {
        printf("Queue is empty\n");
        movement_t empty = {0, 0, 0};
        return empty;
    }
    movement_t value = queue->data[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    return value;
}

int is_empty(queue_t *queue) {
    return queue->size == 0;
}

movement_t peek(queue_t *queue) {
    if (queue->size == 0) {
        printf("Queue is empty\n");
        movement_t empty = {0, 0, 0};
        return empty;
    }
    return queue->data[queue->front];
}

void clear_queue(queue_t *queue) {
    free(queue->data);
    free(queue);
}
