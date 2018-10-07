#include "io61.hh"

// Usage: ./reverse61 [-s SIZE] [-o OUTFILE] [FILE]
//    Copies the input FILE to OUTFILE one character at a time,
//    reversing the order of characters in the input.

int main(int argc, char* argv[]) {
    // Parse arguments
    io61_arguments args(argc, argv, "s:o:i:");

    // Open files, measure file sizes
    io61_profile_begin();
    io61_file* inf = io61_open_check(args.input_file, O_RDONLY);
    io61_file* outf = io61_open_check(args.output_file,
                                      O_WRONLY | O_CREAT | O_TRUNC);

    if ((ssize_t) args.input_size < 0) {
        args.input_size = io61_filesize(inf);
    }
    if ((ssize_t) args.input_size < 0) {
        fprintf(stderr, "reverse61: can't get size of input file\n");
        exit(1);
    }
    if (io61_seek(inf, 0) < 0) {
        fprintf(stderr, "reverse61: input file is not seekable\n");
        exit(1);
    }

    while (args.input_size != 0) {
        --args.input_size;
        io61_seek(inf, args.input_size);
        int ch = io61_readc(inf);
        io61_writec(outf, ch);
    }

    io61_close(inf);
    io61_close(outf);
    io61_profile_end();
}
