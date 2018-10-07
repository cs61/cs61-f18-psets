#include "io61.hh"
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

struct message_set {
    int request_batch;
    size_t request_size;
    size_t response_size;
};

static const struct message_set messages[] = {
    { 1, 100, 100 },
    { 1, 10000, 10000 },
    { 20, 100, 100 },
    { 20, 10000, 10000 },
    { 1, 100, 100 },
    { 1, 10000, 10000 },
    { 20, 100, 100 },
    { 20, 10000, 10000 }
};

// Requester algorithm:
//    for (i = 0; i < request_batch; ++i) {
//        send request of size request_size;
//    }
//    for (i = 0; i < request_batch; ++i) {
//        receive reply;
//    }

// Responder algorithm:
//    for (i = 0; i < request_batch; ++i) {
//        receive request;
//        send reply of size response_size;
//    }


static size_t max_message_size() {
    size_t nmessages = sizeof(messages) / sizeof(messages[0]);
    size_t sz = 0;

    for (size_t mindex = 0; mindex < nmessages; ++mindex) {
        if (messages[mindex].request_size > sz) {
            sz = messages[mindex].request_size;
        }
        if (messages[mindex].response_size > sz) {
            sz = messages[mindex].response_size;
        }
    }

    return sz;
}

void requester(io61_file* outf, io61_file* inf) {
    size_t nmessages = sizeof(messages) / sizeof(messages[0]);
    size_t maxsz = max_message_size();

    char* buf = new char[maxsz];
    memset(buf, 0, maxsz);

    size_t requestid = 0;
    size_t responseid = 0;
    size_t id;

    for (size_t mindex = 0; mindex < nmessages; ++mindex) {
        const struct message_set* m = &messages[mindex];
        printf("requester: phase %zd/%zd\n", mindex, nmessages);
        for (int i = 0; i < m->request_batch; ++i) {
            memcpy(buf, &requestid, sizeof(size_t));
            ++requestid;
            ssize_t r = io61_write(outf, buf, m->request_size);
            assert((size_t) r == m->request_size);
        }
        int x = io61_flush(outf);
        assert(x >= 0);
        for (int i = 0; i < m->request_batch; ++i) {
            ssize_t r = io61_read(inf, buf, m->response_size);
            assert((size_t) r == m->response_size);
            memcpy(&id, buf, sizeof(size_t));
            assert(id == responseid);
            ++responseid;
        }
    }

    printf("requester: done!\n");
    io61_close(inf);
    io61_close(outf);
    delete[] buf;
    exit(0);
}

void responder(io61_file* outf, io61_file* inf) {
    size_t nmessages = sizeof(messages) / sizeof(messages[0]);
    size_t maxsz = max_message_size();
    char* buf = new char[maxsz];
    memset(buf, 0, maxsz);

    for (size_t mindex = 0; mindex < nmessages; ++mindex) {
        const struct message_set* m = &messages[mindex];
        for (int i = 0; i < m->request_batch; ++i) {
            ssize_t r = io61_read(inf, buf, m->request_size);
            assert((size_t) r == m->request_size);
            r = io61_write(outf, buf, m->response_size);
            assert((size_t) r == m->response_size);
            int x = io61_flush(outf);
            assert(x >= 0);
        }
    }

    io61_close(inf);
    io61_close(outf);
    delete[] buf;
    exit(0);
}

int main(int argc, char* argv[]) {
    (void) argc, (void) argv;

    // create a connected socket pair for communicating between processes
    int request_fds[2], response_fds[2];
    int r1 = pipe(request_fds), r2 = pipe(response_fds);
    if (r1 < 0 || r2 < 0) {
        perror("pipe");
        exit(1);
    }

    // fork two children
    pid_t p1 = fork();
    if (p1 == 0) {
        close(request_fds[0]);
        close(response_fds[1]);
        requester(io61_fdopen(request_fds[1], O_WRONLY),
                  io61_fdopen(response_fds[0], O_RDONLY));
    } else if (p1 < 0) {
        perror("fork");
        exit(1);
    }

    pid_t p2 = fork();
    if (p2 == 0) {
        close(request_fds[1]);
        close(response_fds[0]);
        responder(io61_fdopen(response_fds[1], O_WRONLY),
                  io61_fdopen(request_fds[0], O_RDONLY));
    } else if (p2 < 0) {
        perror("fork");
        exit(1);
    }

    time_t start_time = time(0);
    while ((p1 > 0 || p2 > 0) && time(0) < start_time + 5) {
        int status;
        if (p1 > 0 && waitpid(p1, &status, WNOHANG) == p1) {
            printf("requester exits with status %d\n",
                   WIFEXITED(status) ? WEXITSTATUS(status) : -1);
            p1 = -1;            /* child1 has died */
        }
        if (p2 > 0 && waitpid(p2, &status, WNOHANG) == p2) {
            printf("responder exits with status %d\n",
                   WIFEXITED(status) ? WEXITSTATUS(status) : -1);
            p2 = -1;            /* child2 has died */
        }
    }

    if (p1 > 0) {
        kill(p1, SIGKILL);
    }
    if (p2 > 0) {
        kill(p2, SIGKILL);
    }
    exit(p1 < 0 && p2 < 0 ? 0 : 1);
}
