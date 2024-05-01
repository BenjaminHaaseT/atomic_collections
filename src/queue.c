#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>


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