#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define SHM_NAME "/my_shared_memory"
#define SEM_NAME "/my_semaphore"
#define SHM_SIZE sizeof(SharedMemoryState)

typedef struct SharedMemoryState {
    pthread_mutex_t mutex;
    int counter;
} SharedMemoryState;

int main() {
    int shm_fd;
    sem_t *sem;
    SharedMemoryState *state;

    // Initialize a named semaphore with a value of 1.
    // The process that first locks this semaphore will perform the initialization.
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    // Lock the semaphore. If the semaphore's value is 0, this will block
    // until another process releases it.
    sem_wait(sem);

    // Attempt to create the shared memory segment
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd < 0) {
        // The shared memory already exists, just open it
        shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (shm_fd < 0) {
            perror("shm_open");
            sem_post(sem);
            sem_close(sem);
            exit(EXIT_FAILURE);
        }
    } else {
        // Set the size of the shared memory segment
        if (ftruncate(shm_fd, SHM_SIZE) < 0) {
            perror("ftruncate");
            sem_post(sem);
            sem_close(sem);
            close(shm_fd);
            exit(EXIT_FAILURE);
        }
    }

    // Map the shared memory in the address space of the process
    state = (SharedMemoryState *)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (state == MAP_FAILED) {
        perror("mmap");
        sem_post(sem);
        sem_close(sem);
        close(shm_fd);
        exit(EXIT_FAILURE);
    }
    
    // The first process to lock the semaphore will initialize the mutex.
    if (shm_fd > 0) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&state->mutex, &attr);
    }

    // Release the semaphore
    sem_post(sem);

    // Increment the counter
    pthread_mutex_lock(&state->mutex);
    state->counter += 1;
    printf("Counter incremented to %d by process %d\n", state->counter, getpid());
    pthread_mutex_unlock(&state->mutex);

    // Clean up
    munmap(state, SHM_SIZE);
    close(shm_fd);
    sem_close(sem);
    // Do not unlink the semaphore here. It should be done in a deliberate teardown process.

    return 0;
}
