#include "serverinfo.h"
#include "pongboard.hh"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <cstdio>
#include <cerrno>
#include <csignal>
#include <cassert>
#include <atomic>
#include <thread>

static const char* pong_host = PONG_HOST;
static const char* pong_port = PONG_PORT;
static const char* pong_user = PONG_USER;
static struct addrinfo* pong_addr;


// TIME HELPERS
double start_time = 0;

// tstamp()
//    Return the current absolute time as a real number of seconds.
double tstamp() {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return now.tv_sec + now.tv_nsec * 1e-9;
}

// elapsed()
//    Return the number of seconds that have elapsed since `start_time`.
double elapsed() {
    return tstamp() - start_time;
}


// HTTP CONNECTION MANAGEMENT

// `http_connection::cstate` values
typedef enum http_connection_state {
    cstate_idle = 0,          // Waiting to send request
    cstate_waiting = 1,       // Sent request, waiting to receive response
    cstate_headers = 2,       // Receiving headers
    cstate_body = 3,          // Receiving body
    cstate_closed = -1,       // Body complete, connection closed
    cstate_broken = -2        // Parse error
} http_connection_state;


// http_connection
//    This object represents an open HTTP connection to a server.
struct http_connection {
    int fd_;                  // Socket file descriptor

    http_connection_state cstate_ = cstate_idle; // Connection state (see above)
    int status_code_;         // Response status code (e.g., 200, 402)
    size_t content_length_;   // Content-Length value
    bool has_content_length_; // true iff Content-Length was provided
    bool eof_ = false;        // true iff connection EOF has been reached

    char buf_[BUFSIZ];        // Response buffer
    size_t len_;              // Length of response buffer


    http_connection(int fd) {
        assert(fd >= 0);
        this->fd_ = fd;
    }
    ~http_connection() {
        close(this->fd_);
    }

    // disallow copying and assignment
    http_connection(const http_connection&) = delete;
    http_connection& operator=(const http_connection&) = delete;


    void send_request(const char* uri);
    void receive_response_headers();
    void receive_response_body();
    char* truncate_response();
    bool process_response_headers();
    bool check_response_body();
};


// http_connect(ai)
//    Open a new connection to the server described by `ai`. Returns a new
//    `http_connection` object for that server connection. Exits with an
//    error message if the connection fails.
http_connection* http_connect(const struct addrinfo* ai) {
    // connect to the server
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(1);
    }

    int yes = 1;
    (void) setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    int r = connect(fd, ai->ai_addr, ai->ai_addrlen);
    if (r < 0) {
        perror("connect");
        exit(1);
    }

    // construct an http_connection object for this connection
    return new http_connection(fd);
}


// http_close(conn)
//    Close the HTTP connection `conn` and free its resources.
void http_close(http_connection* conn) {
    delete conn;
}


// http_connection::send_request(conn, uri)
//    Send an HTTP POST request for `uri` to this connection.
//    Exit on error.
void http_connection::send_request(const char* uri) {
    assert(this->cstate_ == cstate_idle);

    // prepare and write the request
    char reqbuf[BUFSIZ];
    size_t reqsz = snprintf(reqbuf, sizeof(reqbuf),
                            "POST /%s/%s HTTP/1.0\r\n"
                            "Host: %s\r\n"
                            "Connection: keep-alive\r\n"
                            "\r\n",
                            pong_user, uri, pong_host);
    assert(reqsz < sizeof(reqbuf));

    size_t pos = 0;
    while (pos < reqsz) {
        ssize_t nw = write(this->fd_, &reqbuf[pos], reqsz - pos);
        if (nw == 0) {
            break;
        } else if (nw == -1 && errno != EINTR && errno != EAGAIN) {
            perror("write");
            exit(1);
        } else if (nw != -1) {
            pos += nw;
        }
    }

    if (pos != reqsz) {
        fprintf(stderr, "%.3f sec: connection closed prematurely\n",
                elapsed());
        exit(1);
    }

    // clear response information
    this->cstate_ = cstate_waiting;
    this->status_code_ = -1;
    this->content_length_ = 0;
    this->has_content_length_ = false;
    this->len_ = 0;
}


