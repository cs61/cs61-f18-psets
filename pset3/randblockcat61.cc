#include "io61.hh"

// Usage: ./randblockcat61 [-b MAXBLOCKSIZE] [-r RANDOMSEED] [FILE]
//    Copies the input FILE to standard output in blocks. Each block has a
//    random size between 1 and MAXBLOCKSIZE (which defaults to 4096).

int main(int argc, char* argv[]) {
    // Parse arguments
    srandom(83419);
    io61_arguments args(argc, argv, "b:r:o:i:");
    size_t max_blocksize = args.block_size ? args.block_size : 4096;

    // Allocate buffer, open files
    char* buf = new char[max_blocksize];

    io61_profile_begin();
    io61_file* inf = io61_open_check(args.input_file, O_RDONLY);
    io61_file* outf = io61_open_check(args.output_file,
                                      O_WRONLY | O_CREAT | O_TRUNC);

    // Copy file data
    while (1) {
        size_t m = (random() % max_blocksize) + 1;
        ssize_t amount = io61_read(inf, buf, m);
        if (amount <= 0) {
            break;
        }
        io61_write(outf, buf, amount);
    }

    io61_close(inf);
    io61_close(outf);
    io61_profile_end();
    delete[] buf;
}
