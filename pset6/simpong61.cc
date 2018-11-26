#include "pongboard.hh"
#include <unistd.h>
#include <sys/time.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <csignal>
#include <cassert>
#include <atomic>
#include <thread>
#include <random>


pong_board* main_board;
static unsigned long delay;


// ball_thread(ball)
//    This simple thread function just moves its ball forever.

void ball_thread(pong_ball* ball) {
    while (true) {
        if (ball->move() && delay) {
            usleep(delay);
        }
    }
}


// HELPER FUNCTIONS

// random_int(min, max, random_engine)
//    Returns a random number in the range [`min`, `max`], inclusive.
template <typename T>
int random_int(int min, int max, T& random_engine) {
    return std::uniform_int_distribution<int>(min, max)(random_engine);
}

// is_integer_string, is_real_string
//    Check whether `s` is a correctly-formatted decimal integer or
//    real number.
static bool is_integer_string(const char* s) {
    char* last;
    (void) strtol(s, &last, 10);
    return last != s && !*last && !isspace((unsigned char) *s);
}
static bool is_real_string(const char* s) {
    char* last;
    (void) strtod(s, &last);
    return last != s && !*last && !isspace((unsigned char) *s);
}

// usage()
//    Explain how simpong61 should be run.
static void usage() {
    fprintf(stderr, "\
Usage: ./simpong61 [-1] [-w WIDTH] [-h HEIGHT] [-b NBALLS] [-s NSTICKY]\n\
                   [-d MOVEPAUSE] [-p PRINTTIMER]\n");
    exit(1);
}

// sigusr1_handler()
//    Runs when `SIGUSR1` is received; prints out the current state of the
//    board to standard output. (This function accesses the board in a
//    thread-unsafe way; there is no easy way to fix this and you aren't
//    expected to fix it.)
void signal_handler(int) {
    char buf[BUFSIZ];
    if (main_board) {
        int nballs = 0;
        for (int y = 0; y < main_board->height_; ++y) {
            for (int x = 0; x < main_board->width_; ++x) {
                pong_cell& c = main_board->cell(x, y);
                char ch = '?';
                if (c.ball_) {
                    ch = 'O';
                    ++nballs;
                } else if (c.type_ == cell_empty) {
                    ch = '.';
                } else if (c.type_ == cell_sticky) {
                    ch = '_';
                } else if (c.type_ == cell_obstacle) {
                    ch = 'X';
                }
                if (x < BUFSIZ - 1) {
                    buf[x] = ch;
                }
            }
            buf[std::min(main_board->width_, BUFSIZ - 1)] = '\n';
            write(STDOUT_FILENO, buf, std::min(main_board->width_ + 1, BUFSIZ));
        }
        write(STDOUT_FILENO, "\n", 1);
    }
}

// main(argc, argv)
//    The main loop.
int main(int argc, char** argv) {
    // initialize random number generator
    std::default_random_engine random_engine{std::random_device()()};

    // print board on receiving SIGUSR1
    {
        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        int r = sigaction(SIGUSR1, &sa, nullptr);
        assert(r == 0);
        r = sigaction(SIGALRM, &sa, nullptr);
        assert(r == 0);
    }

    // parse arguments and check size invariants
    int width = 100, height = 31, nballs = 24, nsticky = 12;
    long print_interval = 0;
    bool single_threaded = false;
    int ch;
    while ((ch = getopt(argc, argv, "w:h:b:s:d:p:1")) != -1) {
        if (ch == 'w' && is_integer_string(optarg)) {
            width = strtol(optarg, nullptr, 10);
        } else if (ch == 'h' && is_integer_string(optarg)) {
            height = strtol(optarg, nullptr, 10);
        } else if (ch == 'b' && is_integer_string(optarg)) {
            nballs = strtol(optarg, nullptr, 10);
        } else if (ch == 's' && is_integer_string(optarg)) {
            nsticky = strtol(optarg, nullptr, 10);
        } else if (ch == 'd' && is_real_string(optarg)) {
            delay = (unsigned long) (strtod(optarg, nullptr) * 1000000);
        } else if (ch == 'p' && is_real_string(optarg)) {
            print_interval = (long) (strtod(optarg, nullptr) * 1000000);
        } else if (ch == '1') {
            single_threaded = true;
        } else {
            usage();
        }
    }
    if (optind != argc
        || width < 2
        || height < 2
        || nballs >= width * height
        || nsticky >= width * height) {
        usage();
    }
    if (print_interval > 0) {
        struct itimerval it;
        it.it_interval.tv_sec = print_interval / 1000000;
        it.it_interval.tv_usec = print_interval % 1000000;
        it.it_value = it.it_interval;
        int r = setitimer(ITIMER_REAL, &it, nullptr);
        assert(r == 0);
    }

    // create pong board
    pong_board board(width, height);
    main_board = &board;

    // create sticky locations
    for (int n = 0; n < nsticky; ++n) {
        int x, y;
        do {
            x = random_int(0, width - 1, random_engine);
            y = random_int(0, width - 1, random_engine);
        } while (board.cell(x, y).type_ != cell_empty);
        board.cell(x, y).type_ = cell_sticky;
    }

    // create balls
    std::vector<pong_ball*> balls;
    while ((int) balls.size() < nballs) {
        int x, y;
        do {
            x = random_int(0, width - 1, random_engine);
            y = random_int(0, height - 1, random_engine);
        } while (board.cell(x, y).ball_);

        int dx = random_int(0, 1, random_engine) ? 1 : -1;
        int dy = random_int(0, 1, random_engine) ? 1 : -1;

        balls.push_back(new pong_ball(board, x, y, dx, dy));
    }

    if (!single_threaded) {
        // create ball threads; run first ball on main thread
        for (size_t i = 1; i < balls.size(); ++i) {
            std::thread t(ball_thread, balls[i]);
            t.detach();
        }
        ball_thread(balls[0]);
    } else {
        // single threaded mode: one thread runs all balls
        while (true) {
            for (auto ball : balls) {
                ball->move();
            }
            if (delay) {
                usleep(delay);
            }
        }
    }
}