// http_connection::receive_response_headers()
//    Read the server's response headers and set `status_code_`
//    to the server's status code. If the connection terminates
//    prematurely, `status_code_` is set to -1.
void http_connection::receive_response_headers() {
    assert(this->cstate_ != cstate_idle);
    if (this->cstate_ < 0) {
        return;
    }
    this->buf_[0] = 0;

    // read & parse data until `http_process_response_headers`
    // tells us to stop
    while (this->process_response_headers()) {
        ssize_t nr = read(this->fd_, &this->buf_[this->len_], BUFSIZ);
        if (nr == 0) {
            this->eof_ = true;
        } else if (nr == -1 && errno != EINTR && errno != EAGAIN) {
            perror("read");
            exit(1);
        } else if (nr != -1) {
            this->len_ += nr;
            this->buf_[this->len_] = 0;  // null-terminate
        }
    }

    // Status codes >= 500 mean we are overloading the server
    // and should exit.
    if (this->status_code_ >= 500) {
        fprintf(stderr, "%.3f sec: exiting because of "
                "server status %d (%s)\n", elapsed(),
                this->status_code_, this->truncate_response());
        exit(1);
    }
}


// http_connection::receive_response_body()
//    Read the server's response body. On return, `buf_` holds the
//    response body, which is `len_` bytes long and has been
//    null-terminated.
void http_connection::receive_response_body() {
    assert(this->cstate_ < 0 || this->cstate_ == cstate_body);
    if (this->cstate_ < 0) {
        return;
    }
    // NB: buf_ might contain some body data already!

    // read response body (check_response_body tells us when to stop)
    while (this->check_response_body()) {
        ssize_t nr = read(this->fd_, &this->buf_[this->len_], BUFSIZ);
        if (nr == 0) {
            this->eof_ = true;
        } else if (nr == -1 && errno != EINTR && errno != EAGAIN) {
            perror("read");
            exit(1);
        } else if (nr != -1) {
            this->len_ += nr;
            this->buf_[this->len_] = 0;  // null-terminate
        }
    }
}


// http_connection::truncate_response()
//    Truncate the response text to a manageable length and return
//    that truncated text. Useful for error messages.
char* http_connection::truncate_response() {
    char* eol = strchr(this->buf_, '\n');
    if (eol) {
        *eol = 0;
    }
    if (strnlen(this->buf_, 100) >= 100) {
        this->buf_[100] = 0;
    }
    return this->buf_;
}


// MAIN PROGRAM

bool move_done;

// pong_thread(x, y)
//    Connect to the server at the position `x, y`.
void pong_thread(int x, int y) {
    char url[256];
    snprintf(url, sizeof(url), "move?x=%d&y=%d&style=on", x, y);

    http_connection* conn = http_connect(pong_addr);
    conn->send_request(url);
    conn->receive_response_headers();
    if (conn->status_code_ != 200) {
        fprintf(stderr, "%.3f sec: warning: %d,%d: "
                "server returned status %d (expected 200)\n",
                elapsed(), x, y, conn->status_code_);
    }

    conn->receive_response_body();
    double result = strtod(conn->buf_, nullptr);
    if (result < 0) {
        fprintf(stderr, "%.3f sec: server returned error: %s\n",
                elapsed(), conn->truncate_response());
        exit(1);
    }

    http_close(conn);

    // signal the main thread to continue
    // XXX The handout code uses polling and has data races. For full credit,
    // replace this with a synchronization object that supports blocking.
    move_done = true;
    // and exit!
}


// usage()
//    Explain how pong61 should be run.
static void usage() {
    fprintf(stderr, "Usage: ./pong61 [-h HOST] [-p PORT] [USER]\n");
    exit(1);
}


