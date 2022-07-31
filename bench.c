#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define TARGET_HOST "127.0.0.1"
#define TARGET_PORT 12345
#define BENCH_COUNT 50
#define BENCHMARK_RESULT_FILE "bench.txt"

/* length of unique message (TODO below) should shorter than this */
#define MAX_MSG_LEN 32
#define MIN_MSG_LEN 16
#if MAX_MSG_LEN == MIN_MSG_LEN
#define MASK(num) ((MAX_MSG_LEN - 1))
#elif MIN_MSG_LEN == 0
#if (MAX_MSG_LEN & (MAX_MSG_LEN - 1)) == 0
#define MASK(num) ((num & (MAX_MSG_LEN - 1)))
#else
#define MASK(num) (num % MAX_MSG_LEN)
#endif
#else
#define MASK(num) ((num % (MAX_MSG_LEN - MIN_MSG_LEN) + MIN_MSG_LEN))
#endif

/*
 * Too much concurrent connection would be treated as sort of DDoS attack
 * (mainly caused by configs (kernel: "tcp_max_syn_backlog" and
 * "somaxconn". Application (kecho): "backlog"). Nevertheless, default
 * maximum number of fd per-process is 1024. If you insist to proceed
 * the benchmarking with "MAX_THREAD" larger than these limitation,
 * perform following modifications:
 *
 * (1)
 * Use following commands to adjust kernel attributes:
 *     a. "$ sudo sysctl net.core.somaxconn={depends_on_MAX_THREAD}"
 *     b. "$ sudo sysctl net.ipv4.tcp_max_syn_backlog={ditto}"
 * Note that "$ sudo sysctl net.core.somaxconn" can get current value.
 * "somaxconn" is max amount of established connection, whereas
 * "tcp_max_syn_backlog" is max amount of connection at first step
 * of TCP 3-way handshake (SYN).
 *
 * (2)
 * Use command "$ ulimit -n {ditto}" to enlarge limitation of fd
 * per-process. Note that this modification only effects on process
 * which executes the command and its child processes.
 *
 * (3)
 * Specify "backlog" with value as large as "net.ipv4.tcp_max_syn_backlog".
 *
 * Remember to reset the modifications after benchmarking to keep
 * stability of running machine
 */
#define MAX_THREAD 1000

/*
 * TODO: provide unique message (maybe generate dynamically, or somehow) for
 * each worker could produce benchmarking result which is more conforms to
 * realworld usage.
 */
static pthread_t pt[MAX_THREAD];

/* block all workers before they are all ready to benchmarking kecho */
static int ready;

static pthread_mutex_t res_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t worker_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t worker_wait = PTHREAD_COND_INITIALIZER;

static long time_res[MAX_THREAD] = {0};
static int idx = 0; /* for indexing "time_res" */
static FILE *bench_fd;

static inline long time_diff_us(struct timeval *start, struct timeval *end)
{
    return ((end->tv_sec - start->tv_sec) * 1000000) +
           (end->tv_usec - start->tv_usec);
}

static void *bench_worker(void *str)
{
    int sock_fd;
    char *reqstr = str;
    char recstr[MAX_MSG_LEN];
    struct timeval start, end;

    /* wait until all workers created */
    pthread_mutex_lock(&worker_lock);
    ready += 1;
    if (ready == MAX_THREAD) {
        pthread_cond_broadcast(&worker_wait);
    } else {
        while (ready < MAX_THREAD)
            if (pthread_cond_wait(&worker_wait, &worker_lock)) {
                puts("pthread_cond_wait failed");
                exit(-1);
            }
    }
    pthread_mutex_unlock(&worker_lock);
    /* all workers are ready, let's start bombing the server */

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        exit(-1);
    }

    struct sockaddr_in info = {
        .sin_family = PF_INET,
        .sin_addr.s_addr = inet_addr(TARGET_HOST),
        .sin_port = htons(TARGET_PORT),
    };

    if (connect(sock_fd, (struct sockaddr *) &info, sizeof(info)) == -1) {
        perror("connect");
        exit(-1);
    }

    int recvlen = 0, msglen, reqlen = strlen(reqstr);
    gettimeofday(&start, NULL);
    send(sock_fd, reqstr, reqlen, 0);
    while ((msglen = recv(sock_fd, recstr + recvlen, MAX_MSG_LEN, 0)) &&
           (recvlen += msglen) < reqlen)
        ;
    gettimeofday(&end, NULL);

    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);

    if (strncmp(reqstr, recstr, reqlen)) {
        puts("echo message validation failed");
        exit(-1);
    }

    pthread_mutex_lock(&res_lock);
    time_res[idx++] += time_diff_us(&start, &end);
    pthread_mutex_unlock(&res_lock);
    free(reqstr);

    pthread_exit(NULL);
}

static char *rand_string()
{
    int r = MASK(rand());
    char *str = calloc(r + 1, 1);
    for (int i = 0; i < r; i++) {
        char c = 97 + rand() % 26;
        str[i] = c;
    }
    return str;
}

static void create_worker(int thread_qty)
{
    srand(time(NULL));
    for (int i = 0; i < thread_qty; i++) {
        char *str = rand_string();
        if (pthread_create(&pt[i], NULL, bench_worker, str)) {
            puts("thread creation failed");
            exit(-1);
        }
    }
}

static void bench(void)
{
    for (int i = 0; i < BENCH_COUNT; i++) {
        ready = 0;

        create_worker(MAX_THREAD);

        for (int x = 0; x < MAX_THREAD; x++)
            pthread_join(pt[x], NULL);

        idx = 0;
    }

    for (int i = 0; i < MAX_THREAD; i++)
        fprintf(bench_fd, "%d %ld\n", i, time_res[i] /= BENCH_COUNT);
}

int main(void)
{
    bench_fd = fopen(BENCHMARK_RESULT_FILE, "w");
    if (!bench_fd) {
        perror("fopen");
        return -1;
    }

    bench();

    fclose(bench_fd);

    return 0;
}