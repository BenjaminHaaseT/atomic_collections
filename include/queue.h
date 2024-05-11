#include <stdbool.h>
#ifndef QUEUE_H
#define QUEUE_H

struct queue_node {
    void *_Atomic data;
    struct queue_node *_Atomic next;
};

void queue_node_init(struct queue_node *, void *);
void free_queue_node(struct queue_node *);

struct queue_epoch_node {
    struct queue_node *data;
    struct queue_epoch_node *next;
};

void queue_epoch_node_init(struct queue_epoch_node *, struct queue_node *);
void free_queue_epoch_node(struct queue_epoch_node *restrict);

typedef struct {
    _Atomic unsigned int state;
    _Atomic bool epoch_flag;
    struct queue_node *_Atomic head;
    struct queue_node *_Atomic tail;
    struct queue_epoch_node *_Atomic cur_epoch_stack;
    struct queue_epoch_node *_Atomic final_epoch_stack;
} atm_queue;

void atm_queue_init(atm_queue *);
void *atm_queue_dequeue(atm_queue *);
void atm_queue_enqueue(atm_queue *, void *);
void atm_queue_push_epoch(atm_queue *, struct queue_node *);
void free_atm_queue(atm_queue *);
void free_atm_queue_auto(atm_queue *);

#endif