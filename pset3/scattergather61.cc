#include "io61.hh"
#include <vector>

// Usage: ./scattergather61 [-b BLOCKSIZE] [-i IFILE | -o OFILE]...
//    Copies the input IFILEs to the output OFILEs, alternating
//    with every block. (I.e., read from IFILE1 and write to OFILE1,
//    then read from IFILE2 and write to OFILE2, etc. There may be
//    different numbers of IFILEs and OFILEs.) This is a
//    "scatter/gather" I/O pattern: input is "gathered" from many
//    input files and "scattered" to many output files.
//    Default BLOCKSIZE is 1.

ssize_t read_line(io61_file* f, char* buf, size_t sz, bool lines) {
    if (lines) {
        size_t i = 0;
        while (i != sz) {
            int ch = io61_readc(f);
            if (ch == EOF) {
                break;
            }
            buf[i] = ch;
            ++i;
            if (ch == '\n') {
                break;
            }
        }
        return i;
    } else {
        return io61_read(f, buf, sz);
    }
}


int main(int argc, char* argv[]) {
    // Parse arguments
    io61_arguments args(argc, argv, "b:i:o:l##");
    size_t block_size = args.block_size ? args.block_size : 1;

    // Allocate buffer, open files
    char* buf = new char[block_size];

    io61_profile_begin();
    std::vector<io61_file*> infs, outfs;
    for (auto filename : args.input_files) {
        auto f = io61_open_check(filename, O_RDONLY);
        infs.push_back(f);
    }
    for (auto filename : args.output_files) {
        auto f = io61_open_check(filename, O_WRONLY | O_CREAT | O_TRUNC);
        outfs.push_back(f);
    }

    // Copy file data
    size_t ini = -1, outi = 0;
    while (!infs.empty()) {
        ini = (ini + 1) % infs.size();
        ssize_t amount = read_line(infs[ini], buf, block_size, args.lines);
        if (amount <= 0) {
            io61_close(infs[ini]);
            infs.erase(infs.begin() + ini);
            --ini;
        } else {
            io61_write(outfs[outi], buf, amount);
            outi = (outi + 1) % outfs.size();
        }
    }

    for (auto f : outfs) {
        io61_close(f);
    }
    io61_profile_end();
    delete[] buf;
}
