#include <stdio.h>
#include <stdlib.h>

#ifndef CLIENT_LIST_H /* CLIENT_LIST_H */
#define CLIENT_LIST_H

typedef struct client_list_s {
    int client;
    char *addr;
    struct client_list_s *next;
} client_list_t;

void delete_list(client_list_t **list)
{
    while (*list) {
        client_list_t *delete = *list;
        *list = (*list)->next;
        free(delete->addr);
        free(delete);
    }
}

client_list_t *delete_client(client_list_t **list, int client)
{
    if (!(*list))
        return NULL;

    if ((*list)->client == client) {
        client_list_t *tmp = (*list)->next;
        free((*list)->addr);
        free((*list));
        return tmp;
    }

    (*list)->next = delete_client(&(*list)->next, client);
    return *list;
}

void server_err(const char *str, client_list_t **list)
{
    perror(str);
    delete_list(list);
    exit(-1);
}

client_list_t *new_list(int client, char *addr, client_list_t **list)
{
    client_list_t *new = (client_list_t *) malloc(sizeof(client_list_t));
    if (!new)
        server_err("Fail to allocate memory", list);
    new->addr = strdup(addr);
    if (!new->addr)
        server_err("Fail to duplicate string", list);
    new->client = client;
    new->next = NULL;
    return new;
}

void push_back_client(client_list_t **list, int client, char *addr)
{
    if (!(*list))
        *list = new_list(client, addr, list);
    else {
        client_list_t *tmp = *list;
        while (tmp->next)
            tmp = tmp->next;
        tmp->next = new_list(client, addr, list);
    }
}
#endif /* CLIENT_LIST_H */

#ifndef RUNTIME_STAT /* RUNTIME_STAT */
#define RUNTIME_STAT
#include <stdatomic.h>

enum {
    TRACE_nop = 0,
    TRACE_epll_cnt,
    TRACE_clnt_cnt,
    TRACE_msg_cnt,
    TRACE_send_msg,
    TRACE_recv_msg,
    TRACE_shdn_msg,
};

struct runtime_statistics {
    atomic_uint_fast64_t epll_cnt, clnt_cnt, msg_cnt;
    atomic_uint_fast64_t send_msg, recv_msg, shdn_msg;
};
extern struct runtime_statistics stats;

#ifndef atomic_fetch_add_relaxed
#define atomic_fetch_add_relaxed(x, a) \
    __atomic_fetch_add(x, a, __ATOMIC_RELAXED)
#endif

#define TRACE(ops)                                   \
    do {                                             \
        if (TRACE_##ops)                             \
            atomic_fetch_add_relaxed(&stats.ops, 1); \
    } while (0)


void do_analysis(void)
{
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#define TRACE_PRINT(ops) printf("%-10s: %ld\n", #ops, stats.ops);
    TRACE_PRINT(epll_cnt);
    TRACE_PRINT(clnt_cnt);
    TRACE_PRINT(msg_cnt);
    TRACE_PRINT(send_msg);
    TRACE_PRINT(recv_msg);
    TRACE_PRINT(shdn_msg);
#undef TRACE_PRINT
}
#define RUNTIME_STAT_INIT() atexit(do_analysis)

#endif /* RUNTIME_STAT */
