#include "io61.hh"

// Usage: ./blockcat61 [-b BLOCKSIZE] [-o OUTFILE] [FILE]
//    Copies the input FILE to standard output in blocks.
//    Default BLOCKSIZE is 4096.

int main(int argc, char* argv[]) {
    // Parse arguments
    io61_arguments args(argc, argv, "b:o:i:");
    size_t block_size = args.block_size ? args.block_size : 4096;

    // Allocate buffer, open files
    char* buf = new char[block_size];

    io61_profile_begin();
    io61_file* inf = io61_open_check(args.input_file, O_RDONLY);
    io61_file* outf = io61_open_check(args.output_file,
                                      O_WRONLY | O_CREAT | O_TRUNC);

    // Copy file data
    while (1) {
        ssize_t amount = io61_read(inf, buf, block_size);
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
