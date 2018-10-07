#include "io61.hh"

// Usage: ./cat61 [-s SIZE] [-o OUTFILE] [FILE]
//    Copies the input FILE to OUTFILE one character at a time.

int main(int argc, char* argv[]) {
    // Parse arguments
    io61_arguments args(argc, argv, "s:o:i:");

    io61_profile_begin();
    io61_file* inf = io61_open_check(args.input_file, O_RDONLY);
    io61_file* outf = io61_open_check(args.output_file,
                                      O_WRONLY | O_CREAT | O_TRUNC);

    while (args.input_size > 0) {
        int ch = io61_readc(inf);
        if (ch == EOF) {
            break;
        }
        io61_writec(outf, ch);
        --args.input_size;
    }

    io61_close(inf);
    io61_close(outf);
    io61_profile_end();
}
