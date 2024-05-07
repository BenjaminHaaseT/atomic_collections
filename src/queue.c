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

void *atm_queue_dequeue(atm_queue_t *q)
{
    // increment state, to notify other threads data is being read 
    atomic_fetch_add_explicit(&(q->state), 1, memory_order_relaxed);
    void *res = NULL;

    while (1)
    {
        // read current data held in head, this pointer will never be null
        struct queue_node *cur_head = atomic_load_explicit(&(q->head), memory_order_acquire);
        struct queue_node *cur_head_next = atomic_load_explicit(&(cur_head->next), memory_order_relaxed);
        if (cur_head_next == NULL)
        {
            // we have an empty queue, sentinel node points to NULL
            break;
        }

        // for exchanging with the next nodes data
        void *cur_data = atomic_exchange_explicit(&(cur_head_next->data), NULL, memory_order_relaxed);
        if (cur_data != NULL)
        {
            // this thread gets to update the current head of the queue
            res = cur_data;
            atm_queue_push_epoch(q, cur_head);

            // now update head of the queue
            atomic_store_explicit(&(q->head), cur_head_next, memory_order_release);
            break;
        }       
    }

    // exit the read state
    if (
        atomic_fetch_sub_explicit(&(q->state), 1, memory_order_release) == 1 &&
        !atomic_exchange_explicit(&(q->epoch_flag), true, memory_order_release)
    )
    {
        // if we enter this block acquire on all previous release updates to state and epoch flag
        atomic_thread_fence(memory_order_acquire);

        struct queue_epoch_node *old_cur_epoch_stack = atomic_exchange_explicit(&(q->cur_epoch_stack), NULL, memory_order_relaxed);
        struct queue_epoch_node *old_final_epoch_stack = atomic_exchange_explicit(&(q->final_epoch_stack), old_cur_epoch_stack, memory_order_relaxed);

        // free nodes in the current epoch
        free_queue_epoch_node(old_final_epoch_stack);

        // finally reset epoch flag
        atomic_store_explicit(&(q->epoch_flag), false, memory_order_release);
    }

    return res;
}

void atm_queue_push_epoch(atm_queue_t *q, struct queue_node *node)
{
    // create new epoch node to add to the current epoch stack
    struct queue_epoch_node *neo = malloc(sizeof(struct queue_epoch_node));
    queue_epoch_node_init(neo, node);

    struct queue_epoch_node *cur_stack = atomic_load_explicit(&(q->cur_epoch_stack), memory_order_relaxed);
    neo->next = cur_stack;

    while (!atomic_compare_exchange_strong_explicit(&(q->cur_epoch_stack), &cur_stack, neo, memory_order_relaxed, memory_order_relaxed))
        neo->next = cur_stack;
}

void free_atm_queue(atm_queue_t *q)
{
    struct queue_epoch_node *old_final_epoch_stack = atomic_exchange_explicit(&(q->final_epoch_stack), NULL, memory_order_relaxed);
    free_queue_epoch_node(old_final_epoch_stack);
    struct queue_epoch_node *old_cur_epoch_stack = atomic_exchange_explicit(&(q->cur_epoch_stack), NULL, memory_order_relaxed);
    free_queue_epoch_node(old_cur_epoch_stack);

    struct queue_node *cur = atomic_load_explicit(&(q->head), memory_order_relaxed);
    while (cur)
    {
        struct queue_node *temp = atomic_load_explicit(&(cur->next), memory_order_relaxed);
        free_queue_node(cur);
        cur = temp;
    }

    free(q);
}

void free_atm_queue_auto(atm_queue_t *q)
{
    struct queue_epoch_node *old_final_epoch_stack = atomic_exchange_explicit(&(q->final_epoch_stack), NULL, memory_order_relaxed);
    free_queue_epoch_node(old_final_epoch_stack);
    struct queue_epoch_node *old_cur_epoch_stack = atomic_exchange_explicit(&(q->cur_epoch_stack), NULL, memory_order_relaxed);
    free_queue_epoch_node(old_cur_epoch_stack);

    struct queue_node *cur = atomic_load_explicit(&(q->head), memory_order_relaxed);
    while (cur)
    {
        struct queue_node *temp = atomic_load_explicit(&(cur->next), memory_order_relaxed);
        free_queue_node(cur);
        cur = temp;
    }
}