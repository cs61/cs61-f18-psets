#include "io61.hh"

// Usage: ./stridecat61 [-b BLOCKSIZE] [-t STRIDE] [-s SIZE]
//                      [-o OUTFILE] [FILE]
//    Copies the input FILE to OUTFILE in blocks, shuffling its
//    contents. Reads FILE in a strided access pattern, but writes
//    sequentially. Default BLOCKSIZE is 1 and default STRIDE is
//    1024. This means the input file's bytes are read in the sequence
//    0, 1024, 2048, ..., 1, 1025, 2049, ..., etc.

int main(int argc, char* argv[]) {
    // Parse arguments
    io61_arguments args(argc, argv, "b:t:s:o:");
    size_t block_size = args.block_size ? args.block_size : 1;

    // Allocate buffer, open files, measure file sizes
    char* buf = new char[block_size];

    io61_profile_begin();
    io61_file* inf = io61_open_check(args.input_file, O_RDONLY);

    if ((ssize_t) args.input_size < 0) {
        args.input_size = io61_filesize(inf);
    }
    if ((ssize_t) args.input_size < 0) {
        fprintf(stderr, "stridecat61: can't get size of input file\n");
        exit(1);
    }
    if (io61_seek(inf, 0) < 0) {
        fprintf(stderr, "stridecat61: input file is not seekable\n");
        exit(1);
    }

    io61_file* outf = io61_open_check(args.output_file,
                                      O_WRONLY | O_CREAT | O_TRUNC);

    // Copy file data
    size_t pos = 0, written = 0;
    while (written < args.input_size) {
        // Copy a block
        ssize_t amount = io61_read(inf, buf, block_size);
        if (amount <= 0) {
            break;
        }
        io61_write(outf, buf, amount);
        written += amount;

        // Move `inf` file position to next stride
        pos += args.stride;
        if (pos >= args.input_size) {
            pos = (pos % args.stride) + block_size;
            if (pos + block_size > args.stride) {
                block_size = args.stride - pos;
            }
        }
        int r = io61_seek(inf, pos);
        assert(r >= 0);
    }

    io61_close(inf);
    io61_close(outf);
    io61_profile_end();
    delete[] buf;
}
