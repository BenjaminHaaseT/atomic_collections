#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include "rcu.h"

struct thread_params {
    rcu_t *rcu;
    char c;
    size_t iter;
};

void *cpy(void *data)
{
    // printf("copying data\n");
    int *buf = (int*)data;
    int *new_buf = calloc(26, sizeof(int));
    for (size_t i = 0; i < 26; i++)
        new_buf[i] = buf[i];
    return new_buf;
}

void *thread_body(void *arg)
{
    struct thread_params *params = (struct thread_params*) arg;
    rcu_t *rcu = params->rcu;
    char c = params->c;
    size_t iter = params->iter;

    for (size_t i = 0; i < iter; i++)
    {
        // Read from rcu
        int *cur = (int*) rcu_read(rcu);
        if (!cur)
            cur = calloc(26, sizeof(int));

        // increment count of threads character
        cur[c % 'a']++;
        // Now attempt to update
        rcu_update(rcu, cur);
    }

    return NULL;
}

int test_rcu_init_with()
{
    rcu_t *rcu = malloc(sizeof(rcu_t));
    int *init = calloc(26, sizeof(int));
    rcu_init_with(rcu, cpy, init);
    pthread_t threads[26];
    struct thread_params params[26];

    printf("Spawning threads...\n");
    for (int i = 0; i < 26; i++)
    {
       params[i].rcu = rcu;
       params[i].c = (char)(i + 'a');
       params[i].iter = 10000;

        printf("spawning thread: %c\n", (char)(i + 'a'));
        int status;
        if ((status = pthread_create(threads + i, NULL, thread_body, params + i)))
        {
            fprintf(stderr, "unable to create thread: (%d) %s\n", status, strerror(status));
            return 1;
        }
    }
    printf("threads spawned.\n");

    printf("Joining threads...\n");
    for (int i = 0; i < 26; i++)
    {
        int status;
        if ((status = pthread_join(threads[i], NULL)))
        {
            fprintf(stderr, "unable to join thread: (%d) %s\n", status, strerror(status));
            return 1;
        }
    }

    printf("threads joined.\n");
    printf("Results:\n");
    int *res = rcu_read(rcu);
    for (size_t i = 0; i < 26; i++)
    {
        printf("\t%c: %d\n", (char)i + 'a', res[i]);
    }

    free(res);
    free_rcu(rcu);
    return 0;
}

int test_rcu_without_init()
{
    rcu_t *rcu = malloc(sizeof(rcu_t));
    // use rcu_init() instead
    rcu_init(rcu, cpy);
    pthread_t threads[26];
    struct thread_params params[26];

    printf("Spawning threads...\n");
    for (int i = 0; i < 26; i++)
    {
       params[i].rcu = rcu;
       params[i].c = (char)(i + 'a');
       params[i].iter = 10000;

        printf("spawning thread: %c\n", (char)(i + 'a'));
        int status;
        if ((status = pthread_create(threads + i, NULL, thread_body, params + i)))
        {
            fprintf(stderr, "unable to create thread: (%d) %s\n", status, strerror(status));
            return 1;
        }
    }

    printf("threads spawned.\n");

    printf("Joining threads...\n");
    for (int i = 0; i < 26; i++)
    {
        int status;
        if ((status = pthread_join(threads[i], NULL)))
        {
            fprintf(stderr, "unable to join thread: (%d) %s\n", status, strerror(status));
            return 1;
        }
    }

    printf("threads joined.\n");
    printf("Results:\n");
    int *res = rcu_read(rcu);
    for (size_t i = 0; i < 26; i++)
    {
        printf("\t%c: %d\n", (char)i + 'a', res[i]);
    }

    free(res);
    free_rcu(rcu);
    return 0;
}


int main(void)
{
    printf("Testing rcu with initial data...\n");
    if(test_rcu_init_with())
        return 1;

    printf("Testing rcu without initial data...\n");
    if (test_rcu_without_init())
        return 1;
        
    return 0;
}
