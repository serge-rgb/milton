// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

static size_t bytes_until_end(FILE* fd)
{
    fpos_t fd_pos;
    fgetpos(fd, &fd_pos);
    fseek(fd, 0, SEEK_END);
    size_t len = (size_t)ftell(fd);
    fsetpos(fd, &fd_pos);
    return len;
}

char* read_entire_file(char* fname)
{
    char* contents = NULL;
    FILE* fd = fopen(fname, "r");
    if (fd)
    {
        auto sz = bytes_until_end(fd);
        contents = (char*)calloc(1, sz);
        if (contents)
        {
            auto read = fread(contents, 1, sz, fd);
        }
        else
        {
            fprintf(stderr, "Could not allocate memory for reading file.\n");
        }
        fclose(fd);
    }
    else
    {
        fprintf(stderr, "Could not open shader for reading\n");
    }
    return contents;
}

#define MAX_LINES 10000

char** split_lines(char* contents, i64* out_count, i64* max_line=NULL)
{
    i64 this_len = 0;

    char** lines = (char**)calloc(1, MAX_LINES);
    i64 lines_i = 0;

    char* begin = contents;
    char* iter = contents;
    for(;
        *iter!='\0';
        ++iter)
    {
        ++this_len;
        if (*iter == '\n')
        {
            if (max_line != NULL)
            {
                if (this_len > *max_line)
                {
                    *max_line = this_len;
                }
            }
            // Copy a string from beginning
            char* line = (char*)calloc(1, this_len);
            memcpy(line, begin, this_len);
            lines[lines_i++] = line;
            begin = iter+1;
            this_len = 0;
        }
    }
    *out_count = lines_i;
    return lines;
}

void output_shader(FILE* of, char* fname)
{
    char* contents = read_entire_file(fname);
    char** lines;
    i64 count;
    lines = split_lines(contents, &count);
    for (i64 i = 0; i < count; ++i)
    {
        lines[i][strlen(lines[i])-1]='\0';  // Strip newline
        fprintf(of, "\"%s\\n\"\n", lines[i]);
    }
    fprintf(stderr, "\n");
}

// Assuming this is being called from the build directory
int main()
{
    FILE* outfd = fopen("./../src/shaders.gen.h", "w");
    if (outfd)
    {
        output_shader(outfd, "../src/picker.v.glsl");
    }
    else
    {
        fprintf(stderr, "Could not open output file.\n");
    }
}
