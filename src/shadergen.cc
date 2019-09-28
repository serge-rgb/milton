// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "milton_configuration.h"


typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

void handle_errno(int error);


static size_t
bytes_until_end(FILE* fd)
{
    fpos_t fd_pos;
    fgetpos(fd, &fd_pos);
    fseek(fd, 0, SEEK_END);
    size_t len = (size_t)ftell(fd);
    fsetpos(fd, &fd_pos);
    return len;
}

char*
read_entire_file(const char* fname)
{
    char* contents = NULL;
    FILE* fd = fopen(fname, "r");
    if ( fd ) {
        auto sz = bytes_until_end(fd);
        contents = (char*)calloc(1, sz+2);  // Add 2. A \0 char and a \n if last line not terminated
        if ( contents ) {
            auto read = fread(contents, 1, sz, fd);
            // Append a newline if the last line is not newline terminated.
            if ( contents[read-1] != '\n' ) {
                contents[read++] = '\n';
            }
            contents[read]='\0';
        }
        else {
            fprintf(stderr, "Could not allocate memory for reading file.\n");
        }
        fclose(fd);
    }
    else {
        int error = errno;
        fprintf(stderr, "Could not open shader for reading\n");
        handle_errno(error);
    }
    return contents;
}

#define MAX_LINES 10000

char**
split_lines(char* contents, i64* out_count, i64* max_line=NULL)
{

    char** lines = (char**)calloc(1, sizeof(char**)*MAX_LINES);
    i64 lines_i = 0;

    char* begin = contents;
    char* iter = contents;
    for ( ;
          *iter!='\0';
          ++iter ) {

        if ( *iter == '\n' || *(iter+1) == '\0' ) {
            i64 this_len = iter - begin;

            if ( max_line != NULL ) {
                if ( this_len > *max_line ) {
                    *max_line = this_len;
                }
            }
            // Copy a string from beginning
            char* line = (char*)calloc(1, (size_t)this_len*2 + 2);

            int line_i = 0;
            for ( int i = 0; i < this_len; ++i ) {
                if ( begin[i] == '\"' ) {
                    line[line_i++] = 'Q';
                }
                else if (begin[i] != '\r' && begin[i] != '\n') {
                    line[line_i++] = begin[i];
                }
            }
            line[line_i++] = '\n';
            line[line_i++] = '\0';

            lines[lines_i++] = line;

            begin = iter+1;
        }
    }
    *out_count = lines_i;
    return lines;
}

// Computes the variable name of the form g_NAME_(v|f) for a given shader filename
//    e.g. "../src/my_shader.v.glsl" -> g_my_shader_v
int
shadername(const char* filename, char* out_varname, size_t sz_varname)
{
    if ( filename && out_varname ) {
        // Skip filename until after all forward slashes.
        const char* last_forward_slash = NULL;
        for ( const char* c = filename; *c!='\0'; ++c ) {
            if ( *c == '/' ) {
                last_forward_slash = c;
            }
        }
        if ( last_forward_slash ) {
            filename = last_forward_slash+1;
        }
        size_t len_filename = strlen(filename);
        char prefix[] = "g_";
        size_t len_prefix = strlen(prefix);
        if ( len_filename+3 <= sz_varname ) {
            size_t out_i = 0;
            size_t count_dots = 0;
            // Append the prefix
            for ( size_t pi = 0; pi < len_prefix; ++pi ) {
                out_varname[out_i++] = prefix[pi];
            }
            // Append name. Translate the first dot to _
            for ( const char* c = filename;
                  *c != '\0';
                  ++c ) {
                if ( *c != '.' ) {
                    out_varname[out_i++] = *c;
                } else {
                    if ( count_dots++ == 0 ) {
                        out_varname[out_i++] = '_';
                    } else {
                        break;
                    }
                }
            }
            out_varname[out_i] = '\0';
        } else {
            return -1;
        }
    }
    return 0;
}

#define VARNAME_MAX 128