// main(argc, argv)
//    The main loop.
int main(int argc, char** argv) {
    // parse arguments
    int ch;
    bool nocheck = false, fast = false;
    while ((ch = getopt(argc, argv, "nfh:p:u:")) != -1) {
        if (ch == 'h') {
            pong_host = optarg;
        } else if (ch == 'p') {
            pong_port = optarg;
        } else if (ch == 'u') {
            pong_user = optarg;
        } else if (ch == 'n') {
            nocheck = true;
        } else if (ch == 'f') {
            fast = true;
        } else {
            usage();
        }
    }
    if (optind == argc - 1) {
        pong_user = argv[optind];
    } else if (optind != argc) {
        usage();
    }

    // look up network address of pong server
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    int r = getaddrinfo(pong_host, pong_port, &hints, &pong_addr);
    if (r != 0) {
        fprintf(stderr, "problem looking up %s: %s\n",
                pong_host, gai_strerror(r));
        exit(1);
    }

    // reset pong board and get its dimensions
    int width, height, delay = 100000;
    {
        http_connection* conn = http_connect(pong_addr);
        if (!nocheck && !fast) {
            conn->send_request("reset");
        } else {
            char buf[256];
            sprintf(buf, "reset?nocheck=%d&fast=%d", nocheck, fast);
            conn->send_request(buf);
        }
        conn->receive_response_headers();
        conn->receive_response_body();
        int nchars;
        if (conn->status_code_ != 200
            || sscanf(conn->buf_, "%d %d %n", &width, &height, &nchars) < 2
            || width <= 0 || height <= 0) {
            fprintf(stderr, "bad response to \"reset\" RPC: %d %s\n",
                    conn->status_code_, conn->truncate_response());
            exit(1);
        }
        (void) sscanf(conn->buf_ + nchars, "%d", &delay);
        http_close(conn);
    }
    // measure future times relative to this moment
    start_time = tstamp();

    // print display URL
    printf("Display: http://%s:%s/%s/%s\n",
           pong_host, pong_port, pong_user,
           nocheck ? " (NOCHECK mode)" : "");

    // play game
    pong_board board(width, height);
    pong_ball ball(board, 0, 0, 1, 1);

    while (1) {
        // create a new thread to handle the next position
        // (wrapped in a try-catch block to catch exceptions)
        std::thread th;
        try {
            th = std::thread(pong_thread, ball.x_, ball.y_);
        } catch (std::system_error err) {
            fprintf(stderr, "%.3f sec: cannot create thread: %s\n",
                    elapsed(), err.what());
            exit(1);
        }
        th.detach();

        // wait until that thread signals us to continue
        // XXX The handout code uses polling. For full credit, replace this
        // with a blocking-aware synchronization object.
        while (!move_done) {
            usleep(20000); // *sort of* blocking...
        }
        move_done = false;

        // update position
        ball.move();

        // wait 0.1sec
        usleep(delay);
    }
}


// HTTP PARSING

// http_connection::process_response_headers()
//    Parse the response represented by `conn->buf`. Returns true
//    if more header data remains to be read, false if all headers
//    have been consumed.
bool http_connection::process_response_headers() {
    size_t i = 0;
    while ((this->cstate_ == cstate_waiting || this->cstate_ == cstate_headers)
           && i + 2 <= this->len_) {
        if (this->buf_[i] == '\r' && this->buf_[i + 1] == '\n') {
            this->buf_[i] = 0;
            if (this->cstate_ == cstate_waiting) {
                int minor;
                if (sscanf(this->buf_, "HTTP/1.%d %d",
                           &minor, &this->status_code_) == 2) {
                    this->cstate_ = cstate_headers;
                } else {
                    this->cstate_ = cstate_broken;
                }
            } else if (i == 0) {
                this->cstate_ = cstate_body;
            } else if (strncasecmp(this->buf_, "Content-Length: ", 16) == 0) {
                this->content_length_ = strtoul(this->buf_ + 16, nullptr, 0);
                this->has_content_length_ = true;
            }
            // We just consumed a header line (i+2) chars long.
            // Move the rest of the data down, including terminating null.
            memmove(this->buf_, this->buf_ + i + 2, this->len_ - (i + 2) + 1);
            this->len_ -= i + 2;
            i = 0;
        } else {
            ++i;
        }
    }

    if (this->eof_) {
        this->cstate_ = cstate_broken;
    }
    return this->cstate_ == cstate_waiting || this->cstate_ == cstate_headers;
}


// http_connection::check_response_body()
//    Returns true if more response data should be read into `buf_`,
//    false otherwise (the connection is broken or the response is complete).
bool http_connection::check_response_body() {
    if (this->cstate_ == cstate_body
        && (this->has_content_length_ || this->eof_)
        && this->len_ >= this->content_length_) {
        this->cstate_ = cstate_idle;
    }
    if (this->eof_) {
        if (this->cstate_ == cstate_idle) {
            this->cstate_ = cstate_closed;
        } else if (this->cstate_ != cstate_closed) {
            this->cstate_ = cstate_broken;
        }
    }
    return this->cstate_ == cstate_body;
}
