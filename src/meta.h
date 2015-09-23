//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


#pragma once

#include <assert.h>
#ifndef _WIN32
#include <dirent.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "define_types.h"
#include "memory.h"

// ====
// This is tool does not free memory. It just processes source code, memory shouldn't run out...
// ====

// stb stretchy buffer
#define sb_free(a)      ((a) ? free(stb__sbraw(a)),0 : 0)
#define sb_push(a,v)    (stb__sbmaybegrow(a,1), (a)[stb__sbn(a)++] = (v))
#define sb_count(a)     ((a) ? stb__sbn(a) : 0)
#define sb_add(a,n)     (stb__sbmaybegrow(a,n), stb__sbn(a)+=(n), &(a)[stb__sbn(a)-(n)])
#define sb_last(a)      ((a)[stb__sbn(a)-1])
// My little addition:
#define sb_reset(a)     memset((a), 0, sizeof(*a)*stb__sbn(a)), stb__sbn(a) = 0

#define stb__sbraw(a)   ((int *) (a) - 2)
#define stb__sbm(a)     stb__sbraw(a)[0]
#define stb__sbn(a)     stb__sbraw(a)[1]

#define stb__sbneedgrow(a,n)  ((a)==0 || stb__sbn(a)+(n) >= stb__sbm(a))
#define stb__sbmaybegrow(a,n) (stb__sbneedgrow(a,(n)) ? stb__sbgrow(a,n) : 0)
#define stb__sbgrow(a,n)      ((a) = stb__sbgrowf((a), (n), sizeof(*(a))))

static void * stb__sbgrowf(void *arr, int increment, int itemsize)
{
   int dbl_cur = arr ? 2*stb__sbm(arr) : 0;
   int min_needed = sb_count(arr) + increment;
   int m = dbl_cur > min_needed ? dbl_cur : min_needed;
   int *p = (int *) realloc(arr ? stb__sbraw(arr) : 0, itemsize * m + sizeof(int)*2);
   if (p) {
      if (!arr)
         p[1] = 0;
      p[0] = m;
      return p+2;
   } else {
      #ifdef STRETCHY_BUFFER_OUT_OF_MEMORY
      STRETCHY_BUFFER_OUT_OF_MEMORY ;
      #endif
      return (void *) (2*sizeof(int)); // try to force a NULL pointer exception later
   }
}
void meta_clear_file(const char* path) {
    FILE* fd = fopen(path, "w+");
    assert(fd);
    static const char message[] = "//File generated from template by libserg/meta.h.\n\n";
    fwrite(message, sizeof(char), sizeof(message) - 1, fd);
    fclose(fd);
}

typedef struct {
    char* str;
    size_t len;
} TemplateToken;

typedef struct {
    TemplateToken name;
    TemplateToken substitution;
} Binding;

static const int bytes_in_fd(FILE* fd);
static const char* slurp_file(const char* path, int *out_size);

void meta_expand(
        const char* result_path,
        const char* tmpl_path,
        const int num_bindings,
        ...)
{
    size_t size = 300 * 1024 * 1024;
    void* big_block_of_memory = malloc(size);
    Arena root_arena = arena_init(big_block_of_memory, size);

    enum
    {
        LEX_NOTHING,
        LEX_DOLLAR,
        LEX_BEGIN,
        LEX_INSIDE,
    };

    // Get bindings

    Binding* bindings = NULL;

    va_list ap;

    va_start(ap, num_bindings);
    for (int i = 0; i < num_bindings; ++i) {
        char* name = va_arg(ap, char*);
        char* subst = va_arg(ap, char*);

        TemplateToken tk_name = { name, strlen(name) };
        TemplateToken tk_subst = { subst, strlen(subst) };

        Binding binding = { tk_name, tk_subst };
        sb_push(bindings, binding);
    }
    va_end(ap);

    FILE* out_fd = fopen(result_path, "a");
    assert(out_fd);
    int data_size = 0;
    const char* in_data = slurp_file(tmpl_path, &data_size);
    char* out_data = NULL;

    size_t path_len = strlen(tmpl_path);
    sb_push(out_data, '/'); sb_push(out_data, '/');
    for (int i = 0; i < path_len; ++i) {
        sb_push(out_data, tmpl_path[i]);
    }
    sb_push(out_data, '\n');
    sb_push(out_data, '\n');

    TemplateToken* tokens = NULL;

    int lexer_state = LEX_NOTHING;

    char prev = 0;
    char* name = NULL;
    int name_len = 0;
    for (int i = 0; i < data_size - 1; ++i)  {  // Don't count EOF
        char c = in_data[i];
        // Handle transitions
        {
            if ( lexer_state == LEX_NOTHING && c != '$' ) {
                sb_push(out_data, c);
            }
            if ( lexer_state == LEX_NOTHING && c == '$' ) {
                lexer_state = LEX_DOLLAR;
            } else if ( lexer_state == LEX_DOLLAR && c == '<' ) {
                lexer_state = LEX_INSIDE;
            } else if (lexer_state == LEX_INSIDE && c != '>') {
                // add char to name
                sb_push(name, c);
                ++name_len;
            } else if (lexer_state == LEX_INSIDE && c == '>') {
                // add token
                sb_push(name, '\0');
                TemplateToken token;
                char* new_name = (char*)malloc(strlen(name) + 1);
                strcpy(new_name, name);
                token.str = new_name;
                token.len = name_len;
                sb_reset(name);
                sb_push(tokens, token);
                name_len = 0;
                lexer_state = LEX_NOTHING;

                // Do stupid search on args to get matching subst.
                for (int i = 0; i < sb_count(bindings); ++i)
                {
                    const Binding binding = bindings[i];
                    if (!strcmp(binding.name.str, token.str))
                    {
                        for (int j = 0; j < binding.substitution.len; ++j)
                        {
                            char c = binding.substitution.str[j];
                            sb_push(out_data, c);
                        }
                        break;
                    }
                }
            }
        }
        prev = c;
    }

    fwrite(out_data, sizeof(char), sb_count(out_data), out_fd);
    fclose(out_fd);
    free(big_block_of_memory);
}

static const int bytes_in_fd(FILE* fd)
{
    fpos_t fd_pos;
    fgetpos(fd, &fd_pos);
    fseek(fd, 0, SEEK_END);
    int len = ftell(fd);
    fsetpos(fd, &fd_pos);
    return len;
}

static const char* slurp_file(const char* path, int *out_size)
{
    FILE* fd = fopen(path, "r");
    if (!fd) {
        fprintf(stderr, "ERROR: couldn't slurp %s\n", path);
        assert(fd);
        return NULL;
    }
    int len = bytes_in_fd(fd);
    char* contents = malloc(len + 1);
    if (contents) {
        // Truncating from size_t to int... Technically a bug but will probably never happen
        const int read = (int)fread((void*)contents, 1, (size_t)len, fd);
        fclose(fd);
        *out_size = read + 1;
        contents[read] = '\0';
    }
    return contents;
}