void
output_shader(FILE* of, const char* fname, const char* fname_prelude = NULL)
{
    char* contents = read_entire_file(fname);
    char** prelude_lines = NULL;
    i64 prelude_lines_count = 0;
    if ( fname_prelude ) {
        char* prelude = read_entire_file(fname_prelude);
        prelude_lines = split_lines(prelude, &prelude_lines_count);
    }
    char** lines;
    i64 count;
    lines = split_lines(contents, &count);

    char varname[VARNAME_MAX] = {};
    int err = shadername(fname, varname, VARNAME_MAX);
    if ( err != 0 ) {
        fprintf(stderr, "Error when computing the variable name for file %s\n", fname);
        return;
    }

    fprintf(of, "static char %s[] = \n", varname);
    for ( i64 i = 0; i < prelude_lines_count; ++i ) {
        size_t len = strlen(prelude_lines[i]);
        prelude_lines[i][len-1]='\0';  // Strip newline
        fprintf(of, "\"%s\\n\"\n", prelude_lines[i]);
    }
    for ( i64 i = 0; i < count; ++i ) {
        lines[i][strlen(lines[i])-1]='\0';  // Strip newline
        fprintf(of, "\"%s\\n\"\n", lines[i]);
    }
    fprintf(of, ";\n");
}

// Assuming this is being called from the build directory
int
main(int argc, char** argv)
{
    fprintf(stderr, "Generating shader code...\n");

    FILE* outfd = fopen("src/shaders.gen.h", "w");
    if ( outfd ) {
        output_shader(outfd, "src/picker.v.glsl");
        output_shader(outfd, "src/picker.f.glsl");
        output_shader(outfd, "src/layer_blend.v.glsl");
        output_shader(outfd, "src/layer_blend.f.glsl");
        output_shader(outfd, "src/simple.v.glsl");
        output_shader(outfd, "src/simple.f.glsl");
        output_shader(outfd, "src/outline.v.glsl");
        output_shader(outfd, "src/outline.f.glsl");
        output_shader(outfd, "src/stroke_raster.v.glsl", "src/common.glsl");
        output_shader(outfd, "src/stroke_raster.f.glsl", "src/common.glsl");
        output_shader(outfd, "src/stroke_eraser.f.glsl", "src/common.glsl");
        output_shader(outfd, "src/stroke_info.f.glsl", "src/common.glsl");
        output_shader(outfd, "src/stroke_fill.f.glsl", "src/common.glsl");
        output_shader(outfd, "src/stroke_clear.f.glsl", "src/common.glsl");
        output_shader(outfd, "src/stroke_debug.f.glsl", "src/common.glsl");
        output_shader(outfd, "src/exporter_rect.f.glsl");
        output_shader(outfd, "src/texture_fill.f.glsl");
        output_shader(outfd, "src/quad.v.glsl");
        output_shader(outfd, "src/quad.f.glsl");
        output_shader(outfd, "src/postproc.f.glsl", "third_party/Fxaa3_11.f.glsl");
        output_shader(outfd, "src/blur.f.glsl");

        fclose(outfd);
    }
    else {
        fprintf(stderr, "Could not open output file.\n");
        int error = errno;
        handle_errno(error);
    }
    fprintf(stderr, "Shaders generated OK\n");
    return EXIT_SUCCESS;

}

