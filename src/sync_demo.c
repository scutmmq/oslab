#include "oslab/sync_demo.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

#define PC_BUFFER_SIZE 5
#define PC_ITEMS 8
#define RW_READERS 3
#define RW_WRITERS 2
#define PHILOSOPHERS 5
#define PHILOSOPHER_ROUNDS 2

typedef struct {
    int buffer[PC_BUFFER_SIZE];
    int in;
    int out;
    pthread_mutex_t mutex;
    sem_t empty_slots;
    sem_t full_slots;
} ProducerConsumerState;

static void *producer_thread(void *arg) {
    ProducerConsumerState *state = (ProducerConsumerState *)arg;
    for (int item = 1; item <= PC_ITEMS; ++item) {
        sem_wait(&state->empty_slots);
        pthread_mutex_lock(&state->mutex);
        state->buffer[state->in] = item;
        printf("生产者放入: %d\n", item);
        state->in = (state->in + 1) % PC_BUFFER_SIZE;
        pthread_mutex_unlock(&state->mutex);
        sem_post(&state->full_slots);
    }
    return NULL;
}

static void *consumer_thread(void *arg) {
    ProducerConsumerState *state = (ProducerConsumerState *)arg;
    for (int i = 0; i < PC_ITEMS; ++i) {
        sem_wait(&state->full_slots);
        pthread_mutex_lock(&state->mutex);
        const int item = state->buffer[state->out];
        printf("消费者取出: %d\n", item);
        state->out = (state->out + 1) % PC_BUFFER_SIZE;
        pthread_mutex_unlock(&state->mutex);
        sem_post(&state->empty_slots);
    }
    return NULL;
}

int run_producer_consumer_demo(void) {
    ProducerConsumerState state = {0};
    pthread_t producer;
    pthread_t consumer;

    pthread_mutex_init(&state.mutex, NULL);
    sem_init(&state.empty_slots, 0, PC_BUFFER_SIZE);
    sem_init(&state.full_slots, 0, 0);

    pthread_create(&producer, NULL, producer_thread, &state);
    pthread_create(&consumer, NULL, consumer_thread, &state);
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);

    sem_destroy(&state.empty_slots);
    sem_destroy(&state.full_slots);
    pthread_mutex_destroy(&state.mutex);
    return 0;
}

typedef struct {
    int value;
    int reader_count;
    pthread_mutex_t reader_count_mutex;
    sem_t write_lock;
} ReadersWritersState;

static void *reader_thread(void *arg) {
    ReadersWritersState *state = (ReadersWritersState *)arg;
    for (int round = 0; round < 2; ++round) {
        pthread_mutex_lock(&state->reader_count_mutex);
        ++state->reader_count;
        if (state->reader_count == 1) {
            sem_wait(&state->write_lock);
        }
        pthread_mutex_unlock(&state->reader_count_mutex);

        printf("读者读取共享值: %d\n", state->value);

        pthread_mutex_lock(&state->reader_count_mutex);
        --state->reader_count;
        if (state->reader_count == 0) {
            sem_post(&state->write_lock);
        }
        pthread_mutex_unlock(&state->reader_count_mutex);
    }
    return NULL;
}

static void *writer_thread(void *arg) {
    ReadersWritersState *state = (ReadersWritersState *)arg;
    for (int round = 0; round < 2; ++round) {
        sem_wait(&state->write_lock);
        ++state->value;
        printf("写者写入共享值: %d\n", state->value);
        sem_post(&state->write_lock);
    }
    return NULL;
}

int run_readers_writers_demo(void) {
    ReadersWritersState state = {0};
    pthread_t readers[RW_READERS];
    pthread_t writers[RW_WRITERS];

    pthread_mutex_init(&state.reader_count_mutex, NULL);
    sem_init(&state.write_lock, 0, 1);

    for (int i = 0; i < RW_READERS; ++i) {
        pthread_create(&readers[i], NULL, reader_thread, &state);
    }
    for (int i = 0; i < RW_WRITERS; ++i) {
        pthread_create(&writers[i], NULL, writer_thread, &state);
    }
    for (int i = 0; i < RW_READERS; ++i) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < RW_WRITERS; ++i) {
        pthread_join(writers[i], NULL);
    }

    sem_destroy(&state.write_lock);
    pthread_mutex_destroy(&state.reader_count_mutex);
    return 0;
}

typedef struct {
    sem_t forks[PHILOSOPHERS];
} DiningState;

typedef struct {
    DiningState *state;
    int id;
} PhilosopherArg;

static void *dining_worker(void *arg) {
    PhilosopherArg *philosopher = (PhilosopherArg *)arg;
    DiningState *state = philosopher->state;
    const int id = philosopher->id;
    const int left = id;
    const int right = (id + 1) % PHILOSOPHERS;
    const int first = left < right ? left : right;
    const int second = left < right ? right : left;

    for (int round = 0; round < PHILOSOPHER_ROUNDS; ++round) {
        printf("哲学家 %d 思考\n", id);
        sem_wait(&state->forks[first]);
        sem_wait(&state->forks[second]);
        printf("哲学家 %d 进餐\n", id);
        sem_post(&state->forks[second]);
        sem_post(&state->forks[first]);
    }
    return NULL;
}

int run_dining_philosophers_demo(void) {
    DiningState state;
    pthread_t philosophers[PHILOSOPHERS];
    PhilosopherArg args[PHILOSOPHERS];

    for (int i = 0; i < PHILOSOPHERS; ++i) {
        sem_init(&state.forks[i], 0, 1);
        args[i].state = &state;
        args[i].id = i;
        pthread_create(&philosophers[i], NULL, dining_worker, &args[i]);
    }
    for (int i = 0; i < PHILOSOPHERS; ++i) {
        pthread_join(philosophers[i], NULL);
        sem_destroy(&state.forks[i]);
    }
    return 0;
}
