#ifndef IO61_HH
#define IO61_HH
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

struct io61_file;

io61_file* io61_fdopen(int fd, int mode);
io61_file* io61_open_check(const char* filename, int mode);
int io61_close(io61_file* f);

off_t io61_filesize(io61_file* f);

int io61_seek(io61_file* f, off_t pos);

int io61_readc(io61_file* f);
int io61_writec(io61_file* f, int ch);

ssize_t io61_read(io61_file* f, char* buf, size_t sz);
ssize_t io61_write(io61_file* f, const char* buf, size_t sz);

int io61_flush(io61_file* f);

void io61_profile_begin();
void io61_profile_end();


struct io61_arguments {
    size_t input_size;          // `-s` option: input size. Default SIZE_MAX
    size_t block_size;          // `-b` option: block size. Default 0
    size_t stride;              // `-t` option: stride. Default 1024
    bool lines;                 // `-l` option: read by lines. Default false
    const char* output_file;    // `-o` option: output file. Default nullptr
    const char* input_file;     // input file. Default nullptr
    std::vector<const char*> input_files;   // all input files
    std::vector<const char*> output_files;  // all output files
    const char* program_name;   // name of program
    const char* opts;           // options string

    io61_arguments(int argc, char** argv, const char* opts);
    void usage();
};

#endif
