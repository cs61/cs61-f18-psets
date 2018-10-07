#include "io61.hh"

// Usage: ./ostridecat61 [-b BLOCKSIZE] [-t STRIDE] [-o OUTFILE] [FILE]
//    Copies the input FILE to OUTFILE in blocks, shuffling its
//    contents. Reads FILE sequentially, but writes to its output in a
//    strided access pattern. Default BLOCKSIZE is 1 and default STRIDE is
//    1024. This means the output file's bytes are written in the sequence
//    0, 1024, 2048, ..., 1, 1025, 2049, ..., etc.

int main(int argc, char* argv[]) {
    // Parse arguments
    io61_arguments args(argc, argv, "b:t:o:i:");
    size_t block_size = args.block_size ? args.block_size : 1;

    // Allocate buffer, open files, measure file sizes
    char* buf = new char[block_size];

    io61_profile_begin();
    io61_file* inf = io61_open_check(args.input_file, O_RDONLY);

    args.input_size = io61_filesize(inf);
    if ((ssize_t) args.input_size < 0) {
        fprintf(stderr, "ostridecat61: input file is not seekable\n");
        exit(1);
    }

    io61_file* outf = io61_open_check(args.output_file,
                                      O_WRONLY | O_CREAT | O_TRUNC);
    if (io61_seek(outf, 0) < 0) {
        fprintf(stderr, "ostridecat61: output file is not seekable\n");
        exit(1);
    }

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

        // Move `outf` file position to next stride
        pos += args.stride;
        if (pos >= args.input_size) {
            pos = (pos % args.stride) + block_size;
            if (pos + block_size > args.stride) {
                block_size = args.stride - pos;
            }
        }
        int r = io61_seek(outf, pos);
        assert(r >= 0);
    }

    io61_close(inf);
    io61_close(outf);
    io61_profile_end();
    delete[] buf;
}
