#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>

// ... (Other necessary includes and definitions)

int main() {
    int shm_fd = shm_open("/my_shared_barrier", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(pthread_barrier_t));
    pthread_barrier_t *barrier = (pthread_barrier_t *)mmap(NULL, sizeof(pthread_barrier_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    pthread_barrierattr_t attr;
    pthread_barrierattr_init(&attr);
    pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

    pthread_barrier_init(barrier, &attr, 2); // Set the count to the number of participating processes

    // Now the barrier can be used with pthread_barrier_wait(barrier) in both processes

    // ... (Code to synchronize processes)

    // Destroy the barrier and cleanup when done
    pthread_barrier_destroy(barrier);
    munmap(barrier, sizeof(pthread_barrier_t));
    close(shm_fd);
    shm_unlink("/my_shared_barrier");

    return 0;
}
