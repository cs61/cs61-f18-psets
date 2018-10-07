#include "io61.hh"

// Usage: ./reordercat61 [-b BLOCKSIZE] [-r RANDOMSEED] [-s SIZE]
//                       [-o OUTFILE] [FILE]
//    Copies the input FILE to OUTFILE in blocks. The blocks are
//    transferred in random order, but the resulting output file
//    should be the same as the input. Default BLOCKSIZE is 4096.

int main(int argc, char* argv[]) {
    // Parse arguments
    srandom(83419);
    io61_arguments args(argc, argv, "b:r:s:o:i:");
    size_t block_size = args.block_size ? args.block_size : 4096;

    // Allocate buffer, open files, measure file sizes
    char* buf = new char[block_size];

    io61_profile_begin();
    io61_file* inf = io61_open_check(args.input_file, O_RDONLY);

    if ((ssize_t) args.input_size < 0) {
        args.input_size = io61_filesize(inf);
    }
    if ((ssize_t) args.input_size < 0) {
        fprintf(stderr, "reordercat61: can't get size of input file\n");
        exit(1);
    }
    if (io61_seek(inf, 0) < 0) {
        fprintf(stderr, "reordercat61: input file is not seekable\n");
        exit(1);
    }

    io61_file* outf = io61_open_check(args.output_file,
                                      O_WRONLY | O_CREAT | O_TRUNC);
    if (io61_seek(outf, 0) < 0) {
        fprintf(stderr, "reordercat61: output file is not seekable\n");
        exit(1);
    }

    // Calculate random permutation of file's blocks
    size_t nblocks = args.input_size / block_size;
    if (nblocks > (30 << 20)) {
        fprintf(stderr, "reordercat61: file too large\n");
        exit(1);
    } else if (nblocks * block_size != args.input_size) {
        fprintf(stderr, "reordercat61: input file size not a multiple of block size\n");
        exit(1);
    }

    size_t* blockpos = new size_t[nblocks];
    for (size_t i = 0; i < nblocks; ++i) {
        blockpos[i] = i;
    }

    // Copy file data
    while (nblocks != 0) {
        // Choose block to read
        size_t index = random() % nblocks;
        size_t pos = blockpos[index] * block_size;
        blockpos[index] = blockpos[nblocks - 1];
        --nblocks;

        // Transfer that block
        io61_seek(inf, pos);
        ssize_t amount = io61_read(inf, buf, block_size);
        if (amount <= 0) {
            break;
        }
        io61_seek(outf, pos);
        io61_write(outf, buf, amount);
    }

    io61_close(inf);
    io61_close(outf);
    io61_profile_end();
    delete[] buf;
    delete[] blockpos;
}
