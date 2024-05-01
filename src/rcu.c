#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "rcu.h"


void rcunode_init(rcunode_t *node, void *data)
{
    node->data_ptr = data;
    node->ref_count = 1;
}

void rcunode_inc_ref_count(rcunode_t *node)
{
    atomic_fetch_add_explicit(&(node->ref_count), 1, memory_order_relaxed);
}

void *rcunode_cpy(rcunode_t *node, void *(*cpy)(void*))
{
    return cpy(node->data_ptr);
}

void free_rcunode(rcunode_t *node)
{
    if (atomic_fetch_sub_explicit(&(node->ref_count), 1, memory_order_release) == 1)
    {
        // match all previous releases to ensure we get the correct memory ordering
        atomic_thread_fence(memory_order_acquire);
        free(node->data_ptr);
        node->data_ptr = NULL;
        free(node);
    }
}

void rcu_stack_node_init(struct rcu_stack_node *sn, rcunode_t *data)
{
    sn->data = data;
    sn->next = NULL;
}

void free_rcu_stack_node(struct rcu_stack_node *restrict sn)
{
    while (sn)
    {
        struct rcu_stack_node *temp = sn->next;
        sn->next = NULL;
        if (sn->data)
            free_rcunode(sn->data);
        free(sn);
        sn = temp;
    }
}

void rcu_init(rcu_t *rcu, void *(*cpy)(void*))
{
    rcu->state = 0;
    rcu->epoch_flag = false;
    rcu->data = NULL;
    rcu->cur_epoch_stack = NULL;
    rcu->final_epoch_stack = NULL;
    rcu->cpy = cpy;
}

void rcu_init_with(rcu_t *rcu, void *(*cpy)(void*), void *data)
{
    rcu->state = 0;
    rcu->epoch_flag = false;
    rcu->data = malloc(sizeof(rcunode_t));
    rcunode_init(rcu->data, data);
    rcu->cur_epoch_stack = NULL;
    rcu->final_epoch_stack = NULL;
    rcu->cpy = cpy;
}

void *rcu_read(rcu_t *rcu)
{
    atomic_fetch_add_explicit(&(rcu->state), 1, memory_order_relaxed);

    // read the whatever data is current
    rcunode_t *cur = atomic_load_explicit(&(rcu->data), memory_order_relaxed);
    if (cur)
        rcunode_inc_ref_count(cur);

    // update the state, check if new epoch should begin
    if (
        atomic_fetch_sub_explicit(&(rcu->state), 1, memory_order_release) == 1 &&
        !atomic_exchange_explicit(&(rcu->epoch_flag), true, memory_order_release)
    )
    {
        // synchronizes with all previous release subs and stores/exchanges
        atomic_thread_fence(memory_order_acquire);
        struct rcu_stack_node *old_cur_epoch_stack = atomic_exchange_explicit(&(rcu->cur_epoch_stack), NULL, memory_order_relaxed);
        struct rcu_stack_node *old_final_epoch_stack = atomic_exchange_explicit(&(rcu->final_epoch_stack), old_cur_epoch_stack, memory_order_relaxed);
        // free old final epoch stack
        free_rcu_stack_node(old_final_epoch_stack);
        atomic_store_explicit(&(rcu->epoch_flag), false, memory_order_release);
    }

    if (cur)
    {
        // copy data out from current node
        void *res = rcu->cpy(cur->data_ptr);
        free_rcunode(cur);
        return res;
    }

    return NULL;
}

void rcu_update(rcu_t *rcu, void *data)
{
    rcunode_t *neo = malloc(sizeof(rcunode_t));
    rcunode_init(neo, data);

    // keep attempting to update untill successful
    rcunode_t *cur = atomic_load_explicit(&(rcu->data), memory_order_relaxed);
    while (!atomic_compare_exchange_strong_explicit(&(rcu->data), &cur, neo, memory_order_relaxed, memory_order_relaxed));

    // push the node that was original current data onto cur epoch stack
    rcu_push(rcu, cur);
}

void rcu_push(rcu_t *rcu, rcunode_t *node)
{
    struct rcu_stack_node *neo = malloc(sizeof(struct rcu_stack_node));
    rcu_stack_node_init(neo, node);

    struct rcu_stack_node *cur = atomic_load_explicit(&(rcu->cur_epoch_stack), memory_order_relaxed);
    neo->next = cur;

    while (!atomic_compare_exchange_strong_explicit(&(rcu->cur_epoch_stack), &cur, neo, memory_order_relaxed, memory_order_relaxed))
        neo->next = cur;
}

void free_rcu(rcu_t *rcu)
{
    struct rcu_stack_node *old_final_epoch_stack = atomic_exchange_explicit(&(rcu->final_epoch_stack), NULL, memory_order_relaxed);
    free_rcu_stack_node(old_final_epoch_stack);
    struct rcu_stack_node *old_cur_epoch_stack = atomic_exchange_explicit(&(rcu->cur_epoch_stack), NULL, memory_order_relaxed);
    free_rcu_stack_node(old_cur_epoch_stack);
    rcunode_t *cur = atomic_exchange_explicit(&(rcu->data), NULL, memory_order_relaxed);
    if (cur)
        free_rcunode(cur);
    free(rcu);
}