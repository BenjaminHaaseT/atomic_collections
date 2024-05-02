#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>

#include "queue.h"


void queue_node_init(struct queue_node *node, void *data)
{
    atomic_store_explicit(&(node->data), data, memory_order_relaxed);
    atomic_store_explicit(&(node->next), NULL, memory_order_relaxed);
}

void free_queue_node(struct queue_node *node)
{
    void *data = atomic_load_explicit(&(node->data), memory_order_relaxed);
    if (data)
        free(data);
    atomic_store_explicit(&(node->next), NULL, memory_order_relaxed);
    free(node);
}

void queue_epoch_node_init(struct queue_epoch_node *node, struct queue_node *data)
{
    node->data = data;
    node->next = NULL;
}

void free_queue_epoch_node(struct queue_epoch_node *restrict node)
{
    while (node)
    {
        struct queue_epoch_node *temp = node->next;
        node->next = NULL;
        if (node->data)
        {
            free_queue_node(node->data);
            node->data = NULL;
        }
        free(node);
        node = temp;
    }
}

void atm_queue_init(atm_queue_t *q)
{
    q->state = 0;
    q->epoch_flag = false;
    struct queue_node *init = malloc(sizeof(struct queue_node));
    queue_node_init(init, NULL);
    q->head = init;
    q->tail = init;
    q->cur_epoch_stack = NULL;
    q->final_epoch_stack = NULL;
}

void atm_queue_enqueue(atm_queue_t *q, void *data)
{
    // create new node for the queue
    struct queue_node *neo = malloc(sizeof(struct queue_node));
    queue_node_init(neo, data);

    while (1)
    {
        // load current tail and attempt to replace it's next pointer
        struct queue_node *cur_tail = atomic_load_explicit(&(q->tail), memory_order_acquire);
        struct queue_node *cur_tail_next = NULL;
        if (atomic_compare_exchange_strong_explicit(&(cur_tail->next), &cur_tail_next, neo, memory_order_relaxed, memory_order_relaxed))
        {
            // only the thread that is successful at updating the next pointer of tail gets to update the tail
            atomic_store_explicit(&(q->tail), neo, memory_order_release);
            break;
        }
    }
}