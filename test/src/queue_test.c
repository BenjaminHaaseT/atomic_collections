#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "queue.h"

struct single_producer_args {
    atm_queue_t *q;
    int niter;
    int id;
};

struct single_consumer_args {
    atm_queue_t *q;
    int nvals;
};

int test_queue_single_threaded()
{
    atm_queue_t q;
    atm_queue_init(&q);

    for (int i = 0; i < 10000; i++)
    {
        int *val = malloc(sizeof(int));
        *val = i;
        atm_queue_enqueue(&q, (void*)val);
    }

    for (int i = 0; i < 10000; i++)
    {
        int *val = (int*) atm_queue_dequeue(&q);
        if (*val != i)
        {
            fprintf(stderr, "unexpected value when dequeuing: %d != %d\n", *val, i);
            return 1;
        }
        printf("%d: %d\n", *val, i);
        free(val);
    }

    free_atm_queue_auto(&q);

    return 0;
}


void *single_producer_thread_body(void *args)
{
    struct single_producer_args *ptr = (struct single_producer_args *)args;
    int niter = ptr->niter;
    int id = ptr->id;
    printf("Single producer thread %d executing...\n", id);

    for (int i = 0; i < niter; i++)
    {
        int *val = malloc(sizeof(int));
        *val = (7 * i) + id;
        atm_queue_enqueue(ptr->q, val);
    }

    return NULL;
}

void *single_consumer_thread_body(void *args)
{
    printf("Single consumer thread executing...\n");
    struct single_consumer_args *ptr = (struct single_consumer_args *)args;
    int nvals = ptr->nvals;

    // The number of non empty deqeue calls, empty dequeue calls and total number of calls made by the consumer
    int non_empty_count = 0;
    int empty_count = 0;
    int total = 0;

    while (non_empty_count < nvals)
    {
        int *val = atm_queue_dequeue(ptr->q);
        if (val == NULL)
        {
            empty_count++;
        }
        else
        {
            non_empty_count++;
            printf("consumer received: %d from %d\n", *val, *val % 7);
            free(val);
        }
        total++;
    }

    printf("Non empty dequeues: %d\n", non_empty_count);
    printf("Empty dequeues: %d\n", empty_count);
    printf("Total dequeues: %d\n", total);
    printf("Non empty to total: %.2lf\n", (double)non_empty_count / (double)total);

    return NULL;
}

int test_queue_multi_threaded_single_producer_single_consumer(int niter)
{
    atm_queue_t q;
    atm_queue_init(&q);
    pthread_t consumer_thread, producer_thread;

    struct single_producer_args producer_args;
    producer_args.q = &q;
    producer_args.niter = niter;
    producer_args.id = 1;

    struct single_consumer_args consumer_args;
    consumer_args.q = &q;
    consumer_args.nvals = niter;

    if (pthread_create(&producer_thread, NULL, single_producer_thread_body, &producer_args))
    {
        fprintf(stderr, "unable to spawn producer thread: (%d) %s\n", errno, strerror(errno));
        return 1;
    }

    if (pthread_create(&consumer_thread, NULL, single_consumer_thread_body, &consumer_args))
    {
        fprintf(stderr, "unable to spawn consumer thread: (%d) %s\n", errno, strerror(errno));
        return 1;
    }

    if (pthread_join(producer_thread, NULL))
    {
        fprintf(stderr, "unable to join producer thread: (%d) %s\n", errno, strerror(errno));
        return 1;
    }

    if (pthread_join(consumer_thread, NULL))
    {
        fprintf(stderr, "unable to join consumer thread: (%d) %s\n", errno, strerror(errno));
        return 1;
    }

    free_atm_queue_auto(&q);

    return 0;
}

int test_queue_multi_threaded_multi_producer_single_consumer(int niter)
{
    atm_queue_t q;
    atm_queue_init(&q);
    pthread_t consumer_thread, producer_threads[3];

    struct single_consumer_args consumer_args = { .q=&q, .nvals=3*niter };
    struct single_producer_args producer_args[3];
    producer_args[0] = (struct single_producer_args) { .q=&q, .niter=niter, .id=1 };
    producer_args[1] = (struct single_producer_args) { .q=&q, .niter=niter, .id=2 };
    producer_args[2] = (struct single_producer_args) { .q=&q, .niter=niter, .id=3 };

    // Spawn producers
    for (int i = 0; i < 3; i++)
    {
        if (pthread_create(producer_threads + i, NULL, single_producer_thread_body, producer_args + i))
        {
            fprintf(stderr, "unable to spawn producer thread: (%d) %s\n", errno, strerror(errno));
            return 1;
        }
    }

    // spawn consumer
    if (pthread_create(&consumer_thread, NULL, single_consumer_thread_body, &consumer_args))
    {
        fprintf(stderr, "unable to spawn consumer thread: (%d) %s\n", errno, strerror(errno));
        return 1;
    }

    // join producers
    for (int i = 0; i < 3; i++)
    {
        if (pthread_join(producer_threads[i], NULL))
        {
            fprintf(stderr, "error joining producer thread %d: (%d) %s\n", i + 1, errno, strerror(errno));
            return 1;
        }
    }

    if (pthread_join(consumer_thread, NULL))
    {
        fprintf(stderr, "error joining consumer thread: (%d) %s\n", errno, strerror(errno));
        return 1;
    }
    free_atm_queue_auto(&q);
    return 0;
}

int test_queue_multi_threaded_multi_producer_multi_consumer()
{
    atm_queue_t q;
    atm_queue_init(&q);
    pthread_t consumer_producer_threads[3], producer_threads[4];

    struct multi_consumer_args consumer_args[3];
    struct multi_producer_args producer_args[4];

    producer_args[0] = (struct single_producer_args) { .q=&q, .niter=niter, .id=1 };
    producer_args[1] = (struct single_producer_args) { .q=&q, .niter=niter, .id=2 };
    producer_args[2] = (struct single_producer_args) { .q=&q, .niter=niter, .id=3 };
}


int main(void)
{
    if (test_queue_single_threaded())
        return 1;

    if (test_queue_multi_threaded_single_producer_single_consumer(1000))
        return 1;

    if (test_queue_multi_threaded_multi_producer_single_consumer(1000))
        return 1;
    
    return 0;
}