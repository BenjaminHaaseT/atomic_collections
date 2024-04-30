#include <stdbool.h>
#ifndef RCU_H
#define RCU_H
// #include <stdatomic.h>

typedef struct {
    _Atomic unsigned int ref_count;
    void *data_ptr;
} rcunode_t;

void rcunode_init(rcunode_t *, void *);
void rcunode_inc_ref_count(rcunode_t *);
void *rcunode_cpy(rcunode_t *, void *(*cpy)(void*));
void free_rcunode(rcunode_t *);

struct rcu_stack_node {
    rcunode_t *data;
    struct rcu_stack_node *next;
};

void rcu_stack_node_init(struct rcu_stack_node *, rcunode_t *);
void free_rcu_stack_node(struct rcu_stack_node *restrict);

typedef struct {
    _Atomic unsigned int state;
    _Atomic bool epoch_flag;
    rcunode_t *_Atomic data;
    struct rcu_stack_node *_Atomic cur_epoch_stack;
    struct rcu_stack_node *_Atomic final_epoch_stack;
    void *(*cpy)(void*);
} rcu_t;

void rcu_init(rcu_t *, void *(*cpy)(void*));
void rcu_init_with(rcu_t *, void *(*cpy)(void*), void *data);
void *rcu_read(rcu_t *);
void rcu_update(rcu_t *, void *);
void rcu_push(rcu_t *, rcunode_t *);
void free_rcu(rcu_t *);

#endif