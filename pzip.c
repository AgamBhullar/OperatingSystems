#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/sysinfo.h>

typedef struct {
    char *data;
    size_t start;
    size_t end;
} FileSegment;

typedef struct {
    FileSegment *segments; // Dynamic array of FileSegments
    int front, rear, size;
    int capacity;
    pthread_mutex_t mutex;
    pthread_cond_t cond_var;
} SharedQueue;

typedef struct {
    SharedQueue *queue;
    char *data;
    size_t start;
    size_t end;
    size_t file_size;
    char last_char;
    size_t last_char_count;
} ThreadArg;

volatile int exit_condition = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // Initialize the lock statically

// Function prototypes
int queue_is_empty(SharedQueue *q);
FileSegment dequeue(SharedQueue *q);

// Thread function 
void *compressPart(void *arg) {
    ThreadArg *threadArg = (ThreadArg *) arg;
    SharedQueue *queue = threadArg->queue;
    char *base_data = threadArg->data;  

    //printf("Thread starting, base_data: %p\n", (void *)base_data); // Debug Line

    while (true) {
        pthread_mutex_lock(&queue->mutex);

        while (queue_is_empty(queue) && !exit_condition) {
            pthread_cond_wait(&queue->cond_var, &queue->mutex);
        }

        if (exit_condition && queue_is_empty(queue)) {
            pthread_mutex_unlock(&queue->mutex);
            break;
        }

        FileSegment segment = dequeue(queue);
        //printf("Dequeued segment, start: %zu, end: %zu\n", segment.start, segment.end); // Debug Line
        pthread_mutex_unlock(&queue->mutex);

        char *segment_data = base_data + segment.start;
        size_t segment_length = segment.end - segment.start;
        //printf("Processing segment from %p to %p\n", (void *)segment_data, (void *)(base_data + segment.end)); // Debug Line

        for (size_t i = 0; i < segment_length;) {
            char current_char = segment_data[i];
            size_t count = 0;

            while (i < segment_length && segment_data[i] == current_char) {
                count++;
                i++;
            }

            //printf("Character: %c, Count: %zu, Index: %zu\n", current_char, count, i); // Debug Line
            pthread_mutex_lock(&lock);
            fprintf(stdout, "%zu%c\n", count, current_char);
            pthread_mutex_unlock(&lock);
        }
    }

    //printf("Thread finishing\n"); // Debug Line
    return NULL;
}

void queue_init(SharedQueue *q, int capacity) {
    q->segments = malloc(sizeof(FileSegment) * capacity);
    q->capacity = capacity;
    q->front = q->size = 0; 
    q->rear = capacity - 1;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond_var, NULL);
}

int queue_is_full(SharedQueue *q) {
    return (q->size == q->capacity);
}

int queue_is_empty(SharedQueue *q) {
    return (q->size == 0);
}

void enqueue(SharedQueue *q, FileSegment item) {
    if (queue_is_full(q))
        return;
    q->rear = (q->rear + 1) % q->capacity;
    q->segments[q->rear] = item;
    q->size = q->size + 1;
    pthread_cond_signal(&q->cond_var);
}

FileSegment dequeue(SharedQueue *q) {
    if (queue_is_empty(q))
        return (FileSegment){0}; // Return an empty segment
    FileSegment item = q->segments[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size = q->size - 1;
    return item;
}

void queue_destroy(SharedQueue *q) {
    free(q->segments);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond_var);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2 ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_threads = get_nprocs(); 
    const size_t segment_size = 1024 * 1024; // 1MB per segment

    for (int i = 1; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1) {
            perror("Error opening file");
            continue;
        }

        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            perror("Error getting file size");
            close(fd);
            continue;
        }

        if (sb.st_size == 0) { // Skip empty files
            close(fd);
            continue;
        }

        char *data = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
        if (data == MAP_FAILED) {
            perror("Error mapping file");
            close(fd);
            continue;
        }

        size_t num_segments = (sb.st_size + segment_size - 1) / segment_size;
        SharedQueue queue;
        queue_init(&queue, num_segments);

        pthread_t threads[num_threads];
        ThreadArg args[num_threads];
        for (int j = 0; j < num_threads; ++j) {
            args[j].queue = &queue;
            args[j].data = data; // Correctly pass the base pointer
            if (pthread_create(&threads[j], NULL, compressPart, &args[j]) != 0) {
                perror("Error creating thread");
                // Handle thread creation failure
            }
        }

        for (size_t offset = 0; offset < sb.st_size; offset += segment_size) {
            FileSegment segment = {
                .start = offset,
                .end = (offset + segment_size > sb.st_size) ? sb.st_size : offset + segment_size
            };
            enqueue(&queue, segment);
        }

        pthread_mutex_lock(&queue.mutex);
        exit_condition = 1;
        pthread_cond_broadcast(&queue.cond_var);
        pthread_mutex_unlock(&queue.mutex);

        for (int j = 0; j < num_threads; ++j) {
            if (pthread_join(threads[j], NULL) != 0) {
                perror("Error joining thread");
            }
        }

        munmap(data, sb.st_size);
        close(fd);
        queue_destroy(&queue);
    }

    pthread_mutex_destroy(&lock); // Destroy the lock
    return 0;
}