void
handle_errno(int error)
{
    const char* str = NULL;
    switch ( error ) {
        case E2BIG:           str = "Argument list too long (POSIX.1)"; break;

        case EACCES:          str = "Permission denied (POSIX.1)"; break;

        case EADDRINUSE:      str = "Address already in use (POSIX.1)"; break;

        case EADDRNOTAVAIL:   str = "Address not available (POSIX.1)"; break;

        case EAFNOSUPPORT:    str = "Address family not supported (POSIX.1)"; break;

        case EAGAIN:          str = "Resource temporarily unavailable (may be the same value as EWOULDBLOCK) (POSIX.1)"; break;

        case EALREADY:        str = "Connection already in progress (POSIX.1)"; break;

        // case EBADE:           str = "Invalid exchange"; break;

        case EBADF:           str = "Bad file descriptor (POSIX.1)"; break;

        // case EBADFD:          str = "File descriptor in bad state"; break;

        case EBADMSG:         str = "Bad message (POSIX.1)"; break;

        // case EBADR:           str = "Invalid request descriptor"; break;

        // case EBADRQC:         str = "Invalid request code"; break;

        // case EBADSLT:         str = "Invalid slot"; break;

        case EBUSY:           str = "Device or resource busy (POSIX.1)"; break;

        case ECANCELED:       str = "Operation canceled (POSIX.1)"; break;

        case ECHILD:          str = "No child processes (POSIX.1)"; break;

        // case ECHRNG:          str = "Channel number out of range"; break;

        // case ECOMM:           str = "Communication error on send"; break;

        case ECONNABORTED:    str = "Connection aborted (POSIX.1)"; break;

        case ECONNREFUSED:    str = "Connection refused (POSIX.1)"; break;

        case ECONNRESET:      str = "Connection reset (POSIX.1)"; break;

        case EDEADLK:         str = "Resource deadlock avoided (POSIX.1)"; break;

        // case EDEADLOCK:       str = "Synonym for EDEADLK"; break;

        case EDESTADDRREQ:    str = "Destination address required (POSIX.1)"; break;

        case EDOM:            str = "Mathematics argument out of domain of function (POSIX.1, C99)"; break;

        // case EDQUOT:          str = "Disk quota exceeded (POSIX.1)"; break;

        case EEXIST:          str = "File exists (POSIX.1)"; break;

        case EFAULT:          str = "Bad address (POSIX.1)"; break;

        case EFBIG:           str = "File too large (POSIX.1)"; break;

        // case EHOSTDOWN:       str = "Host is down"; break;

        case EHOSTUNREACH:    str = "Host is unreachable (POSIX.1)"; break;

        case EIDRM:           str = "Identifier removed (POSIX.1)"; break;

        case EILSEQ:          str = "Illegal byte sequence (POSIX.1, C99)"; break;

        case EINPROGRESS:     str = "Operation in progress (POSIX.1)"; break;

        case EINTR:           str = "Interrupted function call (POSIX.1); see signal(7)."; break;

        case EINVAL:          str = "Invalid argument (POSIX.1)"; break;

        case EIO:             str = "Input/output error (POSIX.1)"; break;

        case EISCONN:         str = "Socket is connected (POSIX.1)"; break;

        case EISDIR:          str = "Is a directory (POSIX.1)"; break;

        // case EISNAM:          str = "Is a named type file"; break;

        // case EKEYEXPIRED:     str = "Key has expired"; break;

        // case EKEYREJECTED:    str = "Key was rejected by service"; break;

        // case EKEYREVOKED:     str = "Key has been revoked"; break;

        // case EL2HLT:          str = "Level 2 halted"; break;

        // case EL2NSYNC:        str = "Level 2 not synchronized"; break;

        // case EL3HLT:          str = "Level 3 halted"; break;

        // case EL3RST:          str = "Level 3 halted"; break;

        // case ELIBACC:         str = "Cannot access a needed shared library"; break;

        // case ELIBBAD:         str = "Accessing a corrupted shared library"; break;

        // case ELIBMAX:         str = "Attempting to link in too many shared libraries"; break;

        // case ELIBSCN:         str = "lib section in a.out corrupted"; break;

        // case ELIBEXEC:        str = "Cannot exec a shared library directly"; break;

        case ELOOP:           str = "Too many levels of symbolic links (POSIX.1)"; break;

        // case EMEDIUMTYPE:     str = "Wrong medium type"; break;

        case EMFILE:          str = "Too many open files (POSIX.1); commonly caused by exceeding the RLIMIT_NOFILE resource limit described in getrlimit(2)"; break;

        case EMLINK:          str = "Too many links (POSIX.1)"; break;

        case EMSGSIZE:        str = "Message too long (POSIX.1)"; break;

        // case EMULTIHOP:       str = "Multihop attempted (POSIX.1)"; break;

        case ENAMETOOLONG:    str = "Filename too long (POSIX.1)"; break;

        case ENETDOWN:        str = "Network is down (POSIX.1)"; break;

        case ENETRESET:       str = "Connection aborted by network (POSIX.1)"; break;

        case ENETUNREACH:     str = "Network unreachable (POSIX.1)"; break;

        case ENFILE:          str = "Too many open files in system (POSIX.1); on Linux, this is probably a result of encountering the /proc/sys/fs/file-max limit (see proc(5))."; break;

        case ENOBUFS:         str = "No buffer space available (POSIX.1 (XSI STREAMS option))"; break;

        case ENODATA:         str = "No message is available on the STREAM head read queue (POSIX.1)"; break;

        case ENODEV:          str = "No such device (POSIX.1)"; break;

        case ENOENT:          str = "No such file or directory (POSIX.1)"; break;

                       // Typically, this error results when a specified
                       // pathname does not exist, or one of the components in
                       // the directory prefix of a pathname does not exist, or
                       // the specified pathname is a dangling symbolic link.

        case ENOEXEC:         str = "Exec format error (POSIX.1)"; break;

        // case ENOKEY:          str = "Required key not available"; break;

        case ENOLCK:          str = "No locks available (POSIX.1)"; break;

        case ENOLINK:         str = "Link has been severed (POSIX.1)"; break;

        // case ENOMEDIUM:       str = "No medium found"; break;

        case ENOMEM:          str = "Not enough space (POSIX.1)"; break;

        case ENOMSG:          str = "No message of the desired type (POSIX.1)"; break;

        // case ENONET:          str = "Machine is not on the network"; break;

        // case ENOPKG:          str = "Package not installed"; break;

        case ENOPROTOOPT:     str = "Protocol not available (POSIX.1)"; break;

        case ENOSPC:          str = "No space left on device (POSIX.1)"; break;

        case ENOSR:           str = "No STREAM resources (POSIX.1 (XSI STREAMS option))"; break;

        case ENOSTR:          str = "Not a STREAM (POSIX.1 (XSI STREAMS option))"; break;

        case ENOSYS:          str = "Function not implemented (POSIX.1)"; break;

        // case ENOTBLK:         str = "Block device required"; break;

        case ENOTCONN:        str = "The socket is not connected (POSIX.1)"; break;

        case ENOTDIR:         str = "Not a directory (POSIX.1)"; break;

        case ENOTEMPTY:       str = "Directory not empty (POSIX.1)"; break;

        case ENOTSOCK:        str = "Not a socket (POSIX.1)"; break;

        case ENOTSUP:         str = "Operation not supported (POSIX.1)"; break;

        case ENOTTY:          str = "Inappropriate I/O control operation (POSIX.1)"; break;

        // case ENOTUNIQ:        str = "Name not unique on network"; break;

        case ENXIO:           str = "No such device or address (POSIX.1)"; break;

        // case EOPNOTSUPP:      str = "Operation not supported on socket (POSIX.1)"; break;

                        // (ENOTSUP and EOPNOTSUPP have the same value on Linux,
                        // but according to POSIX.1 these error values should be
                        // distinct.)

        case EOVERFLOW:       str = "Value too large to be stored in data type (POSIX.1)"; break;

        case EPERM:           str = "Operation not permitted (POSIX.1)"; break;

        // case EPFNOSUPPORT:    str = "Protocol family not supported"; break;

        case EPIPE:           str = "Broken pipe (POSIX.1)"; break;

        case EPROTO:          str = "Protocol error (POSIX.1)"; break;

        case EPROTONOSUPPORT: str = "Protocol not supported (POSIX.1)"; break;

        case EPROTOTYPE:      str = "Protocol wrong type for socket (POSIX.1)"; break;

        case ERANGE:          str = "Result too large (POSIX.1, C99)"; break;

        // case EREMCHG:         str = "Remote address changed"; break;

        // case EREMOTE:         str = "Object is remote"; break;

        // case EREMOTEIO:       str = "Remote I/O error"; break;

        // case ERESTART:        str = "Interrupted system call should be restarted"; break;

        case EROFS:           str = "Read-only filesystem (POSIX.1)"; break;

        // case ESHUTDOWN:       str = "Cannot send after transport endpoint shutdown"; break;

        case ESPIPE:          str = "Invalid seek (POSIX.1)"; break;

        // case ESOCKTNOSUPPORT: str = "Socket type not supported"; break;

        case ESRCH:           str = "No such process (POSIX.1)"; break;

        // case ESTALE:          str = "Stale file handle (POSIX.1)"; break;

                        // This error can occur for NFS and for other
                        // filesystems

        // case ESTRPIPE:        str = "Streams pipe error"; break;

        case ETIME:           str = "Timer expired (POSIX.1 (XSI STREAMS option))"; break;

                        // (POSIX.1 says "STREAM ioctl(2) timeout")

        case ETIMEDOUT:       str = "Connection timed out (POSIX.1)"; break;

        case ETXTBSY:         str = "Text file busy (POSIX.1)"; break;

        // case EUCLEAN:         str = "Structure needs cleaning"; break;

        // case EUNATCH:         str = "Protocol driver not attached"; break;

        // case EUSERS:          str = "Too many users"; break;

        // case EWOULDBLOCK:     str = "Operation would block (may be same value as EAGAIN) (POSIX.1)"; break;

        case EXDEV:           str = "Improper link (POSIX.1)"; break;

        // case EXFULL:          str = "Exchange full"; break;
    }
    if ( str ) {
        fprintf(stderr, "Errno is set to \"%s\"\n", str);
    }
}
