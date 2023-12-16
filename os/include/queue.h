
#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

#define MAX_QUEUE_SIZE 10

struct queue_t {
	uint32_t slots;
	uint32_t head;
	uint32_t tail;
	struct pcb_t * proc[MAX_QUEUE_SIZE];
	uint32_t size;
};

void enqueue(struct queue_t * q, struct pcb_t * proc);

struct pcb_t * dequeue(struct queue_t * q);

int empty(struct queue_t * q);
int exhaust(struct queue_t * q);
#endif

