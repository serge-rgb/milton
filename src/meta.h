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

#ifdef _WIN32
//#include "win_dirent.h"
#endif

//#include "serg_io.h"

#include "define_types.h"
#include "memory.h"

// ============================================================
// HERE BE DRAGONS
//  This code is *horrible* and needs to be taken out back and shot
//  alternatively, some polish might make it not suck.
//
//  It's meant for short&quick template expander programs.
//  See template_expand.c
// ============================================================

void meta_clear_file(const char* path)
{
    FILE* fd = fopen(path, "w+");
    assert(fd);
    static const char message[] = "//File generated from template by libserg/meta.h.\n\n";
    fwrite(message, sizeof(char), sizeof(message) - 1, fd);
    fclose(fd);
}

typedef struct
{
    char* str;
    size_t len;
} TemplateToken;

typedef struct
{
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

    Binding* bindings = arena_make_stack(&root_arena, 1000, Binding);

    va_list ap;

    va_start(ap, num_bindings);
    for (int i = 0; i < num_bindings; ++i)
    {
        char* name = va_arg(ap, char*);
        char* subst = va_arg(ap, char*);

        TemplateToken tk_name = { name, strlen(name) };
        TemplateToken tk_subst = { subst, strlen(subst) };

        Binding binding = { tk_name, tk_subst };
        stack_push(bindings, binding);
    }
    va_end(ap);

    FILE* out_fd = fopen(result_path, "a");
    assert(out_fd);
    int data_size = 0;
    const char* in_data = slurp_file(tmpl_path, &data_size);
    char* out_data = arena_make_stack(&root_arena, 10 * 1024 * 1024, char);

    size_t path_len = strlen(tmpl_path);
    stack_push(out_data, '/'); stack_push(out_data, '/');
    for (int i = 0; i < path_len; ++i)
    {
        stack_push(out_data, tmpl_path[i]);
    }
    stack_push(out_data, '\n');
    stack_push(out_data, '\n');

    TemplateToken* tokens = arena_make_stack(&root_arena, 5000, TemplateToken);

    int lexer_state = LEX_NOTHING;

    char prev = 0;
    char* name = arena_make_stack(&root_arena, 1000, char);
    int name_len = 0;
    for (int i = 0; i < data_size - 1; ++i)  // Don't count EOF
    {
        char c = in_data[i];
        // Handle transitions
        {
            if ( lexer_state == LEX_NOTHING && c != '$' )
            {
                stack_push(out_data, c);
            }
            if ( lexer_state == LEX_NOTHING && c == '$' )
            {
                lexer_state = LEX_DOLLAR;
            }
            else if ( lexer_state == LEX_DOLLAR && c == '<' )
            {
                lexer_state = LEX_INSIDE;
            }
            else if (lexer_state == LEX_INSIDE && c != '>')
            {
                // add char to name
                stack_push(name, c);
                ++name_len;
            }
            else if (lexer_state == LEX_INSIDE && c == '>')
            {
                // add token
                stack_push(name, '\0');
                TemplateToken token;
                char* new_name = arena_make_stack(&root_arena, strlen(name)+1, char);
                strcpy(new_name, name);
                token.str = new_name;
                token.len = name_len;
                stack_reset(name);
                stack_push(tokens, token);
                name_len = 0;
                lexer_state = LEX_NOTHING;

                // Do stupid search on args to get matching subst.
                for (int i = 0; i < stack_count(bindings); ++i)
                {
                    const Binding binding = bindings[i];
                    if (!strcmp(binding.name.str, token.str))
                    {
                        for (int j = 0; j < binding.substitution.len; ++j)
                        {
                            char c = binding.substitution.str[j];
                            stack_push(out_data, c);
                        }
                        break;
                    }
                }
            }
        }
        prev = c;
    }

    //stack_push(out_data, '\n');

    fwrite(out_data, sizeof(char), stack_count(out_data), out_fd);
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
    if (!fd)
    {
        fprintf(stderr, "ERROR: couldn't slurp %s\n", path);
        assert(fd);
        return NULL;
    }
    int len = bytes_in_fd(fd);
    char* contents = malloc(len + 1);
    if (contents)
    {
        // Truncating from size_t to int... Technically a bug but will probably never happen
        const int read = (int)fread((void*)contents, 1, (size_t)len, fd);
        fclose(fd);
        *out_size = read + 1;
        contents[read] = '\0';
    }
    return contents;
}

