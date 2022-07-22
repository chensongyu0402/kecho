#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include <linux/module.h>
#include <linux/workqueue.h>
#include <net/sock.h>

#define MODULE_NAME "kecho"

struct echo_server_param {
    struct socket *listen_sock;
};

struct echo_service {
    bool is_stopped;
    struct list_head worker;
};

struct kecho {
    struct socket *sock;
    struct list_head list;
    struct work_struct kecho_work;
};

extern int echo_server_daemon(void *);

#endif

#ifndef RUNTIME_STAT /* RUNTIME_STAT */
#define RUNTIME_STAT
#include <linux/atmioc.h>
enum {
    TRACE_nop = 0,
    TRACE_send_msg,
    TRACE_recv_msg,
    TRACE_shdn_msg,
    TRACE_kmal_err,
    TRACE_recv_err,
    TRACE_send_err,
    TRACE_work_err,
    TRACE_acpt_err,
};

struct runtime_statistics {
    atomic_t send_msg, recv_msg, shdn_msg;
    atomic_t kmal_err, recv_err, send_err;
    atomic_t work_err, acpt_err;
};
extern struct runtime_statistics stats;

#define TRACE(ops)                                   \
    do {                                             \
        if (TRACE_##ops)                             \
            atomic_fetch_add_relaxed(1, &stats.ops); \
    } while (0)

#define TRACE_PRINT(flags, ops) \
    printk(flags MODULE_NAME ": %-10s: %d\n", #ops, stats.ops.counter);

#endif /* RUNTIME_STAT */