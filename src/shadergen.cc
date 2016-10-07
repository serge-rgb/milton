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
        contents = (char*)calloc(1, sz+1);
        if (contents)
        {
            auto read = fread(contents, 1, sz, fd);
            contents[read]='\0';
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
            char* line = (char*)calloc(1, this_len+1);
            memcpy(line, begin, this_len);
            lines[lines_i++] = line;
            begin = iter+1;
            this_len = 0;
        }
    }
    *out_count = lines_i;
    return lines;
}

enum ShaderType
{
    VERTEX,
    FRAGMENT,
};

void output_shader(FILE* of, char* fname, char* varname, ShaderType shader_type, char* fname_prelude = NULL)
{
    char* contents = read_entire_file(fname);
    char** prelude_lines = NULL;
    i64 prelude_lines_count = 0;
    if (fname_prelude)
    {
        char* prelude = read_entire_file(fname_prelude);
        prelude_lines = split_lines(prelude, &prelude_lines_count);
    }
    char** lines;
    i64 count;
    lines = split_lines(contents, &count);
    fprintf(of, "char %s[] = \n", varname);
    fprintf(of, "\"#version 330\\n\"\n");
    fprintf(of, "\"#define HAS_MULTISAMPLE 1\\n\"\n");
    //fprintf(of, "\"#define texture2D texture\\n\"\n");
#if 1
    if (shader_type == VERTEX)
    {
        fprintf(of, "\"#define attribute in\\n\"\n");
        fprintf(of, "\"#define varying out\\n\"\n");
    }
    else if (shader_type == FRAGMENT)
    {
        fprintf(of, "\"#define varying in\\n\"\n");
        fprintf(of, "\"out vec4 out_color;\\n\"\n");
        fprintf(of, "\"#define gl_FragColor out_color\\n\"\n");
    }
#endif

    for (i64 i = 0; i < prelude_lines_count; ++i)
    {
        prelude_lines[i][strlen(prelude_lines[i])-1]='\0';  // Strip newline
        fprintf(of, "\"%s\\n\"\n", prelude_lines[i]);
    }
    for (i64 i = 0; i < count; ++i)
    {
        lines[i][strlen(lines[i])-1]='\0';  // Strip newline
        fprintf(of, "\"%s\\n\"\n", lines[i]);
    }
    fprintf(of, ";\n");
}

// Assuming this is being called from the build directory
int main()
{
    FILE* outfd = fopen("./../src/shaders.gen.h", "w");
    if (outfd)
    {
        output_shader(outfd, "../src/picker.v.glsl", "g_picker_v", VERTEX);
        output_shader(outfd, "../src/picker.f.glsl", "g_picker_f", FRAGMENT);
        output_shader(outfd, "../src/layer_blend.v.glsl", "g_layer_blend_v", VERTEX);
        output_shader(outfd, "../src/layer_blend.f.glsl", "g_layer_blend_f", FRAGMENT);
        output_shader(outfd, "../src/simple.v.glsl", "g_simple_v", VERTEX);
        output_shader(outfd, "../src/simple.f.glsl", "g_simple_f", FRAGMENT);
        output_shader(outfd, "../src/ssaa_resolve.f.glsl", "g_ssaa_resolve_f", FRAGMENT);
        output_shader(outfd, "../src/outline.v.glsl", "g_outline_v", VERTEX);
        output_shader(outfd, "../src/outline.f.glsl", "g_outline_f", FRAGMENT);
        output_shader(outfd, "../src/stroke_raster.v.glsl", "g_stroke_raster_v", VERTEX, "../src/common.glsl");
        output_shader(outfd, "../src/stroke_raster.f.glsl", "g_stroke_raster_f", FRAGMENT, "../src/common.glsl");
        output_shader(outfd, "../src/exporter_rect.f.glsl", "g_exporter_f", FRAGMENT);
        output_shader(outfd, "../src/texture_fill.f.glsl", "g_texture_fill_f", FRAGMENT);
        output_shader(outfd, "../src/quad.v.glsl", "g_quad_v", VERTEX);
        output_shader(outfd, "../src/quad.f.glsl", "g_quad_f", FRAGMENT);
    }
    else
    {
        fprintf(stderr, "Could not open output file.\n");
    }
}
