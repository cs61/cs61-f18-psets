#ifndef PONGBOARD_HH
#define PONGBOARD_HH
#include <vector>
#include <mutex>
#include <condition_variable>
struct pong_ball;


enum pong_celltype {
    cell_empty,
    cell_sticky,
    cell_obstacle
};


struct pong_cell {
    pong_celltype type_ = cell_empty;  // type of cell
    pong_ball* ball_ = nullptr;        // pointer to ball currently in cell
};


struct pong_board {
    int width_;
    int height_;
    std::vector<pong_cell> cells_;     // `width_ * height_`, row-major order
    pong_cell obstacle_cell_;          // represents off-board positions


    // pong_board(width, height)
    //    Construct a new `width x height` pong board with all empty cells.
    pong_board(int width, int height)
        : width_(width), height_(height),
          cells_(width * height, pong_cell()) {
        obstacle_cell_.type_ = cell_obstacle;
    }

    // boards can't be copied, moved, or assigned
    pong_board(const pong_board&) = delete;
    pong_board& operator=(const pong_board&) = delete;


    // cell(x, y)
    //    Return a reference to the cell at position `x, y`. If there is
    //    no such position, returns `obstacle_cell_`, a cell containing an
    //    obstacle.
    pong_cell& cell(int x, int y) {
        if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_) {
            return obstacle_cell_;
        } else {
            return this->cells_[y * this->width_ + x];
        }
    }
};



struct pong_ball {
    pong_board& board_;
    int x_;
    int y_;
    int dx_;
    int dy_;


    // pong_ball(...)
    //    Construct a new ball on `board` with position `x, y` and direction
    //    `dx, dy`.
    pong_ball(pong_board& board, int x, int y, int dx, int dy)
        : board_(board), x_(x), y_(y), dx_(dx), dy_(dy) {
        board_.cell(x, y).ball_ = this;
    }

    // balls can't be copied, moved, or assigned
    pong_ball(const pong_ball&) = delete;
    pong_ball& operator=(const pong_ball&) = delete;


    // move()
    //    Move this ball once on its board. Return true iff the ball
    //    successfully moved.
    //
    //    This function is complex because it must consider obstacles,
    //    collisions, and sticky cells.
    //
    //    You should preserve its current logic while adding sufficient
    //    synchronization to make it thread-safe.
    bool move() {
        // assert that this ball is stored in the board correctly
        pong_board& board = this->board_;
        pong_cell& cur_cell = board.cell(this->x_, this->y_);
        assert(cur_cell.ball_ == this);

        // obstacle: change direction on hitting a board edge
        if (board.cell(this->x_ + this->dx_, this->y_).type_ == cell_obstacle) {
            this->dx_ = -this->dx_;
        }
        if (board.cell(this->x_, this->y_ + this->dy_).type_ == cell_obstacle) {
            this->dy_ = -this->dy_;
        }

        // check next cell
        pong_cell& next_cell = board.cell(this->x_ + this->dx_, this->y_ + this->dy_);
        if (next_cell.ball_) {
            // collision: change both balls' directions without moving them
            if (next_cell.ball_->dx_ != this->dx_) {
                next_cell.ball_->dx_ = this->dx_;
                this->dx_ = -this->dx_;
            }
            if (next_cell.ball_->dy_ != this->dy_) {
                next_cell.ball_->dy_ = this->dy_;
                this->dy_ = -this->dy_;
            }
            return false;
        } else if (next_cell.type_ == cell_obstacle) {
            // obstacle: reverse direction
            this->dx_ = -this->dx_;
            this->dy_ = -this->dy_;
            return false;
        } else if (this->dx_ == 0 && this->dy_ == 0) {
            // on sticky cell: unsuccessful move
            return false;
        } else {
            // otherwise, move into the next cell
            this->x_ += this->dx_;
            this->y_ += this->dy_;
            cur_cell.ball_ = nullptr;
            next_cell.ball_ = this;
            // stop if the next cell is sticky
            if (next_cell.type_ == cell_sticky) {
                this->dx_ = this->dy_ = 0;
            }
            return true;
        }
    }
};

#endif
