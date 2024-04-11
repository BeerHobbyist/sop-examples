#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define SHM_NAME "/my_shared_memory"
#define SHM_SIZE sizeof(SharedMemoryState)

typedef struct SharedMemoryState {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int counter;
} SharedMemoryState;

// Function to increment the counter
void increment_counter(SharedMemoryState *state) {
    pthread_mutex_lock(&state->mutex);
    state->counter += 1;
    printf("Counter incremented to %d by PID %d\n", state->counter, getpid());
    pthread_cond_broadcast(&state->cond); // Notify all waiting processes
    pthread_mutex_unlock(&state->mutex);
}

int main() {
    int init = 0; // Flag to indicate if this process should initialize the shared state

    // Try to create the shared memory object
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd < 0) {
        // If the object already exists, open it without the O_CREAT | O_EXCL flags
        shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
        if (shm_fd < 0) {
            perror("shm_open");
            exit(EXIT_FAILURE);
        }
    } else {
        // If this process created the shared memory object, set the size
        if (ftruncate(shm_fd, SHM_SIZE) < 0) {
            perror("ftruncate");
            exit(EXIT_FAILURE);
        }
        init = 1; // This process will perform the initialization
    }

    // Map the shared memory in the address space of the process
    SharedMemoryState *state = (SharedMemoryState *)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (state == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    if (init) {
        // Initialize mutex and condition variable
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&state->mutex, &mutex_attr);

        pthread_condattr_t cond_attr;
        pthread_condattr_init(&cond_attr);
        pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
        pthread_cond_init(&state->cond, &cond_attr);

        // Initialize counter
        state->counter = 0;
    }

    // Increment the counter
    increment_counter(state);

    // Wait for other processes to increment the counter
    pthread_mutex_lock(&state->mutex);
    pthread_cond_wait(&state->cond, &state->mutex); // Wait for a signal that the counter has changed
    printf("Counter is now %d, as updated by another process.\n", state->counter);
    pthread_mutex_unlock(&state->mutex);

    // Unmap the shared memory
    munmap(state, SHM_SIZE);
    close(shm_fd);

    // The first process (which performed initialization) unlinks the shared memory
    if (init) {
        shm_unlink(SHM_NAME);
    }

    return 0;
}
