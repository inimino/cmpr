/* #includes */
#define _GNU_SOURCE // for memmem
#include "siphash/siphash.h"
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <limits.h>
#include <termios.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <stddef.h>
#include <regex.h>
typedef unsigned char u8;
typedef uint64_t u64;
#define flush_exit(n) flush(); exit(n) // used only by handle_args; let's do this differently

/* #dbgx */
#define dbgd(x) prt(#x ": %d\n", x),flush()
#define dbgx(x) prt(#x ": %x\n", x),flush()
#define dbgf(x) prt(#x ": %f\n", x),flush()
#define dbgp(x) prt(#x ": %p\n", x),flush()
#define dbgs(x) prt(#x ": %.*s\n", len(x), x.buf),flush()

/* #span */
typedef struct {
  u8 *buf;
  u8 *end;
} span;

typedef struct {
  u8 *buf;
  u8 *end;
  u8 *p;
} thran;

#define BUF_SZ (1 << 30)

u8 *input_space; // remains immutable once stdin has been read up to EOF.
u8 *output_space;
u8 *cmp_space;
span out, inp, cmp;
span* outp;

int empty(span);
int len(span);

void init_spans(); // main spanio init function
void init_spans_ioc(size_t,size_t,size_t);

// basic spanio primitives

typedef struct {
  span* outp;
} out_sav;

void prt(const char *, ...);
void w_char(char);
void wrs(span);
void bksp();
void sp();
void terpri();
void w_char_esc(char);
void w_char_esc_pad(char);
void w_char_esc_dq(char);
void w_char_esc_sq(char);
void wrs_esc(span);
out_sav out2cmp();     // redirect all output functions (prt, wrs, etc) to cmp instead of out
//out_sav out2atp(span); // redirect to append to a file (creating paths and files if needed)
void out_rst(out_sav); // undo effect of out2cmp or out2atp
void flush();
//void discard(); // experimental, probably going away
void flush_err();
void write_to_file(span content, const char* filename);
int readable_file(span);
span read_file_into_span(char *filename, span buffer);
span read_file_S_into_span(span filename, span buffer);
span read_file_into_cmp(span filename);
void advance1(span*);
void advance(span*,int);
int find_char(span s, char c); int find_char_rev(span s, char c);
int contains(span, span);
span take_n(int, span*);
span next_line(span*);
span first_n(span, int);
int span_eq(span, span);
int span_cmp(span, span);
span S(char*);
span nullspan();
int copy_file(const char *src, const char *dest); // TODO: maybe take spans instead

span inp_compl();
span cmp_compl();
span out_compl();
/* #read_stdin_into_cmp */
span read_stdin_into_cmp() {
  span ret = {cmp.end,cmp.end};
  char c;
  while ((c = getchar()) != EOF) {
    *cmp.end = c;
    cmp.end++;
    if (len(cmp) == BUF_SZ) { prt("cmp space overflow reading stdin\n"); flush_err(); exit(1); }
  }
  ret.end = cmp.end;
  return ret;
}
/* #spanio_basics */
int empty(span s) {
  return s.end == s.buf;
}

inline int len(span s) { return s.end - s.buf; }

thran thran_of(span s) { return (thran){ s.buf, s.end, s.buf }; }
span thran_a(thran t) { return (span){t.buf, t.p}; }
span thran_b(thran t) { return (span){t.p, t.end}; }
span thran_full(thran t) { return (span) {t.buf, t.end}; }

int out_WRITTEN = 0, cmp_WRITTEN = 0;

void init_spans() {
  init_spans_ioc(BUF_SZ,BUF_SZ,BUF_SZ);
}

void init_spans_ioc(size_t i, size_t o, size_t c) {
  input_space = malloc(i);
  output_space = malloc(o);
  cmp_space = malloc(c);
  out.buf = output_space;
  out.end = output_space;
  inp.buf = input_space;
  inp.end = input_space;
  cmp.buf = cmp_space;
  cmp.end = cmp_space;
  outp = &out;
}

void bksp() { (*outp).end--; }

void sp() { w_char(' '); }

span head_n(int n, span *io) {
  span ret;
  ret.buf = io->buf;
  ret.end = io->buf + n;
  io->buf += n;
  return ret;
}

int span_eq(span s1, span s2) {
  if (len(s1) != len(s2)) return 0;
  for (int i = 0; i < len(s1); ++i) if (s1.buf[i] != s2.buf[i]) return 0;
  return 1;
}

int span_cmp(span s1, span s2) {
  for (;;) {
    if (empty(s1) && !empty(s2)) return 1;
    if (empty(s2) && !empty(s1)) return -1;
    if (empty(s1)) return 0;
    int dif = *(s1.buf++) - *(s2.buf++);
    if (dif) return dif;
  }
}

span S(char *s) {
  if (!s) return (span){0,0};
  span ret = {(u8*)s, (u8*)s + strlen(s) };
  return ret;
}

char* s_buffer(char* buf, int n, span s) {
  size_t l = (n - 1) < len(s) ? (n - 1) : len(s);
  memmove(buf, s.buf, l);
  buf[l] = '\0';
  return buf;
}

char* s(span s) {
  if (len(s) && s.end[-1] == '\0') return (char*)s.buf;
  char* ret = (char*)cmp.end;
  out_sav o = out2cmp();
  wrs(s);
  w_char('\0');
  out_rst(o);
  return ret;
}

void read_and_count_stdin() {
  int c;
  while ((c = getchar()) != EOF) {
    //if (c == ' ') continue;
    assert(c != 0);
    *inp.buf = c;
    inp.buf++;
    if (len(inp) == BUF_SZ) { prt("input overflow\n"); flush_err(); exit(1); }
  }
  inp.end = inp.buf;
  inp.buf = input_space;
}

// set if debugging some crash
const int ALWAYS_FLUSH = 0;

 /* C convenience methods

    We have a copy_file already here.

    We add mkdir_p and pathpart just to simplify out2atp.
 */

/* #copy_file */
int copy_file(const char *src, const char *dest) {
    int source_fd, dest_fd;
    ssize_t n_read, n_written;
    char buffer[4096];

    source_fd = open(src, O_RDONLY);
    if (source_fd < 0) {
        return -1; // Error opening source file
    }

    dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (dest_fd < 0) {
        close(source_fd);
        return -2; // Error opening destination file
    }

    while ((n_read = read(source_fd, buffer, sizeof(buffer))) > 0) {
        char *out_ptr = buffer;
        ssize_t n_left = n_read;
        while (n_left > 0) {
            n_written = write(dest_fd, out_ptr, n_left);
            if (n_written <= 0) {
                if (errno == EINTR) {
                    continue; // Retry if interrupted by signal
                }
                close(source_fd);
                close(dest_fd);
                return -3; // Error writing to destination file
            }
            n_left -= n_written;
            out_ptr += n_written;
        }
    }

    close(source_fd);
    close(dest_fd);

    if (n_read == 0) { // Successfully copied
        return 0;
    } else {
        return -4; // Error reading from source file
    }
}
/* #mkdir_p */
void mkdir_p(span dir) {
    u8* end = cmp.end;
    char old_cwd[PATH_MAX];
    if (getcwd(old_cwd, sizeof(old_cwd)) == NULL) {
        prt("Failed to get current working directory");
        flush_err();
        perror("mkdir_p");
        exit(1);
    }
    span remaining = dir;
    while (!empty(remaining)) {
        int idx = find_char(remaining, '/');
        if (idx == -1) break;
        span component = take_n(idx, &remaining);
        advance1(&remaining); // skip the "/"
        char path[PATH_MAX];
        s_buffer(path, PATH_MAX, component);
        if (chdir(path) != 0) {
            if (mkdir(path, 0755) != 0 || chdir(path) != 0) {
                prt("%.*s", len(component), component.buf);
                flush_err();
                perror("mkdir_p");
                exit(1);
            }
        }
    }
    if (chdir(old_cwd) != 0) {
        prt("Failed to return to directory: %s", old_cwd);
        flush_err();
        perror("mkdir_p");
        exit(1);
    }
    cmp.end = end;
}
/* #pathpart */
span pathpart(span dir) {
    int last_slash = find_char_rev(dir, '/');
    if (last_slash == -1) {
        return (span){ .buf = dir.buf, .end = dir.buf };
    }
    return (span){ .buf = dir.buf, .end = dir.buf + last_slash + 1 };
}

/* #spanio_basics2 */
out_sav out2cmp() { out_sav ret = {0}; ret.outp = outp; outp = &cmp; return ret; }

void out_rst(out_sav sav) {
  outp = sav.outp;
}

void prt(const char * fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *buffer;
  int n = vasprintf(&buffer, fmt, ap);
  memcpy(outp->end, buffer, n);
  free(buffer);
  outp->end += n;
  if (outp->buf + BUF_SZ < outp->end) {
    printf("OUTPUT OVERFLOW (%ld)\n", outp->end - outp->buf);
    exit(7);
  }
  va_end(ap);
  if (ALWAYS_FLUSH) flush();
}

span prs(char * fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  span ret = { .buf = cmp.end };
  char *buffer;
  int n = vasprintf(&buffer, fmt, ap);
  if (BUF_SZ < len(cmp) + n) {
    printf("CMP OVERFLOW (%d)\n", len(cmp) + n);
    exit(7);
  }
  memcpy(cmp.end, buffer, n);
  free(buffer);
  cmp.end += n;
  va_end(ap);
  if (ALWAYS_FLUSH) flush();
  ret.end = cmp.end;
  return ret;
}

void terpri() {
  *outp->end = '\n';
  outp->end++;
  if (ALWAYS_FLUSH) flush();
}

void w_char(char c) {
  *outp->end++ = c;
}

void w_char_esc(char c) {
  if (c < 0x20 || c == 127) {
    outp->end += sprintf((char*)outp->end, "\\%03o", (u8)c);
  } else {
    *outp->end++ = c;
  }
}

void w_char_esc_pad(char c) {
  if (c < 0x20 || c == 127) {
    outp->end += sprintf((char*)outp->end, "\\%03o", (u8)c);
  } else {
    sp();sp();sp();
    *outp->end++ = c;
  }
}

void w_char_esc_dq(char c) {
  if (c < 0x20 || c == 127) {
    outp->end += sprintf((char*)outp->end, "\\%03o", (u8)c);
  } else if (c == '"') {
    *outp->end++ = '\\';
    *outp->end++ = '"';
  } else if (c == '\\') {
    *outp->end++ = '\\';
    *outp->end++ = '\\';
  } else {
    *outp->end++ = c;
  }
}

void w_char_esc_sq(char c) {
  if (c < 0x20 || c == 127) {
    outp->end += sprintf((char*)outp->end, "\\%03o", (u8)c);
  } else if (c == '\'') {
    *outp->end++ = '\\';
    *outp->end++ = '\'';
  } else if (c == '\\') {
    *outp->end++ = '\\';
    *outp->end++ = '\\';
  } else {
    *outp->end++ = c;
  }
}

void wrs(span s) {
  for (u8 *c = s.buf; c < s.end; c++) w_char(*c);
}

void wrs_esc(span s) {
  for (u8 *c = s.buf; c < s.end; c++) w_char_esc(*c);
}

void flush() {
  int *WRITTEN = (output_space < outp->end && outp->end < output_space + BUF_SZ) ? &out_WRITTEN : &cmp_WRITTEN;
  if (*WRITTEN < len(*outp)) {
    //fprintf(flush_target,"%.*s", len(*outp) - *WRITTEN, outp->buf + *WRITTEN);
    fwrite(outp->buf + *WRITTEN, 1, len(*outp) - *WRITTEN, stdout);
    *WRITTEN = len(*outp);
    fflush(stdout);
  }
}

void discard() {
  int *WRITTEN = (output_space < outp->end && outp->end < output_space + BUF_SZ) ? &out_WRITTEN : &cmp_WRITTEN;
  *WRITTEN = len(*outp);
}

void flush_err() {
  int *WRITTEN = (output_space < outp->end && outp->end < output_space + BUF_SZ) ? &out_WRITTEN : &cmp_WRITTEN;
  if (*WRITTEN < len(*outp)) {
    fprintf(stderr, "%.*s", len(*outp) - *WRITTEN, outp->buf + *WRITTEN);
    *WRITTEN = len(*outp);
    fflush(stderr);
  }
}

/* #write_to_file */
void write_to_file_2(span, const char*, int);

void write_to_file(span content, const char* filename) {
  write_to_file_2(content, filename, 0);
}

void write_to_file_2(span content, const char* filename, int clobber) {
  // Attempt to open the file with O_CREAT and O_EXCL to ensure it does not already exist
  /* clobber thing is a manual fixup */
  int flags = O_WRONLY | O_CREAT | O_TRUNC;
  if (!clobber) flags |= O_EXCL;
  int fd = open(filename, flags, 0644);
  if (fd == -1) {
    if (clobber) {
      prt("Error opening %s for writing: File cannot be created or opened.\n", filename);
    } else {
      prt("Error opening %s for writing: File already exists or cannot be created.\n", filename);
    }
    flush();
    exit(EXIT_FAILURE);
  }

  // Write the content of the span to the file
  ssize_t written = write(fd, content.buf, len(content));
  if (written != len(content)) {
    // Handle partial write or write error
    prt("Error writing to file %s.\n", filename);
    flush();
    close(fd); // Attempt to close the file before exiting
    exit(EXIT_FAILURE);
  }

  // Close the file
  if (close(fd) == -1) {
    prt("Error closing %s after writing.\n", filename);
    flush();
    exit(EXIT_FAILURE);
  }
}

void write_to_file_span(span content, span filename_span, int clobber) {
  char filename[filename_span.end - filename_span.buf + 1];
  memcpy(filename, filename_span.buf, filename_span.end - filename_span.buf);
  filename[filename_span.end - filename_span.buf] = '\0';
  write_to_file_2(content, filename, clobber);
}

/* #readable_file */
int readable_file(span path) {
    char buffer[PATH_MAX];
    s_buffer(buffer, PATH_MAX, path);
    struct stat sb;
    if (stat(buffer, &sb) != 0) return 0;
    if (!S_ISREG(sb.st_mode)) return 0;
    if (access(buffer, R_OK) != 0) return 0;
    return 1;
}

span read_file_into_cmp(span filename) {
  span ret = read_file_S_into_span(filename, cmp_compl());
  cmp.end = ret.end;
  return ret;
}

span read_file_S_into_span(span filename, span buffer) {
  char path[2048];
  s_buffer(path,2048,filename);
  return read_file_into_span(path, buffer);
}

span read_file_into_span(char* filename, span buffer) {
  // Open the file
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    prt("Failed to open %s\n", filename);
    flush_err();
    exit(1);
  }

  // Get the file size
  struct stat statbuf;
  if (fstat(fd, &statbuf) == -1) {
    close(fd);
    prt("Failed to get file size for %s\n", filename);
    flush_err();exit(1);
  }

  // Check if the file's size fits into the provided buffer
  size_t file_size = statbuf.st_size;
  if (file_size > len(buffer)) {
    close(fd);
    prt("File content for %s does not fit into the provided buffer\n", filename);
    flush_err();exit(1);
  }

  // Read file contents into the buffer
  ssize_t bytes_read = read(fd, buffer.buf, file_size);
  if (bytes_read == -1) {
    close(fd);
    prt("Failed to read file contents for %s\n", filename);
    flush_err();exit(1);
  }

  // Close the file
  if (close(fd) == -1) {
    prt("Failed to close file %s\n", filename);
    flush_err();exit(1);
  }

  // Create and return a new span that reflects the read content
  span new_span = {buffer.buf, buffer.buf + bytes_read};
  return new_span;
}

/* #take_n */
span take_n(int n, span *io) {
  span ret;
  ret.buf = io->buf;
  ret.end = io->buf + n;
  io->buf += n;
  return ret;
}

void advance1(span *s) {
  if (!empty(*s)) s->buf++;
}

void advance(span *s, int n) {
  if (len(*s) >= n) s->buf += n;
  else s->buf = s->end; // Move to the end if n exceeds span length
}

void shorten1(span *s) {
  if (!empty(*s)) s->end--;
}

void shorten(span *s, int n) {
  if (n <= len(*s)) s->end -= n;
  else s->end = s->buf;
}

int contains(span haystack, span needle) {
  /*
  prt("contains() haystack:\n");
  wrs(haystack);terpri();
  prt("needle:\n");
  wrs(needle);terpri();
  */
  if (len(haystack) < len(needle)) {
    return 0; // Needle is longer, so it cannot be contained
  }
  void *result = memmem(haystack.buf, haystack.end - haystack.buf, needle.buf, needle.end - needle.buf);
  return result != NULL ? 1 : 0;
}

int contains_ptr(span a, span b) {
  return a.buf <= b.buf && b.end <= a.end;
}

int starts_with(span a, span b) {
  return len(b) <= len(a) && 0 == memcmp(a.buf, b.buf, len(b));
}

int ends_with(span a, span b) {
  return len(b) <= len(a) && 0 == memcmp(a.end - len(b), b.buf, len(b));
}

span first_n(span s, int n) {
  span ret;
  if (len(s) < n) n = len(s); // Ensure we do not exceed the span's length
  ret.buf = s.buf;
  ret.end = s.buf + n;
  return ret;
}

span skip_n(span s, int n) {
  if (len(s) <= n) return (span){s.end, s.end};
  return (span){s.buf + n, s.end};
}

void skip_whitespace(span *s) {
  while (isspace(*s->buf)) s->buf++;
}

int find_char(span s, char c) {
  for (int i = 0; i < len(s); ++i) {
    if (s.buf[i] == c) return i;
  }
  return -1; // Character not found
}

int find_char_rev(span s, char c) {
  for (int i = len(s); i; --i) {
    if (s.buf[i-1] == c) return i-1;
  }
  return -1;
}

span trim(span s) {
  while (len(s) && isspace((unsigned char)*s.buf)) s.buf++;
  while (len(s) && isspace((unsigned char)*(s.end - 1))) s.end--;
  return s;
}

span concat(span a, span b) {
  if (a.end == b.buf) return (span){a.buf, b.end};
  span ret = {cmp.end};
  out_sav o = out2cmp();
  wrs(a);
  wrs(b);
  out_rst(o);
  ret.end = cmp.end;
  return ret;
}
/* #next_line */
span next_line(span *input) {
  if (empty(*input)) return nullspan();
  span line;
  line.buf = input->buf;
  while (input->buf < input->end && *input->buf != '\n') {
    input->buf++;
  }
  line.end = input->buf;
  if (input->buf < input->end) { // If '\n' found, move past it for next call
    input->buf++;
  }
  return line;
}

/* #consume_prefix */
span consume_prefix(span prefix, span *input) {
  if (len(*input) < len(prefix) || !span_eq(first_n(*input, len(prefix)), prefix)) {
    return nullspan();
  }
  span ret = {.buf = input->buf};
  input->buf += len(prefix);
  ret.end = input->buf;
  return ret;
}
/* #generic_array_implementation */
#define MAKE_ARENA(E, T, STACK_SIZE) \
typedef struct { \
    E* a; \
    size_t n; \
    size_t cap; \
} T; \
\
typedef struct { \
    E* arena; \
    size_t arena_size; \
    size_t allocated; \
    size_t stack[STACK_SIZE]; \
    size_t stack_top; \
} T##_arena; \
\
T##_arena T##_global_arena; \
\
void T##_arena_alloc(int N) { \
    T##_global_arena.arena = (E*)malloc(sizeof(E) * N); \
    if (!T##_global_arena.arena) { \
        prt("Failed to allocate memory for arena.\n"); \
        flush(); \
        exit(1); \
    } \
    T##_global_arena.arena_size = N; \
    T##_global_arena.allocated = 0; \
    T##_global_arena.stack_top = 0; \
} \
\
void T##_arena_free() { \
    free(T##_global_arena.arena); \
    T##_global_arena.arena = NULL; \
    T##_global_arena.arena_size = 0; \
    T##_global_arena.allocated = 0; \
    T##_global_arena.stack_top = 0; \
} \
\
void T##_arena_push() { \
    if (T##_global_arena.stack_top >= STACK_SIZE) { \
        prt("Arena stack overflow.\n"); \
        flush(); \
        exit(1); \
    } \
    T##_global_arena.stack[T##_global_arena.stack_top++] = T##_global_arena.allocated; \
} \
\
void T##_arena_pop() { \
    if (T##_global_arena.stack_top == 0) { \
        prt("Arena stack underflow.\n"); \
        flush(); \
        exit(1); \
    } \
    T##_global_arena.allocated = T##_global_arena.stack[--T##_global_arena.stack_top]; \
} \
\
T T##_alloc(size_t N) { \
    T t; \
    if (!T##_global_arena.arena) { \
        prt("Arena not allocated.\n"); \
        flush(); \
        exit(1); \
    } \
    if (T##_global_arena.allocated + N > T##_global_arena.arena_size) { \
        prt("Arena overflow.\n"); \
        flush(); \
        exit(1); \
    } \
    t.a = T##_global_arena.arena + T##_global_arena.allocated; \
    t.n = 0; \
    t.cap = N; \
    T##_global_arena.allocated += N; \
    return t; \
} \
\
void T##_push(T* t, E e) { \
    if (t->n >= t->cap) { \
        if (t->a + t->cap == T##_global_arena.arena + T##_global_arena.allocated) { \
            T##_global_arena.allocated += 1; \
            t->cap += 1; \
        } else { \
            size_t new_cap = t->cap ? t->cap * 2 : 2; \
            if (T##_global_arena.allocated + new_cap > T##_global_arena.arena_size) { \
                prt("Arena overflow.\n"); \
                flush(); \
                exit(1); \
            } \
            E* new_a = T##_global_arena.arena + T##_global_arena.allocated; \
            for (size_t i = 0; i < t->n; ++i) { \
                new_a[i] = t->a[i]; \
            } \
            t->a = new_a; \
            T##_global_arena.allocated += new_cap; \
            t->cap = new_cap; \
        } \
    } \
    t->a[t->n++] = e; \
}

/* #first_generic_array_is_spans */
MAKE_ARENA(span,spans,256);

int bool_neq(int, int);
span spanspan(span haystack, span needle);
int is_one_of(span x, spans ys);

span nullspan() {
  return (span){0, 0};
}

int bool_neq(int a, int b) { return ( a || b ) && !( a && b); }

spans split_commas_ws(span s) {
  int n_commas = 0;
  for (int i=0;i<len(s);i++) {
    if (s.buf[i] == ',') n_commas++;
  }
  spans ret = spans_alloc(n_commas + 1);
  //int idx = 0;
  while (len(s)) {
    int comma = find_char(s,',');
    if (comma < 0) {
      //ret.a[idx++] = trim(s);
      spans_push(&ret,trim(s));
      break;
    } else {
      spans_push(&ret,trim(first_n(s,comma)));
      //ret.a[idx++] = trim(first_n(s,comma));
      s = skip_n(s, comma+1);
    }
  }
  return ret;
}

spans split_whitespace(span s) {
  int n_tokens = 0;
  for (int i=0;i<len(s);i++) {
    if (!isspace(s.buf[i]) && (i == 0 || isspace(s.buf[i-1]))) n_tokens++;
  }
  spans ret = spans_alloc(n_tokens);
  int idx = 0;
  while (len(s)) {
    while (len(s) && isspace(*s.buf)) s.buf++;
    if (!len(s)) break;
    span tok = {.buf = s.buf};
    while (len(s) && !isspace(*s.buf)) s.buf++;
    tok.end = s.buf;
    ret.a[idx++] = tok;
  }
  ret.n = idx;
  return ret;
}
/* #json */
typedef struct {
  span s;
} json;

int json_is_null(json);

// constructors
json json_s(span);
json json_n(double);
json json_b(int);
json json_0();
json json_o();
json json_a();
json nulljson();

// extraction
span json_un_s(json);
span json_s2s(json,span*,u8*);

// extend
void json_o_extend(json*,span,json);
void json_a_extend(json*,json);

// predicates
int json_sp(json);
int json_np(json);
int json_bp(json);
int json_0p(json);
int json_op(json);
int json_ap(json);

// lookups
json json_key(span, json);
json json_index(int, json);

// from spans
json json_parse(span);
json make_json(span);
json json_parse_prefix(span*);
json json_parse_prefix_string(span*);
json json_parse_prefix_number(span*);
json json_parse_prefix_littok(span*);

// implementation

int json_is_null(json j) { return !j.s.buf; }

json json_s(span s) {
  out_sav out = out2cmp();
  json ret = {0};
  ret.s.buf = cmp.end;
  prt("\"");
  for (u8* p=s.buf;p<s.end;p++) {
    switch (*p) {
      case '\b':
        prt("\\b");
      case '\f':
        prt("\\f");
      case '\n':
        prt("\\n");
        break;
      case '\r':
        prt("\\r");
      case '\t':
        prt("\\t");
      case '"':
        prt("\\\"");
        break;
      case '\\':
        prt("\\\\");
        break;
      default:
        if (iscntrl(*p)) {
          prt("\\u%04X", *p);
        }
        w_char(*p);
    }
  }
  prt("\"");
  ret.s.end = cmp.end;
  out_rst(out);
  return ret;
}

json json_n(double n) {
  out_sav rst = out2cmp();
  json ret = {.s = {.buf = cmp.end }};
  prt("%G", n);
  ret.s.end = cmp.end;
  out_rst(rst);
  return ret;
}

json json_b(int b) {
  out_sav rst = out2cmp();
  json ret = {.s = {.buf = cmp.end }};
  if (b) prt("true"); else prt("false");
  ret.s.end = cmp.end;
  out_rst(rst);
  return ret;
}

json json_0() {
  out_sav rst = out2cmp();
  json ret = {.s = {.buf = cmp.end }};
  prt("null");
  ret.s.end = cmp.end;
  out_rst(rst);
  return ret;
}

json json_o() {
  out_sav rst = out2cmp();
  json ret = {.s = {.buf = cmp.end }};
  prt("{}");
  ret.s.end = cmp.end;
  out_rst(rst);
  return ret;
}

void json_o_extend(json *j, span key, json val) {
  out_sav rst = out2cmp();
  u8* keybuf = malloc(len(key));
  u8* valbuf = malloc(len(val.s));
  memcpy(keybuf, key.buf, len(key));
  memcpy(valbuf, val.s.buf, len(val.s));
  span key2 = {keybuf, keybuf + len(key)};
  span val2 = {valbuf, valbuf + len(val.s)};
  cmp.end = j->s.end;
  bksp();
  if (*(cmp.end - 1) != '{') prt(",");
  //wrs(key2);
  json_s(key2);
  prt(":");
  wrs(val2);
  prt("}");
  j->s.end = cmp.end;
  free(keybuf);
  free(valbuf);
  out_rst(rst);
}

json json_a() {
  out_sav rst = out2cmp();
  json ret = {.s = {.buf = cmp.end }};
  prt("[]");
  ret.s.end = cmp.end;
  out_rst(rst);
  return ret;
}

void json_a_extend(json *a, json val) {
  out_sav rst = out2cmp();
  cmp.end = a->s.end;
  bksp();
  if (*(cmp.end - 1) != '[') prt(",");
  wrs(val.s);
  prt("]");
  a->s.end = cmp.end;
  out_rst(rst);
}

json nulljson() { return (json) {nullspan()}; }

int json_sp(json j) { return j.s.buf && *j.s.buf == '"'; }
int json_np(json j) {
  if (!j.s.buf) return 0;
  switch(*j.s.buf) {
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return 1;
    default:
      return 0;
  }
}
int json_bp(json j) { return j.s.buf && (*j.s.buf == 't' || *j.s.buf == 'f'); }
int json_0p(json j) { return j.s.buf && *j.s.buf == 'n'; }
int json_op(json j) { return j.s.buf && *j.s.buf == '{'; }
int json_ap(json j) { return j.s.buf && *j.s.buf == '['; }

json json_index(int n, json a) {
  json ret = {0};
  a.s.buf++;
  while (*a.s.buf != ']') {
    skip_whitespace(&a.s);
    ret = json_parse_prefix(&a.s);
    if (json_is_null(ret)) return nulljson();
    if (!n--) return ret;
    skip_whitespace(&a.s);
    if (*a.s.buf != ',') return nulljson();
    a.s.buf++;
  };
  return nulljson();
}

json json_key(span s, json o) {
  o.s.buf++;
  while (*o.s.buf != '}') {
    skip_whitespace(&o.s);
    json key = json_parse_prefix(&o.s);
    if (json_is_null(key)) return key;
    skip_whitespace(&o.s);
    if (*(o.s.buf++) != ':') return nulljson();
    skip_whitespace(&o.s);
    json value = json_parse_prefix(&o.s);
    if (json_is_null(value)) return nulljson();
    span key_s = json_s2s(key, &cmp, cmp_space + BUF_SZ);
    if (span_eq(key_s, s)) return value;
    skip_whitespace(&o.s);
    if (*(o.s.buf++) != ',') return nulljson();
    skip_whitespace(&o.s);
  }
  return nulljson();
}

json make_json(span s) { return (json){s}; }

span json_un_s(json s) {
  return json_s2s(s, &cmp, cmp_space + BUF_SZ);
}
/* #json_parse */
json json_parse(span s) {
  skip_whitespace(&s);
  json ret = json_parse_prefix(&s);
  skip_whitespace(&s);
  if (empty(s)) return ret;
  return nulljson();
}
/* #json_parse_prefix */
json json_parse_prefix(span *input) {
    json ret = {0};
    //skip_whitespace(input);
    ret.s.buf = input->buf;

    char first_char = *input->buf;
    switch (first_char) {
        case '\"':
            ret = json_parse_prefix_string(input);
            break;
        case '-':
        case '0' ... '9':
            ret = json_parse_prefix_number(input);
            break;
        case 't':
        case 'f':
        case 'n':
            ret = json_parse_prefix_littok(input);
            break;
        case '{':
            input->buf++; // consume '{'
            skip_whitespace(input);
            while (*input->buf != '}') {
                json key = json_parse_prefix_string(input);
                if (key.s.buf == NULL) return nulljson();
                skip_whitespace(input);
                if (*input->buf != ':') return nulljson();
                input->buf++; // consume ':'
                skip_whitespace(input);
                json value = json_parse_prefix(input);
                if (value.s.buf == NULL) return nulljson();
                skip_whitespace(input);
                if (*input->buf == ',') input->buf++; // consume ','
                skip_whitespace(input);
            }
            if (*input->buf == '}') input->buf++; // consume '}'
            else return nulljson();
            break;
        case '[':
            input->buf++; // consume '['
            skip_whitespace(input);
            while (*input->buf != ']') {
                json value = json_parse_prefix(input);
                if (value.s.buf == NULL) return nulljson();
                skip_whitespace(input);
                if (*input->buf == ',') input->buf++; // consume ','
                skip_whitespace(input);
            }
            if (*input->buf == ']') input->buf++; // consume ']'
            else return nulljson();
            break;
        default:
            return nulljson();
    }
    ret.s.end = input->buf;
    return ret;
}
/* #json_s2s */
// Utility to convert a hex digit to its integer value
int hex_to_int(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return 10 + c - 'a';
    if ('A' <= c && c <= 'F') return 10 + c - 'A';
    return -1; // Error case, should never happen if input is correct
}

// Function to parse unicode sequence and write as UTF-8
void write_utf8_from_hex(u8 **buf, char *hex) {
    int codepoint = (hex_to_int(hex[0]) << 12) | (hex_to_int(hex[1]) << 8) |
                    (hex_to_int(hex[2]) << 4) | hex_to_int(hex[3]);
    if (codepoint < 0x80) {
        *(*buf)++ = codepoint;
    } else if (codepoint < 0x800) {
        *(*buf)++ = 192 + (codepoint >> 6);
        *(*buf)++ = 128 + (codepoint & 63);
    } else if (codepoint < 0x10000) {
        *(*buf)++ = 224 + (codepoint >> 12);
        *(*buf)++ = 128 + ((codepoint >> 6) & 63);
        *(*buf)++ = 128 + (codepoint & 63);
    } else {
        *(*buf)++ = 240 + (codepoint >> 18);
        *(*buf)++ = 128 + ((codepoint >> 12) & 63);
        *(*buf)++ = 128 + ((codepoint >> 6) & 63);
        *(*buf)++ = 128 + (codepoint & 63);
    }
}

span json_s2s(json j, span *buffer, u8 *max) {
    u8 *buf = buffer->end;
    span ret = { buf, buf };

    if (*j.s.buf != '\"') {
        prt("Expected starting quote in JSON string\n");
        flush();
        exit(1);
    }

    for (u8 *s = j.s.buf + 1; s < j.s.end && *s != '\"'; s++) {
        if (buf >= max) {
            prt("Buffer overflow detected\n");
            flush();
            exit(1);
        }
        if (*s == '\\') {
            s++;
            switch (*s) {
                case 'b': *buf++ = '\b'; break;
                case 'f': *buf++ = '\f'; break;
                case 'n': *buf++ = '\n'; break;
                case 'r': *buf++ = '\r'; break;
                case 't': *buf++ = '\t'; break;
                case '\"': case '\\': case '/': *buf++ = *s; break;
                case 'u':
                    if (s + 4 >= j.s.end) {
                        prt("Incomplete unicode escape in JSON string\n");
                        flush();
                        exit(1);
                    }
                    write_utf8_from_hex(&buf, (char *)(s + 1));
                    s += 4;
                    break;
                default:
                    prt("Unknown escape sequence in JSON string\n");
                    flush();
                    exit(1);
            }
        } else {
            *buf++ = *s;
        }
    }
    ret.end = buf;
    buffer->end = buf;
    return ret;
}

/* #json_parse_prefix_string */
json json_parse_prefix_string(span *input) {
    if (empty(*input) || *input->buf != '\"') return nulljson();
    advance1(input);
    span start = *input;
    while (!empty(*input) && *input->buf != '\"') {
        if (*input->buf == '\\') {
            advance1(input);
            if (empty(*input)) return nulljson();
            if (*input->buf == 'u') {
                for (int i = 0; i < 4; i++) {
                    advance1(input);
                    if (empty(*input) || !isxdigit(*input->buf)) return nulljson();
                }
            } else if (strchr("bfnrt\"\\/", *input->buf) == NULL) {
                return nulljson();
            }
        }
        advance1(input);
    }
    if (empty(*input)) return nulljson();
    advance1(input);
    return make_json((span){start.buf - 1, input->buf});
}

/* #json_parse_prefix_number */
json json_parse_prefix_number(span *input) {
  json ret = {0};
  ret.s.buf = input->buf;

  if (*input->buf == '-') advance1(input);

  if (!isdigit(*input->buf)) return nulljson();
  while (isdigit(*input->buf)) input->buf++;

  if (*input->buf == '.') {
    advance1(input);
    if (!isdigit(*input->buf)) return nulljson();
    while(isdigit(*input->buf)) input->buf++;
  }
  if (*input->buf == 'e' || *input->buf == 'E') {
    advance1(input);
    if (*input->buf == '+' || *input->buf == '-') {
      advance1(input);
    }
    if (!isdigit(*input->buf)) return nulljson();
    while (isdigit(*input->buf)) {
      advance1(input);
    }
  }

  ret.s.end = input->buf;
  return ret;
}
/* #json_parse_prefix_littok */
json json_parse_prefix_littok(span *input) {
  span inner;
  if (!empty(inner = consume_prefix(S("true"), input))) return (json){inner};
  if (!empty(inner = consume_prefix(S("false"), input))) return (json){inner};
  if (!empty(inner = consume_prefix(S("null"), input))) return (json){inner};
  return nulljson();
}

/* #sio */
/* #parserpattern */
/* #jsonparser */
/* #spanspan */
span spanspan(span haystack, span needle) {
  if (empty(needle)) return (span){haystack.buf, haystack.buf};

  if (len(needle) > len(haystack)) return nullspan();

  void *result = memmem(haystack.buf, len(haystack), needle.buf, len(needle));

  if (!result) return (span){haystack.end, haystack.end};

  return (span){result, result + len(needle)};
}

// Checks if a given span is contained in a spans.
// Returns 1 if found, 0 otherwise.
// Actually a more useful function would return an index or -1, so we don't need another function when we care where the thing is.
int is_one_of(span x, spans ys) {
  for (int i = 0; i < ys.n; ++i) {
    if (span_eq(x, ys.a[i])) {
      return 1; // Found
    }
  }
  return 0; // Not found
}

int index_of(span x, spans ys) {
  for (int i=0; i<ys.n; i++) {
    if (span_eq(ys.a[i], x)) return i;
  }
  return -1;
}

/* #inp_compl */
span inp_compl() {
  span compl;
  compl.buf = inp.end;
  compl.end = input_space + BUF_SZ;
  return compl;
}

span cmp_compl() {
  span compl;
  compl.buf = cmp.end;
  compl.end = cmp_space + BUF_SZ;
  return compl;
}

span out_compl() {
  span compl;
  compl.buf = out.end;
  compl.end = output_space + BUF_SZ;
  return compl;
}

/* #config_fields */
#define CONFIG_FIELDS \
    X(cmprdir) \
    X(buildcmd) \
    X(bootstrap) \
    X(cbcopy) \
    X(cbpaste) \
    X(curlbin) \
    X(ollamas) \
    X(model) \
    X(debug)


/* #checksum_setup */
typedef struct {
    u64 __u;
} checksum;

MAKE_ARENA(checksum, checksums, 256)


/* #projfiles */
typedef struct {
    span path;
    span language;
    span contents;
    checksum cksum;
    checksum load_checksum;
} projfile;

MAKE_ARENA(projfile, projfiles, 256)


/* #rope */
#define SEGMENT_SIZE (32 * 1024 * 1024)

typedef struct rope_segment {
    u8 *memory;
    size_t used;
    size_t cap;
    struct rope_segment *next;
} rope_segment;

typedef struct rope {
    rope_segment *head;
} rope;

rope rope_new(size_t initial_size) {
    rope r;
    r.head = (rope_segment *)malloc(sizeof(rope_segment));
    r.head->cap = initial_size > SEGMENT_SIZE ? initial_size : SEGMENT_SIZE;
    r.head->used = 0;
    r.head->memory = (u8 *)malloc(r.head->cap);
    r.head->next = NULL;
    return r;
}

int rope_isnull(rope r) {
    return r.head ? 0 : 1;
}

void rope_release(rope *r) {
    rope_segment *current = r->head;
    while (current) {
        rope_segment *next = current->next;
        free(current->memory);
        free(current);
        current = next;
    }
    r->head = NULL;
}

span rope_alloc_atleast(rope *r, size_t size) {
    rope_segment *current = r->head;
    while (current->next) {
        current = current->next;
    }

    if (current->cap - current->used < size) {
        size_t new_cap = size > SEGMENT_SIZE ? size : SEGMENT_SIZE;
        rope_segment *new_segment = (rope_segment *)malloc(sizeof(rope_segment));
        new_segment->memory = (u8 *)malloc(new_cap);
        new_segment->used = 0;
        new_segment->cap = new_cap;
        new_segment->next = NULL;
        current->next = new_segment;
        current = new_segment;
    }

    span result;
    result.buf = current->memory + current->used;
    current->used += size;
    result.end = current->memory + current->used;
    return result;
}


/* #rev_info */
typedef struct {
    span contents;
    checksums sorted_line_cksums;
    spans ids;
    time_t timestamp;
} rev_block;

typedef struct {
    spans filenames;
    char *fnbuf;
    rev_block *revblocks;
    size_t n_revblocks;
    size_t cap_revblocks;
    rope revrope;
} rev_info;


/* #events_types */
typedef struct {
    span event_str;
    unsigned char strength;
} event_entry;

MAKE_ARENA(event_entry, event_entries, 256)

/* #ui_state */
typedef struct ui_state {
    projfiles files;
    span current_language;
    spans blocks;
    int curr_block_idx;
    int curr_file_idx;
    int marked_index;
    spans lines;
    rev_info revs;
    spans block_idx;
    span search;
    span previous_search;
    span ex_command;
    span config_file_path;
    int terminal_rows;
    int terminal_cols;
    int scrolled_lines;
    span openai_key;
    span anthropic_key;
    span bootstrapprompt;
    spans ollama_models;
    struct timespec now;
    spans outputs_filenames;
    event_entries events;
    span manual_filename;
    span open_block_id;
    int count_prefix;
    int inbox_mode;
    int inbox_start_idx;
    int inbox_end_idx;
    #define X(name) span name;
    CONFIG_FIELDS
    #undef X
} ui_state;

ui_state* state;
/* #parse_int */
int parse_int(span s) {
    if (empty(s) || !isdigit(*s.buf)) {
        prt("Error: initial characters are not digits\n");
        flush();
        exit(1);
    }
    return atoi((char *)s.buf);
}



/* #events_functions */
span read_whole_file(span filename) {
    if (!readable_file(filename)) return (span){0};
    return read_file_into_cmp(filename);
}

span head_line(span* content) {
    if (empty(*content)) return (span){0};

    span line = *content;
    u8 *ptr = line.buf;
    while (ptr < line.end && *ptr != '\n') ptr++;
    line.end = ptr;

    if (ptr < content->end) ptr++;
    content->buf = ptr;
    return line;
}

void event_add_internal(span event_str, unsigned char strength);
spans dir_listing(span dirname);
void event_parse_sn(span content);

void event_parse_content(span content) {
    event_parse_sn(content);  // Use corrected SN-compliant parsing from #event_parse_sn
}

int T_debug_enabled() {
    static int checked = 0;
    static int enabled = 0;
    if (!checked) {
        span debug_file = prs("%.*s/T-debug", len(state->cmprdir), state->cmprdir.buf);
        enabled = readable_file(debug_file);
        checked = 1;
    }
    return enabled;
}

void T_debug_print_events(const char* label) {
    if (!T_debug_enabled()) return;
    fprintf(stderr, "[T-debug] %s: %zu events\n", label, state->events.n);
    for (size_t i = 0; i < state->events.n; i++) {
        fprintf(stderr, "  [%zu] \"%.*s\" %d.\n", i,
                (int)len(state->events.a[i].event_str),
                state->events.a[i].event_str.buf,
                state->events.a[i].strength);
    }
}



/* #event_load_T */
void event_load_T() {
    span path = S(".cmpr/T");
    if (!readable_file(path)) return;
    span content = read_file_into_cmp(path);
    event_parse_sn(content);
}


/* #event_save_T */
void event_save_T() {
    span saved_cmp = cmp;
    span t_file = prs("%.*sT", len(state->cmprdir), state->cmprdir.buf);

    span content = (span){cmp.end, cmp.end};
    out_sav sav = out2cmp();

    for (size_t i = 0; i < state->events.n; i++) {
        prt("\"");
        wrs_esc(state->events.a[i].event_str);
        prt("\" %d.\n", state->events.a[i].strength);
    }

    content.end = cmp.end;
    out_rst(sav);

    write_to_file_span(content, t_file, 1);
    cmp = saved_cmp;
}



/* #event_add_internal */
void event_add_internal(span event_str, unsigned char strength) {
    for (size_t i = 0; i < state->events.n; i++) {
        if (span_eq(state->events.a[i].event_str, event_str)) {
            state->events.a[i].strength = strength;
            return;
        }
    }
    event_entry e;
    e.event_str = event_str;
    e.strength = strength;
    event_entries_push(&state->events, e);
}

/* #event_T0 */
void event_T0() {
    state->events.n = 0;
    event_save_T();
    T_debug_print_events("T0 (cleared)");
}

/* #event_get_id */
int event_get_id(span event_str) {
    span path = prs("%.*sevent-names", len(state->cmprdir), state->cmprdir.buf);
    
    int line_num = 1;
    
    if (readable_file(path)) {
        span content = read_file_into_cmp(path);
        span remaining = content;
        
        while (!empty(remaining)) {
            span line = next_line(&remaining);
            if (span_eq(line, event_str)) {
                return line_num;
            }
            line_num++;
        }
    }
    
    // Not found - append to file
    FILE *f = fopen(s(path), "a");
    if (f) {
        fprintf(f, "%.*s\n", len(event_str), event_str.buf);
        fclose(f);
    }
    
    return line_num;
}


/* #event_add */
void event_add(span event_str, unsigned char strength) {
    event_add_internal(event_str, strength);
    T_debug_print_events("event_add");
    event_save_T();

    char *depth_str = getenv("CMPR_PATTERN_DEPTH");
    int depth = depth_str ? atoi(depth_str) : 0;
    if (depth >= 4) return;

    char event_buf[4096];
    int event_len = len(event_str);
    if (event_len >= (int)sizeof(event_buf)) event_len = sizeof(event_buf) - 1;
    memcpy(event_buf, event_str.buf, event_len);
    event_buf[event_len] = '\0';
    setenv("CMPR_EVENT", event_buf, 1);

    char strength_buf[16];
    snprintf(strength_buf, sizeof(strength_buf), "%d", strength);
    setenv("CMPR_STRENGTH", strength_buf, 1);

    int event_id = event_get_id(event_str);
    char event_id_buf[16];
    snprintf(event_id_buf, sizeof(event_id_buf), "%d", event_id);
    setenv("CMPR_EVENT_ID", event_id_buf, 1);

    char new_depth[16];
    snprintf(new_depth, sizeof(new_depth), "%d", depth + 1);
    setenv("CMPR_PATTERN_DEPTH", new_depth, 1);

    system(".cmpr/scripts/patterns");

    if (depth_str) {
        setenv("CMPR_PATTERN_DEPTH", depth_str, 1);
    } else {
        unsetenv("CMPR_PATTERN_DEPTH");
    }
}
/* #event_memorize */
void event_memorize() {
    span saved_cmp = cmp;
    span events_dir = prs("%.*sevents", len(state->cmprdir), state->cmprdir.buf);
    mkdir(s(events_dir), 0777);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *tm_info = localtime(&ts.tv_sec);

    span filename = prs("%.*s/%04d%02d%02d-%02d%02d%02d-%09ld",
        len(events_dir), events_dir.buf,
        tm_info->tm_year + 1900,
        tm_info->tm_mon + 1,
        tm_info->tm_mday,
        tm_info->tm_hour,
        tm_info->tm_min,
        tm_info->tm_sec,
        (long)ts.tv_nsec);

    span content = (span){cmp.end, cmp.end};
    out_sav sav = out2cmp();

    span text_prefix = S("the text: ");
    for (size_t i = 0; i < state->events.n; i++) {
        span ev = state->events.a[i].event_str;
        // Skip "the text: " events - they're stored in revs and reconstructible
        if (len(ev) >= len(text_prefix) && 
            memcmp(ev.buf, text_prefix.buf, len(text_prefix)) == 0) {
            continue;
        }
        prt("\"");
        wrs_esc(ev);
        prt("\" %d.\n", state->events.a[i].strength);
    }

    content.end = cmp.end;
    out_rst(sav);

    write_to_file_span(content, filename, 0);
    cmp = saved_cmp;
}


/* #event_print_T */
void event_print_T() {
    for (size_t i = 0; i < state->events.n; i++) {
        prt("\"");
        wrs_esc(state->events.a[i].event_str);
        prt("\" %d.\n", state->events.a[i].strength);
    }
    flush();
}

/* #event_query */
void event_query(span event_str) {
    for (size_t i = 0; i < state->events.n; i++) {
        if (span_eq(state->events.a[i].event_str, event_str)) {
            prt("%d\n", state->events.a[i].strength);
            flush();
            return;
        }
    }
    prt("0\n");
    flush();
}

/* #event_recall */
void event_recall(span after_ts, span before_ts, int chronological) {
    span events_dir = prs("%s/events", s(state->cmprdir));
    spans files = dir_listing(events_dir);
    if (files.n == 0) {
        prt("No memories found in "); wrs(events_dir); terpri(); flush_exit(1);
    }

    // Filter files to those matching timestamp format
    spans candidates = spans_alloc(files.n);
    for (size_t i = 0; i < files.n; i++) {
        span fname = files.a[i];
        if (len(fname) < 15) continue; // too short for "YYYYMMDD-HHMMSS"
        // Timestamp and nanoseconds, e.g. "20240307-142159-123456789"
        if (!(isdigit(fname.buf[0]) && isdigit(fname.buf[1]) && isdigit(fname.buf[2]) && isdigit(fname.buf[3]) &&
              isdigit(fname.buf[4]) && isdigit(fname.buf[5]) && isdigit(fname.buf[6]) && isdigit(fname.buf[7]) &&
              fname.buf[8] == '-' &&
              isdigit(fname.buf[9]) && isdigit(fname.buf[10]) && isdigit(fname.buf[11]) &&
              isdigit(fname.buf[12]) && isdigit(fname.buf[13]) && isdigit(fname.buf[14]))) continue;
        // after_ts/before_ts filtering
        int after_ok = 1;
        int before_ok = 1;
        if (len(after_ts)) {
            if (len(fname) < len(after_ts) || span_cmp(first_n(fname, len(after_ts)), after_ts) <= 0) after_ok = 0;
        }
        if (len(before_ts)) {
            if (len(fname) < len(before_ts) || span_cmp(first_n(fname, len(before_ts)), before_ts) >= 0) before_ok = 0;
        }
        if (after_ok && before_ok)
            spans_push(&candidates, fname);
    }
    if (candidates.n == 0) {
        prt("No memories match timestamp criteria."); terpri(); flush_exit(1);
    }

    if (!chronological) { // newest to oldest
        // Assume dir_listing sorts; reverse order
        for (size_t i = 0, j = candidates.n-1; i < j; i++, j--) {
            span t = candidates.a[i];
            candidates.a[i] = candidates.a[j];
            candidates.a[j] = t;
        }
    }

    // Build set of 255-strength events in T
    spans wanted = spans_alloc(state->events.n);
    for (int i = 0; i < state->events.n; i++) {
        if (state->events.a[i].strength == 255) {
            spans_push(&wanted, state->events.a[i].event_str);
        }
    }

    // If wanted set empty, "earliest or most recent" memory
    if (wanted.n == 0) {
        span fname = candidates.a[0];
        span memfile = prs("%s/%s", s(events_dir), s(fname));
        span content = read_file_into_cmp(memfile);
        state->events.n = 0;
        event_parse_sn(content);
        event_save_T();
        prt("Loaded memory "); wrs(fname); terpri();
        flush();
        return;
    }

    // For each memory, check if all wanted events (as 255-strength) exist in that memory
    for (size_t i = 0; i < candidates.n; i++) {
        span fname = candidates.a[i];
        span memfile = prs("%s/%s", s(events_dir), s(fname));
        span content = read_file_into_cmp(memfile);

        // Parse file into a spans of event_strs with strength 255
        spans mem255 = spans_alloc(32);
        span temp = content;
        while (!empty(temp)) {
            span line = head_line(&temp);
            span trimmed = trim(line);
            if (empty(trimmed)) continue;
            if (trimmed.buf[0] != '"') continue;
            // Find last '" <digits>.' backwards
            u8 *p = trimmed.end-1;
            // look for '.'
            while (p > trimmed.buf && *p != '.') p--;
            if (p <= trimmed.buf) continue;
            // find first digit before '.'
            u8 *digits_start = p-1;
            while (digits_start >= trimmed.buf && isdigit(*digits_start)) digits_start--;
            digits_start++;
            if (digits_start > p) continue;
            // The quoted event string should end with '"' before the space before digits_start
            if (digits_start <= trimmed.buf+1) continue;
            if (*(digits_start-1) != ' ' || *(digits_start-2) != '"') continue;
            span event = (span){trimmed.buf+1, digits_start-2};
            span digits = (span){digits_start, p};
            int val = parse_int(digits);
            if (val == 255)
                spans_push(&mem255, event);
        }
        // Check if ALL wanted are in mem255.
        int all_found = 1;
        for (size_t k = 0; k < wanted.n; k++) {
            if (index_of(wanted.a[k], mem255) == -1) {
                all_found = 0;
                break;
            }
        }
        if (all_found) {
            // Load this memory into T
            state->events.n = 0;
            event_parse_sn(content);
            event_save_T();
            prt("Loaded memory "); wrs(fname); terpri();
            flush();
            return;
        }
    }

    prt("No memory containing all current 255-strength events found."); terpri(); flush_exit(1);
}

/* #event_parse_sn */
void event_parse_sn(span content) {
    state->events.n = 0;

    while (!empty(content)) {
        span line = head_line(&content);
        if (empty(line)) continue;

        // Skip leading whitespace
        while (!empty(line) && (*line.buf == ' ' || *line.buf == '\t')) line.buf++;
        if (empty(line)) continue;

        // Must start with double quote
        if (*line.buf != '"') continue;
        line.buf++; // Skip opening quote

        // Find the end pattern: " <digits>.
        // Scan backwards from end of line
        u8 *p = line.end - 1;
        
        // Skip past the period
        if (p < line.buf || *p != '.') continue;
        p--;

        // Scan backwards over digits
        u8 *digits_end = p + 1;
        while (p >= line.buf && *p >= '0' && *p <= '9') p--;
        u8 *digits_start = p + 1;
        
        if (digits_start >= digits_end) continue; // No digits found
        
        // Should have a space before digits
        if (p < line.buf || *p != ' ') continue;
        p--;
        
        // Should have a closing quote
        if (p < line.buf || *p != '"') continue;
        
        // Event string is from line.buf to p (before the closing quote)
        span event_str = (span){line.buf, p};
        
        // Parse strength from digits
        int strength = 0;
        for (u8 *d = digits_start; d < digits_end; d++) {
            strength = strength * 10 + (*d - '0');
        }
        
        event_add_internal(event_str, (unsigned char)strength);
    }
}

/* #network_ret */
typedef struct {
  int success;
  span response;
  span error;
} network_ret;


/* #sbv_state */
typedef struct {
    int *revblock_indices;
    int max_index;
    int current_index;
    spans curr_block_ids;
    checksums sorted_line_cksums;
} sbv_state;


/* #partials */
typedef enum {
    PARTIAL_SP_SP,
    PARTIAL_SP
} PartialType;

typedef struct {
    PartialType type;
    union {
        struct {
            void (*f)(span, span);
            span a;
        } sp_sp;
        void (*f)(span);
    } value;
} Partial;

Partial partial_sp_sp(span a, void(*f)(span, span)) {
    Partial p;
    p.type = PARTIAL_SP_SP;
    p.value.sp_sp.f = f;
    p.value.sp_sp.a = a;
    return p;
}

Partial partial_0_sp(void(*f)(span)) {
    Partial p;
    p.type = PARTIAL_SP;
    p.value.f = f;
    return p;
}

void apply_partial(Partial p, span arg) {
    switch (p.type) {
        case PARTIAL_SP_SP:
            p.value.sp_sp.f(p.value.sp_sp.a, arg);
            break;
        case PARTIAL_SP:
            p.value.f(arg);
            break;
        default:
            // handle error
            break;
    }
}

typedef Partial llm_message_handler;

/* #all_functions */
#include "fdecls.h"

 /*
// search
void start_search();
void perform_search();
void finalize_search();
void search_forward();
void search_backward();
int find_block(span); // find first block containing text

// ex commands
void start_ex();
void handle_ex_command();
void addfile(span);
void addlib(span);
void ex_help();
void set_highlight();
void reset_highlight();
void select_model();
int select_menu(spans opts, int sel); // allows selecting from a short list of options
void print_menu(spans, int);

// pagination and printing
void page_down();
void page_up();
void print_current_blocks();
void render_block_range(int,int);
void print_physical_lines(span, int);
int print_matching_physical_lines(span, span);
span count_physical_lines(span, int*);
void print_multiple_partial_blocks(int,int);
void print_single_block_with_skipping(int,int);

// supporting functions, CLI flags
void cmpr_init(); // handles --init
void print_block(int);
void print_comment(int);
void print_code(int);
int count_blocks();
void clear_display();
*/

/* #ingest_functions */
void get_code(); // read and index current code
void get_revs(); // read and index revs
spans find_blocks(span); // find the blocks in a file
spans find_blocks_language(span file, span language); // find_blocks helper function dispatching on language
void find_all_lines(); // like find_all_blocks, but for lines; applies to the whole project
void index_block_ids();
void ingest(); // updates everything that needs to be updated after code has changed


/* #set_default_clipboard_commands */
char* detect_os() {
    #ifdef _WIN32
        return "Windows";
    #elif __APPLE__
        return "MacOS";
    #elif __linux__
        return "Linux";
    #else
        return "Unknown";
    #endif
}

int is_wsl() {
    char buffer[256];
    FILE* fp = fopen("/proc/version", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp)) {
            fclose(fp);
            return strstr(buffer, "Microsoft") != NULL || strstr(buffer, "WSL") != NULL;
        }
        fclose(fp);
    }
    return 0;
}

void set_default_clipboard_commands() {
    char* os = detect_os();
    if (is_wsl()) {
        if (empty(state->cbcopy)) state->cbcopy = S("clip.exe");
        if (empty(state->cbpaste)) state->cbpaste = S("powershell.exe Get-Clipboard");
    } else if (strcmp(os, "MacOS") == 0) {
        if (empty(state->cbcopy)) state->cbcopy = S("pbcopy");
        if (empty(state->cbpaste)) state->cbpaste = S("pbpaste");
    } else if (strcmp(os, "Linux") == 0) {
        if (empty(state->cbcopy)) state->cbcopy = S("xclip -i -selection clipboard");
        if (empty(state->cbpaste)) state->cbpaste = S("xclip -o -selection clipboard");
    } else if (strcmp(os, "Windows") == 0) {
        if (empty(state->cbcopy)) state->cbcopy = S("clip.exe");
        if (empty(state->cbpaste)) state->cbpaste = S("powershell.exe Get-Clipboard"); 
    }
}


/* #main */
int main(int argc, char** argv) {
    ui_state stack_state = (ui_state){0};
    state = &stack_state;

    init();
    read_(argc, argv);
    main_loop();
    return 0;
}


/* #init */
void init() {
    init_spans_ioc(1UL<<30, 1UL<<30, 1UL<<30);

    projfiles_arena_alloc(1UL<<14);
    spans_arena_alloc(1UL<<20);
    checksums_arena_alloc(1UL<<20);
    event_entries_arena_alloc(1UL<<20);

    state->config_file_path = S(".cmpr/conf");
    state->files = projfiles_alloc(1024);
    state->events = event_entries_alloc(256);
    state->files.n = 0;
    state->events.n = 0;

    set_default_clipboard_commands();
    
    read_openai_key();
    read_anthropic_key();
}


/* #read_ */
void read_(int argc, char** argv) {
    clock_gettime(CLOCK_REALTIME, &state->now);
    handle_args(argc, argv);
    check_conf_vars();
    check_dirs();
    event_load_T();
    get_code();

    // Handle open at block (cmpr #blockid)
    if (!empty(state->open_block_id)) {
        int idx = block_from_arg((char*)state->open_block_id.buf);
        if (idx < 0 || idx >= state->blocks.n) {
            prt("Failed to open %.*s\n", len(state->open_block_id), state->open_block_id.buf);
            flush_exit(1);
        }
        state->curr_block_idx = idx;
    }

    // Handle inbox mode (--inbox)
    if (state->inbox_mode) {
        int inbox_idx = block_from_arg("#INBOX");
        int end_inbox_idx = block_from_arg("#END_INBOX");
        if (inbox_idx < 0 || inbox_idx >= state->blocks.n) {
            prt("Error: #INBOX block not found. See `cmpr --help inbox` for setup.\n");
            flush_exit(1);
        }
        if (end_inbox_idx < 0 || end_inbox_idx >= state->blocks.n) {
            prt("Error: #END_INBOX block not found. See `cmpr --help inbox` for setup.\n");
            flush_exit(1);
        }
        if (end_inbox_idx <= inbox_idx) {
            prt("Error: #END_INBOX must come after #INBOX\n");
            flush_exit(1);
        }
        state->inbox_start_idx = inbox_idx;
        state->inbox_end_idx = end_inbox_idx;
        state->curr_block_idx = inbox_idx;
    }
}
/* #call_llm */
void call_llm(span model, json messages, llm_message_handler cb) {
    network_ret ret;
    int is_gpt = starts_with(model, S("gpt")) || span_eq(model, S("llama.cpp"));
    int is_claude = starts_with(model, S("claude"));

    if (is_gpt) {
        ret = call_gpt(messages, model);
    } else if (is_claude) {
        ret = call_anthropic(messages, model);
    } else {
        ret = call_ollama(messages, model);
    }

    if (!ret.success) {
        wrs(ret.error);
        prt("\nPress any key to continue...");
        flush();
        getch();
        return;
    }

    if (is_gpt) {
        handle_openai_response(ret.response, cb);
    } else if (is_claude) {
        handle_anthropic_response(ret.response, cb);
    } else {
        handle_ollama_response(ret.response, cb);
    }
}


/* #read_openai_key */
void read_openai_key() {
    char path[PATH_MAX];
    struct stat st;
    char *home = getenv("HOME");
  
    if (!home) return;

    snprintf(path, PATH_MAX, "%s/.cmpr/openai-key", home);

    if (stat(path, &st) != 0) return;

    state->openai_key = trim(read_file_into_cmp(S(path)));
}


/* #read_anthropic_key */
void read_anthropic_key() {
    char path[PATH_MAX];
    struct stat st;
    char *home = getenv("HOME");
  
    if (!home) return;

    snprintf(path, PATH_MAX, "%s/.cmpr/anthropic-key", home);

    if (stat(path, &st) != 0) return;

    state->anthropic_key = trim(read_file_into_cmp(S(path)));
}


/* #filename_template */
span filename_template(span template) {
    spans vars = filename_variables();
    return expand_template(template, vars);
}


/* #assoc_spans */
/* #assoc_spans_lookup */
span assoc_spans_lookup(spans assoc_list, span key) {
    for (size_t i = 0; i < assoc_list.n / 2; ++i) {
        if (span_eq(assoc_list.a[i*2], key)) {
            return assoc_list.a[i*2 + 1];
        }
    }
    return nullspan();
}


/* #filename_variables */
spans filename_variables() {
    spans vars = spans_alloc(4);

    // Add cmprdir variable
    span cmprdir_var = S("cmprdir");
    span cmprdir_value = state->cmprdir;
    spans_push(&vars, cmprdir_var);
    spans_push(&vars, cmprdir_value);

    // Add timestamp variable
    span timestamp_var = S("timestamp");
    char timestamp_str[20];
    strftime(timestamp_str, sizeof(timestamp_str), "%Y%m%d-%H%M%S", localtime(&state->now.tv_sec));
    span timestamp_value = prs("%.*s", strlen(timestamp_str), timestamp_str);
    spans_push(&vars, timestamp_var);
    spans_push(&vars, timestamp_value);

    return vars;
}


/* #call_gpt */
network_ret call_gpt(json messages, span model) {
    span base_filename, req_filename, resp_filename, err_filename;
    char timestr[20];
    struct timespec ts;
    network_ret net_result;

    // Use current time to generate unique filenames
    clock_gettime(CLOCK_REALTIME, &ts);
    strftime(timestr, sizeof(timestr), "%Y%m%d-%H%M%S", localtime(&ts.tv_sec));

    // Set up filenames for request, response, error
    base_filename = concat(state->cmprdir, S("api_calls/"));
    base_filename = concat(base_filename, S(timestr));
    req_filename = concat(base_filename, S("-req"));
    resp_filename = concat(base_filename, S("-resp"));
    err_filename = concat(base_filename, S("-err"));

    // Switch to cmp arena for json object construction
    //prt_cmp();

    // Construct json object for API request
    json j = json_o();
    json_o_extend(&j, S("messages"), messages);
    json_o_extend(&j, S("model"), json_s(model));

    // Switch back to standard output arena
    //prt_pop();

    // Write request body to file
    write_to_file_span(j.s, req_filename, 0);

    // Call the network layer via curl wrapper function
    net_result = call_gpt_curl(req_filename, resp_filename, err_filename);

    return net_result;
}



/* #call_gpt_curl */
network_ret call_gpt_curl(span req, span resp, span err) {
    span curl_cmd = S("curl");
    if (!empty(state->curlbin)) {
        curl_cmd = state->curlbin;
    }

    int is_gpt = !span_eq(state->model, S("llama.cpp"));
    span api_key = is_gpt ? state->openai_key : S("[unused]");
    if (is_gpt && empty(api_key)) {
        return (network_ret){.success = 0, .error = S("No API key provided.")};
    }

    span content_type = S("Content-Type: application/json");
    span auth_header = prs("Authorization: Bearer %.*s", len(api_key), api_key.buf);
    span endpoint = is_gpt ? S("https://api.openai.com/v1/chat/completions") : S("http://localhost:8080/v1/chat/completions");

    char cmd_buf[1024];
    snprintf(cmd_buf, sizeof(cmd_buf), 
        "%.*s -sS -d @%.*s -H \"%.*s\" -H \"%.*s\" -o %.*s %.*s 2>%.*s",
        len(curl_cmd), curl_cmd.buf, len(req), req.buf, len(content_type), content_type.buf,
        len(auth_header), auth_header.buf, len(resp), resp.buf, len(endpoint), endpoint.buf, len(err), err.buf);

    int result = system(cmd_buf);
    span response = read_file_into_cmp(resp);
    network_ret ret = {.success = 1, .response = response};

    if (result != 0) {
        ret.success = 0;
        ret.error = read_file_into_cmp(err);
    }

    return ret;
}


/* #call_ollama */
network_ret call_ollama(json messages, span model) {
    json j = json_o();
    json_o_extend(&j, S("messages"), messages);
    json_o_extend(&j, S("model"), json_s(model));
    json_o_extend(&j, S("stream"), json_b(0));

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *tm_info = localtime(&ts.tv_sec);
    char timestamp[20];
    strftime(timestamp, 20, "%Y%m%d-%H%M%S", tm_info);

    span base_filename = prs("%.*s/api_calls/%s", len(state->cmprdir), state->cmprdir.buf, timestamp);

    span request_filename = concat(base_filename, S("-req"));
    span response_filename = concat(base_filename, S("-resp"));
    span error_filename = concat(base_filename, S("-err"));

    write_to_file_span(j.s, request_filename, 0);
    
    network_ret result = call_ollama_curl(request_filename, response_filename, error_filename);
    
    return result;
}


/* #call_ollama_curl */
network_ret call_ollama_curl(span req, span resp, span err) {
    span curl_bin = empty(state->curlbin) ? S("curl") : state->curlbin;
    char cmd[1024]; 
    
    snprintf(cmd, sizeof(cmd), 
             "%.*s -sS -X POST -H \"Content-Type: application/json\" -d @%.*s -o %.*s http://localhost:11434/api/chat 2> %.*s", 
             len(curl_bin), curl_bin.buf, 
             len(req), req.buf, 
             len(resp), resp.buf, 
             len(err), err.buf);

    int curl_result = system(cmd);
    network_ret ret;
    ret.response = read_file_into_cmp(resp);

    if (curl_result != 0) {
        ret.success = 0;
        ret.error = read_file_into_cmp(err);
    } else {
        ret.success = 1;
    }

    return ret;
}


/* #call_anthropic */
network_ret call_anthropic(json messages, span model) {
    json j = json_o();
    json_o_extend(&j, S("messages"), messages);
    json_o_extend(&j, S("model"), json_s(model));
    json_o_extend(&j, S("max_tokens"), json_n(4096));

    span req_template = filename_template(S("{cmprdir}/api_calls/{timestamp}-req"));
    span resp_template = filename_template(S("{cmprdir}/api_calls/{timestamp}-resp"));
    span err_template = filename_template(S("{cmprdir}/api_calls/{timestamp}-err"));

    write_to_file_span(j.s, req_template, 0);

    return call_anthropic_curl(req_template, resp_template, err_template);
}


/* #call_anthropic_curl */
network_ret call_anthropic_curl(span req, span resp, span err) {
    network_ret ret = {0};
    if (empty(state->anthropic_key)) {
        ret.success = 0;
        ret.error = S("No anthropic API key provided.");
        return ret;
    }
    
    span curlbin = empty(state->curlbin) ? S("curl") : state->curlbin;
    
    span command = prs("%.*s -sS -X POST -d @%.*s -H \"Content-Type: application/json\" "
                        "-H \"x-api-key: %.*s\" -H \"anthropic-version: 2023-06-01\" "
                        "-o %.*s --stderr %.*s https://api.anthropic.com/v1/messages",
                        len(curlbin), curlbin.buf,
                        len(req), req.buf,
                        len(state->anthropic_key), state->anthropic_key.buf,
                        len(resp), resp.buf,
                        len(err), err.buf);
    
    int curl_ret = system(s(command));
    
    span response_content = read_file_into_cmp(resp);
    ret.response = response_content;
    
    if (curl_ret != 0) {
        span error_content = read_file_into_cmp(err);
        ret.success = 0;
        ret.error = error_content;
    } else {
        ret.success = 1;
    }
    
    return ret;
}


/* #print_config */
void print_config() {
    #define X(name) prt(#name ": %.*s\n", len(state->name), state->name.buf);
    CONFIG_FIELDS
    #undef X
    flush();
}


/* #argtable */
/* #handle_args */
void handle_args(int argc, char **argv) {


/* #handle_args_2 */
int ind_conf = 0;
	int ind_print_conf = 0;
	int ind_init = 0;
	int ind_help = 0;
	int ind_version = 0;
	int ind_print_block = 0;
	int ind_ofra = 0;
	int ind_print_comment = 0;
	int ind_print_code = 0;
	int ind_expand_block = 0;
	int ind_content_index = 0;
	int ind_grep = 0;
	int ind_count_blocks = 0;
	int ind_files_blocks = 0;
	int ind_print_all = 0;
	int ind_inbox = 0;
	int ind_rewritepl = 0;
	int ind_prompt = 0;
	int ind_llm = 0;
	int ind_after = 0;
	int ind_replace = 0;
	int ind_replace_comment = 0;
	int ind_replace_code = 0;
	int ind_replace_current = 0;
	int ind_run = 0;
	int ind_build = 0;
	int ind_agents = 0;
	int ind_checksum = 0;
	int ind_T0 = 0;
	int ind_event = 0;
	int ind_event_stdin = 0;
	int ind_event_file = 0;
	int ind_strength = 0;
	int ind_query = 0;
	int ind_memorize = 0;
	int ind_recall = 0;
	int ind_recall_first = 0;
	int ind_T = 0;
	int ind_map_error = 0;
	int ind_test_block_map = 0;
	int ind_wants = 0;
	int ind_blocks = 0;
	int ind_wants_status = 0;
	int ind_agents_wants = 0;
	int ind_wants_dashboard = 0;
	int ind_event_report = 0;
	int ind_es = 0;
	int ind_export_docs = 0;
	int ind_snapshot_join = 0;
	int ind_learn = 0;
	int ind_log_stochastic_count_joint = 0;
	int ind_install_agent = 0;
	int ind_install_script = 0;
	int ind_file_argument = 0;
	int ind_open_block = 0;
	int ind_find_deleted = 0;
	int ind_status = 0;
	int ind_work = 0;
	int ind_trace = 0;
	int ind_P = 0;
	int ind_E = 0;
	int ind_induced = 0;
	int ind_induced_single = 0;
	int ind_lpp = 0;
	int ind_es_create = 0;
	int ind_history = 0;
	int ind_log_gap = 0;
	int ind_limit = 0;
	char *arg_work = NULL;

	char *conf_filepath = NULL;
	char *help_topic = NULL;
	char *content_index_search = NULL;
	char *grep_pattern = NULL;
	char *run_block_id = NULL;
	char *arg_print_block = NULL;
	char *arg_print_comment = NULL;
	char *arg_print_code = NULL;
	char *arg_expand_block = NULL;
	char *arg_rewritepl = NULL;
	char *arg_prompt = NULL;
	char *arg_after = NULL;
	char *arg_before = NULL;
	char *arg_replace = NULL;
	char *arg_replace_comment = NULL;
	char *arg_replace_code = NULL;
	char *event_string = NULL;
	char *event_strength_str = NULL;
	char *arg_event_file = NULL;
	char *query_string = NULL;
	char *arg_snapshot_join_es1 = NULL;
	char *arg_snapshot_join_es2 = NULL;
	char *arg_learn_es1 = NULL;
	char *arg_learn_es2 = NULL;
	char *arg_install_agent = NULL;
	char *arg_install_script = NULL;
	char *file_argument = NULL;
	char *arg_open_block = NULL;
	char *arg_induced = NULL;
	char *arg_induced_single = NULL;
	char *arg_lpp_es1 = NULL;
	char *arg_lpp_es2 = NULL;
	char *arg_es_name = NULL;
	char *arg_es_pattern = NULL;
	char *arg_history = NULL;
	char *arg_log_gap = NULL;
	char *arg_limit = NULL;

	int action_arg = 0;













/* #handle_args_3 */
for (int i = 1; i < argc; i++) {
		char *arg = argv[i];
		if (strcmp(arg, "--conf") == 0) {
			if (i+1 >= argc) { prt("Missing <filepath> argument for --conf\n"); flush(); exit(1); }
			ind_conf = 1; conf_filepath = argv[++i];
		} else if (strcmp(arg, "--print-conf") == 0) {
			ind_print_conf = 1; action_arg = 1;
		} else if (strcmp(arg, "--help") == 0) {
			ind_help = 1; action_arg = 1;
			if (i+1 < argc) {
				help_topic = argv[++i];
			}
		} else if (strcmp(arg, "--init") == 0) {
			ind_init = 1; action_arg = 1;
		} else if (strcmp(arg, "--version") == 0) {
			ind_version = 1; action_arg = 1;
		} else if (strcmp(arg, "--print-block") == 0) {
			if (i+1 >= argc) { prt("Missing <index> argument for --print-block\n"); flush(); exit(1); }
			ind_print_block = 1; arg_print_block = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--ofra") == 0) {
			ind_ofra = 1;
		} else if (strcmp(arg, "--print-comment") == 0) {
			if (i+1 >= argc) { prt("Missing <index> argument for --print-comment\n"); flush(); exit(1); }
			ind_print_comment = 1; arg_print_comment = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--print-code") == 0) {
			if (i+1 >= argc) { prt("Missing <index> argument for --print-code\n"); flush(); exit(1); }
			ind_print_code = 1; arg_print_code = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--expand-block") == 0) {
			if (i+1 >= argc) { prt("Missing <id> argument for --expand-block\n"); flush(); exit(1); }
			ind_expand_block = 1; arg_expand_block = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--rewritepl") == 0) {
			if (i+1 >= argc) { prt("Missing <id> argument for --rewritepl\n"); flush(); exit(1); }
			ind_rewritepl = 1; arg_rewritepl = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--prompt") == 0) {
			if (i+1 >= argc) { prt("Missing <id> argument for --prompt\n"); flush(); exit(1); }
			ind_prompt = 1; arg_prompt = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--llm") == 0) {
			ind_llm = 1; action_arg = 1;
		} else if (strcmp(arg, "--after") == 0) {
			if (i+1 >= argc) { prt("Missing <id> argument for --after\n"); flush(); exit(1); }
			ind_after = 1; arg_after = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--before") == 0) {
			if (i+1 >= argc) { prt("Missing <ts> argument for --before\n"); flush(); exit(1); }
			arg_before = argv[++i];
		} else if (strcmp(arg, "--replace") == 0) {
			if (i+1 >= argc) { prt("Missing <id> argument for --replace\n"); flush(); exit(1); }
			ind_replace = 1; arg_replace = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--replace-comment") == 0) {
			if (i+1 >= argc) { prt("Missing <id> argument for --replace-comment\n"); flush(); exit(1); }
			ind_replace_comment = 1; arg_replace_comment = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--replace-code") == 0) {
			if (i+1 >= argc) { prt("Missing <id> argument for --replace-code\n"); flush(); exit(1); }
			ind_replace_code = 1; arg_replace_code = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--replace-current") == 0) {
			ind_replace_current = 1; action_arg = 1;
		} else if (strcmp(arg, "--content-index") == 0) {
			if (i+1 >= argc) { prt("Missing <search> argument for --content-index\n"); flush(); exit(1); }
			ind_content_index = 1; content_index_search = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--grep") == 0) {
			if (i+1 >= argc) { prt("Missing <pattern> argument for --grep\n"); flush(); exit(1); }
			ind_grep = 1; grep_pattern = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--count-blocks") == 0) {
			ind_count_blocks = 1; action_arg = 1;
		} else if (strcmp(arg, "--files-blocks") == 0) {
			ind_files_blocks = 1; action_arg = 1;
		} else if (strcmp(arg, "--print-all") == 0) {
			ind_print_all = 1; action_arg = 1;
		} else if (strcmp(arg, "--inbox") == 0) {
			ind_inbox = 1;
		} else if (strcmp(arg, "--run") == 0) {
			if (i+1 >= argc) { prt("Missing <block_id> argument for --run\n"); flush(); exit(1); }
			ind_run = 1; run_block_id = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--build") == 0) {
			ind_build = 1; action_arg = 1;
		} else if (strcmp(arg, "--agents") == 0) {
			ind_agents = 1; action_arg = 1;
		} else if (strcmp(arg, "--agent-run") == 0) {
			// Not implemented in this parse table since it takes two arguments; skipping for now.
			prt("Unknown flag: --agent-run\n"); flush(); exit(1);
		} else if (strcmp(arg, "--checksum") == 0) {
			ind_checksum = 1; action_arg = 1;
		} else if (strcmp(arg, "--T0") == 0) {
			ind_T0 = 1; action_arg = 1;
		} else if (strcmp(arg, "--event") == 0) {
			if (i+1 >= argc) { prt("Missing <string> argument for --event\n"); flush(); exit(1); }
			ind_event = 1; event_string = argv[++i];
		} else if (strcmp(arg, "--strength") == 0) {
			if (i+1 >= argc) { prt("Missing <value> argument for --strength\n"); flush(); exit(1); }
			ind_strength = 1; event_strength_str = argv[++i];
		} else if (strcmp(arg, "--event-stdin") == 0) {
			ind_event_stdin = 1;
		} else if (strcmp(arg, "--event-file") == 0) {
			if (i+1 >= argc) { prt("Missing <path> argument for --event-file\n"); flush(); exit(1); }
			ind_event_file = 1; arg_event_file = argv[++i];
		} else if (strcmp(arg, "--query") == 0) {
			if (i+1 >= argc) { prt("Missing <string> argument for --query\n"); flush(); exit(1); }
			ind_query = 1; query_string = argv[++i];
		} else if (strcmp(arg, "--memorize") == 0) {
			ind_memorize = 1; action_arg = 1;
		} else if (strcmp(arg, "--recall") == 0) {
			ind_recall = 1; action_arg = 1;
		} else if (strcmp(arg, "--recall-first") == 0) {
			ind_recall_first = 1; action_arg = 1;
		} else if (strcmp(arg, "--T") == 0) {
			ind_T = 1; action_arg = 1;
		} else if (strcmp(arg, "--event-spaces") == 0 || strcmp(arg, "--es") == 0) {
			if (i + 2 < argc && argv[i+1][0] != '-' && argv[i+2][0] != '-') {
				ind_es_create = 1; action_arg = 1;
				arg_es_name = argv[++i];
				arg_es_pattern = argv[++i];
			} else {
			ind_es = 1; action_arg = 1;
			}
		} else if (strcmp(arg, "--P") == 0 || strcmp(arg, "--pattern") == 0) {
			ind_P = 1; action_arg = 1;
		} else if (strcmp(arg, "--E") == 0) {
			ind_E = 1; action_arg = 1;
		} else if (strcmp(arg, "--induced") == 0 && i + 1 < argc) {
			ind_induced = 1; action_arg = 1; arg_induced = argv[++i];
		} else if (strcmp(arg, "--induced-single") == 0 && i + 1 < argc) {
			ind_induced_single = 1; action_arg = 1; arg_induced_single = argv[++i];
		} else if (strcmp(arg, "--lpp") == 0 && i + 2 < argc) {
			ind_lpp = 1; action_arg = 1; arg_lpp_es1 = argv[++i]; arg_lpp_es2 = argv[++i];
		} else if (strcmp(arg, "--wants") == 0) {
			ind_wants = 1; action_arg = 1;
		} else if (strcmp(arg, "--blocks") == 0) {
			ind_blocks = 1;
		} else if (strcmp(arg, "--wants-status") == 0) {
			ind_wants_status = 1; action_arg = 1;
		} else if (strcmp(arg, "--agents-wants") == 0) {
			ind_agents_wants = 1; action_arg = 1;
		} else if (strcmp(arg, "--wants-dashboard") == 0) {
			ind_wants_dashboard = 1; action_arg = 1;
		} else if (strcmp(arg, "--event-report") == 0) {
			ind_event_report = 1; action_arg = 1;
		} else if (strcmp(arg, "--export-docs") == 0) {
			ind_export_docs = 1; action_arg = 1;
		} else if (strcmp(arg, "--find-deleted") == 0) {
			ind_find_deleted = 1; action_arg = 1;
		} else if (strcmp(arg, "--status") == 0) {
			ind_status = 1; action_arg = 1;
		} else if (strcmp(arg, "--work") == 0) {
			ind_work = 1; action_arg = 1;
			if (i+1 < argc && argv[i+1][0] != '-') {
				arg_work = argv[++i];
			}
		} else if (strcmp(arg, "--trace") == 0) {
			ind_trace = 1; action_arg = 1;
		} else if (strcmp(arg, "--install-script") == 0) {
			if (i+1 >= argc) { prt("Missing <name> argument for --install-script\n"); flush(); exit(1); }
			ind_install_script = 1; arg_install_script = argv[++i]; action_arg = 1;
		} else if (strcmp(arg, "--history") == 0) {
			ind_history = 1; action_arg = 1;
			if (i+1 < argc && argv[i+1][0] == '#') {
				arg_history = argv[++i];
			}
		} else if (strcmp(arg, "--log-gap") == 0) {
			ind_log_gap = 1;
			if (i+1 < argc && argv[i+1][0] != '-') {
				arg_log_gap = argv[++i];
			}
		} else if (strcmp(arg, "--limit") == 0) {
			if (i+1 >= argc) { prt("Missing <N> argument for --limit\n"); flush(); exit(1); }
			ind_limit = 1; arg_limit = argv[++i];
		} else if (arg[0] == '-' && arg[1] == '-') {
			prt("Unknown flag: %s\n", arg); flush(); exit(1);
		} else if (arg[0] == '#') {
			// Treat as block argument (cmpr #blockid opens TUI at that block)
			if (ind_open_block) {
				prt("Multiple block arguments provided: %s\n", arg); flush(); exit(1);
			}
			ind_open_block = 1;
			arg_open_block = arg;
		} else {
			// Treat as file argument or positional (file or "-")
			if (ind_file_argument) {
				prt("Multiple file arguments provided: %s\n", arg); flush(); exit(1);
			}
			ind_file_argument = 1;
			file_argument = arg;
		}
	}



















/* #handle_args_events */
// Event system commands - handle BEFORE general action dispatch
	// because --after/--before mean timestamps here, not block ids
	if (ind_T0 || ind_event || ind_event_stdin || ind_event_file || ind_strength || ind_query || ind_memorize || ind_recall || ind_recall_first || ind_T) {
		if ((ind_event || ind_event_stdin || ind_event_file) && !ind_strength) {
			prt("Error: --event, --event-stdin, and --event-file require --strength\n");
			flush_exit(1);
		}
		if (ind_strength && !(ind_event || ind_event_stdin || ind_event_file)) {
			prt("Error: --strength must be used with --event, --event-stdin, or --event-file\n");
			flush_exit(1);
		}
		
		int event_actions = ind_T0 + ind_event + ind_event_stdin + ind_event_file + ind_query + ind_memorize + ind_recall + ind_recall_first + ind_T;
		if (event_actions > 1) {
			prt("Error: --T0, --event, --event-stdin, --event-file, --query, --memorize, --recall, --recall-first, and --T cannot be combined\n");
			flush_exit(1);
		}
		
		check_conf_vars();
		check_dirs();
		event_load_T();
		
		if (ind_T0) {
			event_T0();
			flush_exit(0);
		}
		if (ind_event) {
			span event_span = { (u8 *)event_string, (u8 *)event_string + strlen(event_string) };
			int strength_value = event_strength_str ? atoi(event_strength_str) : 255;
			event_add(event_span, (unsigned char)strength_value);
			flush_exit(0);
		}
		if (ind_event_stdin) {
			int strength_value = event_strength_str ? atoi(event_strength_str) : 255;
			handle_event_large_stdin(strength_value);
			flush_exit(0);
		}
		if (ind_event_file) {
			int strength_value = event_strength_str ? atoi(event_strength_str) : 255;
			handle_event_large_file(S(arg_event_file), strength_value);
			flush_exit(0);
		}
		if (ind_memorize) {
			event_memorize();
			flush_exit(0);
		}
		if (ind_recall) {
			event_recall(arg_after ? S(arg_after) : nullspan(), arg_before ? S(arg_before) : nullspan(), 0);
			flush_exit(0);
		}
		if (ind_recall_first) {
			event_recall(arg_after ? S(arg_after) : nullspan(), arg_before ? S(arg_before) : nullspan(), 1);
			flush_exit(0);
		}
		if (ind_T) {
			event_print_T();
			flush_exit(0);
		}
		if (ind_query) {
			span query_span = { (u8 *)query_string, (u8 *)query_string + strlen(query_string) };
			event_query(query_span);
			flush_exit(0);
		}
	}

/* #handle_args_4 */
if (ind_file_argument) {
		state->manual_filename = S(file_argument);
	}

	// Handle open at block (cmpr #blockid) - store the ID, read_() will navigate after get_code()
	if (ind_open_block) {
		state->open_block_id = S(arg_open_block);
	}

	// Handle --inbox (TUI filtered to inbox region)
	if (ind_inbox) {
		state->inbox_mode = 1;
	}

// Handle --help, --version, --init first
	if (ind_help) {
		//get_code();
		handle_help_topic(help_topic);
		// handle_help_topic calls flush_exit, so we never reach here
	}
	
	if (ind_version) {
		prt("Version: $VERSION$\n");
		flush_exit(0);
	}
	
	if (ind_init && ind_conf) {
		prt("Error: --init and --conf cannot be used together\n");
		flush_exit(1);
	}
	
	if (ind_init) {
		cmpr_init();
		flush_exit(0);
	}
	
	// Update config file path if --conf was used
	if (ind_conf) {
		state->config_file_path = S(conf_filepath);
	}
	
	// Parse config file (always do this unless --init was used)
	parse_config();
	
	// Handle --print-conf
	if (ind_print_conf) {
		print_config();
		flush_exit(0);
	}


	// Handle --install-agent (doesn't need code loading)
	if (ind_install_agent) {
		handle_install_agent(arg_install_agent);
		flush_exit(0);
	}

	// Handle --install-script (doesn't need code loading)
	if (ind_install_script) {
		handle_install_script(arg_install_script);
		flush_exit(0);
	}

	// Count action flags (excluding event-related flags which are handled separately)
	action_arg = ind_print_block + ind_print_comment + ind_print_code + ind_expand_block +
	             ind_content_index + ind_grep + ind_count_blocks + ind_files_blocks + ind_print_all +
	             ind_rewritepl + ind_prompt + ind_llm + ind_after + ind_replace + ind_replace_comment + ind_replace_code + ind_replace_current +
	             ind_run + ind_build + ind_agents + ind_checksum +
	             ind_map_error + ind_test_block_map +
	             ind_wants +
	             ind_wants_status +
	             ind_agents_wants +
	             ind_wants_dashboard +
	             ind_event_report +
	             ind_status +
	             ind_work +
	             ind_es +
	             ind_export_docs +
	             ind_find_deleted +
	             ind_snapshot_join +
	             ind_learn +
	             ind_log_stochastic_count_joint +
	             ind_trace +
	             ind_P +
	             ind_E +
	             ind_induced +
	             ind_induced_single +
	             ind_lpp +
	             ind_es_create +
	             ind_history;
	
	if (action_arg > 1) {
		prt("Error: Only one action argument may be used at a time.\n");
		flush_exit(1);
	}

	check_conf_vars();
	check_dirs();

	// Get code database if needed (for most commands)
	if (action_arg > 0 && !ind_checksum && !ind_wants && !ind_llm && !ind_build && !ind_snapshot_join && !ind_learn && !ind_log_stochastic_count_joint && !ind_es && !ind_P && !ind_E && !ind_induced && !ind_induced_single && !ind_lpp && !ind_es_create) {
		get_code();
	}
	
	// Dispatch to handlers
	if (ind_print_block) {
		int idx = block_from_arg(arg_print_block);
		if (idx < 0 || idx >= state->blocks.n) {
			prt("Block id or index not found: %s\n", arg_print_block);
			flush_exit(1);
		}
		if (ind_ofra) {
			print_block_ofra(idx);
		} else {
			print_block(idx);
		}
		flush_exit(0);
	}
	
	if (ind_print_comment) {
		int idx = block_from_arg(arg_print_comment);
		if (idx < 0 || idx >= state->blocks.n) {
			prt("Block id or index not found: %s\n", arg_print_comment);
			flush_exit(1);
		}
		print_comment(idx);
		flush_exit(0);
	}
	
	if (ind_print_code) {
		int idx = block_from_arg(arg_print_code);
		if (idx < 0 || idx >= state->blocks.n) {
			prt("Block id or index not found: %s\n", arg_print_code);
			flush_exit(1);
		}
		print_code(idx);
		flush_exit(0);
	}
	
	if (ind_expand_block) {
		int idx = block_from_arg(arg_expand_block);
		if (idx < 0 || idx >= state->blocks.n) {
			prt("Block id or index not found: %s\n", arg_expand_block);
			flush_exit(1);
		}
		expand_block(idx);
		flush_exit(0);
	}
	
	if (ind_content_index) {
		content_index(S(content_index_search));
		flush_exit(0);
	}
	
	if (ind_grep) {
		grep_blocks(S(grep_pattern));
		flush_exit(0);
	}
	
	if (ind_count_blocks) {
		prt("%d\n", state->blocks.n);
		flush_exit(0);
	}
	
	if (ind_files_blocks) {
		print_files_blocks();
		flush_exit(0);
	}
	
	if (ind_print_all) {
		for (int i = 0; i < state->blocks.n; i++) {
			wrs(state->blocks.a[i]);
		}
		flush_exit(0);
	}
	
	if (ind_rewritepl) {
		int idx = block_from_arg(arg_rewritepl);
		if (idx < 0 || idx >= state->blocks.n) {
			prt("Block id or index not found: %s\n", arg_rewritepl);
			flush_exit(1);
		}
		state->curr_block_idx = idx;
		nl2pl_rewrite();
		flush_exit(0);
	}
	
	if (ind_prompt) {
		int idx = block_from_arg(arg_prompt);
		if (idx < 0 || idx >= state->blocks.n) {
			prt("Block id or index not found: %s\n", arg_prompt);
			flush_exit(1);
		}
		handle_prompt(idx);
		flush_exit(0);
	}
	
	if (ind_llm) {
		handle_llm();
		flush_exit(0);
	}
	
	if (ind_after) {
		after(S(arg_after));
		flush_exit(0);
	}
	
	if (ind_replace) {
		replace(S(arg_replace));
		flush_exit(0);
	}
	
	if (ind_replace_comment) {
		replace_comment(S(arg_replace_comment));
		flush_exit(0);
	}
	
	if (ind_replace_code) {
		replace_code(S(arg_replace_code));
		flush_exit(0);
	}

	if (ind_replace_current) {
		handle_replace_current();
		flush_exit(0);
	}

	if (ind_run) {
		handle_run(run_block_id);
		flush_exit(0);
	}

	if (ind_build) {
		ensure_conf_var(&state->buildcmd, S("The build command to run"), nullspan());
		char buf[2048] = {0};
		s_buffer(buf, sizeof(buf), state->buildcmd);
		int status = system(buf);
		flush_exit(WIFEXITED(status) ? WEXITSTATUS(status) : 1);
	}

	if (ind_agents) {
		handle_agents();
		flush_exit(0);
	}
	
	if (ind_checksum) {
		handle_checksum();
		flush_exit(0);
	}
	
	if (ind_snapshot_join) {
		handle_snapshot_join(S(arg_snapshot_join_es1), S(arg_snapshot_join_es2));
		flush_exit(0);
	}

	if (ind_learn) {
		handle_learn(S(arg_learn_es1), S(arg_learn_es2));
		flush_exit(0);
	}

	if (ind_log_stochastic_count_joint) {
		handle_log_stochastic_count_joint();
		flush_exit(0);
	}
	
	// Event system commands (have special validation)
	if (ind_T0 || ind_event || ind_strength || ind_query || ind_memorize || ind_recall || ind_recall_first || ind_event_stdin || ind_event_file || ind_T) {
		if ((ind_event || ind_event_stdin || ind_event_file) && !ind_strength) {
			prt("Error: --event, --event-stdin, and --event-file require --strength\n");
			flush_exit(1);
		}
		if (ind_strength && !(ind_event || ind_event_stdin || ind_event_file)) {
			prt("Error: --strength must be used with --event\n");
			flush_exit(1);
		}
		
		int event_actions = ind_T0 + ind_event + ind_event_stdin + ind_event_file + ind_query + ind_memorize + ind_recall + ind_recall_first + ind_T;
		if (event_actions > 1) {
			prt("Error: --T0, --event, --event-stdin, --event-file, --query, --memorize, --recall, --recall-first, and --T cannot be combined\n");
			flush_exit(1);
		}
		
		check_conf_vars();
		check_dirs();
		event_load_T();
		
		if (ind_T0) {
			event_T0();
			flush_exit(0);
		}
		if (ind_event) {
			span event_span = { (u8 *)event_string, (u8 *)event_string + strlen(event_string) };
			int strength_value = event_strength_str ? atoi(event_strength_str) : 255;
			event_add(event_span, (unsigned char)strength_value);
			flush_exit(0);
		}
		if (ind_event_stdin) {
			int strength_value = event_strength_str ? atoi(event_strength_str) : 255;
			handle_event_large_stdin(strength_value);
			flush_exit(0);
		}
		if (ind_event_file) {
			int strength_value = event_strength_str ? atoi(event_strength_str) : 255;
			handle_event_large_file(S(arg_event_file), strength_value);
			flush_exit(0);
		}
		if (ind_memorize) {
			event_memorize();
			flush_exit(0);
		}
		if (ind_recall) {
			event_recall(arg_after ? S(arg_after) : nullspan(), arg_before ? S(arg_before) : nullspan(), 0);
			flush_exit(0);
		}
		if (ind_recall_first) {
			event_recall(arg_after ? S(arg_after) : nullspan(), arg_before ? S(arg_before) : nullspan(), 1);
			flush_exit(0);
		}
		if (ind_T) {
			event_print_T();
			flush_exit(0);
		}
		if (ind_query) {
			span query_span = { (u8 *)query_string, (u8 *)query_string + strlen(query_string) };
			event_query(query_span);
			flush_exit(0);
		}
	}
	
	if (ind_map_error) {
		prt("Error: --map-error not yet implemented\n");
		flush_exit(1);
	}
	
// if (ind_test_block_map) {
// block_map_selftest();
// flush_exit(0);
// }
	
	if (ind_wants) {
		handle_wants(ind_blocks);
		flush_exit(0);
	}

	if (ind_wants_status) {
		handle_wants_status();
		flush_exit(0);
	}

	if (ind_agents_wants) {
		handle_agents_wants();
		flush_exit(0);
	}

	if (ind_wants_dashboard) {
		handle_wants_dashboard();
		flush_exit(0);
	}

	if (ind_export_docs) {
		handle_export_docs();
		flush_exit(0);
	}

	if (ind_find_deleted) {
		get_revs();
		find_all_deleted_blocks();
		flush_exit(0);
	}

	if (ind_event_report) {
		handle_event_report();
		flush_exit(0);
	}

	if (ind_status) {
		handle_status();
		flush_exit(0);
	}

	if (ind_work) {
		handle_work(arg_work);
		flush_exit(0);
	}

	if (ind_trace) {
		handle_trace();
		flush_exit(0);
	}

	if (ind_es) {
		handle_es();
		flush_exit(0);
	}

	if (ind_P) {
		handle_P();
		flush_exit(0);
	}

	if (ind_E) {
		handle_E();
		flush_exit(0);
	}

	if (ind_induced) {
		handle_induced(arg_induced);
		flush_exit(0);
	}

	if (ind_induced_single) {
		handle_induced_single(arg_induced_single);
		flush_exit(0);
	}

	if (ind_lpp) {
		handle_lpp(arg_lpp_es1, arg_lpp_es2);
		flush_exit(0);
	}

	if (ind_es_create) {
		handle_es_create(arg_es_name, arg_es_pattern);
		flush_exit(0);
	}

	if (ind_history) {
		double log_gap_factor = ind_log_gap ? (arg_log_gap ? atof(arg_log_gap) : 2.0) : 0.0;
		int limit = ind_limit ? atoi(arg_limit) : 0;
		handle_history(arg_history ? S(arg_history) : nullspan(), log_gap_factor, limit);
		flush_exit(0);
	}

	// No action arg - return to enter interactive mode
}






















/* #handle_snapshot_join */
void handle_snapshot_join(span es1, span es2) {
    // Build filter paths
    char es1_path[256], es2_path[256];
    snprintf(es1_path, sizeof(es1_path), ".cmpr/es/%.*s", len(es1), es1.buf);
    snprintf(es2_path, sizeof(es2_path), ".cmpr/es/%.*s", len(es2), es2.buf);
    
    // Check filters exist and are executable
    if (access(es1_path, X_OK) != 0) {
        prt("Error: Event space filter not found: %s\n", es1_path);
        flush();
        exit(1);
    }
    if (access(es2_path, X_OK) != 0) {
        prt("Error: Event space filter not found: %s\n", es2_path);
        flush();
        exit(1);
    }
    
    // Open events directory
    DIR *dir = opendir(".cmpr/events");
    if (!dir) {
        flush();
        return;
    }
    
    // Collect snapshot filenames
    char *snapshots[4096];
    int n = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL && n < 4096) {
        if (de->d_name[0] == '.') continue;
        snapshots[n++] = strdup(de->d_name);
    }
    closedir(dir);
    
    if (n == 0) {
        flush();
        return;
    }
    
    // Sort newest first (reverse strcmp)
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (strcmp(snapshots[i], snapshots[j]) < 0) {
                char *tmp = snapshots[i];
                snapshots[i] = snapshots[j];
                snapshots[j] = tmp;
            }
        }
    }
    
    // Process each snapshot
    for (int i = 0; i < n; i++) {
        char snap_path[512];
        snprintf(snap_path, sizeof(snap_path), ".cmpr/events/%s", snapshots[i]);
        
        // Run through ES1 filter
        char cmd1[1024];
        snprintf(cmd1, sizeof(cmd1), "cat '%s' | '%s'", snap_path, es1_path);
        FILE *fp1 = popen(cmd1, "r");
        char *es1_lines[1024];
        int es1_n = 0;
        if (fp1) {
            char buf[4096];
            while (fgets(buf, sizeof(buf), fp1) && es1_n < 1024) {
                size_t l = strlen(buf);
                if (l > 0 && buf[l-1] == '\n') buf[l-1] = 0;
                if (buf[0]) es1_lines[es1_n++] = strdup(buf);
            }
            pclose(fp1);
        }
        
        // Run through ES2 filter
        char cmd2[1024];
        snprintf(cmd2, sizeof(cmd2), "cat '%s' | '%s'", snap_path, es2_path);
        FILE *fp2 = popen(cmd2, "r");
        char *es2_lines[1024];
        int es2_n = 0;
        if (fp2) {
            char buf[4096];
            while (fgets(buf, sizeof(buf), fp2) && es2_n < 1024) {
                size_t l = strlen(buf);
                if (l > 0 && buf[l-1] == '\n') buf[l-1] = 0;
                if (buf[0]) es2_lines[es2_n++] = strdup(buf);
            }
            pclose(fp2);
        }
        
        // Check for strength 255 in each
        int es1_has_255 = 0, es2_has_255 = 0;
        for (int j = 0; j < es1_n; j++) {
            char *p = strrchr(es1_lines[j], ' ');
            if (p && atoi(p+1) == 255) { es1_has_255 = 1; break; }
        }
        for (int j = 0; j < es2_n; j++) {
            char *p = strrchr(es2_lines[j], ' ');
            if (p && atoi(p+1) == 255) { es2_has_255 = 1; break; }
        }
        
        // Output if both have 255
        if (es1_has_255 && es2_has_255) {
            prt("%s\n", snapshots[i]);
            for (int j = 0; j < es1_n; j++) prt("%s\n", es1_lines[j]);
            for (int j = 0; j < es2_n; j++) prt("%s\n", es2_lines[j]);
            prt("\n");
        }
        
        // Free lines
        for (int j = 0; j < es1_n; j++) free(es1_lines[j]);
        for (int j = 0; j < es2_n; j++) free(es2_lines[j]);
    }
    
    // Free snapshots
    for (int i = 0; i < n; i++) free(snapshots[i]);
    flush();
}

/* #help_text_nl2pl */
/* #print_physical_lines */
void print_physical_lines(span block, int lines_to_print) {
    while (!empty(block) && lines_to_print > 0) {
        span line = next_line(&block); // Get the next logical line from the block

        // Handle blank lines
        if (line.end == line.buf) {
            if (lines_to_print > 0) {
                terpri(); // Print a newline for a blank logical line
                lines_to_print--;
            }
            continue; // Move to the next line
        }

        // Calculate the number of physical lines required for this logical line
        int line_length = line.end - line.buf;
        int physical_lines_needed = (line_length / state->terminal_cols) + (line_length % state->terminal_cols != 0);

        if (physical_lines_needed <= lines_to_print) {
            // If the entire logical line fits within the remaining physical lines
            for (int i = 0; i < line_length; i += state->terminal_cols) {
                int chars_to_print = (i + state->terminal_cols > line_length) ? (line_length - i) : state->terminal_cols;
                prt("%.*s\n", chars_to_print, line.buf + i); // Print a segment of the logical line
            }
            lines_to_print -= physical_lines_needed;
        } else {
            // If the logical line does not fit entirely, print parts of it to fit in the remaining lines
            for (int i = 0; i < lines_to_print * state->terminal_cols; i += state->terminal_cols) {
                int chars_to_print = (i + state->terminal_cols > line_length) ? (line_length - i) : state->terminal_cols;
                prt("%.*s\n", chars_to_print, line.buf + i);
            }
            lines_to_print = 0; // We've filled the remaining lines
        }
    }
}

/* #print_files_blocks */
void print_files_blocks() {
    for (int f = 0; f < state->files.n; f++) {
        projfile *file = &state->files.a[f];
        prt("file: %s", s(file->path));
        terpri();
	if (empty(file->contents)) continue;
        int first = first_block_in_file(f);
        int last  = last_block_in_file(f);
        for (int i = first; i <= last; i++) {
            span block = state->blocks.a[i];
            spans ids = ids_for_block(block);
            prt("Block %d", i + 1);
            if (ids.n > 0) {
                prt(": %s", s(ids.a[0]));
            }
            terpri();
        }
    }
    flush();
}

/* #clear_display */
void clear_display() {
    prt("\033[2J\033[H"); // Escape codes to clear the screen and move the cursor to the top-left corner
    flush();
}


/* #block_sanity_check */
void block_sanity_check(span file, spans blocks) {
    if (empty(file)) {
        if (blocks.n != 1 || !empty(blocks.a[0])) {
            prt("Error: Empty file must have exactly one empty block.\n");
            flush();
            exit(EXIT_FAILURE);
        }
        return; // Early exit for empty file
    }

    // Check if the first block begins where the input span begins
    if (blocks.a[0].buf != file.buf) {
        prt("Error: The first block does not start where input begins.\n");
        flush();
        exit(EXIT_FAILURE);
    }

    // Check if the last block ends where the input ends
    if (blocks.a[blocks.n - 1].end != file.end) {
        prt("Error: The last block does not end where input ends.\n");
        flush();
        exit(EXIT_FAILURE);
    }

    // Ensure all blocks tile the file and none are empty
    for (int i = 1; i < blocks.n; ++i) {
        if (blocks.a[i].buf != blocks.a[i - 1].end || empty(blocks.a[i])) {
            prt("Error: Blocks do not properly tile the file or a block is empty.\n");
            flush();
            exit(EXIT_FAILURE);
        }
    }
}


/* #inp_sanity_checks */
void inp_sanity_checks() {
    // Check blocks tile inp
    if (state->blocks.n == 0) {
        if (!empty(inp)) {
            prt("inp should be empty when there are no blocks.");
            flush_exit(1);
        }
    } else {
        if (state->blocks.a[0].buf != inp.buf) {
            prt("The first block should start at the beginning of inp.");
            flush_exit(1);
        }
        for (size_t i = 1; i < state->blocks.n; i++) {
            if (state->blocks.a[i].buf != state->blocks.a[i - 1].end) {
                prt("Blocks are not contiguous.");
                flush_exit(1);
            }
            if (empty(state->blocks.a[i])) {
                prt("No block should be empty.");
                flush_exit(1);
            }
        }
        if (state->blocks.a[state->blocks.n - 1].end != inp.end) {
            prt("The last block should end at the end of inp.");
            flush_exit(1);
        }
    }

    // Check files tile inp
    if (state->files.n == 0) {
        if (!empty(inp)) {
            prt("inp should be empty when there are no files.");
            flush_exit(1);
        }
    } else {
        if (state->files.a[0].contents.buf != inp.buf) {
            prt("The first file should start at the beginning of inp.");
            flush_exit(1);
        }
        for (size_t i = 1; i < state->files.n; i++) {
            if (state->files.a[i].contents.buf != state->files.a[i - 1].contents.end) {
                prt("Files are not contiguous.");
                flush_exit(1);
            }
        }
        if (state->files.a[state->files.n - 1].contents.end != inp.end) {
            prt("The last file should end at the end of inp.");
            flush_exit(1);
        }
    }

    // Check every file is tiled by blocks
    size_t block_idx = 0;
    for (size_t i = 0; i < state->files.n; i++) {
        if (!empty(state->files.a[i].contents)) {
            if (state->files.a[i].contents.buf != state->blocks.a[block_idx].buf) {
                prt("File does not align with the start of a block.");
                flush_exit(1);
            }
            while (block_idx < state->blocks.n && 
                   state->blocks.a[block_idx].buf < state->files.a[i].contents.end) {
                if (state->blocks.a[block_idx].end > state->files.a[i].contents.end) {
                    prt("Block exceeds the end of the file.");
                    flush_exit(1);
                }
                block_idx++;
            }
        }
    }
}


/* #jk_implementation */
/* #find_all_blocks */
void find_all_blocks() {
   state->blocks = spans_alloc(256);

   for (size_t i = 0; i < state->files.n; i++) {
       if (!empty(state->files.a[i].contents)) {
           spans file_blocks = find_blocks_language(state->files.a[i].contents, state->files.a[i].language);
           for (size_t j = 0; j < file_blocks.n; j++) {
               spans_push(&state->blocks, file_blocks.a[j]);
           }
       }
   }

   if (state->blocks.n == 0) {
       state->curr_block_idx = -1;
   } else if (state->curr_block_idx >= (int)state->blocks.n) {
       state->curr_block_idx = state->blocks.n - 1;
   }
}

/* #find_all_lines */
void find_all_lines() {
    span input_copy = inp;
    int line_count = 0;
    
    while (!empty(input_copy)) {
        next_line(&input_copy);
        line_count++;
    }

    state->lines = spans_alloc(line_count);
    input_copy = inp;

    for (int i = 0; i < line_count; i++) {
        spans_push(&state->lines, next_line(&input_copy));
    }
}


/* #selected_checksum */
checksum selected_checksum(span input) {
    static const char key[16] = "ABCDEFGHIJKLMNOP";
    u64 result;
    siphash(input.buf, len(input), key, (uint8_t*)&result, sizeof(result));
    return (checksum){result};
}


/* #get_code */
void get_code() {
    for (int i = 0; i < state->files.n; i++) {
        state->files.a[i].contents = read_file_S_into_span(state->files.a[i].path, inp_compl());
        inp.end = state->files.a[i].contents.end; // Advance inp to not overwrite contents
        state->files.a[i].load_checksum = selected_checksum(state->files.a[i].contents);
    }

    if (state->files.n == 0) state->curr_file_idx = -1;
    else state->curr_file_idx = 0;

    ingest();
}



/* #ingest */
void ingest() {
    find_all_blocks();
    find_all_lines();
    index_block_ids();
    inp_sanity_checks();
}


/* #blocks */
/* #files */
/* #index_block_ids */
void index_block_ids() {
    int id_count = 0;
    for (int i = 0; i < state->blocks.n; i++) {
        span block = state->blocks.a[i];
        span line = next_line(&block);
        if (line.buf == line.end) continue;
        if (line.buf[0] != '#') {
            spans tokens = split_whitespace(line);
            for (int j = 0; j < tokens.n; j++) {
                if (tokens.a[j].buf[0] == '#') {
                    id_count++;
                }
            }
        }
    }

    state->block_idx = spans_alloc(id_count);

    for (int i = 0; i < state->blocks.n; i++) {
        span block = state->blocks.a[i];
        span line = next_line(&block);
        if (line.buf == line.end) continue;
        if (line.buf[0] != '#') {
            spans tokens = split_whitespace(line);
            for (int j = 0; j < tokens.n; j++) {
                if (tokens.a[j].buf[0] == '#') {
                    spans_push(&state->block_idx, tokens.a[j]);
                }
            }
        }
    }
}


/* #ids_for_block */
spans ids_for_block(span block) {
    int id_count = 0;
    span block_copy = block;
    span line = next_line(&block_copy);
    if (line.buf[0] != '#') {
        spans tokens = split_whitespace(line);
        for (int j = 0; j < tokens.n; j++) {
            if (tokens.a[j].buf[0] == '#') {
                id_count++;
            }
        }
    }

    spans ids = spans_alloc(id_count);

    line = next_line(&block);
    if (line.buf[0] != '#') {
        spans tokens = split_whitespace(line);
        for (int j = 0; j < tokens.n; j++) {
            if (tokens.a[j].buf[0] == '#') {
                spans_push(&ids, tokens.a[j]);
            }
        }
    }

    assert(ids.n >= 0);
    return ids;
}


/* #block_idx */
/* #block_for_span */
int block_for_span(span s) {
    for (int i = 0; i < state->blocks.n; i++) {
        if (contains_ptr(state->blocks.a[i], s)) {
            return i;
        }
    }
    return -1;
}


/* #id_for_block */
span id_for_block(span block) {
    span line = next_line(&block);
    spans tokens = split_whitespace(line);
    for (int i = 0; i < tokens.n; i++) {
        if (tokens.a[i].buf[0] == '#') {
            return tokens.a[i];
        }
    }
    return nullspan();
}



/* #current_block_checksum */
checksum current_block_checksum() {
    return selected_checksum(state->blocks.a[state->curr_block_idx]);
}


/* #set_current_block */
void set_current_block(int idx) {
    if (idx < 0 || idx >= state->blocks.n) {
        prt("Error: Block index %d out of range.\n", idx);
        flush_err();
        exit(1);
    }

    state->curr_block_idx = idx;
    state->scrolled_lines = 0;

    span block = state->blocks.a[idx];
    state->curr_file_idx = file_for_block(block);
}


/* #block_id_jump */
void block_id_jump() {
    span current_block = state->blocks.a[state->curr_block_idx];
    span id = id_for_block(current_block);
    int idx = 0;

    if (!empty(id)) {
        idx = index_of(id, state->block_idx);
        if (idx == -1) idx = 0;
    }

    idx = select_menu_searchable(state->block_idx, idx);
    if (idx != -1) {
        span selected_id = state->block_idx.a[idx];
        set_current_block(block_for_span(selected_id));
    }
}

/* #refs_for_block */
spans refs_for_block(span block) {
    spans refs = spans_alloc(8);
    span top = next_line(&block);

    // Top line tokens
    span tokens = top;
    while (!empty(tokens)) {
        // skip leading whitespace
        while (!empty(tokens) && isspace(*tokens.buf)) advance1(&tokens);
        if (empty(tokens)) break;
        // find next whitespace/token end
        u8* start = tokens.buf;
        u8* p = start;
        while (p < tokens.end && !isspace(*p)) p++;
        span tok = (span){start, p};
        if (!empty(tok) && *tok.buf == '@')
            spans_push(&refs, tok);
        tokens.buf = p;
        // No advance1 for whitespace here, loop'll skip ws next round
    }

    // Remaining lines
    while (!empty(block)) {
        span line = next_line(&block);
        if (len(line) >= 1 && *line.buf == '@') {
            if (!(len(line) >= 3 && line.buf[0] == '@' && line.buf[1] == '-' && isspace(line.buf[2]))) {
                // Extract first whitespace-separated token
                u8* start = line.buf, *p = line.buf;
                while (p < line.end && !isspace(*p)) p++;
                spans_push(&refs, (span){start, p});
            }
        }
    }
    return refs;
}

/* #mentions_for_block */
spans mentions_for_block(span block) {
    span block_copy = block;
    spans own_ids = ids_for_block(block_copy);
    spans result = spans_alloc(8);

    // Skip topline
    next_line(&block);

    // Scan remaining content for #blockid tokens
    u8* p = block.buf;
    while (p < block.end) {
        if (*p == '#') {
            u8* start = p;
            p++; // skip '#'
            // collect alphanumeric and underscore
            while (p < block.end && (isalnum(*p) || *p == '_')) p++;
            if (p > start + 1) { // at least one char after '#'
                span tok = (span){start, p};
                // check if it's one of our own ids
                int is_own = 0;
                for (int i = 0; i < own_ids.n; i++) {
                    if (span_eq(tok, own_ids.a[i])) { is_own = 1; break; }
                }
                // check if already in result
                int is_dup = 0;
                if (!is_own) {
                    for (int i = 0; i < result.n; i++) {
                        if (span_eq(tok, result.a[i])) { is_dup = 1; break; }
                    }
                }
                if (!is_own && !is_dup) {
                    spans_push(&result, tok);
                }
            }
        } else {
            p++;
        }
    }
    return result;
}

/* #referrers_to_block */
spans referrers_to_block(int block_idx) {
    spans result = spans_alloc(8);
    span block = state->blocks.a[block_idx];
    spans our_ids = ids_for_block(block);
    for (int i = 0; i < state->blocks.n; i++) {
        if (i == block_idx) continue;
        spans refs = refs_for_block(state->blocks.a[i]);
        for (int j = 0; j < refs.n; j++) {
            span ref = refs.a[j];
            if (len(ref) < 2 || ref.buf[0] != '@') continue;
            span bare = ref;
            advance1(&bare); // skip '@'
            int colon = find_char(bare, ':');
            if (colon >= 0) bare = first_n(bare, colon);
            for (int k = 0; k < our_ids.n; k++) {
                span id = our_ids.a[k];
                if (len(id) < 2 || id.buf[0] != '#') continue;
                span our_id_bare = id; 
                advance1(&our_id_bare); // skip '#'
                if (span_eq(bare, our_id_bare)) {
                    // get the first id for this referrer block
                    spans referrer_ids = ids_for_block(state->blocks.a[i]);
                    if (referrer_ids.n > 0)
                        spans_push(&result, referrer_ids.a[0]);
                    break;
                }
            }
        }
    }
    return result;
}

/* #block_refs_jump */
void block_refs_jump() {
    if (state->curr_block_idx < 0) return;
    spans breadcrumb = spans_alloc(0);
    int idx = refs_menu(state->curr_block_idx, breadcrumb);
    if (idx != -1) set_current_block(idx);
}

/* #get_revdir */
span get_revdir() {
    static char buf[2048] = {0};
    span revs = S("revs");
    span revdir = concat(state->cmprdir, revs);
    s_buffer(buf, 2048, revdir);
    return S(buf);
}



/* #complain_and_exit */
/* #complain_and_prompt */
/* #get_revs */
void get_revs_2();

void get_revs() {
    span revdir = get_revdir();
    DIR *dir = opendir(s(revdir));
    if (!dir) {
        prt("Cannot open revs directory: %s\n", s(revdir));
        flush();
        exit(1);
    }

    int file_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
        file_count++;

    closedir(dir);
    file_count += 8;
    state->revs.filenames = spans_alloc(file_count);

    size_t buf_size = 16 * file_count;
    state->revs.fnbuf = (char *)malloc(buf_size);
    if (!state->revs.fnbuf) {
        prt("Memory allocation failed for filenames buffer\n");
        flush();
        exit(1);
    }

    char *buf_ptr = state->revs.fnbuf;
    dir = opendir(s(revdir));
    if (!dir) {
        prt("Cannot reopen revs directory: %s\n", s(revdir));
        flush();
        exit(1);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strlen(entry->d_name) == 15 && isdigit(entry->d_name[0]) && isdigit(entry->d_name[1]) &&
            isdigit(entry->d_name[2]) && isdigit(entry->d_name[3]) && isdigit(entry->d_name[4]) &&
            isdigit(entry->d_name[5]) && isdigit(entry->d_name[6]) && isdigit(entry->d_name[7]) &&
            entry->d_name[8] == '-' && isdigit(entry->d_name[9]) && isdigit(entry->d_name[10]) &&
            isdigit(entry->d_name[11]) && isdigit(entry->d_name[12]) && isdigit(entry->d_name[13]) &&
            isdigit(entry->d_name[14]))
        {
            strcpy(buf_ptr, entry->d_name);
            state->revs.filenames.a[state->revs.filenames.n++] = S(buf_ptr);
            buf_ptr += 16;
        }
    }

    closedir(dir);

    qsort(state->revs.filenames.a, state->revs.filenames.n, sizeof(span), span_cmp_wrapper);

    /*
    for (size_t i = 0; i < state->revs.filenames.n; i++)
    {
        wrs(state->revs.filenames.a[i]);
        terpri();
    }

    flush();
    exit(0);
    */
    // before calling get_revs_2 we initialize the rope
    //state->revs.revrope = rope_new(16 * 1024 * 1024);
    get_revs_2();
}



/* #read_file_into */
span read_file_into(span filename, rope *r) {
    char buf[PATH_MAX] = {0};
    s_buffer(buf, PATH_MAX, filename);

    FILE *file = fopen(buf, "rb");
    if (!file) {
        prt("Failed to open file: %s\n", buf);
        flush();
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    span file_span = rope_alloc_atleast(r, file_size);
    size_t read_size = fread(file_span.buf, 1, file_size, file);
    if (read_size != file_size) {
        prt("Error reading file: %s\n", buf);
        flush();
        exit(1);
    }

    file_span.end = file_span.buf + read_size;
    fclose(file);

    return file_span;
}
/* #get_revs_2 */
void get_revs_2() {
    clear_display();
    time_t latest_rev_timestamp;
    int prev_revblocks_count;

    if (state->revs.n_revblocks > 0) {
        latest_rev_timestamp = state->revs.revblocks[0].timestamp;
        prev_revblocks_count = state->revs.n_revblocks;
    } else {
        latest_rev_timestamp = 0;
        prev_revblocks_count = 0;
    }

    if (rope_isnull(state->revs.revrope)) {
        state->revs.revrope = rope_new(32 * 1024 * 1024);
    }

    int num_projfiles = state->files.n;
    checksums* working_set = malloc(num_projfiles * sizeof(checksums));
    for (int i = 0; i < num_projfiles; ++i) {
        working_set[i] = sorted_line_checksums(state->files.a[i].contents);
    }

    for (int i = state->revs.filenames.n - 1; i >= 0; --i) {
        span bname = state->revs.filenames.a[i];
        span rev_path = prs("%.*s/revs/%.*s", len(state->cmprdir), state->cmprdir.buf, len(bname), bname.buf);
        fprintf(stderr, "\033[Hrev hashing: %d/%d", (int)(state->revs.filenames.n - i), (int)state->revs.filenames.n);
        fflush(stderr);

        span rev_contents = read_file_into(rev_path, &state->revs.revrope);
        if (empty(rev_contents)) {
            continue;
        }

        time_t rev_timestamp = parse_rev_fname(bname);
        if (rev_timestamp <= latest_rev_timestamp) {
            break;
        }

        if (!get_revs_cache_get(bname, rev_contents)) {
            get_revs_cache_put(working_set, bname, rev_contents);
        }
    }

    if (prev_revblocks_count > 0) {
        int new_revblocks_count = state->revs.n_revblocks - prev_revblocks_count;
        rev_block* new_revblocks = malloc(new_revblocks_count * sizeof(rev_block));
        memcpy(new_revblocks, state->revs.revblocks + prev_revblocks_count, new_revblocks_count * sizeof(rev_block));
        memmove(state->revs.revblocks + new_revblocks_count, state->revs.revblocks, prev_revblocks_count * sizeof(rev_block));
        memcpy(state->revs.revblocks, new_revblocks, new_revblocks_count * sizeof(rev_block));
        free(new_revblocks);
    }

    free(working_set);
}
/* #revs_cache_design */
/* #get_revs_cache_get */
int get_revs_cache_get(span bname, span rev_contents) {
    u8* cmp_end_backup = cmp.end;
    span rev_cache_path = prs("%.*s/cache/v8/revs/%.*s", len(state->cmprdir), state->cmprdir.buf, len(bname), bname.buf);
    if (!readable_file(rev_cache_path)) return 0;
    span rev_cache_contents = read_file_into_cmp(rev_cache_path);
    int result = parse_revfile_cache(bname, rev_cache_contents, rev_contents);
    cmp.end = cmp_end_backup;
    return result;
}


/* #scan_checksum */
checksum scan_checksum(span input) {
    if (len(input) < 16) {
        prt("Input too short for checksum\n");
        flush();
        exit(1);
    }

    checksum result = {0};
    for (int i = 0; i < 16; ++i) {
        char c = input.buf[i];
        if (!isxdigit(c)) {
            prt("Invalid hex digit in input: %.*s\n", len(input), input.buf);
            flush();
            exit(1);
        }
        result.__u = (result.__u << 4) | (isdigit(c) ? c - '0' : tolower(c) - 'a' + 10);
    }
    
    return result;
}



/* #scan_int */
int scan_int(span* sp) {
    span s = *sp;
    u8* start = s.buf;

    while (s.buf < s.end && *s.buf >= '0' && *s.buf <= '9') {
        s.buf++;
    }

    if (start == s.buf) {
        prt("Error: Expected digits but found none.\n");
        flush();
        exit(1);
    }

    int value = atoi((char*)start);
    sp->buf = s.buf;
    return value;
}


/* #scan_hex */
int scan_hex(span* s) {
    char* start = (char*)s->buf;
    char* endptr;
    int result = strtol(start, &endptr, 16);
    if (endptr == start) {
        prt("Invalid hex input\n");
        flush();
        exit(1);
    }
    s->buf = (u8*)endptr;
    return result;
}

/* #parse_hex */
int parse_hex(span s) {
    if (empty(s)) {
        prt("Error: Empty span provided to parse_hex\n");
        flush();
        exit(1);
    }
    char *endptr;
    int value = strtol((char*)s.buf, &endptr, 16);
    if (endptr == (char*)s.buf) {
        prt("Error: No valid hexadecimal digits found in span\n");
        flush();
        exit(1);
    }
    return value;
}


/* #parse_revfile_cache */
#define SECTION_BLOCKS 0
#define SECTION_SCS 1
#define SECTION_IDS 2

int parse_revfile_cache(span bname, span rev_cache, span rev_contents) {
  span line;
  int n_blocks = -1;
  int n_existing_revblocks = state->revs.n_revblocks;

  while (!empty(rev_cache)) {
    line = next_line(&rev_cache);

    if (empty(line)) break; // end header section

    if (starts_with(line, S("Language: "))) continue;
    else if (starts_with(line, S("Blocks: "))) {
      n_blocks = parse_int(skip_n(line, 8));
    } else {
      prt("Unknown header line: %.*s\n", len(line), line.buf);
    }
  }

  time_t timestamp = parse_rev_fname(bname);

  while (!empty(rev_cache)) {
    int section_type, block_number;
    int failure;
    parse_section_header_line(&failure, &section_type, &block_number, &rev_cache);
    if (section_type < 0) return 0;
    int rev_block_idx = n_existing_revblocks + block_number - 1;
    switch (section_type) {
      case SECTION_BLOCKS:
        parse_blocks_lines(&failure, timestamp, n_blocks, rev_contents, &rev_cache);
        break;
      case SECTION_SCS:
        parse_scs_lines(&failure, rev_block_idx, &rev_cache);
        break;
      case SECTION_IDS:
        parse_ids_lines(&failure, rev_block_idx, &rev_cache);
        break;
    }
    if (failure) return 0;
  }

  return 1;
}

/* #parse_section_header_line */
void parse_section_header_line(int *failure, int *section_type, int *block_number, span *rev_cache) {
    span line = next_line(rev_cache);
    *failure = 0;
    *section_type = -1;
    *block_number = -1;

    while(empty(line)) line = next_line(rev_cache);

    if (span_eq(line, S("blocks"))) {
        *section_type = SECTION_BLOCKS;
    } else if (starts_with(line, S("block ")) && ends_with(line, S(" scs"))) {
        *section_type = SECTION_SCS;
        *block_number = parse_int(skip_n(line, 6));
    } else if (starts_with(line, S("block ")) && ends_with(line, S(" ids"))) {
        *section_type = SECTION_IDS;
        *block_number = parse_int(skip_n(line, 6));
    } else {
        prt("failed to parse as section header line (press any key to continue): %.*s\n", len(line), line.buf);flush();getch();
        *failure = 1;
    }
}


/* #parse_blocks_lines */
void parse_blocks_lines(int *failure, time_t timestamp, int n_blocks, span rev_contents, span* rev_cache) {
    int count = 0;
    while (!empty(*rev_cache)) {
        span line = next_line(rev_cache);
        if (empty(trim(line))) break;
        int comma = find_char(line, ',');
        if (comma < 0) { *failure = 1; return; }
        span start_s = first_n(line, comma);
        span end_s = skip_n(line, comma + 1);
        int start = parse_int(trim(start_s));
        int end = parse_int(trim(end_s));
        if (start < 0 || end < start || end > len(rev_contents)) { *failure = 1; return; }
        if (state->revs.n_revblocks == state->revs.cap_revblocks) {
            size_t newcap = state->revs.cap_revblocks ? 2 * state->revs.cap_revblocks : 4;
            state->revs.revblocks = realloc(state->revs.revblocks, newcap * sizeof(rev_block));
            state->revs.cap_revblocks = newcap;
        }
        rev_block *b = &state->revs.revblocks[state->revs.n_revblocks++];
        b->contents.buf = rev_contents.buf + start;
        b->contents.end = rev_contents.buf + end;
        b->timestamp = timestamp;
        b->ids.n = -1;
        b->ids.a = NULL;
        b->ids.cap = 0;
        ++count;
    }
    if (count != n_blocks) { *failure = 1; return; }
}

/* #parse_scs_lines */
void parse_scs_lines(int *failure, int rev_block_idx, span* rev_cache) {
    while (!empty(*rev_cache)) {
        span line = next_line(rev_cache);
        if (len(line) == 0) break;
    }

    state->revs.revblocks[rev_block_idx].sorted_line_cksums.n = -1;
    state->revs.revblocks[rev_block_idx].sorted_line_cksums.a = NULL;
    state->revs.revblocks[rev_block_idx].sorted_line_cksums.cap = 0;
}

/* #parse_ids_lines */
void parse_ids_lines(int *failure, int rev_block_idx, span* rev_cache) {
    // Set sentinel to indicate IDs not loaded
    state->revs.revblocks[rev_block_idx].ids.n = -1;
    state->revs.revblocks[rev_block_idx].ids.a = NULL;
    state->revs.revblocks[rev_block_idx].ids.cap = 0;

    // Consume the lines without parsing
    while (!empty(*rev_cache)) {
        span line = next_line(rev_cache);
        if (empty(trim(line))) break;
    }
}
/* #get_revs_cache_put */
void get_revs_cache_put(checksums* working_set, span bname, span content) {
    if (empty(content))
        return;

    size_t projfile_count = state->files.n;
    int best_match_index = -1;
    int max_intersection = -1;
    
    checksums_arena_push();
    checksums rev_cksums = sorted_line_checksums(content);

    for (size_t i = 0; i < projfile_count; ++i) {
        int intersection = cksums_intersection(rev_cksums, working_set[i]);
        if (intersection > max_intersection) {
            max_intersection = intersection;
            best_match_index = i;
        }
    }
    checksums_arena_pop();

    if (best_match_index == -1)
        return;

    span language = state->files.a[best_match_index].language;
    spans blocks = find_blocks_language(content, language);

    int prev_n_revblocks = state->revs.n_revblocks;

    if (state->revs.n_revblocks + blocks.n > state->revs.cap_revblocks) {
        state->revs.cap_revblocks = 2 * (state->revs.n_revblocks + blocks.n);
        state->revs.revblocks = realloc(state->revs.revblocks, state->revs.cap_revblocks * sizeof(rev_block));
    }

    time_t timestamp = parse_rev_fname(bname);
    rev_block *revblocks = state->revs.revblocks + state->revs.n_revblocks;

    for (size_t i = 0; i < blocks.n; ++i) {
        revblocks[i].contents = blocks.a[i];
        /* Set sentinel - checksums NOT stored on revblock, loaded lazily */
        revblocks[i].sorted_line_cksums.n = -1;
        revblocks[i].sorted_line_cksums.a = NULL;
        revblocks[i].sorted_line_cksums.cap = 0;
        revblocks[i].ids = ids_for_block(blocks.a[i]);
        assert(revblocks[i].ids.n >= 0);
        revblocks[i].timestamp = timestamp;
    }

    state->revs.n_revblocks += blocks.n;

    span cmprdir = state->cmprdir;
    u8* end = out.end;
    u8* ce = cmp.end;
    pr_revinfo(language, blocks, prev_n_revblocks, content);
    span output = (span){end, out.end};
    span cache_path = prs("%.*s/cache/v8/revs/%.*s", len(cmprdir), cmprdir.buf, len(bname), bname.buf);
    write_to_file_span(output, cache_path, 1);
    out.end = end;
    cmp.end = ce;
}

/* #pr_revinfo */
void pr_checksum(checksum cksum) {
    prt("%016lX\n", cksum.__u);
}

void pr_relative_span(span container, span subsection) {
    prt("%ld,%ld\n", subsection.buf - container.buf, subsection.end - container.buf);
}

void pr_revinfo(span language, spans blocks, int prev_n_revblocks, span contents) {
    prt("Language: %.*s\nBlocks: %ld\n\n", len(language), language.buf, blocks.n);
    
    prt("blocks\n");
    for (size_t i = 0; i < blocks.n; i++) {
        span block = blocks.a[i];
        pr_relative_span(contents, block);
    }

    for (size_t i = 0; i < blocks.n; i++) {
        span block = blocks.a[i];
        
        checksums_arena_push();
        checksums scs = sorted_line_checksums(block);
        
        prt("\nblock %ld scs\n", i + 1);
        for (size_t j = 0; j < scs.n; j++) {
            pr_checksum(scs.a[j]);
        }
        checksums_arena_pop();
    }

    for (size_t i = 0; i < blocks.n; i++) {
        rev_block rb = state->revs.revblocks[prev_n_revblocks + i];
        if (rb.ids.n > 0) {
            prt("\nblock %ld ids\n", i + 1);
            for (size_t j = 0; j < rb.ids.n; j++) {
                pr_relative_span(rb.contents, rb.ids.a[j]);
            }
        }
    }
}

/* #prs_checksum */
span prs_checksum(checksum c) {
  out_sav sav = out2cmp();
  span ret = {cmp.end};
  pr_checksum(c);
  bksp();
  ret.end = cmp.end;
  out_rst(sav);
  return ret;
}

/* #SBV_design */
/* #getkey */
#define ARROW_U 256
#define ARROW_D 257
#define ARROW_R 258
#define ARROW_L 259

int getkey() {
    struct termios oldt, newt;
    int ret;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while (1) {
        char c;
        int n = read(STDIN_FILENO, &c, 1);
        if (n < 0) {
            if (errno == EAGAIN) continue;
            break;
        }

        if (c == '\033') {
            char seq[2];
            ret = '\033';
            if (read(STDIN_FILENO, &seq[0], 1) == 0) break;
            if (read(STDIN_FILENO, &seq[1], 1) == 0) break;

            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': ret = ARROW_U; goto restore;
                    case 'B': ret = ARROW_D; goto restore;
                    case 'C': ret = ARROW_R; goto restore;
                    case 'D': ret = ARROW_L; goto restore;
                }
            }
            //ret = '\033'; goto restore;
            continue;
        }

        ret = (unsigned char)c;
        break;
    }

restore:
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ret;
}


/* #refs_menu */
int refs_menu(int block_idx, spans breadcrumb) {
    span block = state->blocks.a[block_idx];
    span blockid = id_for_block(block);
    spans refs = refs_for_block(block);
    spans mentions = mentions_for_block(block);
    spans ins = referrers_to_block(block_idx);

    int num_refs = refs.n;
    int num_mentions = mentions.n;
    int num_in = ins.n;
    int num_out = num_refs + num_mentions;
    int num_total = num_out + num_in;
    if(num_total == 0) {
        prt("no references\n");
        flush();
        getkey();
        return -1;
    }
    int sel = 0;

    for(;;) {
        clear_display();
        if(breadcrumb.n > 0) {
            for(int i=0; i<breadcrumb.n; i++) {
                if(i > 0) prt(" > ");
                wrs(breadcrumb.a[i]);
            }
            terpri();
        }
        prt("References for ");
        wrs(blockid);
        terpri();
        terpri();
        // Print @refs
        for(int i=0; i<num_refs; i++) {
            if(sel == i) set_highlight();
            prt("  ");
            wrs(refs.a[i]);
            if(sel == i) reset_highlight();
            terpri();
        }
        // Print #mentions
        for(int i=0; i<num_mentions; i++) {
            if(sel == num_refs + i) set_highlight();
            prt("  ");
            wrs(mentions.a[i]);
            if(sel == num_refs + i) reset_highlight();
            terpri();
        }
        // Divider between outgoing and incoming
        if(num_out > 0 && num_in > 0) {
            prt("  ...");
            terpri();
        }
        // Print incoming refs
        for(int i=0; i<num_in; i++) {
            if(sel == num_out + i) set_highlight();
            prt("  ");
            wrs(ins.a[i]);
            if(sel == num_out + i) reset_highlight();
            terpri();
        }
        // push nav hint to bottom
        int lines_used = (breadcrumb.n > 0 ? 1 : 0) + 2 + num_out + ((num_out>0&&num_in>0)?1:0) + num_in + 1;
        int scr_rows = state->terminal_rows ? state->terminal_rows : 24;
        for(int i=lines_used; i<scr_rows-2; i++) terpri();
        prt("j/k:move  @:drill  enter:jump  q:back");
        flush();

        int k = getkey();
        if(k == 'q' || k == 27) return -1;
        if(k == '\n' || k == '\r') {
            int idx = -1;
            if(sel < num_refs) {
                // @ref
                span ref = refs.a[sel];
                span have = ref;
                advance1(&have); // skip '@'
                int col_idx = find_char(have, ':');
                span id = col_idx<0 ? have : first_n(have,col_idx);
                idx = block_by_id(id);
            } else if(sel < num_out) {
                // #mention
                span ref = mentions.a[sel - num_refs];
                span have = ref;
                if(!empty(have) && have.buf[0]=='#') advance1(&have);
                idx = block_by_id(have);
            } else {
                // incoming
                int in_idx = sel - num_out;
                if(in_idx >= 0 && in_idx < num_in) {
                    span ref = ins.a[in_idx];
                    span have = ref;
                    if(!empty(have) && have.buf[0]=='#') advance1(&have);
                    idx = block_by_id(have);
                }
            }
            if(idx != -1) return idx;
        }
        if(k == 'j' || k == ARROW_D) {
            if(sel < num_total - 1) sel++;
        }
        if(k == 'k' || k == ARROW_U) {
            if(sel > 0) sel--;
        }
        if(k == '@') {
            int sub_idx = -1;
            if(sel < num_refs) {
                span ref = refs.a[sel];
                span have = ref;
                advance1(&have);
                int col_idx = find_char(have, ':');
                span id = col_idx<0 ? have : first_n(have,col_idx);
                sub_idx = block_by_id(id);
            } else if(sel < num_out) {
                span ref = mentions.a[sel - num_refs];
                span have = ref;
                if(!empty(have) && have.buf[0]=='#') advance1(&have);
                sub_idx = block_by_id(have);
            } else {
                int in_idx = sel - num_out;
                if(in_idx >= 0 && in_idx < num_in) {
                    span ref = ins.a[in_idx];
                    if(!empty(ref) && ref.buf[0]=='#') advance1(&ref);
                    sub_idx = block_by_id(ref);
                }
            }
            if(sub_idx != -1) {
                spans new_breadcrumb = spans_alloc(breadcrumb.n + 1);
                for(int i=0; i<breadcrumb.n; i++) spans_push(&new_breadcrumb, breadcrumb.a[i]);
                spans_push(&new_breadcrumb, blockid);
                int ret = refs_menu(sub_idx, new_breadcrumb);
                if(ret != -1) return ret;
            }
        }
    }
}

/* #sbv_display */
void sbv_display(sbv_state* sbvs) {
    char offset[32];
    char timestamp[32];
    rev_block* rb = &state->revs.revblocks[sbvs->revblock_indices[sbvs->current_index]];
    
    clear_display();

    if (sbvs->current_index == 0) {
        snprintf(offset, sizeof(offset), "curr");
    } else {
        snprintf(offset, sizeof(offset), "-%d", sbvs->current_index);
    }

    struct tm *tm_info = localtime(&rb->timestamp);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    prt("Block %d, ver: %s, %s, j/k, Enter, q\n", state->curr_block_idx + 1, offset, timestamp);
    wrs(rb->contents);
    prt("Block %d, ver: %s, %s, j/k, Enter, q", state->curr_block_idx + 1, offset, timestamp);
    flush();
}



/* #load_revblock_checksums */
checksums load_revblock_checksums(int revblock_idx) {
    rev_block *rb = &state->revs.revblocks[revblock_idx];

    char timestamp_str[17];
    time_t t = rb->timestamp;
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(timestamp_str, sizeof(timestamp_str), "%Y%m%d-%H%M%S", &tm);

    span cmprdir = state->cmprdir;
    span prefix = concat(cmprdir, S("cache/v8/revs/"));
    span ts_span = S(timestamp_str); // on stack, so copy to cmp for pointers OK
    span cache_path = concat(prefix, ts_span);

    if (!readable_file(cache_path)) {
        checksums empty = { .n = 0, .a = NULL, .cap = 0 };
        return empty;
    }

    u8 *old_end = cmp.end;
    span cache_contents = read_file_into_cmp(cache_path);

    int n_block = 0;
    for (int i = 0; i <= revblock_idx; ++i) {
        if (state->revs.revblocks[i].timestamp == rb->timestamp)
            ++n_block;
    }

    char section_buf[64];
    int section_len = snprintf(section_buf, sizeof(section_buf), "block %d scs", n_block);
    span want_section = S(section_buf);
    want_section.end = want_section.buf + section_len;

    span cur = cache_contents;
    span line;
    for (;;) {
        if (empty(cur)) break;
        line = next_line(&cur);
        if (empty(trim(line))) break; // first blank line = end of header
    }

    int in_section = 0;
    int num_lines = 0;
    span tmpcur = cur;
    while (!empty(tmpcur)) {
        span l = next_line(&tmpcur);
        if (empty(trim(l))) {
            in_section = 0;
            continue;
        }
        if (!in_section) {
            if (span_eq(l, want_section)) {
                in_section = 1;
            }
            continue;
        }
        ++num_lines;
    }

    checksums cksums = checksums_alloc(num_lines);

    tmpcur = cur;
    in_section = 0;
    int count = 0;
    while (!empty(tmpcur)) {
        span l = next_line(&tmpcur);
        if (empty(trim(l))) {
            in_section = 0;
            continue;
        }
        if (!in_section) {
            if (span_eq(l, want_section)) {
                in_section = 1;
            }
            continue;
        }
        if (count < num_lines) {
            checksum c = scan_checksum(l);
            checksums_push(&cksums, c);
            ++count;
        }
    }

    cmp.end = old_end;
    return cksums;
}


/* #load_revblock_ids */
spans load_revblock_ids(int revblock_idx) {
    rev_block* rb = &state->revs.revblocks[revblock_idx];
    time_t ts = rb->timestamp;
    int found = 0;
    span fname = nullspan();
    for (int i = 0; i < state->revs.filenames.n; ++i) {
        if (parse_rev_fname(state->revs.filenames.a[i]) == ts) {
            fname = state->revs.filenames.a[i];
            found = 1;
            break;
        }
    }
    if (!found) {
        spans empty = {NULL, 0, 0};
        return empty;
    }
    span cache_path = prs("%.*s/cache/v8/revs/%.*s", len(state->cmprdir), state->cmprdir.buf, len(fname), fname.buf);
    if (!readable_file(cache_path)) {
        spans empty = {NULL, 0, 0};
        return empty;
    }
    u8* cmp_save = cmp.end;
    span cache_contents = read_file_into_cmp(cache_path);

    int n_block = 0;
    for (int i = 0; i <= revblock_idx; ++i) {
        if (state->revs.revblocks[i].timestamp == ts)
            ++n_block;
    }
    span want_section = prs("block %d ids", n_block);

    span rem = cache_contents;
    span line;
    int num_lines = 0, found_section = 0;
    // Skip header: move until blank line
    while (!empty(rem)) {
        line = next_line(&rem);
        if (empty(trim(line))) break;
    }
    // Find section
    while (!empty(rem)) {
        line = next_line(&rem);
        if (empty(trim(line))) continue;
        if (span_eq(trim(line), want_section)) { found_section = 1; break; }
    }
    if (!found_section) {
        cmp.end = cmp_save;
        spans empty = {NULL, 0, 0};
        return empty;
    }
    // Count num_lines in section (lines up to blank line or EOF)
    u8* sec_save = rem.buf;
    while (!empty(rem)) {
        line = next_line(&rem);
        if (empty(trim(line))) break;
        ++num_lines;
    }
    // Allocate
    spans ids = spans_alloc(num_lines);
    // Re-scan the section for actual lines/ids
    rem.buf = sec_save;
    for (int i = 0; i < num_lines; ++i) {
        line = next_line(&rem);
        int comma = find_char(line, ',');
        if (comma < 0) continue;
        span s_start = first_n(line, comma);
        span s_end = skip_n(line, comma + 1);
        int start = parse_int(s_start);
        int endval = parse_int(s_end);
        if (start > endval || start < 0 || endval > len(rb->contents)) continue;
        span idspan = (span){rb->contents.buf + start, rb->contents.buf + endval};
        spans_push(&ids, idspan);
    }
    cmp.end = cmp_save;
    return ids;
}


/* #sbv_populate */
void sbv_populate(sbv_state* sbvs) {
    int i, max_idx = sbvs->max_index;
    if (sbvs->current_index > max_idx) {
        if (max_idx == -1 && sbvs->current_index == 0) {
            for (i = 0; i < state->revs.n_revblocks; i++) {
                if (span_eq(state->blocks.a[state->curr_block_idx], state->revs.revblocks[i].contents)) {
                    sbvs->revblock_indices[0] = i;
                    sbvs->max_index = 0;
                    return;
                }
            }
        } else {
            for (i = sbvs->revblock_indices[max_idx] + 1; i < state->revs.n_revblocks; i++) {
                if (rev_block_match(sbvs, i)) {
                    sbvs->revblock_indices[++sbvs->max_index] = i;
                    return;
                }
            }
        }
        sbvs->current_index--;
    }
}

int block_id_match(spans curr_ids, spans rev_ids) {
    for (int i = 0; i < curr_ids.n; i++) {
        for (int j = 0; j < rev_ids.n; j++) {
            if (span_eq(curr_ids.a[i], rev_ids.a[j])) {
                return 1;
            }
        }
    }
    return 0;
}

int rev_block_match(sbv_state* sbvs, int revblock_idx) {
    rev_block* current_revblock = &state->revs.revblocks[revblock_idx];
    
    for (int i = 0; i <= sbvs->max_index; i++) {
        if (span_eq(current_revblock->contents, state->revs.revblocks[sbvs->revblock_indices[i]].contents)) {
            return 0;
        }
    }
    if (block_id_match(sbvs->curr_block_ids, current_revblock->ids)) {
        return 1;
    }
    
    int curr_uniq = sbvs->sorted_line_cksums.n;
    int rev_uniq;
    int intersection;
    
    if (current_revblock->sorted_line_cksums.n == -1) {
        checksums_arena_push();
        checksums rev_cksums = load_revblock_checksums(revblock_idx);
        rev_uniq = rev_cksums.n;
        intersection = cksums_intersection(sbvs->sorted_line_cksums, rev_cksums);
        checksums_arena_pop();
    } else {
        rev_uniq = current_revblock->sorted_line_cksums.n;
        intersection = cksums_intersection(sbvs->sorted_line_cksums, current_revblock->sorted_line_cksums);
    }
    
    return (curr_uniq > 8 && rev_uniq > 8 && intersection > 8);
}

/* #select_block_version */
void select_block_version() {
    if (state->curr_block_idx == -1) return;
    get_revs();
    sbv_state sbvs;
    sbvs.max_index = -1;
    sbvs.current_index = 0;
    sbvs.revblock_indices = (int *)malloc(state->revs.n_revblocks * sizeof(int));
    sbvs.curr_block_ids = ids_for_block(state->blocks.a[state->curr_block_idx]);
    sbvs.sorted_line_cksums = sorted_line_checksums(state->blocks.a[state->curr_block_idx]);

    while (1) {
        sbv_populate(&sbvs);
        sbv_display(&sbvs);
        int key = getkey();

        if (key == 'q' || key == 27) {
            break;
        } else if (key == 'k' || key == ARROW_U) {
            sbvs.current_index++;
        } else if (key == 'j' || key == ARROW_D) {
            if (sbvs.current_index > 0) sbvs.current_index--;
        } else if (key == '\n') {
            replace_block(state->revs.revblocks[sbvs.revblock_indices[sbvs.current_index]].contents);
            break;
        }
    }

    free(sbvs.revblock_indices);
}


/* #sorted_line_checksums */
// fix for clang
int checksum_cmp(const void* a, const void* b) {
    const checksum* cksum1 = (const checksum*)a;
    const checksum* cksum2 = (const checksum*)b;
    return (cksum1->__u > cksum2->__u) - (cksum1->__u < cksum2->__u);
}

checksums sorted_line_checksums(span input) {
    int line_count = 0;
    span temp = input;
    while (!empty(temp)) {
        next_line(&temp);
        line_count++;
    }

    checksums cksums = checksums_alloc(line_count);
    temp = input;
    while (!empty(temp)) {
        span line = next_line(&temp);
        checksum cksum = selected_checksum(line);
        checksums_push(&cksums, cksum);
    }

    qsort(cksums.a, cksums.n, sizeof(checksum), checksum_cmp);

    int offset = 0;
    for (int i = 1; i < cksums.n; i++) {
        if (cksums.a[i].__u != cksums.a[offset].__u) {
            offset++;
            cksums.a[offset] = cksums.a[i];
        }
    }
    cksums.n = offset + 1;

    return cksums;
}


/* #cksums_intersection */
int cksums_intersection(checksums a, checksums b) {
    int i = 0, j = 0, intersection_count = 0;

    while (i < a.n && j < b.n) {
        if (a.a[i].__u < b.a[j].__u) {
            i++;
        } else if (a.a[i].__u > b.a[j].__u) {
            j++;
        } else {
            intersection_count++;
            i++;
            j++;
        }
    }

    return intersection_count;
}


/* #find_blocks_language_c */
spans find_blocks_language_c(span file) {
    if (empty(file)) {
        // Handle special case for empty file
        spans single_empty_block = spans_alloc(1);
        single_empty_block.a[0].buf = file.buf;
        single_empty_block.a[0].end = file.end;
        single_empty_block.n = 1;
        return single_empty_block;
    }

    int block_count = 0;
    span copy = file;
    int is_first_line = 1;

    // First loop: count blocks
    while (!empty(copy)) {
        span line = next_line(&copy);
        if (is_first_line || starts_with(line, S("/*"))) {
            block_count++;
            is_first_line = 0;
        }
    }

    spans blocks = spans_alloc(block_count);
    copy = file; // Reset copy for second loop
    span* previous_block = NULL;
    int index = 0;
    is_first_line = 1;

    // Second loop: assign spans
    while (!empty(copy)) {
        span line = next_line(&copy);
        if (is_first_line || starts_with(line, S("/*"))) {
            if (previous_block != NULL) {
                previous_block->end = line.buf;
            }
            blocks.a[index].buf = line.buf;
            previous_block = &blocks.a[index++];
            is_first_line = 0;
        }
    }
    if (previous_block != NULL) {
        previous_block->end = file.end;
    }
    blocks.n = index;

    return blocks;
}


/* #find_blocks_language_markdown */
spans find_blocks_language_markdown(span file) {
    if (empty(file)) {
        spans result = spans_alloc(1);
        result.a[0] = file;
        result.n = 1;
        return result;
    }
    
    span copy = file;
    int block_count = 0;
    
    while (!empty(copy)) {
        span line = next_line(&copy);
        if (line.buf == file.buf || *line.buf == '#') {
            block_count++;
        }
    }
    
    spans blocks = spans_alloc(block_count);
    copy = file;
    int index = 0;
    span* prev_block = NULL;
    
    while (!empty(copy)) {
        span line = next_line(&copy);
        if (line.buf == file.buf || *line.buf == '#') {
            if (prev_block != NULL) {
                prev_block->end = line.buf;
            }
            blocks.a[index].buf = line.buf;
            prev_block = &blocks.a[index];
            index++;
        }
    }
    
    if (prev_block != NULL) {
        prev_block->end = file.end;
    }
    
    blocks.n = index;
    return blocks;
}


/* #parse_rev_fname */
time_t parse_rev_fname(span basename) {
    struct tm tm_info = {0};
    char buf[16] = {0};
    
    s_buffer(buf, 9, first_n(basename, 8));
    strptime(buf, "%Y%m%d", &tm_info);

    advance(&basename, 9);
    memset(buf, 0, sizeof(buf));

    s_buffer(buf, 7, first_n(basename, 6));
    strptime(buf, "%H%M%S", &tm_info);
    
    return mktime(&tm_info);
}


/* #find_blocks_language_python */
spans find_blocks_language_python(span file) {
    int block_count = 0;
    span copy = file;
    span line;
    int quote_count = 0;

    // First loop: count blocks
    while (!empty(copy)) {
        line = next_line(&copy);
        if (starts_with(line, S("\"\"\"")) || copy.buf == file.buf) {
            quote_count++;
            // Skip the ending quote of a block
            if (quote_count % 2 == 0) continue;
            block_count++;
        }
    }

    spans blocks = spans_alloc(block_count);
    copy = file; // Reset copy for second loop
    span* previous_block = NULL;
    int index = 0;
    quote_count = 0;

    // Second loop: assign spans
    while (!empty(copy)) {
        line = next_line(&copy);
        if (starts_with(line, S("\"\"\"")) || copy.buf == file.buf) {
            quote_count++;
            if (quote_count % 2 == 0) continue;
            if (previous_block != NULL) {
                previous_block->end = line.buf;
            }
            blocks.a[index].buf = line.buf;
            previous_block = &blocks.a[index++];
        }
    }
    if (previous_block != NULL) {
        previous_block->end = file.end;
    }
    blocks.n = index;

    // Sanity check
    for (int i = 0; i < blocks.n; ++i) {
        if (i == 0 && blocks.a[i].buf != file.buf) {
            prt("Error: First block does not start where input begins.\n");
            flush();
            exit(EXIT_FAILURE);
        }
        if (i == blocks.n - 1 && blocks.a[i].end != file.end) {
            prt("Error: Last block does not end where input ends.\n");
            flush();
            exit(EXIT_FAILURE);
        }
        if (i > 0 && blocks.a[i].buf != blocks.a[i - 1].end) {
            prt("Error: Block start does not match previous block end.\n");
            flush();
            exit(EXIT_FAILURE);
        }
    }

    return blocks;
}



/* #find_blocks_language_none */
spans find_blocks_language_none(span file) {
    spans blocks = spans_alloc(1);
    spans_push(&blocks, file);
    return blocks;
}


/* #find_blocks_language */
spans find_blocks_language(span file_contents, span language) {
    if (span_eq(language, S("C"))) {
        return find_blocks_language_c(file_contents);
    } else if (span_eq(language, S("Python"))) {
        return find_blocks_language_python(file_contents);
    } else if (span_eq(language, S("JavaScript"))) {
        return find_blocks_language_c(file_contents);  // Note: JavaScript uses C rules.
    } else if (span_eq(language, S("Markdown"))) {
        return find_blocks_language_markdown(file_contents);
    } else if (span_eq(language, S("none"))) {
        return find_blocks_language_none(file_contents);
    } else {
        prt("Error: Unsupported language.");
        flush();
        exit(1);
    }
}



/* #getch */
char getch(void) {
  char buf = 0;
  struct termios old = {0}, new = {0};
  if (tcgetattr(0, &old) < 0) perror("tcgetattr()");
  new = old;
  new.c_lflag &= ~(ICANON | ECHO);
  new.c_cc[VMIN] = 1;  // Set to block until at least one character is read
  new.c_cc[VTIME] = 0; // Disable the timeout

  if (tcsetattr(0, TCSANOW, &new) < 0) perror("tcsetattr ICANON");
  if (read(0, &buf, 1) < 0) perror("read()");
  if (tcsetattr(0, TCSADRAIN, &old) < 0) perror("tcsetattr ~ICANON");

  return buf;
}


/* #main_loop */
void main_loop() {
    state->marked_index = -1;
    state->count_prefix = 0;

    while (1) {
        check_conf_vars();
        clear_display();
        print_current_blocks();
        flush();

        char ch = getch();
        if (ch == 0) break; // EOF - exit gracefully
        clock_gettime(CLOCK_REALTIME, &state->now);

        if ((ch >= '1' && ch <= '9') || (ch == '0' && state->count_prefix > 0)) {
            state->count_prefix = state->count_prefix * 10 + (ch - '0');
            continue;
        }

        handle_keystroke(ch);
        state->count_prefix = 0;
    }
}

/* #count_physical_lines */
span count_physical_lines(span input, int *max_physical_lines) {
    span result = input;
    int line_count = 0;
    int chars_in_line = 0;

    while (!empty(input) && line_count < *max_physical_lines) {
        if (*input.buf == '\n' || chars_in_line == state->terminal_cols) {
            line_count++;
            if (*input.buf == '\n') input.buf++;
            chars_in_line = 0;
        } else {
            input.buf++;
            chars_in_line++;
        }
    }

    *max_physical_lines -= line_count;
    result.end = input.buf;
    return result;
}
 /* page_down() and page_up()

Here we implement dual functions that handle pagination within the current block.

Both of these require there to be a current block, so if curr_block_idx == -1 they simply return.

We define content_rows as the number of terminal_rows minus two, since we always have a header line and a ruler line reserved at the top and bottom of the screen resp.

We simply increment or decrement state->scrolled_lines by content_rows, except that we always want to fill the screen.
For example, if a block has 23 physical lines and the terminal has 24 rows, then our content area is 22 rows, and when we paginate downwards we will show the last 22 lines of content (skipping only the first line).
Redrawing is handled in the main loop, so all we do here is update scrolled_lines as needed, returning void.

If we are already scrolled to the bottom, scrolling down will have no effect (similarly if scrolled_lines = 0 for scrolling up).

We have a helper function (count_physical_lines) that counts physical lines up to a maximum.
It updates the int passed to it by reference to indicate the remaining number of lines (<= the maximum before the call) that have not been printed (will only be non-zero if the block was short of content).
In page_down, we first call this with scrolled_lines and get a span back which is the part that is already "scrolled off" the top of the screen as the return value.

We make a copy of the block (blocks indexed by current_block, both on state).
We update .buf of this copy to the .end of the scrolled-off part, thus getting the part of the block currently visible on the screen as well as anything "below" the screen.

We then call the helper function again on this remainder content with terminal_rows as the number, to get the number of lines occupied by the currently displayed content, up to a full screen's worth.
If there is less than one full screen's worth currently displayed, then we reduce scrolled_lines by the remaining number, so that the screen becomes full.

(Note that as count_physical_lines decrements the remaining physical lines to print while it is counting off lines, we need to subtract to get the actual number of physical lines of content that would be printed.)

Otherwise, we increase scrolled_lines by a full screenfull, and then we call the helper function a third time.
Now, again, we can check if it will print a full screenfull, and if not, we can again reduce scrolled_lines such that the result will be a full screen of content ending with the last physical line of the block.

The page_up function is a bit simpler, as we can always unconditionally scroll up by a full page of lines, so we simply decrease scrolled_lines by a screenful (with a minimum of zero, obviously).
*/

void page_down() {
    if (state->curr_block_idx == -1) return;
    int lines_to_skip = state->scrolled_lines;
    int content_rows = state->terminal_rows - 2;
    span block_copy = state->blocks.a[state->curr_block_idx];
    span scrolled_off = count_physical_lines(block_copy, &lines_to_skip);

    block_copy.buf = scrolled_off.end;
    int lines_for_screen = content_rows;
    count_physical_lines(block_copy, &lines_for_screen);

    if (lines_for_screen > 0) {
        state->scrolled_lines -= lines_for_screen;
    } else {
        state->scrolled_lines += content_rows;
        lines_to_skip = state->scrolled_lines;
        block_copy = state->blocks.a[state->curr_block_idx];
        scrolled_off = count_physical_lines(block_copy, &lines_to_skip);

        /* *** manual fixup *** */
        block_copy.buf = scrolled_off.end;
        lines_for_screen = content_rows;
        count_physical_lines(block_copy, &lines_for_screen);

        if (lines_for_screen > 0) {
            state->scrolled_lines -= lines_for_screen;
        }
    }
}

void page_up() {
    state->scrolled_lines -= (state->terminal_rows - 2);
    if (state->scrolled_lines < 0) {
        state->scrolled_lines = 0;
    }
}
 /*
In toggle_visual, we test if we are in visual selection mode.
If the marked index is not -1 then we are in visual mode, and we leave the mode (by setting it to -1).
Otherwise we enter it by setting marked index to be the current index.
In either case we then reflect the new state in the display by calling print_current_blocks()
*/

void toggle_visual() {
    if (state->marked_index != -1) {
        // Leave visual mode
        state->marked_index = -1;
    } else {
        // Enter visual mode
        state->marked_index = state->curr_block_idx;
    }
    // Reflect the new state in the display
    print_current_blocks();
}

 /*
In print_current_blocks, we print either the current block, if we are in normal mode, or the set of selected blocks if we are in visual mode.

Before this, we write a helper function that gets the screen dimensions (rows and cols) from the terminal.
This function will update the state directly.

If marked_index != -1 and != curr_block_idx, then we have a "visual" selected range of more than 1 block.

First we determine which of marked_index and curr_block_idx is lower and make that our block range start, and then one past the other is our block range end (considered as an exclusive endpoint).

The difference between the two is then the number of selected blocks.
At this point we know how many blocks we are displaying.
Finally we pass state, inclusive start, and exclusive end of range to another function that handles rendering.

Helper functions:

- render_block_range(int,int) -- also supports rendering a single block (if range includes only one block).
*/

void get_screen_dimensions() {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  state->terminal_rows = w.ws_row;
  state->terminal_cols = w.ws_col;
}

void print_current_blocks() {
  get_screen_dimensions();

  // manual
  if (state->curr_file_idx == -1) {
    render_empty_project_state();
    return;
  }
  if (state->curr_block_idx == -1) {
    render_empty_file_state();
    return;
  }

  if (state->marked_index != -1 && state->marked_index != state->curr_block_idx) {
    int start = state->curr_block_idx < state->marked_index ? state->curr_block_idx : state->marked_index;
    int end = state->curr_block_idx > state->marked_index ? state->curr_block_idx + 1 : state->marked_index + 1;

    render_block_range(start, end);
  } else {
    // Normal mode or visual mode with only one block selected.
    render_block_range(state->curr_block_idx, state->curr_block_idx + 1);
  }
}


/* #render_empty_project_state */
void render_empty_project_state() {
    int remaining_rows = state->terminal_rows;
    prt("Block -");
    remaining_rows--;
    terpri();
    prt("The project is empty, use :allfiles to add all files in the project directory to the project, or edit .cmpr/conf to add files manually.");
    remaining_rows--;
    terpri();
    while (remaining_rows > 1) {
        remaining_rows--;
        terpri();
    }
    print_ruler();
}

void render_empty_file_state() {
    int remaining_rows = state->terminal_rows;
    prt("Block -");
    remaining_rows--;
    terpri();
    prt("The file %.*s is empty, hit 'e' to edit it.", len(state->files.a[state->curr_file_idx].path), state->files.a[state->curr_file_idx].path.buf);
    remaining_rows--;
    terpri();
    while (remaining_rows > 1) {
        remaining_rows--;
        terpri();
    }
    print_ruler();
}

 /*
In render_block_range, we get the state and a range of blocks (first endpoint inclusive, second exclusive).

First we calculate the length in lines of each of the blocks that we have.

Then we decide, based on the number of blocks, the layout information, and the terminal dimensions, how many lines from each we can fit on the screen.
*/

//* *** manually stubbed, needs thought ***/

void render_block_range(int start, int end) {

    if (end - start == 1) {
      print_single_block_with_skipping(start, state->scrolled_lines);
    } else {
      print_multiple_partial_blocks(start, end);
    }
}

 /*
In print_multiple_partial_blocks, we get a state and we should print as much as we can of the blocks.
Currently, we just print the number of blocks that there are.
*/

// Placeholder for print_multiple_partial_blocks, assuming it's defined elsewhere
void print_multiple_partial_blocks(int start_block, int end_block) {
  prt("%d blocks (printing multiple blocks coming soon!)\n", end_block - start_block);
}


/* #keybinds */
/* #handle_keystroke */
void handle_keystroke(char input) {
    terpri();
    switch (input) {
        case 'j':
            handle_j();
            break;
        case 'k':
            handle_k();
            break;
        case 'g':
            handle_g();
            break;
        case 'G':
            handle_G();
            break;
        case 'e':
            edit_current_block();
            break;
        case 'o':
            insert_block_after();
            break;
        case 'O':
            insert_block_before();
            break;
        // case ''':
            // prompt_palette();
            break;
        case 'r':
            nl2pl_rewrite();
            break;
        // case 'R':
            // replace_code_clipboard();
            break;
        case 'u':
            //rev_decr();
            break;
        case 'U':
            select_block_version();
            break;
        case ' ':
            page_down();
            break;
        case 'b':
            page_up();
            break;
        case 'B':
            compile();
            break;
        case 'v':
            //toggle_visual();
            break;
        case '/':
            start_search();
            break;
        case ':':
            start_ex();
            break;
        case 'n':
            search_forward();
            // TODO: move this into n/N implementation
            state->scrolled_lines = 0;
            break;
        case 'N':
            search_backward();
            state->scrolled_lines = 0;
            break;
        case '#':
            block_id_jump();
            break;
        case '@':
            block_refs_jump();
            break;
        case 'd':
            delete_block();
            break;
        case 'p':
            paste_after();
            break;
        case 'P':
            paste_before();
            break;
        case '?':
            keyboard_help();
            break;
        case 'q':
            prt("goodbye\n");
            flush();
            exit(0);
            break;
        default:
            break;
    }
}
/* #delete_block */
void delete_block() {
    if (state->curr_block_idx == -1) {
        prt("No block to delete\n");
        return;
    }

    int count = state->count_prefix > 0 ? state->count_prefix : 1;
    int deleted = 0;

    for (int i = 0; i < count && state->curr_block_idx != -1; i++) {
        span block = state->blocks.a[state->curr_block_idx];
        int file_idx = file_for_block(block);

        // Find block ID if any (first #token on first line)
        span block_copy = block;
        span line = next_line(&block_copy);
        span block_id = nullspan();
        if (len(line) > 0 && line.buf[0] != '#') {  // not markdown
            spans tokens = split_whitespace(line);
            for (int t = 0; t < tokens.n; t++) {
                if (tokens.a[t].buf[0] == '#') {
                    block_id = tokens.a[t];
                    break;
                }
            }
        }

        projfile *pf = &state->files.a[file_idx];

        size_t block_len = len(block);
        u8 *block_start = block.buf;
        u8 *block_end = block.end;
        size_t tail_len = inp.end - block_end;

        memmove(block_start, block_end, tail_len);
        inp.end -= block_len;
        pf->contents.end -= block_len;

        for (int j = file_idx + 1; j < state->files.n; j++) {
            state->files.a[j].contents.buf -= block_len;
            state->files.a[j].contents.end -= block_len;
        }

        new_rev(S(""), file_idx);
        ingest();

        deleted++;

        if (len(block_id) > 0) {
            prt("Deleted %.*s\n", len(block_id), block_id.buf);
        } else {
            prt("Deleted block %d\n", state->curr_block_idx + 1);
        }

        // Adjust cursor: stay at same index if there's a next block, else go to previous
        if (state->curr_block_idx >= state->blocks.n) {
            state->curr_block_idx = state->blocks.n - 1;
        }
        // Update file index for new current block
        if (state->curr_block_idx >= 0) {
            state->curr_file_idx = file_for_block(state->blocks.a[state->curr_block_idx]);
        }
    }

    if (deleted > 1) {
        prt("Deleted %d blocks total\n", deleted);
    }

    state->scrolled_lines = 0;
}
/* #find_last_deleted_block */
typedef struct {
    span id;
    span contents;
    time_t last_seen;
    time_t deleted_at;
} deleted_block_info;

// Helper: Check if block ID exists in current codebase
static int block_id_exists_in_current(span id) {
    return index_of(id, state->block_idx) != -1;
}

// Helper: Load cache for timestamp, returns cached_cache span
static span load_cache_for_timestamp(time_t ts, u8 **cmp_save_ptr) {
    for (int fi = 0; fi < state->revs.filenames.n; fi++) {
        if (parse_rev_fname(state->revs.filenames.a[fi]) == ts) {
            span cache_path = prs("%.*s/cache/v8/revs/%.*s",
                len(state->cmprdir), state->cmprdir.buf,
                len(state->revs.filenames.a[fi]), state->revs.filenames.a[fi].buf);
            if (readable_file(cache_path))
                return read_file_into_cmp(cache_path);
            break;
        }
    }
    return nullspan();
}

// Helper: Parse first block ID from cache section
// Returns the first ID span if block is deleted (ID not in current), nullspan() otherwise
// Also sets *is_deleted to 1 if block has IDs but none exist in current
static span parse_first_deleted_id_from_cache(span cache, int block_num, rev_block *rb, int *is_deleted) {
    *is_deleted = 0;
    if (empty(cache)) return nullspan();

    // Find "block N ids" section
    u8 *p = cache.buf, *e = cache.end;
    // Skip header lines until blank
    while (p < e) { u8 *l = p; while (p < e && *p != '\n') p++; if (p == l) { p++; break; } p++; }

    char ids_hdr[64];
    snprintf(ids_hdr, sizeof(ids_hdr), "block %d ids", block_num);
    int ids_hdr_len = strlen(ids_hdr);

    u8 *idssec = NULL;
    while (p < e) {
        u8 *ls = p;
        while (p < e && *p != '\n') p++;
        int linelen = p - ls;
        p++;
        if (linelen >= ids_hdr_len && memcmp(ls, ids_hdr, ids_hdr_len) == 0) { idssec = p; break; }
    }

    if (!idssec) return nullspan();

    // Parse IDs and check against current block_idx
    int found_in_current = 0, has_id = 0;
    span first_id = nullspan();
    p = idssec;
    while (p < e) {
        u8 *il = p;
        while (p < e && *p != '\n') p++;
        if (p == il) break;
        span line = (span){il, p};
        p++;
        int comma = find_char(line, ',');
        if (comma < 0) continue;
        int start = parse_int(first_n(line, comma));
        int endval = parse_int(skip_n(line, comma + 1));
        if (start < 0 || endval <= start || endval > len(rb->contents)) continue;
        span id = (span){rb->contents.buf + start, rb->contents.buf + endval};
        if (!has_id) {
            first_id = id;
            has_id = 1;
        }
        if (block_id_exists_in_current(id)) found_in_current = 1;
    }

    if (has_id && !found_in_current) {
        *is_deleted = 1;
        return first_id;
    }
    return nullspan();
}

span find_last_deleted_block() {
    u8 *cmp_save = cmp.end;
    time_t cached_ts = 0;
    span cached_cache = nullspan();
    int block_num = 0;
    int total = state->revs.n_revblocks;

    for (int i = total - 1; i >= 0; i--) {
        if ((total - 1 - i) % 10000 == 0) {
            fprintf(stderr, "\033[Hfind deleted: %d/%d", total - 1 - i, total);
            fflush(stderr);
        }
        rev_block *rb = &state->revs.revblocks[i];

        // When we see a new timestamp, count total blocks with that timestamp
        if (i == total - 1 || rb->timestamp != state->revs.revblocks[i+1].timestamp) {
            block_num = 0;
            for (int j = i; j >= 0 && state->revs.revblocks[j].timestamp == rb->timestamp; j--)
                block_num++;
        }

        if (rb->ids.n == 0) { block_num--; continue; }

        // IDs already loaded (not sentinel)
        if (rb->ids.n != (size_t)-1) {
            int found = 0;
            for (size_t j = 0; j < rb->ids.n && !found; j++)
                if (block_id_exists_in_current(rb->ids.a[j])) found = 1;
            if (!found) {
                fprintf(stderr, "\033[H\033[K");
                fflush(stderr);
                cmp.end = cmp_save;
                return rb->contents;
            }
            block_num--;
            continue;
        }

        // Load cache if new timestamp
        if (rb->timestamp != cached_ts) {
            cmp.end = cmp_save;
            cached_cache = nullspan();
            cached_ts = rb->timestamp;
            cached_cache = load_cache_for_timestamp(cached_ts, &cmp_save);
        }

        if (empty(cached_cache)) { block_num--; continue; }

        int is_deleted = 0;
        span first_id = parse_first_deleted_id_from_cache(cached_cache, block_num, rb, &is_deleted);

        if (is_deleted && !empty(first_id)) {
            fprintf(stderr, "\033[H\033[K");
            fflush(stderr);
            cmp.end = cmp_save;
            return rb->contents;
        }
        block_num--;
    }

    fprintf(stderr, "\033[H\033[K");
    fflush(stderr);
    cmp.end = cmp_save;
    return nullspan();
}

void find_all_deleted_blocks() {
    u8 *cmp_save = cmp.end;
    time_t cached_ts = 0;
    span cached_cache = nullspan();
    int block_num = 0;
    int total = state->revs.n_revblocks;

    // Limit search to avoid processing entire history (which can have 500k+ blocks)
    // Process at most 50000 revblocks (roughly 100-200 recent revisions)
    int max_revblocks = 50000;
    int start_idx = total - 1;
    int end_idx = (total > max_revblocks) ? total - max_revblocks : 0;

    // Collect deleted blocks - use malloc, not spans arena
    deleted_block_info *deleted = NULL;
    int n_deleted = 0;
    int cap_deleted = 0;

    for (int i = start_idx; i >= end_idx; i--) {
        if ((start_idx - i) % 5000 == 0) {
            fprintf(stderr, "\033[Hfind deleted: %d/%d", start_idx - i, start_idx - end_idx);
            fflush(stderr);
        }
        rev_block *rb = &state->revs.revblocks[i];

        // When we see a new timestamp, count total blocks with that timestamp
        if (i == total - 1 || rb->timestamp != state->revs.revblocks[i+1].timestamp) {
            block_num = 0;
            for (int j = i; j >= 0 && state->revs.revblocks[j].timestamp == rb->timestamp; j--)
                block_num++;
        }

        if (rb->ids.n == 0) { block_num--; continue; }

        span primary_id = nullspan();
        int is_deleted = 0;

        // IDs already loaded (not sentinel)
        if (rb->ids.n != (size_t)-1) {
            int found_in_current = 0;
            for (size_t j = 0; j < rb->ids.n && !found_in_current; j++)
                if (block_id_exists_in_current(rb->ids.a[j])) found_in_current = 1;
            if (!found_in_current && rb->ids.n > 0) {
                is_deleted = 1;
                primary_id = rb->ids.a[0];
            }
        } else {
            // Load cache if new timestamp
            if (rb->timestamp != cached_ts) {
                cmp.end = cmp_save;
                cached_cache = nullspan();
                cached_ts = rb->timestamp;
                cached_cache = load_cache_for_timestamp(cached_ts, &cmp_save);
            }

            primary_id = parse_first_deleted_id_from_cache(cached_cache, block_num, rb, &is_deleted);
        }

        // If deleted and we haven't seen this ID yet, record it
        if (is_deleted && !empty(primary_id)) {
            // Check if already seen (search in deleted array)
            int already_seen = 0;
            for (int di = 0; di < n_deleted && !already_seen; di++) {
                if (span_eq(deleted[di].id, primary_id)) already_seen = 1;
            }
            if (!already_seen) {
                // Grow array if needed
                if (n_deleted >= cap_deleted) {
                    cap_deleted = cap_deleted ? cap_deleted * 2 : 64;
                    deleted = realloc(deleted, cap_deleted * sizeof(deleted_block_info));
                }
                deleted[n_deleted].id = primary_id;
                deleted[n_deleted].contents = rb->contents;
                deleted[n_deleted].last_seen = rb->timestamp;
                deleted[n_deleted].deleted_at = 0;  // Will compute below
                n_deleted++;
            }
        }

        block_num--;
    }

    // Compute deleted_at timestamps: find first rev timestamp after last_seen
    // Filenames are in chronological order
    for (int di = 0; di < n_deleted; di++) {
        time_t last_seen = deleted[di].last_seen;
        time_t deleted_at = 0;

        for (int fi = 0; fi < state->revs.filenames.n; fi++) {
            time_t ts = parse_rev_fname(state->revs.filenames.a[fi]);
            if (ts > last_seen) {
                deleted_at = ts;
                break;
            }
        }
        deleted[di].deleted_at = deleted_at;
    }

    // Sort by deleted_at descending (most recently deleted first)
    // Simple bubble sort since n_deleted is typically small
    for (int i = 0; i < n_deleted - 1; i++) {
        for (int j = 0; j < n_deleted - 1 - i; j++) {
            if (deleted[j].deleted_at < deleted[j+1].deleted_at) {
                deleted_block_info tmp = deleted[j];
                deleted[j] = deleted[j+1];
                deleted[j+1] = tmp;
            }
        }
    }

    // Clear progress line
    fprintf(stderr, "\033[H\033[K");
    fflush(stderr);

    // Print results
    if (n_deleted == 0) {
        prt("No deleted blocks found\n");
    } else {
        for (int di = 0; di < n_deleted; di++) {
            char timestamp[32];
            if (deleted[di].deleted_at > 0) {
                struct tm *tm_info = localtime(&deleted[di].deleted_at);
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
            } else {
                snprintf(timestamp, sizeof(timestamp), "(deleted after last rev)");
            }
            prt("%s %.*s\n", timestamp, (int)len(deleted[di].id), deleted[di].id.buf);
        }
    }
    flush();

    if (deleted) free(deleted);
    cmp.end = cmp_save;
}
/* #paste_after */
void paste_after() {
    if (state->curr_block_idx == -1 || state->blocks.n == 0) {
        prt("No current block\n");
        return;
    }

    get_revs();
    span deleted = find_last_deleted_block();
    if (len(deleted) == 0) {
        prt("Nothing to paste\n");
        return;
    }

    span block = state->blocks.a[state->curr_block_idx];
    int file_idx = file_for_block(block);
    projfile *pf = &state->files.a[file_idx];
    u8 *after_block = block.end;
    size_t tail_len = inp.end - after_block;

    memmove(after_block + len(deleted), after_block, tail_len);
    inp.end += len(deleted);
    memcpy(after_block, deleted.buf, len(deleted));

    pf->contents.end += len(deleted);
    for (int i = file_idx + 1; i < state->files.n; ++i) {
        state->files.a[i].contents.buf += len(deleted);
        state->files.a[i].contents.end += len(deleted);
    }

    new_rev(S(""), file_idx);
    ingest();

    state->curr_block_idx += 1;
    state->scrolled_lines = 0;
    prt("Pasted after current block\n");
}
/* #paste_before */
void paste_before() {
    if (state->curr_block_idx == -1 || state->blocks.n == 0) {
        prt("No current block\n");
        return;
    }

    get_revs();
    span deleted = find_last_deleted_block();
    if (len(deleted) == 0) {
        prt("Nothing to paste\n");
        return;
    }

    span block = state->blocks.a[state->curr_block_idx];
    int file_idx = file_for_block(block);
    projfile *pf = &state->files.a[file_idx];
    u8 *before_block = block.buf;
    size_t tail_len = inp.end - before_block;

    memmove(before_block + len(deleted), before_block, tail_len);
    inp.end += len(deleted);
    memcpy(before_block, deleted.buf, len(deleted));

    pf->contents.end += len(deleted);
    for (int i = file_idx + 1; i < state->files.n; ++i) {
        state->files.a[i].contents.buf += len(deleted);
        state->files.a[i].contents.end += len(deleted);
    }

    new_rev(S(""), file_idx);
    ingest();

    // curr_block_idx stays the same - points to original block which moved down
    state->curr_block_idx += 1;
    state->scrolled_lines = 0;
    prt("Pasted before current block\n");
}
/* #keyboard_help */
void keyboard_help() {
    clear_display();
    prt("Keyboard shortcuts:\n\n");
    prt("Navigation:\n");
    prt("  j/k     - Move down/up one block\n");
    prt("  g/G     - Jump to first/last block\n");
    prt("  space/b - Page down/up within block\n");
    prt("  #       - Jump to block by ID (use / to filter)\n");
    prt("  @       - Browse references (@refs, #mentions, incoming)\n");
    prt("\n");
    prt("Editing:\n");
    prt("  e       - Edit current block in $EDITOR\n");
    prt("  o/O     - Insert new block after/before current\n");
    prt("  d       - Delete current block\n");
    prt("  p/P     - Paste deleted block after/before current\n");
    prt("  r       - Rewrite code from comment (LLM), to clipboard\n");
    prt("  U       - Select block version from history\n");
    prt("\n");
    prt("Search:\n");
    prt("  /       - Enter search mode\n");
    prt("  n/N     - Repeat search forward/backward\n");
    prt("\n");
    prt("Other:\n");
    prt("  :       - Enter ex command mode (:help for commands)\n");
    prt("  B       - Build project with configured command\n");
    prt("  ?       - Show this help\n");
    prt("  q       - Quit\n");
    prt("\nPress any key to return...\n");
    flush();
    getch();
}

/* #handle_jkgG */
void handle_j() {
    if (state->curr_file_idx == -1) return;
    
    int count = state->count_prefix > 0 ? state->count_prefix : 1;
    int max_idx = state->inbox_mode ? state->inbox_end_idx : state->blocks.n - 1;
    
    for (int i = 0; i < count; i++) {
        if (state->curr_block_idx == -1) {
            // Empty file state - in inbox mode, go to inbox_start
            if (state->inbox_mode) {
                state->curr_block_idx = state->inbox_start_idx;
                state->curr_file_idx = file_for_block(state->blocks.a[state->curr_block_idx]);
                state->scrolled_lines = 0;
                return;
            }
            if (state->curr_file_idx + 1 < state->files.n) {
                state->curr_file_idx += 1;
                if (empty(state->files.a[state->curr_file_idx].contents))
                    return;
                state->curr_block_idx = first_block_in_file(state->curr_file_idx);
                state->scrolled_lines = 0;
            }
        } else {
            // Check inbox bounds
            if (state->inbox_mode && state->curr_block_idx >= max_idx) {
                return;
            }
            if (state->blocks.a[state->curr_block_idx].end == state->files.a[state->curr_file_idx].contents.end) {
                if (state->curr_file_idx + 1 < state->files.n) {
                    state->curr_file_idx += 1;
                    if (empty(state->files.a[state->curr_file_idx].contents)) {
                        if (!state->inbox_mode) state->curr_block_idx = -1;
                        return;
                    }
                    state->curr_block_idx = first_block_in_file(state->curr_file_idx);
                    // Clamp to inbox bounds
                    if (state->inbox_mode && state->curr_block_idx > max_idx) {
                        state->curr_block_idx = max_idx;
                    }
                    state->scrolled_lines = 0;
                }
            } else {
                if (state->curr_block_idx + 1 <= max_idx) {
                    state->curr_block_idx += 1;
                    state->scrolled_lines = 0;
                }
            }
        }
    }
}

void handle_k() {
    if (state->curr_file_idx == -1) return;
    
    int count = state->count_prefix > 0 ? state->count_prefix : 1;
    int min_idx = state->inbox_mode ? state->inbox_start_idx : 0;
    
    for (int i = 0; i < count; i++) {
        if (state->curr_block_idx == -1) {
            // Empty file state - in inbox mode, go to inbox_end
            if (state->inbox_mode) {
                state->curr_block_idx = state->inbox_end_idx;
                state->curr_file_idx = file_for_block(state->blocks.a[state->curr_block_idx]);
                state->scrolled_lines = 0;
                return;
            }
            if (state->curr_file_idx - 1 >= 0) {
                state->curr_file_idx -= 1;
                if (empty(state->files.a[state->curr_file_idx].contents))
                    return;
                state->curr_block_idx = last_block_in_file(state->curr_file_idx);
                state->scrolled_lines = 0;
            }
        } else {
            // Check inbox bounds
            if (state->inbox_mode && state->curr_block_idx <= min_idx) {
                return;
            }
            if (state->blocks.a[state->curr_block_idx].buf == state->files.a[state->curr_file_idx].contents.buf) {
                if (state->curr_file_idx - 1 >= 0) {
                    state->curr_file_idx -= 1;
                    if (empty(state->files.a[state->curr_file_idx].contents)) {
                        if (!state->inbox_mode) state->curr_block_idx = -1;
                        return;
                    }
                    state->curr_block_idx = last_block_in_file(state->curr_file_idx);
                    // Clamp to inbox bounds
                    if (state->inbox_mode && state->curr_block_idx < min_idx) {
                        state->curr_block_idx = min_idx;
                    }
                    state->scrolled_lines = 0;
                }
            } else {
                if (state->curr_block_idx - 1 >= min_idx) {
                    state->curr_block_idx -= 1;
                    state->scrolled_lines = 0;
                }
            }
        }
    }
}

void handle_g() {
    if (state->files.n == 0) return;

    if (state->inbox_mode) {
        state->curr_block_idx = state->inbox_start_idx;
        state->curr_file_idx = file_for_block(state->blocks.a[state->curr_block_idx]);
        state->scrolled_lines = 0;
        return;
    }

    state->curr_file_idx = 0;
    if (empty(state->files.a[0].contents)) {
        state->curr_block_idx = -1;
    } else {
        state->curr_block_idx = 0;
        state->scrolled_lines = 0;
    }
}

void handle_G() {
    if (state->files.n == 0) return;

    if (state->count_prefix > 0) {
        // NG goes to block N (1-indexed)
        int target = state->count_prefix - 1;
        if (state->inbox_mode) {
            // In inbox mode, clamp to inbox range
            if (target < state->inbox_start_idx) target = state->inbox_start_idx;
            if (target > state->inbox_end_idx) target = state->inbox_end_idx;
        } else {
            if (target >= state->blocks.n) target = state->blocks.n - 1;
            if (target < 0) target = 0;
        }
        state->curr_block_idx = target;
        state->curr_file_idx = file_for_block(state->blocks.a[target]);
        state->scrolled_lines = 0;
    } else {
        // G with no count goes to last block (or inbox_end in inbox mode)
        if (state->inbox_mode) {
            state->curr_block_idx = state->inbox_end_idx;
            state->curr_file_idx = file_for_block(state->blocks.a[state->curr_block_idx]);
            state->scrolled_lines = 0;
            return;
        }
        state->curr_file_idx = state->files.n - 1;
        if (empty(state->files.a[state->files.n - 1].contents)) {
            state->curr_block_idx = -1;
        } else {
            state->curr_block_idx = state->blocks.n - 1;
            state->scrolled_lines = 0;
        }
    }
}
/* #first_block_in_file */
int first_block_in_file(int file_idx) {
    for (int i = 0; i < state->blocks.n; ++i) {
        if (file_for_block(state->blocks.a[i]) == file_idx) {
            return i;
        }
    }
    return -1;
}

int last_block_in_file(int file_idx) {
    for (int i = state->blocks.n - 1; i >= 0; --i) {
        if (file_for_block(state->blocks.a[i]) == file_idx) {
            return i;
        }
    }
    return -1;
}


/* #start_search */
void start_search() {
    static char search_buffer[256] = {"/"}; // Static buffer for search, pre-initialized with "/"
    state->search = (span){.buf = (u8*)search_buffer, .end = (u8*)search_buffer + 1}; // Initialize search span to contain just "/"

    perform_search(); // Perform initial search display/update

    char input;
    while ((input = getch()) != '\n') { // Continue until Enter is pressed
        if (input == 0) {
            // EOF - exit search mode gracefully
            state->search = nullspan();
            print_current_blocks();
            return;
        }
        if (input == 0x1b) { // Escape - exit search mode
            state->search = nullspan();
            print_current_blocks();
            return;
        }
        if (input == '\b' || input == 127) { // Handle backspace (ASCII DEL on some systems)
            if (state->search.buf < state->search.end) {
                state->search.end--; // Shorten the span
                if (state->search.end == state->search.buf) {
                    // If we've deleted the initial "/", exit search mode
                    print_current_blocks();
                    return;
                }
            }
        } else if ((state->search.end - state->search.buf) < sizeof(search_buffer) - 1) {
            // Ensure there's space for more characters
            *state->search.end++ = input; // Extend the span
        }

        perform_search(); // Update search results after each modification
    }

    finalize_search(); // Finalize search on Enter
}
/* #start_ex */
void start_ex() {
    static char ex_buf[256] = ":";
    state->ex_command = (span){(u8*)ex_buf, (u8*)ex_buf + 1};

    prt("\033[%d;1H\033[K", state->terminal_rows);
    prt("%.*s", len(state->ex_command), state->ex_command.buf);
    flush();

    char ch;
    while ((ch = getch()) != '\n') {
        if (ch == 0) {
            // EOF - exit ex mode gracefully
            state->ex_command = nullspan();
            print_current_blocks();
            return;
        }
        if (ch == 0x1b) { // Escape - exit ex mode
            state->ex_command = nullspan();
            print_current_blocks();
            return;
        }
        if (ch == '\b' || ch == 127) { // Handle backspace
            if (state->ex_command.end > state->ex_command.buf + 1) {
                state->ex_command.end--;
            } else { // Exit ex mode if only ":" is left
                state->ex_command = nullspan();
                print_current_blocks();
                return;
            }
        } else { // Append non-backspace input, including UTF-8
            if (state->ex_command.end < state->ex_command.buf + sizeof(ex_buf)) {
                *(state->ex_command.end++) = ch;
            }
        }
        // Move to and clear the last row of the screen
        prt("\033[%d;1H\033[K", state->terminal_rows);
        // Write ex_command buffer on this last terminal row
        prt("%.*s", len(state->ex_command), state->ex_command.buf);
        flush();
    }
    handle_ex_command();
}
/* #extable */
/* #handle_ex_command */
void handle_ex_command() {
    if (starts_with(state->ex_command, S(":bootstrap"))) {
        bootstrap();
    } else if (starts_with(state->ex_command, S(":addfile "))) {
        span file_path = skip_n(state->ex_command, len(S(":addfile ")));
        ex_addfile(file_path);
    } else if (starts_with(state->ex_command, S(":addlib "))) {
        span lib_path = skip_n(state->ex_command, len(S(":addlib ")));
        ex_addlib(lib_path);
    } else if (span_eq(state->ex_command, S(":allfiles"))) {
        ex_allfiles();
    } else if (starts_with(state->ex_command, S(":help"))) {
        ex_help();
    } else if (starts_with(state->ex_command, S(":model"))) {
        select_model();
    } else if (span_eq(state->ex_command, S(":expand"))) {
        ex_expand();
    } else if (span_eq(state->ex_command, S(":reload"))) {
        ex_reload();
    } else if (span_eq(state->ex_command, S(":config"))) {
        ex_config();
    }
    state->ex_command = nullspan();
}
/* #ex_help */
void ex_help() {
    clear_display();
    prt("Ex Commands:\n\n");
    prt(":help       - Print this help message.\n");
    prt(":model      - Select the LLM to use for \"r\" and other commands.\n");
    prt(":expand     - Expand block references and display the result.\n");
    prt(":reload     - Reload all project files from disk (detect external changes).\n");
    prt(":config     - Edit and reload the config file.\n");
    prt(":allfiles   - Add all source files in project directory to config.\n");
    prt(":bootstrap  - Run the bootstrap command, put result on clipboard.\n");
    prt(":addfile    - Add a file to the project (e.g. :addfile ./foo.c).\n");
    prt(":addlib     - Add a library to the project.\n");
    prt("\n");
    prt("Press any key to continue...");
    flush();
    getch();
}
/* #set_highlight */
void set_highlight() {
    prt("\033[7m");
}

void reset_highlight() {
    prt("\033[0m");
}




/* #print_menu */
void print_menu(spans opts, int sel) {
    clear_display();

    int term_rows = state->terminal_rows;
    int num_opts = opts.n;
    int prompt_line = 1;
    int sel_row = (term_rows - prompt_line - 1) / 2;
    int max_above = sel_row;

    int start = sel > max_above ? sel - max_above : 0;
    int end = start + term_rows - prompt_line - 1;

    if (end > num_opts) {
        end = num_opts;
        start = end - term_rows + prompt_line + 1;
        if (start < 0) start = 0;
    }

    for (int i = 0; i < start; i++) terpri();

    for (int i = start; i < sel; i++) {
        prt("%.*s\n", len(opts.a[i]), opts.a[i].buf);
    }

    set_highlight();
    prt("%.*s\n", len(opts.a[sel]), opts.a[sel].buf);
    reset_highlight();

    for (int i = sel + 1; i < end; i++) {
        prt("%.*s\n", len(opts.a[i]), opts.a[i].buf);
    }

    while (end++ < term_rows - prompt_line) terpri();

    prt("Use j/k or Up/Down to move, Enter to select, and q to exit without change.");
    flush();
}


/* #select_menu */
int select_menu(spans options, int selected_index) {
    int ch;
    int state = 0;
    if (selected_index == -1) selected_index = 0;
    print_menu(options, selected_index);
    while ((ch = getch())) {
        switch (ch) {
            case '\033':
                state = 1;
                break;
            case 'q':
                return -1;
            case '[':
                if (state == 1) state = 2;
                break;
            case 'A': // up arrow
                if (state == 2 && selected_index > 0) {
                    selected_index--;
                    print_menu(options, selected_index);
                }
                state = 0;
                break;
            case 'B': // down arrow
                if (state == 2 && selected_index < options.n - 1) {
                    selected_index++;
                    print_menu(options, selected_index);
                }
                state = 0;
                break;
            case 'j':
                if (selected_index < options.n - 1) {
                    selected_index++;
                    print_menu(options, selected_index);
                }
                break;
            case 'k':
                if (selected_index > 0) {
                    selected_index--;
                    print_menu(options, selected_index);
                }
                break;
            case '\n': // enter key
                return selected_index;
            default:
                state = 0;
                break;
        }
    }
    return selected_index;
}


/* #select_menu_searchable */
// Helper: case-insensitive substring search
static int contains_ci(span haystack, span needle) {
    int hlen = len(haystack);
    int nlen = len(needle);
    if (nlen == 0) return 1;
    if (hlen < nlen) return 0;
    for (int i = 0; i <= hlen - nlen; i++) {
        int match = 1;
        for (int j = 0; j < nlen; j++) {
            char hc = tolower(haystack.buf[i + j]);
            char nc = tolower(needle.buf[j]);
            if (hc != nc) { match = 0; break; }
        }
        if (match) return 1;
    }
    return 0;
}

// Helper: check if item matches all space-separated tokens in filter
static int matches_filter(span item, char* filter) {
    if (filter[0] == '\0') return 1;
    char* p = filter;
    while (*p) {
        // Skip spaces
        while (*p == ' ') p++;
        if (!*p) break;
        // Find end of token
        char* start = p;
        while (*p && *p != ' ') p++;
        span token = {(u8*)start, (u8*)p};
        if (!contains_ci(item, token)) return 0;
    }
    return 1;
}

// Helper: build filtered list and index mapping
static spans build_filtered(spans options, char* filter, int* map, int* map_count) {
    spans filtered = spans_alloc(options.n);
    *map_count = 0;
    for (int i = 0; i < options.n; i++) {
        if (matches_filter(options.a[i], filter)) {
            spans_push(&filtered, options.a[i]);
            map[(*map_count)++] = i;
        }
    }
    return filtered;
}

// Helper: print a line and clear to end of line (reduces flicker)
static void print_line_clear(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    prt("%s\033[K\n", buffer);  // Print content, clear to EOL, newline
}

// Helper: print menu with filter (flicker-reduced version)
static void print_menu_filtered(spans opts, int sel, char* filter, int filter_mode) {
    // Move cursor to home position without clearing screen
    prt("\033[H");

    int term_rows = state->terminal_rows;
    int num_opts = opts.n;
    int prompt_lines = filter_mode ? 2 : 1;  // Extra line for filter display
    int sel_row = (term_rows - prompt_lines - 1) / 2;
    int max_above = sel_row;
    int lines_printed = 0;

    if (num_opts == 0) {
        print_line_clear("(no matches)");
        lines_printed++;
        for (int i = 1; i < term_rows - prompt_lines; i++) {
            prt("\033[K\n");  // Clear line and newline
            lines_printed++;
        }
    } else {
        int start = sel > max_above ? sel - max_above : 0;
        int end = start + term_rows - prompt_lines - 1;

        if (end > num_opts) {
            end = num_opts;
            start = end - term_rows + prompt_lines + 1;
            if (start < 0) start = 0;
        }

        // Blank lines before content
        for (int i = 0; i < start; i++) {
            prt("\033[K\n");
            lines_printed++;
        }

        // Items before selection
        for (int i = start; i < sel && i < num_opts; i++) {
            prt("%.*s\033[K\n", len(opts.a[i]), opts.a[i].buf);
            lines_printed++;
        }

        // Selected item
        if (sel < num_opts) {
            set_highlight();
            prt("%.*s", len(opts.a[sel]), opts.a[sel].buf);
            reset_highlight();
            prt("\033[K\n");
            lines_printed++;
        }

        // Items after selection
        for (int i = sel + 1; i < end; i++) {
            prt("%.*s\033[K\n", len(opts.a[i]), opts.a[i].buf);
            lines_printed++;
        }

        // Blank lines after content
        while (lines_printed < term_rows - prompt_lines) {
            prt("\033[K\n");
            lines_printed++;
        }
    }

    // Filter line (if in filter mode)
    if (filter_mode) {
        prt("/%s\033[K\n", filter);
    }

    // Prompt line
    prt("j/k:move  /:search  enter:select  q:cancel\033[K");

    // Clear any remaining lines below
    prt("\033[J");

    flush();
}

int select_menu_searchable(spans options, int initial_index) {
    char filter[256] = {0};
    int filter_len = 0;
    int filter_mode = 0;
    int* index_map = malloc(options.n * sizeof(int));
    int map_count = options.n;

    // Initialize index map (identity mapping)
    for (int i = 0; i < options.n; i++) index_map[i] = i;

    spans filtered = options;  // Start with full list
    int sel = initial_index >= 0 && initial_index < options.n ? initial_index : 0;

    // Initial draw needs to clear screen first
    clear_display();
    print_menu_filtered(filtered, sel, filter, filter_mode);

    for (;;) {
        int k = getkey();

        if (k == 'q') {
            free(index_map);
            return -1;
        }

        if (k == 27) {  // Escape
            if (filter_mode && filter_len > 0) {
                // Clear filter
                filter[0] = '\0';
                filter_len = 0;
                filter_mode = 0;
                // Rebuild to full list
                for (int i = 0; i < options.n; i++) index_map[i] = i;
                map_count = options.n;
                filtered = options;
                sel = 0;
                print_menu_filtered(filtered, sel, filter, filter_mode);
            } else {
                free(index_map);
                return -1;
            }
            continue;
        }

        if (k == '\n' || k == '\r') {
            if (filtered.n > 0 && sel < map_count) {
                int result = index_map[sel];
                free(index_map);
                return result;
            }
            continue;
        }

        if (k == '/') {
            filter_mode = 1;
            print_menu_filtered(filtered, sel, filter, filter_mode);
            continue;
        }

        if (k == 'j' || k == ARROW_D) {
            if (filtered.n > 0 && sel < filtered.n - 1) {
                sel++;
                print_menu_filtered(filtered, sel, filter, filter_mode);
            }
            continue;
        }

        if (k == 'k' || k == ARROW_U) {
            if (sel > 0) {
                sel--;
                print_menu_filtered(filtered, sel, filter, filter_mode);
            }
            continue;
        }

        if (filter_mode) {
            if (k == 127 || k == 8) {  // Backspace or DEL
                if (filter_len > 0) {
                    filter[--filter_len] = '\0';
                    filtered = build_filtered(options, filter, index_map, &map_count);
                    sel = 0;
                    print_menu_filtered(filtered, sel, filter, filter_mode);
                }
            } else if (k >= 32 && k < 127) {  // Printable character
                if (filter_len < 255) {
                    filter[filter_len++] = k;
                    filter[filter_len] = '\0';
                    filtered = build_filtered(options, filter, index_map, &map_count);
                    sel = 0;
                    print_menu_filtered(filtered, sel, filter, filter_mode);
                }
            }
        }
    }
}

/* #select_model */
void select_model() {
    spans_arena_push();
    spans models = spans_alloc(20);
    spans_push(&models, S("clipboard"));
    spans_push(&models, S("gpt-3.5-turbo"));
    spans_push(&models, S("gpt-4-turbo"));
    spans_push(&models, S("gpt-4o"));
    spans_push(&models, S("claude-3-5-sonnet-20240620"));
    spans_push(&models, S("claude-3-opus-20240229"));
    spans_push(&models, S("claude-3-sonnet-20240229"));
    spans_push(&models, S("claude-3-haiku-20240307"));
    spans_push(&models, S("llama.cpp"));
    for (size_t i = 0; i < state->ollama_models.n; ++i) {
        spans_push(&models, state->ollama_models.a[i]);
    }

    int selected_index = 0;
    for (size_t i = 0; i < models.n; ++i) {
        if (span_eq(models.a[i], state->model)) {
            selected_index = i;
            break;
        }
    }

    selected_index = select_menu(models, selected_index);
    if (selected_index >= 0 && selected_index < models.n) {
        state->model = models.a[selected_index];
        save_conf();
    }

    spans_arena_pop();
}


/* #bootstrap */
void bootstrap() {
    ensure_conf_var(&state->bootstrap, S("The bootstrap command generates your initial prompt on stdout. See README for details."), nullspan());
    
    char buf[2048] = {0};
    s_buffer(buf, sizeof(buf), state->bootstrap);
    prt("Running bootstrap command: %s\n", buf);
    flush();
    
    // Run command and capture output into cmp space
    FILE* fp = popen(buf, "r");
    if (!fp) {
        prt("Error: Could not run bootstrap command.\n");
        prt("Press any key to continue...");
        flush();
        getch();
        return;
    }
    
    span cmp_free = cmp_compl();
    u8* start = cmp_free.buf;
    u8* p = start;
    u8* end = cmp_free.end;
    
    int c;
    while ((c = fgetc(fp)) != EOF && p < end) {
        *p++ = (u8)c;
    }
    pclose(fp);
    
    state->bootstrapprompt = (span){start, p};
    cmp.end = p;
    
    send_to_clipboard(state->bootstrapprompt);
    prt("Bootstrap output (%d bytes) sent to clipboard.\n", (int)len(state->bootstrapprompt));
    prt("Press any key to continue...");
    flush();
    getch();
}
/* #perform_search */
void perform_search() {
    int remaining_lines = state->terminal_rows;
    span search_span = {state->search.buf + 1, state->search.end};
    int match_count = 0;
    int first_match_index = -1;
    span first_match_span = nullspan();

    for (int i = 0; i < state->blocks.n; i++) {
        span match = spanspan(state->blocks.a[i], search_span);
        if (!empty(match) || empty(search_span)) {
            if (first_match_index == -1) {
                first_match_index = i;
                first_match_span = match;
            }
            match_count++;
        }
    }

    clear_display();

    if (first_match_index != -1) {
        prt("Block %d:\n", first_match_index + 1);
        remaining_lines -= 1;

        int initial_lines_to_print = (remaining_lines - 8) / 2;
        print_physical_lines(state->blocks.a[first_match_index], initial_lines_to_print);
        remaining_lines -= initial_lines_to_print;

        prt("\n");
        remaining_lines -= 1;

        prt("Match:\n");
        remaining_lines -= 1;

        int lines_printed = print_matching_physical_lines(state->blocks.a[first_match_index], first_match_span);
        remaining_lines -= lines_printed;

        prt("\n");
        remaining_lines -= 1;
    }

    prt("%d blocks matched\n", match_count);
    remaining_lines -= 1;

    while (remaining_lines > 1) {
        terpri();
        remaining_lines -= 1;
    }

    wrs(state->search);
    flush();
}

/* #print_ruler */
void print_ruler() {
    span current_file_path = (state->curr_file_idx != -1) ? state->files.a[state->curr_file_idx].path : S("-");
    span model = state->model;
    span debug_info = get_debug_info();

    int block_count = state->blocks.n;
    int current_block_number = (state->curr_block_idx != -1) ? state->curr_block_idx + 1 : 0;
    
    // Check if current file is modified (cksum differs from load_checksum)
    int is_modified = 0;
    if (state->curr_file_idx != -1) {
        projfile *f = &state->files.a[state->curr_file_idx];
        if (f->cksum.__u != f->load_checksum.__u) {
            is_modified = 1;
        }
    }

    if (state->count_prefix > 0) {
        prt("%d ", state->count_prefix);
    }

    if (state->curr_file_idx == -1 && state->curr_block_idx == -1) {
        prt("Block -/0, Line -, File -, Model %.*s", len(model), model.buf);
    } else if (state->curr_block_idx == -1) {
        prt("Block -/%d, Line -, File %.*s, Model %.*s", block_count, len(current_file_path), current_file_path.buf, len(model), model.buf);
    } else {
        int top_visible_line = state->scrolled_lines + 1;
        prt("Block %d/%d, Line %d, File %.*s%s, Model %.*s", 
            current_block_number, block_count, top_visible_line, 
            len(current_file_path), current_file_path.buf,
            is_modified ? " [+]" : "",
            len(model), model.buf);
    }

    if (empty(debug_info)) {
        prt(", ? for help");
    } else {
        prt(", %.*s", len(debug_info), debug_info.buf);
    }

    flush();
}
/* #get_debug_info */
span get_debug_info() {
    span result = nullspan();
    
    if (contains(state->debug, S("sa"))) {
        span high_point = prs("%zu/%zu", spans_global_arena.allocated, spans_global_arena.arena_size);
        result = concat(result, high_point);
    }

    if (contains(state->debug, S("inp"))) {
        span inp_len = prs("inp:%d", len(inp));
        if (!empty(result)) {
            result = concat(result, S(" "));
        }
        result = concat(result, inp_len);
    }

    return result;
}


/* #print_single_block_with_skipping */
void print_single_block_with_skipping(int block_index, int skipped_lines) {
    span block = state->blocks.a[block_index];
    int physical_lines = skipped_lines;
    span skipped_span = count_physical_lines(block, &physical_lines);
    span block_suffix = block;
    block_suffix.buf = skipped_span.end;

    int remaining_rows = state->terminal_rows;
    
    // Get block ID for header
    spans ids = ids_for_block(block);
    if (ids.n > 0) {
        prt("Block %d  %.*s\n", block_index + 1, len(ids.a[0]), ids.a[0].buf);
    } else {
        prt("Block %d  (anonymous)\n", block_index + 1);
    }
    --remaining_rows;

    int remaining_content_lines = remaining_rows - 1;
    span content_to_print = count_physical_lines(block_suffix, &remaining_content_lines);
    wrs(content_to_print);

    while (remaining_content_lines-- > 0) {
        terpri();
    }

    print_ruler();
}
/* #print_matching_physical_lines */
int print_matching_physical_lines(span block, span match) {

    int physical_lines_printed = 0;
    int terminal_width = state->terminal_cols;

    while (!empty(block)) {
        span line = next_line(&block);

        if ((match.buf >= line.buf) && (match.end <= line.end)) {
            int start_offset = match.buf - line.buf;
            int match_length = len(match);
            int start_physical_line = start_offset / terminal_width;
            int end_physical_line = (start_offset + match_length) / terminal_width;

            for (int i = start_physical_line; i <= end_physical_line; ++i) {
                int line_start = i * terminal_width;
                int line_end = (i + 1) * terminal_width;
                if (line_end > len(line)) {
                    line_end = len(line);
                }
                prt("%.*s\n", line_end - line_start, line.buf + line_start);
                physical_lines_printed++;
            }
            break;
        }
    }

    return physical_lines_printed;
}

/* #finalize_search */
void finalize_search() {
    span search_term = skip_n(state->search, 1); // Skip the slash
    int found = -1;
    for (int i = 0; i < state->blocks.n && found == -1; i++) {
        if (contains(state->blocks.a[i], search_term)) {
            found = i;
        }
    }
    if (found != -1) {
        set_current_block(found);
        state->previous_search = state->search;
        state->search = nullspan();
    }
}


/* #search_forward */
void search_forward() {
    if(empty(state->previous_search)) return;
    span search_term = skip_n(state->previous_search, 1);
    int match_index = -1;
    for (int i = 0; i < state->blocks.n; ++i) {
        if (i > state->curr_block_idx && contains(state->blocks.a[i], search_term)) {
            match_index = i;
            break;
        }
    }
    if (match_index != -1) {
        set_current_block(match_index);
    }
}

void search_backward() {
    if(empty(state->previous_search)) return;
    span search_term = skip_n(state->previous_search, 1);
    int match_index = -1;
    for (int i = state->blocks.n - 1; i >= 0; --i) {
        if (i < state->curr_block_idx && contains(state->blocks.a[i], search_term)) {
            match_index = i;
            break;
        }
    }
    if (match_index != -1) {
        set_current_block(match_index);
    }
}


/* #Settings */
void handle_conf_language(span language) {
    state->current_language = language;
    // Set the language for all previously added files if they have no language set
    if (state->files.n > 0 && empty(state->files.a[0].language)) {
        for (int i = 0; i < state->files.n; i++) {
            if (empty(state->files.a[i].language)) {
                state->files.a[i].language = language;
            }
        }
    }
}

void handle_conf_file(span file_path) {
    projfile file = { .path = file_path, .language = state->current_language, .contents = nullspan() };
    projfiles_push(&state->files, file);
}


/* #parse_config */
void parse_config() {
    span cmp_free_space = cmp_compl();
    span config_content = read_file_S_into_span(state->config_file_path, cmp_free_space);
    cmp.end = config_content.end; // Update cmp to avoid overwriting config

    int manual_file = empty(state->manual_filename) ? 0 : 1;

    while (!empty(config_content)) {
        span line = next_line(&config_content);
        int pos = find_char(line, ':');
        if (pos < 0) continue; // Skip line if no colon found

        span key = {line.buf, line.buf + pos};
        span value = {line.buf + pos + 1, line.end};

        // Skip initial whitespace in the value
        while (value.buf < value.end && isspace(*value.buf)) value.buf++;

        // Handle special keys
        if (span_eq(key, S("language"))) {
            if (!manual_file) handle_conf_language(value);
        } else if (span_eq(key, S("file"))) {
            if (!manual_file) handle_conf_file(value);
        } else {
            // Handle general configuration keys
            #define X(name) \
                if (span_eq(key, S(#name))) { \
                    state->name = value; \
                    continue; \
                }
            CONFIG_FIELDS
            #undef X
        }
    }

    if (manual_file) {
        handle_conf_language(S("C"));
        handle_conf_file(state->manual_filename);
    }

    state->ollama_models = split_commas_ws(state->ollamas);
}

/* #read_line */
span read_line(span *buffer, span default_value) {
    assert(len(*buffer) > 0); // Ensure buffer is not empty
    span line = { .buf = buffer->buf, .end = buffer->buf }; // Initialize line span to empty
    if (!empty(default_value)) { // If default value is provided
        memcpy(buffer->buf, default_value.buf, len(default_value)); // Copy default into buffer
        line.end += len(default_value); // Adjust end of line span
    }
    prt("> %.*s", len(line), line.buf); // Print prompt and current line content
    flush(); // Ensure output is visible
    char ch;
    while ((ch = getch()) != '\n') { // Read input until enter is hit
        if (ch == 0) { // EOF - return what we have (default or empty)
            break;
        }
        if (ch == '\b' || ch == 127) { // Handle backspace (ASCII DEL or backspace)
            if (line.buf < line.end) { // Check if there's a character to delete
                line.end--; // Shorten the span by one
                prt("\033[D \033[D"); // Move cursor back, clear character, move back again
            }
        } else { // For all other characters
            *line.end++ = ch; // Append character to span
            w_char(ch); // Print character
        }
        flush(); // Ensure output is visible
    }
    *buffer = (span){ .buf = line.end, .end = buffer->end }; // Adjust input buffer span to exclude the read line
    return (span){ .buf = line.buf, .end = line.end }; // Return the span containing user input
}
/* #save_conf_files */
void save_conf_files() {
    span last_written_language = nullspan();
    for (int i = 0; i < state->files.n; i++) {
        if (!span_eq(last_written_language, state->files.a[i].language)) {
            last_written_language = state->files.a[i].language;
            prt("\nlanguage: %.*s\n", len(last_written_language), last_written_language.buf);
        }
        prt("file: %.*s\n", len(state->files.a[i].path), state->files.a[i].path.buf);
    }
}

/* #save_conf */
void save_conf() {
    span original_cmp_end = {cmp.end, cmp.end};
    out_sav sav = out2cmp();

    #define X(name) prt(#name ": %.*s\n", len(state->name), state->name.buf);
    CONFIG_FIELDS
    #undef X

    save_conf_files();
    out_rst(sav);
    original_cmp_end.end = cmp.end;

    write_to_file_span(original_cmp_end, state->config_file_path, 1);
    cmp.end = original_cmp_end.buf;
}


/* #add_projfile(span) */
int add_projfile(span file_path_span) {
    char file_path[2048] = {0};
    s_buffer(file_path, 2048, file_path_span);

    FILE *file = fopen(file_path, "a+");
    if (file == NULL) {
        prt("Error: Cannot create or write to file %s.\nPress any key to continue...\n", file_path);
        flush();
        getch();
        return 0;
    }
    fclose(file);

    projfile new_file = {.path = file_path_span, .language = nullspan(), .contents = nullspan()};
    projfiles_push(&state->files, new_file);
    return 1;
}


/* #check_dirs */
void check_dirs() {
    span dirs[] = {
        S("revs/"),
        S("tmp/"),
        S("api_calls/"),
        S("cache/"),
        S("cache/v8/"),
        S("cache/v8/revs/"),
        S("outputs/"),
        S("events/"),
        S("sync/")
    };
    
    char buffer[1024];

    for (int i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        snprintf(buffer, sizeof(buffer), "%.*s/%.*s", len(state->cmprdir), state->cmprdir.buf, len(dirs[i]), dirs[i].buf);
        mkdir(buffer, 0777);
    }
}
/* #check_conf_vars */
void check_conf_vars() {
    if (empty(state->cmprdir)) {
      state->cmprdir = S(".cmpr/");
    }
    // Ensure cmprdir ends with a slash for correct path concatenation
    if (!empty(state->cmprdir) && state->cmprdir.end[-1] != '/') {
      state->cmprdir = concat(state->cmprdir, S("/"));
    }
    if (empty(state->model)) {
      state->model = S("clipboard");
    }
}
/* #ensure_conf_var */
void ensure_conf_var(span* var, span message, span default_value) {
    if (!empty(*var)) return; // If the configuration variable is already set, return immediately

    prt("%.*s\n", len(message), message.buf); // Print the message explaining the configuration setting
    if (!empty(default_value)) {
        prt("Default: %.*s\n", len(default_value), default_value.buf); // Show default value if provided
    }

    span buffer = cmp_compl(); // Get complement of cmp space as a span for input
    *var = read_line(&buffer, default_value); // Read new value from user

    cmp.end = buffer.buf; // Update cmp.end to the end of the returned span from read_line

    save_conf(); // Rewrite the configuration file with the updated setting
}



/* #edit_current_block */
void edit_current_block() {
    if (state->curr_file_idx == -1 && state->curr_block_idx == -1) {
        return;
    }

    span tmp_file = tmp_filename();
    span content_to_write;

    if (state->curr_block_idx == -1) {
        content_to_write = nullspan();
    } else {
        content_to_write = state->blocks.a[state->curr_block_idx];
    }

    write_to_file_span(content_to_write, tmp_file, 0);
    prt("Temp file %s written for editing.\n", s(tmp_file));
    flush();

    if (launch_editor(s(tmp_file)) == 0) {
        prt("Editor exited successfully, creating rev.\n");
        flush();
        handle_edited_file(s(tmp_file));
    } else {
        prt("Editor exited with error, changes not saved.\n");
        flush();
        getch();
    }
}



/* #insert_block_after */
void insert_block_after() {
    if (state->curr_block_idx == -1 || state->blocks.n == 0) {
        return;
    }

    span tmp_file = tmp_filename();
    write_to_file_span(nullspan(), tmp_file, 0);
    prt("Temp file %.*s written for editing.\n", (int)len(tmp_file), tmp_file.buf);
    flush();

    if (launch_editor(s(tmp_file)) == 0) {
        span new_content = read_file_into_cmp(tmp_file);
        if (len(new_content) == 0) {
            prt("No content added, cancelled.\n");
            flush();
            getch();
            return;
        }
        prt("Editor exited successfully, creating rev.\n");
        flush();

        span block = state->blocks.a[state->curr_block_idx];
        int file_idx = file_for_block(block);
        projfile *pf = &state->files.a[file_idx];
        u8 *after_block = block.end;
        size_t tail_len = inp.end - after_block;

        memmove(after_block + len(new_content), after_block, tail_len);
        inp.end += len(new_content);
        memcpy(after_block, new_content.buf, len(new_content));

        pf = &state->files.a[file_idx];
        pf->contents.end += len(new_content);
        for (int i = file_idx + 1; i < state->files.n; ++i) {
            state->files.a[i].contents.buf += len(new_content);
            state->files.a[i].contents.end += len(new_content);
        }

        size_t old_blocks_n = state->blocks.n;
        ingest();

        new_rev(S(""), file_idx);

        if (state->blocks.n > old_blocks_n) {
            state->curr_block_idx += 1;
        }
        state->scrolled_lines = 0;
    } else {
        prt("Editor exited with error, changes not saved.\n");
        flush();
        getch();
    }
}
/* #insert_block_before */
void insert_block_before() {
    if (state->curr_block_idx == -1 || state->blocks.n == 0) {
        return;
    }

    span tmp_file = tmp_filename();
    write_to_file_span(nullspan(), tmp_file, 0);
    prt("Temp file %.*s written for editing.\n", (int)len(tmp_file), tmp_file.buf);
    flush();

    if (launch_editor(s(tmp_file)) == 0) {
        span new_content = read_file_into_cmp(tmp_file);
        if (len(new_content) == 0) {
            prt("No content added, cancelled.\n");
            flush();
            getch();
            return;
        }
        prt("Editor exited successfully, creating rev.\n");
        flush();

        span block = state->blocks.a[state->curr_block_idx];
        int file_idx = file_for_block(block);
        projfile *pf = &state->files.a[file_idx];
        u8 *before_block = block.buf;
        size_t tail_len = inp.end - before_block;

        memmove(before_block + len(new_content), before_block, tail_len);
        inp.end += len(new_content);
        memcpy(before_block, new_content.buf, len(new_content));

        pf = &state->files.a[file_idx];
        pf->contents.end += len(new_content);
        for (int i = file_idx + 1; i < state->files.n; ++i) {
            state->files.a[i].contents.buf += len(new_content);
            state->files.a[i].contents.end += len(new_content);
        }

        ingest();

        new_rev(S(""), file_idx);

        state->scrolled_lines = 0;
    } else {
        prt("Editor exited with error, changes not saved.\n");
        flush();
        getch();
    }
}
/* #tmp_filename */
span tmp_filename() {
    time_t now = time(NULL);
    struct tm *tm_struct = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", tm_struct);
    
    span current_language = current_block_language();
    char* extension = "";
    if (span_eq(current_language, S("C"))) {
        extension = ".c";
    } else if (span_eq(current_language, S("Python"))) {
        extension = ".py";
    } else if (span_eq(current_language, S("JavaScript"))) {
        extension = ".js";
    } else if (span_eq(current_language, S("Markdown"))) {
        extension = ".md";
    }

    static char filename[1024];
    snprintf(filename, sizeof(filename), "%stmp/%s%s", s(state->cmprdir), timestamp, extension);
    return S(filename);
}



/* #launch_editor */
int launch_editor(char* filename) {
    char* editor = getenv("EDITOR");
    if (editor == NULL) {
        editor = "vi"; // Default to vi if EDITOR is not set
    }

    pid_t pid = vfork();
    if (pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        execlp(editor, editor, filename, (char*)NULL);
        // If execlp returns, it means it failed
        perror("execlp failed");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status); // Return the exit status of the editor
        } else {
            return -1; // Editor didn't exit normally
        }
    }
}



/* #file_for_block */
int file_for_block(span block) {
    for (int i = 0; i < state->files.n; ++i) {
        if (contains_ptr(state->files.a[i].contents, block)) {
            return i;
        }
    }
    prt("Error: Block not found in any file.\n");
    flush_err();
    exit(1);
}


/* #current_block_language */
span current_block_language() {
   if (state->curr_file_idx == -1) {
       prt("Error: Attempted to get language in empty project state\n");
       flush_err();
       exit(1);
   }

   if (state->curr_block_idx == -1) {
       return guess_language_from_filename(state->files.a[state->curr_file_idx].path);
   }

   return language_for_block(state->blocks.a[state->curr_block_idx]);
}


/* #guess_language_from_filename */
span guess_language_from_filename(span filename) {
   span extension = filename;
   for (u8* p = filename.end - 1; p >= filename.buf; p--) {
       if (*p == '.') {
           extension.buf = p + 1;
           break;
       }
   }

   if (span_eq(extension, S("c"))) return S("C");
   if (span_eq(extension, S("py"))) return S("Python");
   if (span_eq(extension, S("js"))) return S("JavaScript");
   if (span_eq(extension, S("md"))) return S("Markdown");

   return S("C");
}

/* #language_for_block */
span language_for_block(span block) {
    int file_index = file_for_block(block);
    return state->files.a[file_index].language;
}


/* #handle_edited_file */
void handle_edited_file(char *filename) {
   span original_block;
   
   if (state->curr_block_idx == -1) {
       original_block = state->files.a[state->curr_file_idx].contents;
   } else {
       original_block = state->blocks.a[state->curr_block_idx];
   }

   struct stat st;
   if (stat(filename, &st) != 0) {
       prt("Failed to get file size for %s: %s\n", filename, strerror(errno));
       flush_err();
       exit(1);
   }
   
   size_t new_size = st.st_size;
   size_t old_size = len(original_block);
   ptrdiff_t size_diff = new_size - old_size;

   if (size_diff != 0) {
       memmove(original_block.buf + new_size, original_block.end, inp.end - original_block.end);
       inp.end += size_diff;
   }

   span gap = {original_block.buf, original_block.buf + new_size};
   span result = read_file_into_span(filename, gap);

   if (!span_eq(result, gap)) {
       prt("Unexpected file size change for %s\n", filename);
       flush_err();
       exit(1);
   }

   for (int i = state->curr_file_idx; i < state->files.n; i++) {
       if (i == state->curr_file_idx) {
           state->files.a[i].contents.end += size_diff;
       } else {
           state->files.a[i].contents.buf += size_diff;
           state->files.a[i].contents.end += size_diff;
       }
   }

   ingest();
   new_rev(S(filename), state->curr_file_idx);
}


/* #compute_file_checksum */
checksum compute_file_checksum(span path) {
    if (!readable_file(path)) {
        checksum c = { .__u = 0 };
        return c;
    }
    span content = read_file_into_cmp(path);
    return selected_checksum(content);
}

/* #checksum_is_known */
int checksum_is_known(checksum cs) {
    span hex = prs_checksum(cs);
    span path = concat(state->cmprdir, S("sync/known-checksums"));
    if (!readable_file(path)) return 0;
    span file = read_file_into_cmp(path);
    span lines = file;
    while (!empty(lines)) {
        span line = next_line(&lines);
        if (starts_with(line, hex)) return 1;
    }
    return 0;
}

/* #replace_known_checksum */
void replace_known_checksum(checksum old_cs, checksum new_cs, span filename) {
    char path_buf[PATH_MAX];
    s_buffer(path_buf, PATH_MAX, concat(state->cmprdir, S("sync/known-checksums")));
    span path = S(path_buf);

    char old_hex_buf[17], new_hex_buf[17];
    s_buffer(old_hex_buf, sizeof(old_hex_buf), prs_checksum(old_cs));
    s_buffer(new_hex_buf, sizeof(new_hex_buf), prs_checksum(new_cs));
    span old_hex = S(old_hex_buf);
    span new_hex = S(new_hex_buf);

    span file_content = nullspan();
    int has_content = 0;
    if (readable_file(path)) {
        file_content = read_file_into_cmp(path);
        has_content = 1;
    }

    out_sav sav = out2cmp();
    u8* new_start = cmp.end;

    if (has_content) {
        span lines = file_content;
        while (!empty(lines)) {
            span ln = next_line(&lines);
            int write_line = 1;
            if (old_cs.__u != 0) {
                if (starts_with(ln, old_hex)) write_line = 0;
            }
            if (write_line) {
                wrs(ln);
                terpri();
            }
        }
    }

    wrs(new_hex);
    w_char('\t');
    wrs(filename);
    terpri();

    span new_content = (span){ new_start, cmp.end };
    write_to_file_span(new_content, path, 1);
    out_rst(sav);
}

/* #save_external_rev */
void save_external_rev(int file_index, struct timespec ts) {
    span file_path = state->files.a[file_index].path;
    span file_contents = read_file_into_cmp(file_path);
    span rev_path = unique_rev_path(ts);
    write_to_file_span(file_contents, rev_path, 1);
    prt("Detected external changes to %.*s, saving as rev %.*s\n",
        (int)len(file_path), file_path.buf,
        (int)len(rev_path), rev_path.buf);
}

/* #unique_rev_path */
span unique_rev_path(struct timespec ts) {
    char tsbuf[32];
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    snprintf(tsbuf, sizeof(tsbuf), "%04d%02d%02d-%02d%02d%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    span prefix = concat(state->cmprdir, S("revs/"));
    span ts_span = S(tsbuf);
    span base_path = concat(prefix, ts_span);
    char pathbuf[PATH_MAX];
    s_buffer(pathbuf, sizeof(pathbuf), base_path);
    if (access(pathbuf, F_OK) != 0) return base_path;

    int divisors[] = {100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};
    u64 nsec = ts.tv_nsec;
    char frac[16] = ".";
    int k = 1;
    for (int i = 0; i < 9; ++i) {
        frac[k++] = '0' + (nsec / divisors[i]) % 10;
        frac[k] = 0;
        span path_span = prs("%s%s", tsbuf, frac);
        span full_span = concat(prefix, path_span);
        s_buffer(pathbuf, sizeof(pathbuf), full_span);
        if (access(pathbuf, F_OK) != 0) return full_span;
    }
    return nullspan();
}


/* #new_rev */
void new_rev(span tmp_filename, int file_index) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    span rev_path = unique_rev_path(ts);
    prt("writing new rev %.*s\n", len(rev_path), rev_path.buf);
    write_to_file_span(state->files.a[file_index].contents, rev_path, 1);
    update_projfile(file_index, tmp_filename, rev_path);
}

/* #update_projfile */
void update_projfile(int file_index, span tmp_filename, span rev_path) {
    span projfile_path = state->files.a[file_index].path;
    char projfile_buf[PATH_MAX];
    s_buffer(projfile_buf, PATH_MAX, projfile_path);
    struct stat file_stat;
    if (stat(projfile_buf, &file_stat) != 0) {
        prt("stat failed on %.*s: %s\n", len(projfile_path), projfile_path.buf, strerror(errno));
        flush_exit(1);
    }
    checksum old_checksum = compute_file_checksum(projfile_path);
    int known = 0;
    if (old_checksum.__u != 0)
        known = checksum_is_known(old_checksum);
    if (old_checksum.__u != 0 && !known)
        save_external_rev(file_index, file_stat.st_ctim);

    char revpath_buf[PATH_MAX];
    s_buffer(revpath_buf, PATH_MAX, rev_path);
    int copy_ret = copy_file(revpath_buf, projfile_buf);
    if (copy_ret < 0) {
        prt("copy_file failed (%d) %.*s -> %.*s: %s\n", copy_ret, len(rev_path), rev_path.buf, len(projfile_path), projfile_path.buf, strerror(errno));
        flush_exit(1);
    }
    if (chmod(projfile_buf, file_stat.st_mode) != 0) {
        prt("chmod failed on %.*s: %s\n", len(projfile_path), projfile_path.buf, strerror(errno));
        flush_exit(1);
    }
    checksum new_checksum = compute_file_checksum(projfile_path);
    replace_known_checksum(old_checksum, new_checksum, projfile_path);

    if (!empty(tmp_filename)) {
        char tmp_buf[PATH_MAX];
        s_buffer(tmp_buf, PATH_MAX, tmp_filename);
        if (unlink(tmp_buf) != 0) {
            prt("unlink failed for %.*s: %s\n", len(tmp_filename), tmp_filename.buf, strerror(errno));
            flush_exit(1);
        }
    }
}

/* #gpt_message */
json gpt_message(span role, span message) {
    json resp = json_o();
    json_o_extend(&resp, S("role"), json_s(role));
    json_o_extend(&resp, S("content"), json_s(message));
    return resp;
}



/* #send_to_llm */
void send_to_llm(span prompt, llm_message_handler cb) {
    if (span_eq(state->model, S("clipboard"))) {
        send_to_clipboard(prompt);
        return;
    }

    //prt_cmp();
    json messages = json_a();
    //int system_index = find_block(S("#systemprompt"));
    //int system_index = block_by_id(S("systemprompt"));
    //if (system_index != -1) {
        //json_a_extend(&messages, gpt_message(S("system"), state->blocks.a[system_index]));
    //}

    if (!empty(state->bootstrapprompt)) {
        json_a_extend(&messages, gpt_message(S("user"), state->bootstrapprompt));
        json_a_extend(&messages, gpt_message(S("assistant"), S("OK")));
    }

    json_a_extend(&messages, gpt_message(S("user"), prompt));
    //prt_pop();

    call_llm(state->model, messages, cb);
}



/* #llm_stdout_handler */
void llm_stdout_handler(span response) {
    prt("%.*s", len(response), response.buf);
    flush();
}

/* #handle_llm */
void handle_llm(void) {
    span input = read_stdin_into_cmp();
    if (len(input) == 0) {
        prt("No input provided\n");
        flush_err();
        exit(1);
    }
    send_to_llm(input, simple_message_handler(llm_stdout_handler));
}

/* #handle_openai_response */
void handle_openai_response(span response, llm_message_handler cb) {
    json res_json = json_parse(response);
    if (json_is_null(res_json)) {
        prt("Failed to parse JSON: %.*s", len(response), response.buf);
        flush();
        exit(1);
    }

    json choices = json_key(S("choices"), res_json);
    if (json_is_null(choices)) {
        prt("Missing 'choices' in response: %.*s", len(response), response.buf);
        flush();
        exit(1);
    }

    json first_choice = json_index(0, choices);
    if (json_is_null(first_choice)) {
        prt("No choices available: %.*s", len(response), response.buf);
        flush();
        exit(1);
    }

    json message = json_key(S("message"), first_choice);
    if (json_is_null(message)) {
        prt("Missing 'message' in first choice: %.*s", len(response), response.buf);
        flush();
        exit(1);
    }

    json content = json_key(S("content"), message);
    if (json_is_null(content)) {
        prt("Missing 'content' in message: %.*s", len(response), response.buf);
        flush();
        exit(1);
    }

    span result_span = json_un_s(content);
    //result_span = strip_markdown_codeblock(result_span);
    apply_partial(cb, result_span);
}


/* #handle_ollama_response */
void handle_ollama_response(span response, llm_message_handler cb) {
    json res = json_parse(response);
    if (json_is_null(res)) {
        prt("Error parsing JSON: %.*s", len(response), response.buf);
        flush();
        exit(1);
    }

    json message = json_key(S("message"), res);
    if (json_is_null(message)) {
        prt("Missing 'message': %.*s", len(response), response.buf);
        flush();
        exit(1);
    }

    json content = json_key(S("content"), message);
    if (json_is_null(content)) {
        prt("Missing 'content': %.*s", len(response), response.buf);
        flush();
        exit(1);
    }

    span content_span = json_un_s(content);
    //span stripped_content = strip_markdown_codeblock(content_span);
    apply_partial(cb, content_span);
}


/* #handle_anthropic_response */
void handle_anthropic_response(span response, llm_message_handler cb) {
    json response_json = json_parse(response);
    if (json_is_null(response_json)) {
        prt("Failed to parse response: %s", s(response));
        flush_exit(1);
    }

    json content_json = json_key(S("content"), response_json);
    if (json_is_null(content_json)) {
        prt("Failed to index 'content' in response: %s", s(response));
        flush_exit(1);
    }

    json text_json = json_index(0, content_json);
    if (json_is_null(text_json)) {
        prt("Failed to index '0' in 'content': %s", s(response));
        flush_exit(1);
    }

    json text_value_json = json_key(S("text"), text_json);
    if (json_is_null(text_value_json)) {
        prt("Failed to index 'text' in 'content': %s", s(response));
        flush_exit(1);
    }

    span text_value = json_un_s(text_value_json);
    apply_partial(cb, text_value);
}


/* #block_comment_part */
int find_comment_end_c(span block) {
    for (int i = 0; i < len(block) - 1; i++) {
        if (block.buf[i] == '*' && block.buf[i+1] == '/') {
            return i + 2; // Include the length of "*/"
        }
    }
    return -1;
}

int find_comment_end_python(span block) {
    int count = 0;
    for (int i = 0; i < len(block) - 2; i++) {
        if (block.buf[i] == '"' && block.buf[i+1] == '"' && block.buf[i+2] == '"') {
            count++;
            if (count == 2) {
                return i + 3; // Include the length of "\"\"\""
            }
        }
    }
    return -1;
}

span block_comment_part(span block) {
    span language = language_for_block(block);
    int end_idx = -1;
    if (span_eq(language, S("Python"))) {
        end_idx = find_comment_end_python(block);
    } else if (span_eq(language, S("C")) || span_eq(language, S("JavaScript"))) {
        end_idx = find_comment_end_c(block);
    } else if (span_eq(language, S("Markdown"))) {
        return block; // Markdown blocks are considered full comments
    }

    if (end_idx != -1) {
        while (isspace(block.buf[end_idx]) && end_idx < len(block)) {
            end_idx++; // Skip whitespace after the comment end
        }
        return first_n(block, end_idx);
    }

    return nullspan(); // No comment part found or not applicable
}



/* #block_comment_part_excl */
span block_comment_part_excl(span block) {
    span language = language_for_block(block);
    span comment = block_comment_part(block);
    comment = trim(comment);

    if (span_eq(language, S("C")) || span_eq(language, S("JavaScript"))) {
        if (starts_with(comment, S("/*"))) {
            advance(&comment, 2);
        }
        if (ends_with(comment, S("*/"))) {
            shorten(&comment, 2);
        }
    } else if (span_eq(language, S("Python"))) {
        if (starts_with(comment, S("\"\"\""))) {
            advance(&comment, 3);
        }
        if (ends_with(comment, S("\"\"\""))) {
            shorten(&comment, 3);
        }
    }

    return comment;
}



/* #block_code_part */
span block_code_part(span block) {
    span comment = block_comment_part(block);
    block.buf = comment.end;
    return block;
}


/* #get_palette */
/* #apply_prompt */
void apply_prompt(span prompt_name) {
    if (span_eq(prompt_name, S("NL -> PL rewrite"))) {
        nl2pl_rewrite();
    } else if (span_eq(prompt_name, S("NL <- PL rewrite"))) {
        pl2nl_rewrite();
    } else if (span_eq(prompt_name, S("NL PL agreement"))) {
        agreement();
    } else if (span_eq(prompt_name, S("NL PL agreement to PL patch"))) {
        agreement_to_pl_diff();
    } else if (span_eq(prompt_name, S("NL PL agreement to NL patch"))) {
        agreement_to_nl_diff();
    // } else if (span_eq(prompt_name, S("block to one-line summary"))) {
        // summarize_block();
    } else if (span_eq(prompt_name, S("NL description to step-by-step algorithm"))) {
        nl2algo();
    } else {
        prt("Unknown prompt: %.*s\n", len(prompt_name), prompt_name.buf);
        flush();
        getch();
    }
}


/* #nl2plrewrite */
/* #prompt_template_design */
/* #hc_prompts */
// Hardcoded prompt templates
span pt_nl2pl_rewrite() { return S("```{langtag}\n{context}\n```\n\n(above: references)\n---\n(below: current task)\n\n```{langtag}\n{comment}\n```\n\nWrite the code only for the current task. Reply only with a code block beginning with \"```{langtag}\". Do not include comments.\n"); }
span pt_agreement() { return S("TODO"); }
span pt_agreement_to_nl_diff() { return S("TODO"); }
span pt_agreement_to_pl_diff() { return S("TODO"); }
span pt_pl2nl_rewrite() { return S("TODO"); }
span pt_nl2algo() { return S("TODO"); }
span pt_summarize_block() { return S("TODO"); }



/* #get_prompt_template */
span get_prompt_template(span name) {
    if (span_eq(name, S("agreement_to_nl_diff"))) return pt_agreement_to_nl_diff();
    if (span_eq(name, S("agreement_to_pl_diff"))) return pt_agreement_to_pl_diff();
    if (span_eq(name, S("agreement"))) return pt_agreement();
    if (span_eq(name, S("nl2algo"))) return pt_nl2algo();
    if (span_eq(name, S("pl2nl_rewrite"))) return pt_pl2nl_rewrite();
    if (span_eq(name, S("nl2pl_rewrite"))) return pt_nl2pl_rewrite();
    if (span_eq(name, S("summarize_block"))) return pt_summarize_block();

    prt("Unknown prompt template: %.*s\nPress any key to continue...", len(name), name.buf);
    flush_err();
    getch();
    return nullspan();
}


/* #agreement */
/*output:
...text from the LLM...
*/

void agreement() {
}


/* #agreement_to_nl_diff */
void agreement_to_nl_diff() {
    span prompt_template = get_prompt_template(S("agreement_to_nl_diff"));
    spans template_vars = current_block_template_vars();

    output_template_var(&template_vars, S("agreement"));

    span expanded_template = expand_template(prompt_template, template_vars);
    wrs(expanded_template);
    flush();
    getch();

    //send_to_llm(expanded_template, proposed_diff_SAV);
    llm_message_handler cb = make_output_saver(S("agreement_to_nl_diff"));
    send_to_llm(expanded_template, cb);
}

/* #output_template_var */
void output_template_var(spans* ctx, span human_name) {
    span output = lookup_output(human_name);

    if (!empty(output)) {
        spans_push(ctx, human_name);
        spans_push(ctx, output);
    }
}


/* #lookup_output */
span lookup_output(span output_of) {
    assert(state->curr_block_idx != -1);
    checksum current_cksum = selected_checksum(state->blocks.a[state->curr_block_idx]);
    span current_cksum_span = prs_checksum(current_cksum);
    get_outputs();
    for (int i = (int)state->outputs_filenames.n - 1; i >= 0; i--) {
        span basename = state->outputs_filenames.a[i];
        spans headers = read_output_headers(basename);
        if (span_eq(assoc_spans_lookup(headers, S("checksum")), current_cksum_span) &&
            span_eq(assoc_spans_lookup(headers, S("output_of")), output_of)) {
            return read_output_body(basename);
        }
    }
    return nullspan();
}


/* #expand_template */
span expand_template(span template, spans vars) {
    out_sav sav = out2cmp();
    span ret = {.buf = cmp.end, .end = cmp.end};
    spans parts = parse_template(template);
    for (size_t i = 0; i < parts.n; i++) {
        if (i % 2 == 0) {
            print_template_literal(parts.a[i]);
        } else {
            eval_template_variable(parts.a[i], vars);
        }
    }
    ret.end = cmp.end;
    out_rst(sav);
    return ret;
}


/* #print_template_literal */
void print_template_literal(span input) {
    while (!empty(input)) {
        if (len(input) > 1 && input.buf[0] == '\\' && (input.buf[1] == '\\' || input.buf[1] == '{')) {
            w_char(input.buf[1]);
            advance(&input, 2);
        } else {
            w_char(input.buf[0]);
            advance1(&input);
        }
    }
}



/* #gcb */
/* #current_block_template_vars */
spans current_block_template_vars() {
    spans vars = spans_alloc(8);
    
    span lang = current_block_language();
    span langtag = nullspan();
    
    if (span_eq(lang, S("C"))) langtag = S("c");
    else if (span_eq(lang, S("Python"))) langtag = S("py");
    else if (span_eq(lang, S("JavaScript"))) langtag = S("js");
    else if (span_eq(lang, S("Markdown"))) langtag = S("md");
    
    spans_push(&vars, S("langtag"));
    spans_push(&vars, langtag);

    span comment = block_comment_part(state->blocks.a[state->curr_block_idx]);
    spans_push(&vars, S("context"));
    spans_push(&vars, expand_refs_2(comment, S("context")));
    
    spans_push(&vars, S("comment"));
    spans_push(&vars, expand_refs_2(comment, S("body")));

    spans_push(&vars, S("code"));
    spans_push(&vars, block_code_part(state->blocks.a[state->curr_block_idx]));

    return vars;
}


/* #eval_template_variable */
void eval_template_variable(span var_name, spans vars) {
    var_name = trim(var_name);
    int found = 0;

    assert(vars.n % 2 == 0);

    for(size_t i = 0; i < vars.n; i += 2) {
        if (span_eq(vars.a[i], var_name)) {
            found = 1;
            if (empty(vars.a[i + 1]) && vars.a[i + 1].buf == (u8*)0) {
                prt("Error: Variable '%.*s' has null span value.\n", len(var_name), var_name.buf);
                prt("Press any key to continue...\n");
                flush_err();
                getch();
                return;
            }
            wrs(vars.a[i + 1]);
            return;
        }
    }

    if (!found) {
        prt("Error: Variable '%.*s' not found.\n", len(var_name), var_name.buf);
        prt("Press any key to continue...\n");
        flush_err();
        getch();
    }
}



/* #template_language_design */
/* #parse_template */
spans parse_template(span input) {
    spans result = spans_alloc(10);
    span current_span = { .buf = input.buf, .end = input.buf };
    int in_syntax = 0;

    while (current_span.end < input.end) {
        if (!in_syntax) {
            if (*current_span.end == '\\') {
                current_span.end++;
                if (current_span.end < input.end && (*current_span.end == '{' || *current_span.end == '\\')) {
                    current_span.end++;
                }
            } else if (*current_span.end == '{') {
                span literal_span = { .buf = current_span.buf, .end = current_span.end };
                spans_push(&result, literal_span);
                in_syntax = 1;
                current_span.buf = ++current_span.end;
            } else {
                current_span.end++;
            }
        } else {
            if (*current_span.end == '}') {
                span syntax_span = { .buf = current_span.buf, .end = current_span.end };
                spans_push(&result, syntax_span);
                in_syntax = 0;
                current_span.buf = ++current_span.end;
            } else {
                current_span.end++;
            }
        }
    }

    if (current_span.buf < current_span.end) {
        spans_push(&result, current_span);
    }

    return result;
}


/* #print_block */
void print_comment(int index) {
    if (index < 0 || index >= state->blocks.n) return;
    span block = state->blocks.a[index];
    span comment_part = block_comment_part(block);
    wrs(comment_part);
    terpri();
}

void print_code(int index) {
    if (index < 0 || index >= state->blocks.n) return;
    span block = state->blocks.a[index];
    span comment_part = block_comment_part(block);
    if (comment_part.end == NULL) {
        prt("Warning: block %d has no comment terminator (malformed block)\n", index + 1);
        return;
    }
    span code_part = block;
    code_part.buf = comment_part.end;
    wrs(code_part);
    terpri();
}

void print_block(int index) {
    if (index < 0 || index >= state->blocks.n) return;
    span block = state->blocks.a[index];
    wrs(block);
    terpri();
}

int count_blocks() {
    return state->blocks.n;
}


/* #print_block_ofra */
void print_block_ofra(int index) {
    if (index < 0 || index >= state->blocks.n) return;
    span block = state->blocks.a[index];
    
    // Get block ID (first one if multiple)
    spans ids = ids_for_block(block);
    if (ids.n > 0) {
        prt("ID: %s\n", s(ids.a[0]));
    } else {
        prt("ID: %d\n", index + 1);  // Use 1-based index as fallback
    }
    
    // Compute and print checksum
    checksum cs = selected_checksum(block);
    prt("Checksum: %016llX\n", (unsigned long long)cs.__u);
    
    // Get and print file path
    int file_idx = file_for_block(block);
    prt("File: %s\n", s(state->files.a[file_idx].path));
    
    // Find previous block in same file and get its ID
    if (index > 0) {
        span prev_block = state->blocks.a[index - 1];
        int prev_file_idx = file_for_block(prev_block);
        if (prev_file_idx == file_idx) {
            spans prev_ids = ids_for_block(prev_block);
            if (prev_ids.n > 0) {
                prt("After: %s\n", s(prev_ids.a[0]));
            } else {
                prt("After: %d\n", index);  // 1-based index of previous
            }
        } else {
            prt("After:\n");  // First block in file
        }
    } else {
        prt("After:\n");  // First block overall
    }
    
    // Determine type from language
    span lang = state->files.a[file_idx].language;
    if (span_eq(lang, S("C")) || span_eq(lang, S("c"))) {
        prt("Type: C\n");
    } else if (span_eq(lang, S("Python")) || span_eq(lang, S("python"))) {
        prt("Type: Python\n");
    } else {
        prt("Type: none\n");
    }
    
    // Blank line separating headers from body
    prt("\n");
    
    // Print block content
    wrs(block);
    terpri();
}
/* #content_index */
void content_index(span search_text) {
    int first_match = 1;
    for (int i = 0; i < state->blocks.n; i++) {
        if (contains(state->blocks.a[i], search_text)) {
            if (!first_match) {
                prt(" ");
            }
            first_match = 0;
            prt("%d", i + 1);  // one-based index
        }
    }
    prt("\n");
}

/* #block_from_arg */
int block_from_arg(char* arg) {
    span sarg = S(arg);
    if (!empty(sarg) && isdigit(*sarg.buf)) {
        int idx = parse_int(sarg);
        return idx > 0 ? idx - 1 : -1;
    }
    if (!empty(sarg) && *sarg.buf == '#')
        advance1(&sarg);
    return block_by_id(sarg);
}

/* #block_id_arg */
int block_id_arg(span block_id_or_int) {
    if (!empty(block_id_or_int) && isdigit(*block_id_or_int.buf)) {
        int n = parse_int(block_id_or_int);
        return n > 0 ? n - 1 : -1;
    } else {
        if (!empty(block_id_or_int) && *block_id_or_int.buf == '#')
            advance1(&block_id_or_int);
        return block_by_id(block_id_or_int);
    }
}


/* #handle_run */
void handle_run(char* run_block_id) {
    int block_idx = block_by_id(S(run_block_id));
    if (block_idx == -1) {
        prt("Error: Block not found: %s\n", run_block_id);
        flush_exit(1);
    }
    span block = state->blocks.a[block_idx];
    span comment_part = block_comment_part(block);
    span code_part = block;
    code_part.buf = comment_part.end;
    if (empty(code_part)) {
        prt("Error: Block %s has no code part\n", run_block_id);
        flush_exit(1);
    }
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/cmpr_run_%d.sh", getpid());
    write_to_file_span(code_part, S(tmp_path), 1);
    char chmod_cmd[512];
    snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s", tmp_path);
    int chmod_result = system(chmod_cmd);
    if (chmod_result != 0) {
        prt("Error: failed to chmod %s\n", tmp_path);
        flush_exit(WEXITSTATUS(chmod_result));
    }
    int exit_code = system(tmp_path);
    unlink(tmp_path);
    exit(WEXITSTATUS(exit_code));
}

/* #handle_agent_run */
void handle_agent_run(char* agent_name, char* mode) {
    // Validate and normalize mode
    char mode_lower[16];
    if (strcasecmp(mode, "CHECK") == 0) {
        strcpy(mode_lower, "check");
    } else if (strcasecmp(mode, "FIX") == 0) {
        strcpy(mode_lower, "fix");
    } else {
        prt("Error: Invalid mode '%s'. Must be CHECK or FIX.\n", mode);
        flush_exit(1);
    }
    
    // Construct block ID: #<agent_name>_<mode>_impl
    char block_id[256];
    snprintf(block_id, sizeof(block_id), "%s_%s_impl", agent_name, mode_lower);
    
    // Find the block
    int block_idx = block_by_id(S(block_id));
    if (block_idx == -1) {
        prt("Error: Agent implementation block not found: %s\n", block_id);
        prt("Expected block ID format: #<agent_name>_<mode>_impl\n");
        flush_exit(1);
    }
    
    // Extract code part
    span block = state->blocks.a[block_idx];
    span comment_part = block_comment_part(block);
    span code_part = block;
    code_part.buf = comment_part.end;
    
    if (empty(code_part)) {
        prt("Error: Block %s has no code part\n", block_id);
        flush_exit(1);
    }
    
    // Write to temporary file
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/cmpr_agent_run_%d.sh", getpid());
    write_to_file_span(code_part, S(tmp_path), 1);
    
    // Make executable
    char chmod_cmd[512];
    snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s", tmp_path);
    int chmod_result = system(chmod_cmd);
    if (chmod_result != 0) {
        prt("Error: failed to chmod %s\n", tmp_path);
        flush_exit(WEXITSTATUS(chmod_result));
    }
    
    // Execute the agent
    int exit_code = system(tmp_path);
    
    // Clean up
    unlink(tmp_path);
    
    // Exit with agent's exit code
    exit(WEXITSTATUS(exit_code));
}


/* #handle_agents */
extern char *available_agents[];

void handle_agents() {
    prt("%-24s %-12s %-16s\n", "AGENT", "INSTALLED", "RUNNING");
    prt("%-24s %-12s %-16s\n", "------------------------", "------------", "----------------");
    
    int count = 0;
    
    // Track which agents we've listed (to avoid duplicates)
    char listed[256][64];
    int listed_count = 0;
    
    // First: embedded agents from available_agents[]
    for (int i = 0; available_agents[i] != NULL; i++) {
        char *name = available_agents[i];
        
        // Track as listed
        if (listed_count < 256) {
            strncpy(listed[listed_count], name, 63);
            listed[listed_count][63] = 0;
            listed_count++;
        }
        
        char path[512];
        snprintf(path, sizeof(path), ".cmpr/agents/%s", name);
        int installed = (access(path, X_OK) == 0);
        
        char running_str[32] = "-";
        if (installed) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "pgrep -f '[.]cmpr/agents/%s' 2>/dev/null | head -1", name);
            FILE *fp = popen(cmd, "r");
            if (fp) {
                int pid = 0;
                if (fscanf(fp, "%d", &pid) == 1 && pid > 0) {
                    snprintf(running_str, sizeof(running_str), "yes (%d)", pid);
                } else {
                    snprintf(running_str, sizeof(running_str), "no");
                }
                pclose(fp);
            }
        }
        
        prt("%-24s %-12s %-16s\n", name, installed ? "yes" : "no", running_str);
        count++;
    }
    
    // Second: scan .cmpr/agents/ for custom agents
    DIR *dir = opendir(".cmpr/agents");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (entry->d_type == DT_DIR) continue;
            
            // Check if already listed
            int already_listed = 0;
            for (int i = 0; i < listed_count; i++) {
                if (strcmp(listed[i], entry->d_name) == 0) {
                    already_listed = 1;
                    break;
                }
            }
            if (already_listed) continue;
            
            // Check if executable
            char path[512];
            snprintf(path, sizeof(path), ".cmpr/agents/%s", entry->d_name);
            if (access(path, X_OK) != 0) continue;
            
            // Check if running
            char running_str[32] = "no";
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "pgrep -f '[.]cmpr/agents/%s' 2>/dev/null | head -1", entry->d_name);
            FILE *fp = popen(cmd, "r");
            if (fp) {
                int pid = 0;
                if (fscanf(fp, "%d", &pid) == 1 && pid > 0) {
                    snprintf(running_str, sizeof(running_str), "yes (%d)", pid);
                }
                pclose(fp);
            }
            
            prt("%-24s %-12s %-16s\n", entry->d_name, "yes", running_str);
            count++;
        }
        closedir(dir);
    }
    
    prt("\nTotal: %d agents\n", count);
}


/* #handle_install_agent */
void handle_install_agent(char *agent_name) {
    span script = get_agent_script(S(agent_name));
    if (script.buf == 0 || script.buf == script.end) {
        prt("Unknown agent: %s\n", agent_name);
        prt("Available agents: claude\n");
        flush_exit(1);
    }
    
    // Create .cmpr/agents/ directory
    system("mkdir -p .cmpr/agents");
    
    // Build path: .cmpr/agents/<agent_name>
    char path[256];
    snprintf(path, sizeof(path), ".cmpr/agents/%s", agent_name);
    
    // Write script to file
    FILE *f = fopen(path, "w");
    if (!f) {
        prt("Error: Cannot write to %s\n", path);
        flush_exit(1);
    }
    fwrite(script.buf, 1, script.end - script.buf, f);
    fclose(f);
    
    // Make executable
    chmod(path, 0755);
    
    prt("Installed agent: %s\n", path);
    prt("Run with: %s &\n", path);
}
/* #script_Tcks_impl */
span script_Tcks(span s) {
    if (empty(s) || span_eq(S("Tcks"), s)) return S(
        "#!/bin/sh\n"
        "cmpr --event \"Tcks: $(cmpr --T | cmpr --checksum)\" --strength 255\n"
    ); else return nullspan();
}


/* #script_blockid_impl */
span script_blockid(span s) {
    if (empty(s) || span_eq(S("blockid"), s)) return S(
        "#!/bin/sh\n"
        "cmpr --T|awk '/\"The blockid is: (.*)\" 255[.]/{print $4}'|tr -d '\".'\n"
    ); else return nullspan();
}

/* #script_checksum_impl */
span script_checksum(span s) {
    if (empty(s) || span_eq(S("checksum"), s)) return S(
        "#!/bin/sh\n"
        "# \"The blkcks is the block checksum.\"\n"
        "# \"The blkcks is: \"\n"
        "\n"
        "BID=$(scripts/blockid)\n"
        "\n"
        "# Recall previous state for this BID (loads old blkcks if any)\n"
        "cmpr --recall 2>/dev/null\n"
        "\n"
        "# Compute and add fresh checksum (may create surprise-high if changed)\n"
        "cmpr --event \"The blkcks is the block checksum.\" --strength 255\n"
        "cmpr --event \"The blkcks is: $(cmpr --print-block \"$BID\" | cmpr --checksum)\" --strength 255\n"
    ); else return nullspan();
}

/* #script_patterns_impl */
span script_patterns(span s) {
    if (empty(s) || span_eq(S("patterns"), s)) return S(
"#!/bin/bash\n"
"# Pattern processing for event system\n"
"# Called by cmpr --event with CMPR_EVENT, CMPR_STRENGTH, CMPR_PATTERN_DEPTH set\n"
"\n"
"t_debug() { [ -f .cmpr/T-debug ] && echo \"[T-debug] patterns: $*\" >&2; }\n"
"\n"
"lsi_should_increment() {\n"
"    local n=$1\n"
"    [ \"$n\" -eq 0 ] && return 0\n"
"    local random_bits=$(od -An -tu4 -N4 /dev/urandom | tr -d ' ')\n"
"    local mask=$(( (1 << n) - 1 ))\n"
"    local masked=$(( random_bits & mask ))\n"
"    [ \"$masked\" -eq \"$mask\" ]\n"
"}\n"
"\n"
"[ -z \"$CMPR_EVENT\" ] && exit 0\n"
"STRENGTH=\"${CMPR_STRENGTH:-0}\"\n"
"SN_LINE=\"\\\"$CMPR_EVENT\\\" $STRENGTH.\"\n"
"\n"
"# Induced-single: scan .cmpr/induced-single/N/ directories for exact match\n"
"for dir in .cmpr/induced-single/*/; do\n"
"    [ -d \"$dir\" ] || continue\n"
"    [ -f \"${dir}name\" ] && [ -x \"${dir}script\" ] || continue\n"
"    want_name=$(cat \"${dir}name\")\n"
"    if [ \"$CMPR_EVENT\" = \"$want_name\" ]; then\n"
"        t_debug \"induced-single ${dir} matches\"\n"
"        \"${dir}script\"\n"
"    fi\n"
"done\n"
"\n"
"matched_es=\"\"\n"
"for filter in .cmpr/es/*; do\n"
"    [ -x \"$filter\" ] || continue\n"
"    es_name=$(basename \"$filter\")\n"
"    if echo \"$SN_LINE\" | \"$filter\" | grep -q .; then\n"
"        matched_es=\"$matched_es $es_name\"\n"
"    fi\n"
"done\n"
"[ -z \"$matched_es\" ] && exit 0\n"
"\n"
"for es in $matched_es; do\n"
"    if [ -x \".cmpr/induced/$es\" ]; then\n"
"        t_debug \"induced $es -> .cmpr/induced/$es\"\n"
"        \".cmpr/induced/$es\"\n"
"    fi\n"
"done\n"
"\n"
"for es in $matched_es; do\n"
"    if [ -x \".cmpr/surprise-high/$es\" ]; then\n"
"        other_events=$(cmpr --T | \".cmpr/es/$es\" | grep ' 255\\.$')\n"
"        count=$(echo \"$other_events\" | grep -v \"^\\\"$CMPR_EVENT\\\" \" | grep -c . || echo 0)\n"
"        if [ \"$count\" -gt 0 ]; then\n"
"            t_debug \"surprise-high $es -> .cmpr/surprise-high/$es\"\n"
"            \".cmpr/surprise-high/$es\"\n"
"        fi\n"
"    fi\n"
"done\n"
"\n"
"for es in $matched_es; do\n"
"    shopt -s nullglob\n"
"    for pattern_file in .cmpr/patterns/*-*; do\n"
"        [ -f \"$pattern_file\" ] || continue\n"
"        fname=$(basename \"$pattern_file\")\n"
"        es1=\"${fname%%-*}\"\n"
"        es2=\"${fname##*-}\"\n"
"        other_es=\"\" search_col=\"\"\n"
"        if [ \"$es\" = \"$es1\" ]; then other_es=\"$es2\"; search_col=1\n"
"        elif [ \"$es\" = \"$es2\" ]; then other_es=\"$es1\"; search_col=2\n"
"        else continue; fi\n"
"\n"
"        if [ \"$STRENGTH\" = \"255\" ]; then\n"
"            other_filter=\".cmpr/es/$other_es\"\n"
"            if [ -x \"$other_filter\" ]; then\n"
"                other_events=$(cmpr --T | \"$other_filter\" | grep ' 255\\.$')\n"
"                if [ -n \"$other_events\" ]; then\n"
"                    echo \"$other_events\" | while IFS= read -r other_line; do\n"
"                        [ -z \"$other_line\" ] && continue\n"
"                        other_event=$(echo \"$other_line\" | sed 's/^\"\\(.*\\)\" [0-9]*\\.$/\\1/')\n"
"                        if [ \"$search_col\" = \"1\" ]; then\n"
"                            pair_prefix=\"\\\"$CMPR_EVENT\\\" \\\"$other_event\\\"\"\n"
"                        else\n"
"                            pair_prefix=\"\\\"$other_event\\\" \\\"$CMPR_EVENT\\\"\"\n"
"                        fi\n"
"                        current_line=$(grep -F \"$pair_prefix\" \"$pattern_file\" 2>/dev/null | head -1)\n"
"                        if [ -n \"$current_line\" ]; then\n"
"                            current_count=$(echo \"$current_line\" | sed -n 's/.* \\([0-9]*\\)\\.$/\\1/p')\n"
"                            [ -z \"$current_count\" ] && current_count=0\n"
"                        else current_count=0; fi\n"
"                        if lsi_should_increment \"$current_count\"; then\n"
"                            new_count=$((current_count + 1))\n"
"                            new_line=\"$pair_prefix $new_count.\"\n"
"                            if [ -n \"$current_line\" ]; then\n"
"                                grep -vF \"$pair_prefix\" \"$pattern_file\" > \"$pattern_file.tmp\"\n"
"                                echo \"$new_line\" >> \"$pattern_file.tmp\"\n"
"                                mv \"$pattern_file.tmp\" \"$pattern_file\"\n"
"                            else echo \"$new_line\" >> \"$pattern_file\"; fi\n"
"                        fi\n"
"                    done\n"
"                fi\n"
"            fi\n"
"        fi\n"
"\n"
"        if [ -f \"$pattern_file\" ]; then\n"
"            while IFS= read -r line; do\n"
"                [ -z \"$line\" ] && continue\n"
"                if [ \"$search_col\" = \"1\" ]; then\n"
"                    match_event=$(echo \"$line\" | sed -n 's/^\"\\([^\"]*\\)\" \"\\([^\"]*\\)\" \\([0-9]*\\)\\.$/\\1/p')\n"
"                    emit_event=$(echo \"$line\" | sed -n 's/^\"\\([^\"]*\\)\" \"\\([^\"]*\\)\" \\([0-9]*\\)\\.$/\\2/p')\n"
"                    emit_strength=$(echo \"$line\" | sed -n 's/^\"\\([^\"]*\\)\" \"\\([^\"]*\\)\" \\([0-9]*\\)\\.$/\\3/p')\n"
"                else\n"
"                    match_event=$(echo \"$line\" | sed -n 's/^\"\\([^\"]*\\)\" \"\\([^\"]*\\)\" \\([0-9]*\\)\\.$/\\2/p')\n"
"                    emit_event=$(echo \"$line\" | sed -n 's/^\"\\([^\"]*\\)\" \"\\([^\"]*\\)\" \\([0-9]*\\)\\.$/\\1/p')\n"
"                    emit_strength=$(echo \"$line\" | sed -n 's/^\"\\([^\"]*\\)\" \"\\([^\"]*\\)\" \\([0-9]*\\)\\.$/\\3/p')\n"
"                fi\n"
"                if [ \"$match_event\" = \"$CMPR_EVENT\" ] && [ -n \"$emit_event\" ] && [ -n \"$emit_strength\" ]; then\n"
"                    current_strength=$(cmpr --query \"$emit_event\" 2>/dev/null || echo 0)\n"
"                    if [ \"$emit_strength\" -gt \"$current_strength\" ]; then\n"
"                        t_debug \"populate $fname: \\\"$emit_event\\\" $emit_strength.\"\n"
"                        cmpr --event \"$emit_event\" --strength \"$emit_strength\"\n"
"                    fi\n"
"                fi\n"
"            done < \"$pattern_file\"\n"
"        fi\n"
"    done\n"
"done\n"
    ); else return nullspan();
}

/* #get_script */
span get_script(span name) {
  span res = script_Tcks(name);
  if (!empty(res)) return res;
  res = script_blockid(name);
  if (!empty(res)) return res;
  res = script_checksum(name);
  if (!empty(res)) return res;
  res = script_patterns(name);
  if (!empty(res)) return res;
  return nullspan();
}
/* #handle_install_script */
void handle_install_script(char *script_name) {
  span name = S(script_name);
  span script = get_script(name);
  if (empty(script)) {
    prt("Unknown script: %s\nAvailable scripts:\n", script_name);
    // List available scripts
    span s = script_Tcks(nullspan());
    if (!empty(s)) prt("  Tcks\n");
    flush_exit(1);
  }

  mkdir(".cmpr", 0700);
  mkdir(".cmpr/scripts", 0700);

  char path_buf[PATH_MAX];
  snprintf(path_buf, sizeof(path_buf), ".cmpr/scripts/%s", script_name);
  write_to_file_span(script, S(path_buf), 1);
  chmod(path_buf, 0755);
  prt("Installed script '%s' to %s\n", script_name, path_buf);
  flush();
}

/* #help_text_summary_impl */
span help_text_summary(span s) {
  if (empty(s) || span_eq(s, S("help_text_summary")))
    return S(
"cmpr code swiss army knife\n"
"\n"
"Usage: cmpr [--conf <filepath>] [--print-conf|--help|--init|--version|--status] [(--print-block [--ofra]|--print-code|--print-comment|--expand-block) <id>] [--rewritepl <id>] [--prompt <id>] [--llm] [--content-index <search>] [--grep <pattern>] [--count-blocks|--files-blocks|--print-all|--inbox] [--after <id>] [--before <ts>] [(--replace|--replace-comment|--replace-code|--replace-current) <id>] [--run <block_id>] [--build] [--agents] [--install-agent <name>] [--install-script <name>] [--checksum] [--find-deleted] [--T0] [--event <string> --strength <value>] [--event-stdin --strength <value>] [--event-file <path> --strength <value>] [--query <string>] [--memorize] [--recall] [--recall-first] [--T] [--trace] [--work [event]] [--event-spaces|--es] [--P|--pattern] [--E] [--induced <es>] [--induced-single <event>] [--lpp <es1> <es2>] [--wants [--blocks]] [--wants-status] [--agents-wants] [--wants-dashboard] [--event-report] [--export-docs] [--history [#blockid] [--log-gap [factor]] [--limit N]] [FILE|-]\n"
"\n"
"For help on available topics: cmpr --help topics\n"
"Every CLI flag can also be used after --help to get a description of that flag or usage examples: cmpr --help --grep\n"
);
  else
    return nullspan();
}







/* #help_text_topics_impl */
span help_text_topics(span s) {
  if (empty(s) || span_eq(S("help_text_topics"), s))
    return S(
"topics\n"
"basic\n"
"blocks\n"
"inbox\n"
"editing\n"
"search\n"
"nl2pl\n"
"events\n"
"agents\n"
"reports\n"
"wants\n"
"claude-setup\n"
);
  else
    return nullspan();
}

/* #help_text_basic_impl */
span help_text_basic(span s) {
  if (empty(s) || span_eq(S("help_text_basic"), s))
    return S(
      "cmpr basics\n"
      "\n"
      "All cmpr state is maintained in .cmpr in your project directory (like .git), also set up via `cmpr --init` in a new project.\n"
      "In .cmpr/conf is the \"project manifest\" or list of files that cmpr will know about.\n"
      "You can add all the files in your project or just start with one to try it.\n"
      "Recommended starter example .cmpr/conf:\n"
      "\n"
      "cmprdir: .cmpr/\n"
      "buildcmd: make\n"
      "[ ... other config ... ]\n"
      "model: gpt-4.1\n"
      "\n"
      "language: C\n"
      "file: .cmpr/conf\n"
      "file: my-project-code\n"
      "\n"
      "Note that \"language: C\" only refers to the cmpr \"blockizing style\".\n"
      "You should use \"C\" regardless of the actual programming language in your project, unless it is Python.\n"
      "\n"
      "Only use \"language: Python\" for Python files, and \"language: none\" for files that you don't want to be blockized at all.\n"
      "Each language line applies to all file lines up to the next language line.\n"
      "\n"
      "Replace buildcmd with your actual build command.\n"
      "Use 'B' in the TUI or `cmpr --build` from the command line to run it.\n"
      "\n"
      "cmpr can be used via TUI, reached by running `cmpr` with no arguments (or with a single filename).\n"
      "It can be used from the shell via CLI, see cmpr --help for basic usage.\n"
      "\n"
      "cmpr organizes your code into blocks, which are marked by actual block comments in your source code.\n"
      "Next, read `cmpr --help blocks` for the basics of blocks and cmpr source-code access.\n"
      "\n"
      "Basic Commands\n"
      "==============\n"
      "\n"
      "--help [topic]\n"
      "  Display help information.\n"
      "  cmpr --help         # basic usage\n"
      "  cmpr --help topics  # list of topics\n"
      "  cmpr --help --flag  # help for specific flag\n"
      "\n"
      "--version\n"
      "  Version information.\n"
      "\n"
      "--init\n"
      "  Initialize .cmpr/ directory structure in current directory.\n"
      "  Use when setting up cmpr in a new project, then manually edit .cmpr/conf to add your source files.\n"
      "\n"
      "--conf <filepath>\n"
      "  Use alternate configuration file.\n"
      "  Example: cmpr --conf /path/to/custom.conf --print-block '#root'\n"
      "\n"
      "--print-conf\n"
      "  Display current configuration.\n"
      "\n"
      "--status\n"
      "  Print quick project health dashboard.\n"
      "  Shows: inbox items, total wants, total blocks, anonymous blocks.\n"
      "  Example: cmpr --status\n"
      "\n"
      "--build\n"
      "  Run the configured buildcmd and exit with the build command's exit status.\n"
      "  Example: cmpr --build\n"
      "\n"
      "--checksum\n"
      "  Compute checksum of input from stdin.\n"
      "  Useful for verifying content integrity.\n"
      "  Example: cat file.txt | cmpr --checksum\n"
      "\n"
      "--find-deleted\n"
      "  List all deleted blocks with their deletion timestamps.\n"
      "  Prints blocks in reverse order of when they were deleted (most recent first).\n"
      "  Each line shows: timestamp block_id (where timestamp is when the block was deleted).\n"
      "  Searches recent revision history for blocks that no longer exist.\n"
      "  Example: cmpr --find-deleted\n"
      );
  else return nullspan();
}



/* #help_text_blocks_impl */
span help_text_blocks(span s) {
  static char txt[] =
"In cmpr every source code file listed in the manifest is divided into blocks.\n"
"Blocks are contiguous: the concatenation of all the blocks in the file gives back the original file.\n"
"In the \"C\" blockizing style, which should be used in all programming languages that support C-style block comments, a block starts with a block comment that begins in column 0.\n"
"That means you can still use block comments inside a cmpr block by indenting your block comment start delimiter, if you really absolutely must.\n"
"In Python, triple-quotes are used, also starting in column 0.\n"
"\n"
"The definition of the blocks in a file is just the lines between any line that starts a block.\n"
"So blockizing is deterministic and very simple to understand.\n"
"\n"
"## Block ids\n"
"\n"
"A block has an id, like a hashtag: #example_block is an example of a block id.\n"
"The blockid goes right after the opening \"/*\" on the same line separated by a single space: \"/* #example\"\n"
"A block without an id is called an anonymous block, and generally the first thing you should do with these (e.g. if you're onboarding an existing codebase to cmpr for the first time) is give them names.\n"
"(Block numbers will change as blocks are added or removed from your codebase, so they make horrible identifiers.)\n"
"\n"
"## Root and INBOX blocks\n"
"\n"
"The basic block that every project should create first, and from which you should be able to find everything else in your codebase, is #root.\n"
"\n"
"If you're starting a new cmpr project, or adapting an existing codebase to use cmpr, we highly recommend creating two blocks:\n"
"\n"
"- #root should contain your project overview and will gradually be filled in with pointers to other blocks so that codebase navigation becomes easy.\n"
"- #INBOX and #END_INBOX define a region for staging new blocks. AI coding assistants can add experience reports or programmer feedback requests here using `cmpr --after '#INBOX'`. The inbox is a staging area; items graduate to their proper locations during review.\n"
"\n"
"Use `cmpr --inbox` to open a TUI filtered to just the inbox region. See `cmpr --help inbox` for details.\n"
"\n"
"(You can of course establish your own conventions (e.g. by documenting them in the root block) if you don't like these ones.)\n"
"\n"
"Basic Block Commands\n"
"====================\n"
"\n"
"--files-blocks\n"
"  Print structured list of all files and their blocks.\n"
"  Shows file names (in manifest order) and block IDs that each file contains.\n"
"\n"
"--print-block <id>\n"
"  Print complete block (both NL comment and PL code parts).\n"
"  <id> can be also be a one-based index in case of an anonymous block.\n"
"  Example: cmpr --print-block '#root'\n"
"\n"
"--print-comment <id>\n"
"  Print only the NL (natural language) comment part of a block.\n"
"  Example: cmpr --print-comment '#argtable'\n"
"\n"
"--print-code <id>\n"
"  Print only the PL (programming language) code part of a block.\n"
"  Example: cmpr --print-code '#agent_root' | bash\n"
"\n"
"Rarely-used commands:\n"
"\n"
"--count-blocks   # prints number of blocks\n"
"--print-all      # prints concatenation of all files\n"
"\n"
"(Pop /bin/sh quiz: how would you find the average size in bytes of the blocks in your project using cmpr --count-blocks, --print-all, and POSIX utilities only?)\n"
"\n"
"See also: block editing commands (--help editing) and nl2pl (--help nl2pl).\n"
;
  if (empty(s) || span_eq(S("help_text_blocks"), s)) return (span){(u8*)txt, (u8*)txt + sizeof(txt) - 1};
  else return nullspan();
}
/* #help_text_inbox_impl */
span help_text_inbox(span s) {
  static char txt[] =
"The Inbox\n"
"=========\n"
"\n"
"The inbox is a staging area for new blocks, typically used by AI coding assistants to deposit experience reports, experiments, or work-in-progress.\n"
"\n"
"## Structure\n"
"\n"
"The inbox is defined by two marker blocks:\n"
"\n"
"  /* #INBOX\n"
"  ... inbox description ...\n"
"  */\n"
"\n"
"  /* #some_new_block */\n"
"  /* #another_pending_item */\n"
"\n"
"  /* #END_INBOX\n"
"  Marker block. Everything between #INBOX and #END_INBOX is \"in the inbox\".\n"
"  */\n"
"\n"
"## Commands\n"
"\n"
"--inbox\n"
"  Open the TUI filtered to show only blocks in the inbox region.\n"
"  Navigation (j/k/g/G) is constrained to blocks between #INBOX and #END_INBOX.\n"
"  Press 'q' to exit back to the shell.\n"
"\n"
"## Workflow\n"
"\n"
"1. AI assistants add blocks using `cmpr --after '#INBOX'`\n"
"2. Run `cmpr --inbox` to review pending items\n"
"3. Move blocks to their proper locations (or delete if not needed)\n"
"4. The inbox shrinks as items graduate to permanent homes\n"
"\n"
"## Setup\n"
"\n"
"If your project doesn't have an inbox yet, create these two blocks:\n"
"\n"
"  /* #INBOX\n"
"  Staging area for new blocks.\n"
"  */\n"
"\n"
"  /* #END_INBOX */\n"
"\n"
"Place them in a file where you want inbox items to accumulate.\n"
;
  if (empty(s) || span_eq(S("help_text_inbox"), s)) return (span){(u8*)txt, (u8*)txt + sizeof(txt) - 1};
  else return nullspan();
}
/* #help_text_editing_impl */
span help_text_editing(span s) {
  if (empty(s) || span_eq(S("help_text_editing"), s)) return S(
"You can use the TUI `cmpr` or `cmpr foo.c` to navigate around your codebase by blocks (using j/k) and edit them using 'e', which will open up vim (or your configured $EDITOR) on a temporary file containing that block, and then replace it back into the file after you save and successfully (status code 0) exit vim.\n"
"\n"
"Coding agents should use the CLI commands instead.\n"
"\n"
"Block Editing Commands\n"
"======================\n"
"\n"
"--after <id>\n"
"  Insert new block after the specified block ID.\n"
"  Reads new block content (NL + PL) from stdin.\n"
"  New block is inserted in the same file.\n"
"  Example: cat newblock.txt | cmpr --after '#INBOX'\n"
"\n"
"--replace <id>\n"
"  Replace entire block (both NL and PL parts) with content from stdin.\n"
"  Completely overwrites existing block.\n"
"  Example: cat updated.txt | cmpr --replace '#blockid'\n"
"\n"
"--replace-comment <id>\n"
"  Replace only the NL (comment) part, keeping PL unchanged.\n"
"  Use this to update documentation without touching code.\n"
"  Example: cat new_comment.txt | cmpr --replace-comment '#blockid'\n"
"\n"
"--replace-code <id>\n"
"  Replace only the PL (code) part, keeping NL unchanged.\n"
"  Less preferred than --rewritepl which generates from NL.\n"
"  Example: cat new_code.c | cmpr --replace-code '#blockid'\n"
"\n"
"--replace-current\n"
"  Replace block using OFRA format from stdin (checksum-protected).\n"
"  Verifies checksum to prevent concurrent modification conflicts.\n"
"  Used by TUI and safe editing workflows.\n"
"  \n"
"  OFRA format:\n"
"    ID: #blockid\n"
"    Checksum: <16-char hex>\n"
"    \n"
"    <block content>\n"
"  \n"
"  If checksum doesn't match current block, operation fails.\n"
"  Example: cat ofra_block.txt | cmpr --replace-current\n"
"\n"
"--print-block --ofra <id>\n"
"  Print block in OFRA format (includes ID and Checksum headers).\n"
"  Use this output with --replace-current for safe editing.\n"
"  Example: cmpr --print-block --ofra '#myblock' > /tmp/edit.txt\n"
"\n"
"General editing recipe\n"
"=====================\n"
"\n"
"There is no \"before\" even though there is an --after, so if you want to put a block before the first block in a file, follow the general editing recipe we describe here.\n"
"\n"
"This also applies to handling blocks that are anonymous, since you can't use --replace with numeric blockids at all without introducing race conditions in case anyone else is editing the codebase at the same time.\n"
"\n"
"The \"general editing recipe\" lets you do anything with blocks and makes cmpr a complete swiss army knife.\n"
"But this knife is sharp so be careful with it.\n"
"\n"
"In the general editing recipe, you construct a temp file, concatenating any cmpr --print-* commands or whatever other information you want into it, edit it using any tools you like, and then finally run `cmpr --replace \\#foo < path/to/that/file` to replace #foo with whatever you have constructed.\n"
"\n"
"Note this allows you to totally rewrite the block structure of a file, and fix (or make) all kinds of mistakes, for example:\n"
"\n"
"delete a block: cmpr --replace '#foo' </dev/null # or: true | cmpr --replace ...\n"
"split a block into two:\n"
"    cmpr --print-block '#foo' >/path/to/tmpfile\n"
"    vim /path/to/tmpfile                         # add a block comment opening in column 0\n"
"    cmpr --replace '#foo' </path/to/tmpfile\n"
"    cmpr --files-blocks | grep -A3 '#foo'        # check the current block structure\n"
"\n"
"You can also use this to rename a block, or even remove (or indent) the block comment part of a block, making it become part of the previous block in the file.\n"
"\n"
"You can also do terrible mistakes like sending a Python block which is triple-quote delimited into a C-style file.\n"
"In this case the Python block won't start a new block, since that's not how the file is blockized.\n"
"You can fix this by editing the block that you put the Python block after, which will now contain the Python block entirely as part of its PL section.\n"
"Copy it into a file and fix it and then --replace and check the block structure as above (applies to any surgery of this sort).\n"
"\n"
); else return nullspan();
}

/* #help_text_search_impl */
span help_text_search(span s) {
  if (empty(s) || span_eq(S("help_text_search"),s))
    return S(
"As your general-purpose code database and swiss army knife, of course cmpr offers a search feature.\n"
"In the TUI there is a very limited \"/\" keybind which just searches for literal strings (no regex features) so you get exactly what you type.\n"
"Use n/N keys to move forward and back in the search results.\n"
"\n"
"From the CLI we have powerful --grep which uses POSIX EREs, but usually you don't need it at all.\n"
"Instead you should look up the root block, and follow references from there, or if the navigation in the project is broken, then you should do --files-blocks and read all the blockids.\n"
"Usually you can understand everything and find everything from there.\n"
"\n"
"However, if the blocks are all anonymous or the blockids are terrible, then you probably have some onboarding work to do and you should go through and name some blocks, add some mentions of important waypoints into your root block, etc etc.\n"
"\n"
"Search and Navigation Commands\n"
"==============================\n"
"\n"
"--grep <pattern>\n"
"  Search all blocks using POSIX Extended Regular Expression.\n"
"  Searches both NL (comment) and PL (code) parts.\n"
"  Returns space-separated list of matching block IDs.\n"
"  Outputs \"#id\" when NL matches, \"#id:code\" for PL-only matches.\n"
"  Anonymous blocks are returned by index.\n"
"  \n"
"  Pattern syntax: POSIX ERE (not JavaScript regex)\n"
"  - Use [0-9] instead of \\d\n"
"  - Use [a-zA-Z0-9_] instead of \\w\n"
"  - Use [[:space:]] instead of \\s\n"
"  \n"
"  Example: cmpr --grep 'handle.*help'\n"
"  Example: cmpr --grep '#[a-z_]+'      # pointless but it's a way to search for lowercase blockids\n"
"\n"
"--content-index <search>\n"
"  Search for literal string (not regex) across all blocks.\n"
"  This is mostly used internally or by scripts.\n"
"  Returns space-separated list of one-based indices.\n"
"  Example: cmpr --content-index 'event_system'\n"
"\n"
"--files-blocks\n"
"  Print structured list of all files and blocks.\n"
"  Each line shows either \"file: filename\" or \"Block N: #id\" (or \"Block N\" if anonymous).\n"
"  Best and shortest project overview and highly recommended for orientation in almost any project.\n"
"  \n"
"  Filter to specific file:\n"
"  cmpr --files-blocks | grep -A 1000 'file: spanio.c' | \\\n"
"    grep -B 1000 -m 1 '^file:' | head -n -1\n"
"\n"
"Recommended Navigation Pattern:\n"
"  1. Start at root: cmpr --print-comment '#root'\n"
"  2. Follow references to hubs (2-3 hops to reach any block)\n"
"  3. Use --grep only when navigation doesn't work\n"
"\n"
"Fallback Navigation Pattern:\n"
"  1. Start with cmpr --count-blocks or just yolo it with --files-blocks.\n"
"  2. Read the blockids and think about which ones are probably the ones you want.\n"
"  3. Use --print-comment <id> on the most promising blockids until you find what you need.\n"
"\n"
"Third choice navigation:\n"
"  1. Use cmpr --grep on some likely strings (error messages, code snippets)\n"
"  2. Use regular grep and then guess the blockids or look nearby using sed or use --files-blocks and guess.\n"
"  3. If you have just a line number and no relevant code at all, then use sed or awk or similar to read lines nearby.\n"
"\n"
"For weird things like Makefiles and other things that don't fall into a standard code file pattern, if you really want to manage them using blocks (for example, to benefit from cmpr's revision control) then you can put them in blocks and use a cmpr --print-code > path/to/whatever pattern.\n"
);
  else return nullspan();
}

/* #help_text_nl2pl_impl */
span help_text_nl2pl(span s) {
  if (empty(s) || span_eq(S("help_text_nl2pl"), s)) return S(
"The nl2pl subsystem is the very oldest part of cmpr and it wrote almost all of the rest, until in the GPT-5 era, models became good enough to write the code directly.\n"
"However, for efficiency, nl2pl is still strongly recommended.\n"
"We are gradually migrating the cmpr codebase itself back to nl2pl code generation.\n"
"\n"
"In times of accumulating tech debt to try ideas, you may have a lot of \"manually maintained\" code (written by gpt5 or similar class model) however this tech debt should always be cleaned up bringing the codebase into a clean state where every block that has PL at all is successfully and repeatably generated by --rewritepl (or 'R' in the TUI).\n"
"\n"
"Natural Language to Code Generation\n"
"===================================\n"
"\n"
"--rewritepl <id>\n"
"  Regenerate PL (code) from NL (comment) using LLM.\n"
"  Sends NL part to configured LLM API.\n"
"  Replaces PL part with generated code.\n"
"  This is the preferred way to update code after changing NL.\n"
"  \n"
"  Typical Workflow:\n"
"  1. Edit NL: cat new_nl.txt | cmpr --replace-comment '#blockid'\n"
"  2. Generate code: cmpr --rewritepl '#blockid'\n"
"  3. Verify: cmpr --print-code '#blockid'\n"
"  4. Build or test.\n"
"\n"
"  If the code is not as you expect:\n"
"  1. Use --expand-block to see what nl2pl is seeing (context from refs)\n"
"  2. Add negative advice in the NL in an \"Implementation notes\" section\n"
"  3. Add an explicit algorithm in English prose\n"
"  4. Include identifier names at the bottom (model will use them)\n"
"  5. Include code snippets with \"Hint: \" prefix\n"
"  6. Resist writing PL directly - iterate on NL instead\n"
"\n"
"--prompt <id>\n"
"  Print the prompt that would be sent to the LLM for nl2pl conversion.\n"
"  Does NOT call the LLM - just shows what prompt would be used.\n"
"  Useful for debugging and understanding LLM context.\n"
"  Example: cmpr --prompt '#blockid'\n"
"\n"
"--expand-block <id>\n"
"  Print the block with all @references expanded.\n"
"  Shows the full context that nl2pl will see.\n"
"  Useful for debugging when --rewritepl produces unexpected code.\n"
"  Example: cmpr --expand-block '#myblock'\n"
"\n"
"--llm\n"
"  Send stdin to the configured LLM and print response to stdout.\n"
"  General-purpose LLM access, not block-specific.\n"
"  Example: echo \"Explain quicksort\" | cmpr --llm\n"
"\n"
"Configuration:\n"
"  LLM settings are in .cmpr/conf:\n"
"    model: gpt-4.1     (or claude-3-opus, etc.)\n"
"\n"
"Block References (@):\n"
"  Use @blockid in NL to include another block as context.\n"
"  Use @blockid:code to include only the PL part.\n"
"  Use @blockid:all to include all references recursively.\n"
"  These are expanded before sending to the LLM.\n"
"\n"
"Manually Maintained Blocks:\n"
"  Add \"Manually maintained.\" as last line of NL comment.\n"
"  Only use when you MUST write PL directly.\n"
"  Avoid when possible - nl2pl subsystem will skip these blocks.\n"
"\n"
"If generated PL is wrong, fix the NL, not the PL.\n"
"\n"
  ); 
  else return nullspan();
}

/* #help_text_events_impl */
span help_text_events(span s) {
  if (empty(s) || span_eq(S("help_text_events"), s))
    return S(
"Event System (Temporal Reasoning)\n"
"==================================\n"
"\n"
"The event system provides temporal reasoning through tracking events\n"
"in transient memory (T). Events are strings with strength values (0-255).\n"
"\n"
"Core Concepts:\n"
"  T - Transient memory (current event state, stored in .cmpr/T)\n"
"  E - Events (strings with associated strength values)\n"
"  S - Strength (0=false, 255=true, values between represent uncertainty)\n"
"  ES - Event Space (a filter that matches a category of events)\n"
"  P - Pattern (total reactive machinery: ES + induced + LPP)\n"
"  SN - Strength Notation: \"event string\" strength.\n"
"\n"
"Basic Commands:\n"
"\n"
"--T0\n"
"  Reset T to empty state.\n"
"  Use at start of new work session to clear transient context.\n"
"  Example: cmpr --T0\n"
"\n"
"--event <string> --strength <value>\n"
"  Add event to T with specified strength (0-255).\n"
"  Events are deduplicated - adding duplicate updates strength.\n"
"  Example: cmpr --event \"The blockid is: #root.\" --strength 255\n"
"\n"
"--event-stdin --strength <value>\n"
"  Read event text from stdin (for large events up to 4MiB).\n"
"  Content is hashed and stored in revs, with hash event added to T.\n"
"  Example: cat large_content.txt | cmpr --event-stdin --strength 255\n"
"\n"
"--event-file <path> --strength <value>\n"
"  Read event text from file (for large events up to 4MiB).\n"
"  Example: cmpr --event-file myfile.txt --strength 255\n"
"\n"
"--T\n"
"  Print current T state as SN (strength-notation) lines.\n"
"  Format: \"event_string\" strength.\n"
"  Example: cmpr --T\n"
"\n"
"--query <string>\n"
"  Query the strength of an event in T.\n"
"  Prints the strength value (0-255), or 0 if not found.\n"
"  Example: cmpr --query \"The blockid is: #root.\"\n"
"\n"
"Memory Commands:\n"
"\n"
"--memorize\n"
"  Save current T to timestamped snapshot in .cmpr/events/\n"
"  Creates permanent record of current transient state.\n"
"  Example: cmpr --memorize\n"
"\n"
"--recall\n"
"  Search memorized snapshots for most recent match.\n"
"  Finds the most recent snapshot containing all 255-strength events in T.\n"
"  Loads matching snapshot into T.\n"
"  Example:\n"
"    cmpr --T0\n"
"    cmpr --event \"The blockid is: #root.\" --strength 255\n"
"    cmpr --recall  # loads last snapshot about #root\n"
"\n"
"--recall-first\n"
"  Like --recall but finds the oldest matching snapshot.\n"
"  Useful for finding when something first happened.\n"
"  Example: cmpr --recall-first\n"
"\n"
"Monitoring Commands:\n"
"\n"
"--trace\n"
"  Watch .cmpr/T for changes and print them incrementally.\n"
"  Prints changes as a log stream (does not clear terminal).\n"
"  Useful for monitoring T while other processes add events.\n"
"  Exit with Ctrl+C.\n"
"  Example: cmpr --trace\n"
"\n"
"--work [event]\n"
"  Interactive work mode with T-debug enabled.\n"
"  Clears terminal and displays T on changes (uses entr).\n"
"  Optionally adds an initial event.\n"
"  Useful for focused work sessions watching T state.\n"
"  Example: cmpr --work \"Starting task: #myblock\"\n"
"\n"
"Pattern Overview Commands:\n"
"\n"
"--P (or --pattern)\n"
"  Show complete reactive pattern overview.\n"
"  Displays all event spaces, induced scripts, induced-single triggers,\n"
"  surprise handlers, and LPP patterns with their connections.\n"
"  Shows what reactive automation is configured.\n"
"  Example: cmpr --P\n"
"\n"
"--E\n"
"  Show event system statistics.\n"
"  Displays: |E| unique events ever observed, |M| total memories,\n"
"  event space coverage (how many indexed events match each ES),\n"
"  and current T event count.\n"
"  Rebuilds .cmpr/event-names index if stale.\n"
"  Example: cmpr --E\n"
"\n"
"Event Spaces (ES):\n"
"\n"
"An Event Space is a filter that matches a category of events.\n"
"ES filters are executable scripts in .cmpr/es/ that read stdin\n"
"and output matching lines.\n"
"\n"
"--es (or --event-spaces)\n"
"  List all event spaces with their attached infrastructure.\n"
"  Shows induced patterns, surprise handlers, and LPP connections.\n"
"  Example: cmpr --es\n"
"\n"
"--es <name> <pattern>\n"
"  Create a new event space.\n"
"  Creates .cmpr/es/<name> as executable grep -E filter.\n"
"  Example: cmpr --es myevents '^\"My event:'\n"
"\n"
"Open vs Closed ES:\n"
"  Open ES: Variable part (e.g., \"The blockid is: #foo\")\n"
"  Closed ES: Fixed set of states (e.g., \"The block is justified.\")\n"
"\n"
"  Pattern: Bind open ES once, then reference closed ES for states.\n"
"  Example:\n"
"    cmpr --event \"The blockid is: #myblock\" --strength 255  # bind open\n"
"    cmpr --event \"The block is justified.\" --strength 255   # closed state\n"
"\n"
"Reactive Automation:\n"
"\n"
"--induced <es>\n"
"  Install induced script template for an event space.\n"
"  Creates .cmpr/induced/<es> with hello-world template.\n"
"  Edit the script to define actual behavior.\n"
"  Requires ES to exist; errors if script already exists.\n"
"  Example: cmpr --induced BID\n"
"\n"
"--induced-single <event>\n"
"  Install exact-match trigger for a specific event.\n"
"  Creates .cmpr/induced-single/N/name and script.\n"
"  Fires only when this exact event is added to T.\n"
"  Example: cmpr --induced-single \"Build completed successfully.\"\n"
"\n"
"--lpp <es1> <es2>\n"
"  Create LPP (Latent Pattern Propagation) pattern file.\n"
"  Creates .cmpr/patterns/<es1>-<es2> with template.\n"
"  LPP defines cross-product inference rules between two event spaces.\n"
"  Both ES must exist.\n"
"  Example: cmpr --lpp BID status\n"
"\n"
"Induced Patterns:\n"
"\n"
"When events are added that match an ES, scripts in .cmpr/induced/<es>\n"
"are automatically executed. This enables reactive automation.\n"
"\n"
"Example: .cmpr/induced/BID runs when \"The blockid is: ...\" is added.\n"
"\n"
"Common Workflow:\n"
"  1. Clear T: cmpr --T0\n"
"  2. Set context: cmpr --event \"The blockid is: #foo\" --strength 255\n"
"  3. Add facts: cmpr --event \"Block is reachable\" --strength 255\n"
"  4. Save snapshot: cmpr --memorize\n"
"\n"
"T is TRANSIENT - cleared between work sessions.\n"
"Snapshots provide HISTORICAL queries via --recall.\n"
"\n"
"Multi-Agent Communication:\n"
"Multiple agents can share T without conflict by using different\n"
"event prefixes. Formulaic beginnings like \"The blockid is:\" help\n"
"prevent collisions.\n"
"\n"
"See also: cmpr --help agents, cmpr --help wants\n"
    ); 
  else
    return nullspan();
}

/* #help_text_agents_impl */
span help_text_agents(span s) {
  if (empty(s) || span_eq(S("help_text_agents"), s)) return S(
"Agent System\n"
"============\n"
"\n"
"Agents are background processes that watch for changes and maintain wants\n"
"(desired states). All agent communication is via T (transient memory).\n"
"\n"
"Quick Start:\n"
"  cmpr --agents                    # List agents and status\n"
"  cmpr --install-agent claude      # Install embedded agent\n"
"  .cmpr/agents/claude &            # Run in background\n"
"\n"
"BASIC CONTRACT:\n"
"\n"
"An agent takes responsibility for one or more wants. All communication\n"
"is via cmpr events - no stdout/stderr for normal operation:\n"
"\n"
"  cmpr --event \"Agent: myagent\" --strength 255\n"
"  cmpr --event \"We want X to be true.\" --strength 255\n"
"  cmpr --event \"Status: ok\" --strength 255   # or \"Status: failing\"\n"
"  cmpr --memorize\n"
"\n"
"Agents typically use entr(1) to watch files and re-check on changes.\n"
"\n"
"Commands:\n"
"\n"
"--agents\n"
"  List all agents with installation and running status.\n"
"  Shows both embedded agents (available via --install-agent) and\n"
"  custom agents in .cmpr/agents/.\n"
"  Running detection uses pgrep, no PID files needed.\n"
"  Example: cmpr --agents\n"
"\n"
"--install-agent <name>\n"
"  Extract embedded agent script to .cmpr/agents/<name>.\n"
"  Makes the script executable.\n"
"  Currently available: claude\n"
"  Example: cmpr --install-agent claude\n"
"\n"
"--install-script <name>\n"
"  Extract embedded utility script to .cmpr/scripts/<name>.\n"
"  Makes the script executable.\n"
"  Available scripts: Tcks, blockid, checksum, patterns\n"
"  Example: cmpr --install-script patterns\n"
"\n"
"--run <block_id>\n"
"  Execute the PL (code) part of a block as a shell script.\n"
"  Writes code to temp file, runs it, returns exit code.\n"
"  Useful for blocks that contain executable scripts.\n"
"  Example: cmpr --run '#my_script_block'\n"
"\n"
"Agent Architecture:\n"
"\n"
"Agents watch files and emit events to T. The decision state progression:\n"
"\n"
"  1. Tracked  - Want is recorded but not verified\n"
"  2. Checked  - Agent can determine if want is satisfied\n"
"  3. Assisted - Agent can help fix violations\n"
"  4. Owned    - Agent automatically maintains the want\n"
"\n"
"Creating a Custom Agent:\n"
"\n"
"1. Create .cmpr/agents/myagent (executable shell script)\n"
"2. Use entr or inotifywait to watch relevant files\n"
"3. On changes, emit events via cmpr --event\n"
"4. Use cmpr --memorize to save state for --recall\n"
"\n"
"Example agent structure:\n"
"\n"
"  #!/bin/bash\n"
"  cmpr --T0\n"
"  cmpr --event \"Agent: myagent\" --strength 255\n"
"  # ... check something ...\n"
"  cmpr --event \"Status: ok\" --strength 255\n"
"  cmpr --memorize\n"
"  \n"
"  echo \"watched_file.txt\" | entr -ns '\n"
"    cmpr --T0\n"
"    cmpr --event \"Agent: myagent\" --strength 255\n"
"    # ... re-check ...\n"
"    cmpr --memorize\n"
"  '\n"
"\n"
"See also: cmpr --help events, cmpr --help wants\n"
"\n"
); else return nullspan();
}

/* #help_text_reports_impl */
span help_text_reports(span s) {
  static const char t[] =
"HTML Reports and Dashboards\n"
"===========================\n"
"\n"
"Commands that generate HTML reports for system visibility.\n"
"\n"
"--wants-dashboard\n"
"  Generate All Wants Dashboard HTML report.\n"
"  Checks staleness and regenerates if needed (daily).\n"
"  Creates public_html/wants_dashboard.html\n"
"  Shows all wants grouped by decision state.\n"
"  Requires pandoc for HTML conversion.\n"
"  Example: cmpr --wants-dashboard\n"
"\n"
"--event-report\n"
"  Generate Event System Activity Report HTML.\n"
"  Checks staleness and regenerates if needed (daily).\n"
"  Creates public_html/event_activity.html\n"
"  Shows agent runs, work sessions, and metrics over time.\n"
"  Requires pandoc for HTML conversion.\n"
"  Example: cmpr --event-report\n"
"\n"
"--export-docs\n"
"  Generate markdown reports in docs/ directory for GitHub.\n"
"  Creates:\n"
"    - docs/wants_dashboard.md\n"
"    - docs/event_activity.md\n"
"    - docs/README.md\n"
"  Always regenerates (no staleness check).\n"
"  Suitable for committing to version control.\n"
"  Example: cmpr --export-docs\n"
"\n"
"Visualization Generators (execute via --print-code):\n"
"  #generate_timeline_html      - Event timeline scatter plot\n"
"  #generate_metric_plot        - Metric tracking over time\n"
"  #generate_snapshot_stats     - Snapshot statistics\n"
"  #generate_visualization_index - Navigation page\n"
"\n"
"All HTML visualizations are self-contained.\n"
"Reports contain only metadata, no sensitive data.\n"
"\n"
"Note: Current visualizations use Chart.js, but we plan to remove\n"
"this dependency in favor of a simpler approach.\n"
"\n";
  if (empty(s) || span_eq(S("help_text_reports"), s)) return (span){ (u8*)t, (u8*)t + sizeof(t) - 1 };
  return nullspan();
}

/* #help_text_wants_impl */
span help_text_wants(span s) {
  if (empty(s) || span_eq(S("help_text_wants"), s)) return S(
"Want Tracking and Decision States\n"
"==================================\n"
"\n"
"Want = statement of desired state, dual to an agent.\n"
"\n"
"Commands:\n"
"\n"
"--wants\n"
"  Find and print all want statements in the project.\n"
"  Searches all files for SN lines starting with \"We want \".\n"
"  Scans source files, .cmpr/T, and .cmpr/events/*\n"
"  Example: cmpr --wants\n"
"\n"
"--agents-wants\n"
"  Show relationship between wants and agents.\n"
"  Displays decision state for each want:\n"
"    - TRACKED: Want recorded, no agent\n"
"    - CHECKED: Has CHECK implementation\n"
"    - ASSISTED: Has CHECK and FIX implementations\n"
"    - OWNED: Automatic maintenance (future)\n"
"  \n"
"  Output grouped by decision state showing:\n"
"    - Want text\n"
"    - Block ID containing want\n"
"    - Agent ID (if any)\n"
"    - Implementation blocks\n"
"  \n"
"  Example: cmpr --agents-wants\n"
"\n"
"Want Format (SN notation):\n"
"  \"We want <description of desired state>\" <strength>.\n"
"\n"
"Example want statements:\n"
"  \"We want all blocks reachable from #root in ≤2 hops\" 255.\n"
"  \"We want the build to complete without warnings\" 255.\n"
"\n"
"Decision State Progression:\n"
"  tracked → checked → assisted → owned\n"
"\n"
"Each state adds capability:\n"
"  - Checked: Can verify if satisfied\n"
"  - Assisted: Can help fix violations\n"
"  - Owned: Automatically maintains\n"
"\n"
"Wants establish event spaces (desired state, complement).\n"
"Agents verify and maintain wants through CHECK and FIX modes.\n"
"\n"
); else return nullspan();
}

/* #help_text_agent_qa_impl */
span help_text_agent_qa(span s) {
  if (empty(s) || span_eq(S("help_text_agent_qa"), s)) return S(
"Agent QA (Manual Quality Assurance)\n"
"====================================\n"
"\n"
"Pipe this help text to an assistant: cmpr --help agent-qa | claude\n"
"\n"
"---\n"
"\n"
"ASSISTANT INSTRUCTIONS:\n"
"\n"
"You are performing manual QA on an agent.\n"
"\n"
"NAMING CONVENTIONS:\n"
"\n"
"New style (#cmpra_ namespace):\n"
"  #cmpra_<name>         - Agent hub (want + references, NL only)\n"
"  #cmpra_<name>_check   - CHECK implementation\n"
"  #cmpra_<name>_fix     - FIX implementation (optional)\n"
"\n"
"Old style:\n"
"  #agent_<name>         - Agent definition\n"
"  #<name>_agent_check   - CHECK implementation\n"
"  #<name>_agent_fix     - FIX implementation\n"
"\n"
"Both styles are valid.\n"
"\n"
"COMMUNICATION CONTRACT:\n"
"\n"
"ALL agent communication is via T (transient memory):\n"
"  - Agents use `cmpr --event \"...\" --strength N` for ALL results\n"
"  - NO stdout/stderr for normal operation\n"
"  - Callers read results via `cmpr --T`\n"
"\n"
"Required event patterns:\n"
"  \"Agent: <name>\" 255.\n"
"  \"Mode: CHECK|FIX\" 255.\n"
"  \"Status: <result>\" 255.\n"
"\n"
"STEP 1: Check if agent exists\n"
"  Run: cmpr --agents | grep <agent_name>\n"
"  \n"
"  If no match: Agent does not exist. Stop.\n"
"  If match: Continue.\n"
"\n"
"STEP 2: Find agent blocks\n"
"  Run: cmpr --files-blocks | grep -i <agent_name>\n"
"  \n"
"  Look for hub and impl blocks in either naming style.\n"
"\n"
"STEP 3: Read agent hub/definition\n"
"  Run: cmpr --print-comment '#cmpra_<name>' or '#agent_<name>'\n"
"  \n"
"  Check for:\n"
"    - Want statement\n"
"    - References to CHECK/FIX implementations\n"
"\n"
"STEP 4: Verify CHECK implementation\n"
"  Clear T and run:\n"
"    cmpr --T0\n"
"    cmpr --print-code '#<check_block>' | bash\n"
"    cmpr --T\n"
"  \n"
"  Verify output contains required event patterns.\n"
"\n"
"STEP 5: Report\n"
"  - Hub: exists/missing\n"
"  - CHECK: exists/missing (and uses T communication)\n"
"  - FIX: exists/missing\n"
"  - Run command\n"
"\n"
"---\n"
"\n"
); else return nullspan();
}

/* #help_text_claude_setup_impl */
span help_text_claude_setup(span s) {
  if (empty(s) || span_eq(S("help_text_claude_setup"),s))
    return S(
"Claude Code Integration Guide\n"
"=============================\n"
"\n"
"This guide helps Claude Code work effectively with cmpr-managed codebases.\n"
"\n"
"Session Start:\n"
"  Run this command at the beginning of every session:\n"
"  \n"
"  cmpr --help && cmpr --count-blocks && cmpr --T && cmpr --print-block '#root'\n"
"\n"
"Put what you are working on into T with --event, so that other agents (and humans) can see your progress.\n"
"\n"
"Key Principles:\n"
"\n"
"1. BLOCKS, NOT FILES\n"
"   - Code is organized into blocks, not files\n"
"   - Use cmpr commands, not file-based tools (cat, sed, Read, Edit)\n"
"   - A block = NL comment + PL code\n"
"\n"
"2. SEARCH: Use cmpr --grep\n"
"   - Returns block IDs, not file paths\n"
"   - Example: cmpr --grep 'handle_event'\n"
"   - For structure: cmpr --files-blocks\n"
"\n"
"3. READ: Use cmpr --print-*\n"
"   - cmpr --print-block '#blockid'  (full block)\n"
"   - cmpr --print-comment '#blockid' (NL only)\n"
"   - cmpr --print-code '#blockid'   (PL only)\n"
"\n"
"4. EDIT: Use cmpr --replace\n"
"   - Read first: cmpr --print-block '#blockid'\n"
"   - Modify and pipe back: cat modified.txt | cmpr --replace '#blockid'\n"
"   - For new blocks: cat new.txt | cmpr --after '#existing_blockid'\n"
"\n"
"5. COMMUNICATE VIA T\n"
"   - Check context: cmpr --T\n"
"   - Add events: cmpr --event \"Working on: #blockid\" --strength 255\n"
"   - Save state: cmpr --memorize\n"
"   - Recall context: cmpr --recall\n"
"\n"
"Common Patterns:\n"
"\n"
"Safe Edit Pattern:\n"
"  cmpr --print-block '#foo' > /tmp/foo.txt\n"
"  # modify /tmp/foo.txt\n"
"  cmpr --replace '#foo' < /tmp/foo.txt\n"
"\n"
"NL2PL Pattern (preferred for code):\n"
"  # 1. Write NL (natural language spec)\n"
"  # 2. Regenerate code: cmpr --rewritepl '#blockid'\n"
"\n"
"Finding Things:\n"
"  cmpr --print-block '#root'     # start here\n"
"  cmpr --files-blocks            # see all blocks\n"
"  cmpr --grep 'pattern'          # search content\n"
"  cmpr --help --flag             # per-flag help\n"
"\n"
"Pitfalls to Avoid:\n"
"  - DON'T use cat/Read to read source files directly\n"
"  - DON'T use sed/Edit to modify source files directly\n"
"  - DON'T forget to check T at session start\n"
"  - DON'T create files outside the block system\n"
"\n"
"Experience Reports:\n"
"  When you learn something useful, create an experience report:\n"
"  cat report.txt | cmpr --after '#INBOX'\n"
"  Use naming: #claude_experience_report_<topic>_<date>\n"
"  Then put \"The experience report is: ...\" in T, and memorize.\n"
"  You can later use --recall.\n"
"\n"
"See also: cmpr --help basic, cmpr --help blocks, cmpr --help events\n"
"\n"
    );
  else
    return nullspan();
}

/* #help_text_history_impl */
span help_text_history(span s) {
  if (empty(s) || span_eq(S("help_text_history"), s)) return S(
"Block History\n"
"=============\n"
"\n"
"--history [#blockid] [--log-gap [factor]] [--limit N]\n"
"  Show block change history from the revision store.\n"
"\n"
"  Without arguments:\n"
"    Lists recently changed blocks across the codebase (most recent first).\n"
"    Each line shows: timestamp, block ID.\n"
"    Useful for \"what changed recently?\" at a glance.\n"
"\n"
"  With #blockid:\n"
"    Shows timestamps when that specific block's content changed.\n"
"    Tracks the block by ID across time, including renames and moves.\n"
"    Duplicate content (reverts) are collapsed to earliest timestamp.\n"
"\n"
"  Options:\n"
"    --log-gap [factor]\n"
"      Show recent changes densely, older changes sparsely.\n"
"      The gap between shown entries doubles (or multiplies by factor) going\n"
"      back in time. Default factor: 2.0\n"
"\n"
"      This reflects how recent changes matter more than ancient history.\n"
"      Without --log-gap, all changes in range are shown.\n"
"\n"
"      Example: with factor 2.0, gaps are 1s, 2s, 4s, 8s, 16s, ...\n"
"      The 10th entry is at least 512 seconds older than the 9th.\n"
"\n"
"    --limit N\n"
"      Show at most N entries. Applied after --log-gap filtering.\n"
"      Reports how many entries were skipped if limit was exceeded.\n"
"\n"
"  Examples:\n"
"    cmpr --history                     # recent changes, all blocks\n"
"    cmpr --history '#root'             # history of #root block\n"
"    cmpr --history --log-gap           # sparse view of recent changes\n"
"    cmpr --history --log-gap 1.5       # gentler thinning (factor 1.5)\n"
"    cmpr --history '#foo' --limit 10   # last 10 changes to #foo\n"
"\n"
"  Output format:\n"
"    Without blockid (recent changes):\n"
"      YYYY-MM-DD HH:MM:SS  #blockid\n"
"      YYYY-MM-DD HH:MM:SS  #other\n"
"      ... (+3 skipped)\n"
"      YYYY-MM-DD HH:MM:SS  #another\n"
"\n"
"    With blockid (block history):\n"
"      YYYY-MM-DD HH:MM:SS  245 bytes  12 lines\n"
"      YYYY-MM-DD HH:MM:SS  198 bytes  10 lines\n"
"      ... (+5 skipped)\n"
"      YYYY-MM-DD HH:MM:SS  150 bytes  8 lines\n"
"\n"
"  Notes:\n"
"    - Timestamps are local time (same as TUI's 'U' Select Block Version).\n"
"    - Uses the same revision store as the TUI's 'U' keybinding.\n"
"    - Block identity follows IDs; content-identical reverts are deduplicated.\n"
"    - \"Skipped\" counts show entries filtered by --log-gap or --limit.\n"
"    - Duplicate block IDs appear as separate entries. If the same #blockid\n"
"      appears multiple times in the output at the same timestamp, you have\n"
"      multiple blocks with identical IDs (use cmpr --files-blocks to find them).\n"
"\n"
"  See also: TUI 'U' keybinding for interactive block version selection.\n"
"\n"
); else return nullspan();
}

/* #agent_script_claude_impl */
span agent_script_claude(span s) {
    if (empty(s) || span_eq(S("agent_script_claude"), s)) return S(
        "#!/bin/bash\n"
        "set -euo pipefail\n"
        "\n"
        "# Initial check\n"
        "cmpr --T0 2>/dev/null || true\n"
        "cmpr --event \"Agent: claude\" --strength 255 2>/dev/null || true\n"
        "if [ ! -f \"CLAUDE.md\" ]; then\n"
        "    cmpr --event \"Status: CLAUDE.md missing\" --strength 255 2>/dev/null || true\n"
        "elif grep -q \"## cmpr Block System\" CLAUDE.md; then\n"
        "    cmpr --event \"Status: ok\" --strength 255 2>/dev/null || true\n"
        "else\n"
        "    cmpr --event \"Status: prologue missing\" --strength 255 2>/dev/null || true\n"
        "fi\n"
        "cmpr --memorize 2>/dev/null || true\n"
        "\n"
        "# Watch and check on changes\n"
        "echo \"CLAUDE.md\" | entr -ns '\n"
        "cmpr --T0 2>/dev/null || true\n"
        "cmpr --event \"Agent: claude\" --strength 255 2>/dev/null || true\n"
        "if [ ! -f \"CLAUDE.md\" ]; then\n"
        "    cmpr --event \"Status: CLAUDE.md missing\" --strength 255 2>/dev/null || true\n"
        "elif grep -q \"## cmpr Block System\" CLAUDE.md; then\n"
        "    cmpr --event \"Status: ok\" --strength 255 2>/dev/null || true\n"
        "else\n"
        "    cmpr --event \"Status: prologue missing\" --strength 255 2>/dev/null || true\n"
        "fi\n"
        "cmpr --memorize 2>/dev/null || true\n"
        "'\n"
    ); else return nullspan();
}

/* #get_help_text */
// Available agents list
char *available_agents[] = {"claude", NULL};

span get_help_text(span topic) {
    if (empty(topic) || span_eq(topic, S("summary"))) {
        return help_text_summary(nullspan());
    } else if (span_eq(topic, S("topics"))) {
        return help_text_topics(nullspan());
    } else if (span_eq(topic, S("basic"))) {
        return help_text_basic(nullspan());
    } else if (span_eq(topic, S("blocks"))) {
        return help_text_blocks(nullspan());
    } else if (span_eq(topic, S("inbox"))) {
        return help_text_inbox(nullspan());
    } else if (span_eq(topic, S("editing"))) {
        return help_text_editing(nullspan());
    } else if (span_eq(topic, S("search"))) {
        return help_text_search(nullspan());
    } else if (span_eq(topic, S("nl2pl"))) {
        return help_text_nl2pl(nullspan());
    } else if (span_eq(topic, S("events"))) {
        return help_text_events(nullspan());
    } else if (span_eq(topic, S("agents"))) {
        return help_text_agents(nullspan());
    } else if (span_eq(topic, S("reports"))) {
        return help_text_reports(nullspan());
    } else if (span_eq(topic, S("wants"))) {
        return help_text_wants(nullspan());
    } else if (span_eq(topic, S("agent-qa"))) {
        return help_text_agent_qa(nullspan());
    } else if (span_eq(topic, S("claude-setup"))) {
        return help_text_claude_setup(nullspan());
    } else if (span_eq(topic, S("history"))) {
        return help_text_history(nullspan());
    }
    // Flag-to-topic mapping: --flag -> relevant help topic
    if (len(topic) > 2 && topic.buf[0] == '-' && topic.buf[1] == '-') {
        span flag = { topic.buf + 2, topic.end };
        // Search
        if (span_eq(flag, S("grep")) || span_eq(flag, S("content-index")) || span_eq(flag, S("files-blocks"))) {
            return help_text_search(nullspan());
        }
        // Events
        if (span_eq(flag, S("T0")) || span_eq(flag, S("T")) || span_eq(flag, S("event")) ||
            span_eq(flag, S("memorize")) || span_eq(flag, S("recall")) || span_eq(flag, S("recall-first")) ||
            span_eq(flag, S("query")) || span_eq(flag, S("trace")) || span_eq(flag, S("work")) ||
            span_eq(flag, S("es")) || span_eq(flag, S("event-spaces")) ||
            span_eq(flag, S("event-stdin")) || span_eq(flag, S("event-file")) || span_eq(flag, S("strength")) ||
            span_eq(flag, S("P")) || span_eq(flag, S("pattern")) || span_eq(flag, S("E")) ||
            span_eq(flag, S("induced")) || span_eq(flag, S("induced-single")) || span_eq(flag, S("lpp"))) {
            return help_text_events(nullspan());
        }
        // Blocks
        if (span_eq(flag, S("print-block")) || span_eq(flag, S("print-code")) || span_eq(flag, S("print-comment")) ||
            span_eq(flag, S("expand-block")) || span_eq(flag, S("count-blocks"))) {
            return help_text_blocks(nullspan());
        }
        // Editing
        if (span_eq(flag, S("replace")) || span_eq(flag, S("replace-code")) || span_eq(flag, S("replace-comment")) ||
            span_eq(flag, S("replace-current")) || span_eq(flag, S("ofra")) || span_eq(flag, S("after"))) {
            return help_text_editing(nullspan());
        }
        // Agents
        if (span_eq(flag, S("agents")) || span_eq(flag, S("agent-run")) ||
            span_eq(flag, S("install-agent")) || span_eq(flag, S("install-script")) || span_eq(flag, S("run"))) {
            return help_text_agents(nullspan());
        }
        // Wants
        if (span_eq(flag, S("wants")) || span_eq(flag, S("wants-status")) || span_eq(flag, S("wants-dashboard")) ||
            span_eq(flag, S("agents-wants"))) {
            return help_text_wants(nullspan());
        }
        // NL2PL
        if (span_eq(flag, S("rewritepl")) || span_eq(flag, S("prompt")) || span_eq(flag, S("llm"))) {
            return help_text_nl2pl(nullspan());
        }
        // Reports
        if (span_eq(flag, S("event-report")) || span_eq(flag, S("export-docs")) || span_eq(flag, S("wants-dashboard"))) {
            return help_text_reports(nullspan());
        }
        // Basic
        if (span_eq(flag, S("init")) || span_eq(flag, S("version")) || span_eq(flag, S("conf")) ||
            span_eq(flag, S("print-conf")) || span_eq(flag, S("status")) || span_eq(flag, S("build")) ||
            span_eq(flag, S("checksum")) || span_eq(flag, S("find-deleted"))) {
            return help_text_basic(nullspan());
        }
        // Inbox
        if (span_eq(flag, S("inbox"))) {
            return help_text_inbox(nullspan());
        }
        // History
        if (span_eq(flag, S("history")) || span_eq(flag, S("log-gap")) || span_eq(flag, S("limit"))) {
            return help_text_history(nullspan());
        }
    }
    return nullspan();
}

span get_agent_script(span name) {
    if (empty(name)) {
        return nullspan();
    } else if (span_eq(name, S("claude"))) {
        return agent_script_claude(nullspan());
    }
    return nullspan();
}


/* #handle_help_topic */
void handle_help_topic(char *topic) {
    span s = get_help_text(S(topic));
    
    if (s.buf == 0) {
        fprintf(stderr, "Unknown help topic: %s\n\n", topic ? topic : "");
        s = get_help_text(S("topics"));
        if (s.buf) {
            fwrite(s.buf, 1, s.end - s.buf, stdout);
        }
        flush_exit(1);
    }
    
    fwrite(s.buf, 1, s.end - s.buf, stdout);
    flush_exit(0);
}
/* #handle_prompt */
void handle_prompt(int block_idx) {
    state->curr_block_idx = block_idx;
    span op = S("nl2pl_rewrite");
    span template = get_prompt_template(op);
    spans vars = current_block_template_vars();
    span expanded_prompt = expand_template(template, vars);
    prt("%.*s", (int)(expanded_prompt.end - expanded_prompt.buf), expanded_prompt.buf);
}

/* #handle_checksum */
void handle_checksum(void) {
    size_t capacity = 1 << 20;
    size_t size = 0;
    u8 *buffer = malloc(capacity);
    if (!buffer) {
        prt("Error: Failed to allocate memory\n");
        flush_exit(1);
    }
    while (1) {
        if (size == capacity) {
            capacity *= 2;
            if (capacity > (1ULL << 30)) {
                prt("Error: Input too large\n");
                free(buffer);
                flush_exit(1);
            }
            u8 *new_buffer = realloc(buffer, capacity);
            if (!new_buffer) {
                prt("Error: Realloc failed\n");
                free(buffer);
                flush_exit(1);
            }
            buffer = new_buffer;
        }
        size_t bytes_read = fread(buffer + size, 1, capacity - size, stdin);
        if (bytes_read == 0) {
            if (feof(stdin)) break;
            if (ferror(stdin)) {
                prt("Error reading stdin\n");
                free(buffer);
                flush_exit(1);
            }
        }
        size += bytes_read;
    }
    span input = {buffer, buffer + size};
    checksum cs = selected_checksum(input);
    prt("%016llX\n", (unsigned long long)cs.__u);
    free(buffer);
}



/* #handle_event_large */
void handle_event_large_stdin(int strength) {
    size_t capacity = 1 << 20;
    size_t size = 0;
    u8 *buffer = malloc(capacity);
    if (!buffer) {
        prt("Error: Failed to allocate memory\n");
        flush_exit(1);
    }
    while (1) {
        if (size == capacity) {
            capacity *= 2;
            if (capacity > (1ULL << 22)) {  // 4MiB limit
                prt("Error: Input too large (max 4MiB)\n");
                free(buffer);
                flush_exit(1);
            }
            u8 *new_buffer = realloc(buffer, capacity);
            if (!new_buffer) {
                prt("Error: Realloc failed\n");
                free(buffer);
                flush_exit(1);
            }
            buffer = new_buffer;
        }
        size_t bytes_read = fread(buffer + size, 1, capacity - size, stdin);
        if (bytes_read == 0) {
            if (feof(stdin)) break;
            if (ferror(stdin)) {
                prt("Error reading stdin\n");
                free(buffer);
                flush_exit(1);
            }
        }
        size += bytes_read;
    }
    
    span content = {buffer, buffer + size};
    checksum cs = selected_checksum(content);
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    span rev_path = unique_rev_path(ts);
    write_to_file_span(content, rev_path, 1);
    
    span hash_hex = prs_checksum(cs);
    span hash_event = prs("the hash: %.*s", (int)len(hash_hex), hash_hex.buf);
    span text_event = prs("the text: %.*s", (int)size, buffer);
    
    event_add_internal(hash_event, (unsigned char)strength);
    event_add_internal(text_event, (unsigned char)strength);
    event_save_T();
    
    prt("Stored %zu bytes in %.*s\n", size, (int)len(rev_path), rev_path.buf);
    free(buffer);
}

void handle_event_large_file(span path, int strength) {
    span content = read_file_into_cmp(path);
    if (len(content) == 0) {
        prt("Error: Could not read file or file is empty\n");
        flush_exit(1);
    }
    if (len(content) > (1ULL << 22)) {  // 4MiB limit
        prt("Error: File too large (max 4MiB)\n");
        flush_exit(1);
    }
    
    checksum cs = selected_checksum(content);
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    span rev_path = unique_rev_path(ts);
    write_to_file_span(content, rev_path, 1);
    
    span hash_hex = prs_checksum(cs);
    span hash_event = prs("the hash: %.*s", (int)len(hash_hex), hash_hex.buf);
    span text_event = prs("the text: %.*s", (int)len(content), content.buf);
    
    event_add_internal(hash_event, (unsigned char)strength);
    event_add_internal(text_event, (unsigned char)strength);
    event_save_T();
    
    prt("Stored %zu bytes in %.*s\n", len(content), (int)len(rev_path), rev_path.buf);
}
/* #handle_wants */
void handle_wants(int show_blocks) {
    get_code();

    // Track seen wants to dedupe
    span seen[1024];
    int seen_count = 0;

    for (int i = 0; i < state->blocks.n; i++) {
        span block = state->blocks.a[i];
        span block_copy = block;  // Keep original for ids_for_block

        while (block.buf < block.end) {
            span line = head_line(&block);

            // Skip leading whitespace
            while (line.buf < line.end && (*line.buf == ' ' || *line.buf == '\t')) {
                line.buf++;
            }

            u8 *line_start = line.buf;

            // Line must start with "
            if (line.buf >= line.end || *line.buf != '"') continue;

            // Search backwards for " <digits>.
            u8 *p = line.end - 1;
            if (p < line.buf || *p != '.') continue;
            u8 *line_end = p + 1;
            p--;

            u8 *digit_end = p + 1;
            while (p >= line.buf && *p >= '0' && *p <= '9') p--;
            if (p < line.buf || p + 1 == digit_end) continue;
            if (*p != ' ') continue;
            p--;
            if (p < line.buf || *p != '"') continue;

            span event_str = {line.buf + 1, p};
            span want_prefix = S("We want ");
            int prefix_len = want_prefix.end - want_prefix.buf;

            if (event_str.end - event_str.buf >= prefix_len &&
                memcmp(event_str.buf, want_prefix.buf, prefix_len) == 0) {

                span sn_line = {line_start, line_end};

                // Check if already seen
                int is_dup = 0;
                for (int j = 0; j < seen_count; j++) {
                    if (span_eq(seen[j], sn_line)) {
                        is_dup = 1;
                        break;
                    }
                }

                if (!is_dup && seen_count < 1024) {
                    seen[seen_count] = sn_line;
                    seen_count++;

                    wrs(sn_line);

                    if (show_blocks) {
                        spans ids = ids_for_block(block_copy);
                        if (ids.n > 0) {
                            prt(" [");
                            for (int k = 0; k < ids.n; k++) {
                                if (k > 0) prt(" ");
                                wrs(ids.a[k]);
                            }
                            prt("]");
                        } else {
                            prt(" [block %d]", i + 1);
                        }
                    }

                    terpri();
                }
            }
        }
    }

    flush();
}

/* #handle_wants_status */
void handle_wants_status() {
    // Find the wants_status_report block
    int block_idx = block_by_id(S("wants_status_report"));
    if (block_idx == -1) {
        prt("Error: #wants_status_report block not found\n");
        flush_exit(1);
    }
    
    span block = state->blocks.a[block_idx];
    span comment_part = block_comment_part(block);
    span code_part = block;
    code_part.buf = comment_part.end;
    
    if (empty(code_part)) {
        prt("Error: #wants_status_report block has no code\n");
        flush_exit(1);
    }
    
    // Write to temp file
    char tmpfile[256];
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/cmpr_wants_status_%d.sh", getpid());
    
    FILE *f = fopen(tmpfile, "w");
    if (!f) {
        prt("Error: Failed to create temp file\n");
        flush_exit(1);
    }
    
    fwrite(code_part.buf, 1, code_part.end - code_part.buf, f);
    fclose(f);
    
    // Make executable
    chmod(tmpfile, 0700);
    
    // Execute
    int status = system(tmpfile);
    
    // Clean up
    unlink(tmpfile);
    
    // Exit with script's exit code
    flush_exit(WEXITSTATUS(status));
}

/* #handle_agents_wants */
void handle_agents_wants() {
    // Structure to hold want information
    typedef struct {
        char *want_sn_line;  // Full SN line: "We want..." 255.
        char *block_id;
        char *agent_id;
        char *state;  // "tracked", "checked", "assisted", "owned"
        int has_check;
        int has_fix;
        // Event system fields
        char *event_space;        // e.g., "BR (Block Reachability)"
        char *last_check_time;    // e.g., "2025-12-27T05:25:46+00:00"
        char *last_check_status;  // e.g., "constraint not satisfied"
        int unreferenced_count;   // -1 if not applicable
    } WantInfo;
    
    WantInfo *wants = NULL;
    int want_count = 0;
    int want_capacity = 0;
    
    // Helper: Extract event space from block's NL comment
    char* extract_event_space(const char *block_id_str) {
        int block_idx = block_for_span(S((char*)block_id_str));
        if (block_idx == -1) return NULL;
        
        span block = state->blocks.a[block_idx];
        span comment = block_comment_part(block);
        if (empty(comment)) return NULL;
        
        // Search for "Event space:" in comment
        span needle = S("Event space:");
        span rest = comment;
        while (rest.buf < rest.end) {
            u8 *line_end = rest.buf;
            while (line_end < rest.end && *line_end != '\n') line_end++;
            span line = {rest.buf, line_end};
            
            if (contains(line, needle)) {
                // Extract text after "Event space:"
                u8 *start = line.buf;
                while (start < line.end && (line.end - start) >= (needle.end - needle.buf)) {
                    if (memcmp(start, needle.buf, needle.end - needle.buf) == 0) {
                        start += (needle.end - needle.buf);
                        // Skip whitespace
                        while (start < line.end && (*start == ' ' || *start == '\t')) start++;
                        // Extract until end of line or newline
                        int len = line.end - start;
                        char *result = malloc(len + 1);
                        memcpy(result, start, len);
                        result[len] = '\0';
                        return result;
                    }
                    start++;
                }
            }
            
            rest.buf = line_end;
            if (rest.buf < rest.end && *rest.buf == '\n') rest.buf++;
        }
        
        return NULL;
    }
    
    // Helper: Find latest snapshot for an agent
    char* find_latest_agent_snapshot(const char *agent_id_str) {
        // List files in .cmpr/events/
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "grep -l 'Agent: %s' .cmpr/events/* 2>/dev/null | sort -r | head -1", agent_id_str);
        
        FILE *fp = popen(cmd, "r");
        if (!fp) return NULL;
        
        char path[512];
        if (fgets(path, sizeof(path), fp)) {
            // Remove newline
            char *nl = strchr(path, '\n');
            if (nl) *nl = '\0';
            pclose(fp);
            return strdup(path);
        }
        
        pclose(fp);
        return NULL;
    }
    
    // Helper: Parse snapshot file for agent status
    void parse_agent_snapshot(const char *snapshot_path, WantInfo *want) {
        FILE *fp = fopen(snapshot_path, "r");
        if (!fp) return;
        
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            // Parse SN lines
            if (line[0] != '"') continue;
            
            // Find closing quote and strength
            char *p = line + strlen(line) - 1;
            while (p > line && (*p == '\n' || *p == '\r')) p--;
            if (p <= line || *p != '.') continue;
            p--;
            while (p > line && *p >= '0' && *p <= '9') p--;
            if (p <= line || *p != ' ') continue;
            p--;
            if (p <= line || *p != '"') continue;
            
            *p = '\0';  // Terminate event string
            char *event = line + 1;  // Skip opening quote
            
            // Check for known event patterns
            if (strncmp(event, "Check time: ", 12) == 0) {
                want->last_check_time = strdup(event + 12);
            } else if (strncmp(event, "Timestamp: ", 11) == 0 && !want->last_check_time) {
                want->last_check_time = strdup(event + 11);
            } else if (strncmp(event, "Agent result: ", 14) == 0) {
                want->last_check_status = strdup(event + 14);
            } else if (strncmp(event, "Status: ", 8) == 0 && !want->last_check_status) {
                want->last_check_status = strdup(event + 8);
            } else if (strncmp(event, "Unreferenced blocks: ", 21) == 0) {
                want->unreferenced_count = atoi(event + 21);
            }
        }
        
        fclose(fp);
    }
    
    // Step 1: Collect all wants using handle_wants logic
    // Helper to parse SN line and extract want
    void collect_want(span line, const char *source_file) {
        // Skip leading whitespace
        u8 *line_start = line.buf;
        while (line.buf < line.end && (*line.buf == ' ' || *line.buf == '\t')) {
            line.buf++;
        }
        
        // Save start after whitespace
        line_start = line.buf;
        
        if (line.buf >= line.end || *line.buf != '"') return;
        
        // Search backwards for pattern " <digits>.
        u8 *p = line.end - 1;
        if (p < line.buf || *p != '.') return;
        u8 *line_end = p + 1;  // Save end position (inclusive of '.')
        p--;
        
        u8 *digit_end = p + 1;
        while (p >= line.buf && *p >= '0' && *p <= '9') p--;
        if (p < line.buf || p + 1 == digit_end) return;
        
        if (*p != ' ') return;
        p--;
        
        if (p < line.buf || *p != '"') return;
        
        span event_str = {line.buf + 1, p};
        
        // Check if starts with "We want "
        span want_prefix = S("We want ");
        if (event_str.end - event_str.buf >= want_prefix.end - want_prefix.buf &&
            memcmp(event_str.buf, want_prefix.buf, want_prefix.end - want_prefix.buf) == 0) {
            
            // Allocate space if needed
            if (want_count >= want_capacity) {
                want_capacity = want_capacity == 0 ? 16 : want_capacity * 2;
                wants = realloc(wants, want_capacity * sizeof(WantInfo));
            }
            
            // Store full SN line
            int len = line_end - line_start;
            wants[want_count].want_sn_line = malloc(len + 1);
            memcpy(wants[want_count].want_sn_line, line_start, len);
            wants[want_count].want_sn_line[len] = '\0';
            
            // Initialize other fields
            wants[want_count].block_id = NULL;
            wants[want_count].agent_id = NULL;
            wants[want_count].state = "tracked";
            wants[want_count].has_check = 0;
            wants[want_count].has_fix = 0;
            wants[want_count].event_space = NULL;
            wants[want_count].last_check_time = NULL;
            wants[want_count].last_check_status = NULL;
            wants[want_count].unreferenced_count = -1;
            
            want_count++;
        }
    }
    
    // Scan loaded blocks for wants
    for (int i = 0; i < state->blocks.n; i++) {
        span block = state->blocks.a[i];
        span comment = block_comment_part(block);
        
        if (!empty(comment)) {
            span rest = comment;
            while (rest.buf < rest.end) {
                u8 *line_end = rest.buf;
                while (line_end < rest.end && *line_end != '\n') line_end++;
                
                span line = {rest.buf, line_end};
                collect_want(line, NULL);
                
                rest.buf = line_end;
                if (rest.buf < rest.end && *rest.buf == '\n') rest.buf++;
            }
        }
    }
    
    // Step 2: Match wants to blocks
    for (int w = 0; w < want_count; w++) {
        // Search for the want text in block comments
        for (int i = 0; i < state->blocks.n; i++) {
            span block = state->blocks.a[i];
            span comment = block_comment_part(block);
            
            if (!empty(comment)) {
                // Check if this block's comment contains the want
                span want_span = S(wants[w].want_sn_line);
                if (contains(comment, want_span)) {
                    // Extract block ID
                    span id = state->block_idx.a[i];
                    if (!empty(id)) {
                        int id_len = id.end - id.buf;
                        char *id_str = malloc(id_len + 1);
                        memcpy(id_str, id.buf, id_len);
                        id_str[id_len] = '\0';
                        
                        if (!wants[w].block_id) {
                            wants[w].block_id = id_str;
                        } else {
                            free(id_str);
                        }
                    }
                }
            }
        }
    }
    
    // Step 3: Find all agents
    typedef struct {
        char *agent_id;
        char *referenced_block;  // Block ID mentioned in agent's NL
    } AgentInfo;
    
    AgentInfo *agents = NULL;
    int agent_count = 0;
    int agent_capacity = 0;
    
    for (int i = 0; i < state->blocks.n; i++) {
        span id = state->block_idx.a[i];
        if (empty(id)) continue;
        
        // Check if block ID ends with "_agent"
        span agent_suffix = S("_agent");
        if (id.end - id.buf >= agent_suffix.end - agent_suffix.buf) {
            u8 *suffix_pos = id.end - (agent_suffix.end - agent_suffix.buf);
            if (memcmp(suffix_pos, agent_suffix.buf, agent_suffix.end - agent_suffix.buf) == 0) {
                // This is an agent block
                if (agent_count >= agent_capacity) {
                    agent_capacity = agent_capacity == 0 ? 16 : agent_capacity * 2;
                    agents = realloc(agents, agent_capacity * sizeof(AgentInfo));
                }
                
                int id_len = id.end - id.buf;
                agents[agent_count].agent_id = malloc(id_len + 1);
                memcpy(agents[agent_count].agent_id, id.buf, id_len);
                agents[agent_count].agent_id[id_len] = '\0';
                
                // Extract referenced block from NL comment
                span block = state->blocks.a[i];
                span comment = block_comment_part(block);
                agents[agent_count].referenced_block = NULL;
                
                if (!empty(comment)) {
                    // Look for "As seen in #blockid" or "See #blockid"
                    u8 *p = comment.buf;
                    while (p < comment.end) {
                        if (*p == '#') {
                            // Found a potential block reference
                            u8 *ref_start = p;
                            p++;
                            while (p < comment.end && 
                                   ((*p >= 'a' && *p <= 'z') || 
                                    (*p >= 'A' && *p <= 'Z') ||
                                    (*p >= '0' && *p <= '9') ||
                                    *p == '_')) {
                                p++;
                            }
                            
                            if (p > ref_start + 1) {
                                int ref_len = p - ref_start;
                                char *ref = malloc(ref_len + 1);
                                memcpy(ref, ref_start, ref_len);
                                ref[ref_len] = '\0';
                                
                                if (!agents[agent_count].referenced_block) {
                                    agents[agent_count].referenced_block = ref;
                                } else {
                                    free(ref);
                                }
                            }
                        } else {
                            p++;
                        }
                    }
                }
                
                agent_count++;
                fprintf(stderr, "DEBUG: Found agent: %s, referenced_block: %s\n", agents[agent_count].agent_id, agents[agent_count].referenced_block ? agents[agent_count].referenced_block : "NULL");
            }
        }
    }
    
    // Step 4: Match wants to agents and extract event system data
    for (int w = 0; w < want_count; w++) {
        if (!wants[w].block_id) continue;
        
        // Extract event space for this want's block
        wants[w].event_space = extract_event_space(wants[w].block_id);
        
        for (int a = 0; a < agent_count; a++) {
            if (agents[a].referenced_block && 
                strcmp(wants[w].block_id, agents[a].referenced_block) == 0) {
                // This agent references the block containing this want
                wants[w].agent_id = strdup(agents[a].agent_id);
                
                // Check for _check_impl and _fix_impl
                char check_id[256];
                char fix_id[256];
                snprintf(check_id, sizeof(check_id), "%s_check_impl", agents[a].agent_id);
                snprintf(fix_id, sizeof(fix_id), "%s_fix_impl", agents[a].agent_id);
                
                // Look for these blocks
                for (int i = 0; i < state->block_idx.n; i++) {
                    span id = state->block_idx.a[i];
                    if (!empty(id)) {
                        char id_str[256];
                        int id_len = id.end - id.buf;
                        if (id_len < 256) {
                            memcpy(id_str, id.buf, id_len);
                            id_str[id_len] = '\0';
                            
                            if (strcmp(id_str, check_id) == 0) {
                                wants[w].has_check = 1;
                            }
                            if (strcmp(id_str, fix_id) == 0) {
                                wants[w].has_fix = 1;
                            }
                        }
                    }
                }
                
                // Determine state
                if (wants[w].has_check && wants[w].has_fix) {
                    wants[w].state = "assisted";
                } else if (wants[w].has_check) {
                    wants[w].state = "checked";
                }
                
                // Extract event system data if CHECK exists
                if (wants[w].has_check) {
                    char *snapshot = find_latest_agent_snapshot(agents[a].agent_id);
                    if (snapshot) {
                        parse_agent_snapshot(snapshot, &wants[w]);
                        free(snapshot);
                    }
                }
                
                break;
            }
        }
    }
    
    // Step 5: Output results grouped by state
    const char *states[] = {"assisted", "checked", "tracked"};
    const char *headers[] = {
        "=== ASSISTED",
        "=== CHECKED",
        "=== TRACKED"
    };
    
    for (int s = 0; s < 3; s++) {
        const char *state_name = states[s];
        
        // Count wants in this state
        int count = 0;
        for (int w = 0; w < want_count; w++) {
            if (strcmp(wants[w].state, state_name) == 0) {
                count++;
            }
        }
        
        if (count > 0) {
            char header[128];
            snprintf(header, sizeof(header), "%s (%d wants) ===", headers[s], count);
            prt("%s\n", header);
            
            // Print wants in this state
            for (int w = 0; w < want_count; w++) {
                if (strcmp(wants[w].state, state_name) == 0) {
                    // Print full SN line
                    prt("%s\n", wants[w].want_sn_line);
                    
                    prt("  Block: %s\n", wants[w].block_id ? wants[w].block_id : "unknown");
                    prt("  Agent: %s\n", wants[w].agent_id ? wants[w].agent_id : "none");
                    
                    // Print event space if available
                    if (wants[w].event_space) {
                        prt("  Event Space: %s\n", wants[w].event_space);
                    }
                    
                    // Print temporal information if available
                    if (wants[w].last_check_time) {
                        prt("  Last CHECK: %s\n", wants[w].last_check_time);
                    }
                    if (wants[w].last_check_status) {
                        prt("  Status: %s\n", wants[w].last_check_status);
                    }
                    if (wants[w].unreferenced_count >= 0) {
                        prt("  Unreferenced blocks: %d\n", wants[w].unreferenced_count);
                    }
                    
                    prt("\n");
                }
            }
        }
    }
    
    flush();
}

/* #handle_wants_dashboard */
void handle_wants_dashboard() {
    const char *html_path = "public_html/wants_dashboard.html";
    struct stat st;
    char file_date[16] = "";
    char today[16];
    
    // Get today's date
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    strftime(today, sizeof(today), "%Y%m%d", tm_now);
    
    int needs_regen = 0;
    if (stat(html_path, &st) == 0) {
        struct tm *tm_file = localtime(&st.st_mtime);
        strftime(file_date, sizeof(file_date), "%Y%m%d", tm_file);
        if (strcmp(file_date, today) != 0) {
            needs_regen = 1;
        }
    } else {
        needs_regen = 1;
    }
    
    if (!needs_regen) {
        prt("Current: %s\n", html_path);
        flush();
        return;
    }
    
    // Create public_html directory
    system("mkdir -p public_html");
    
    // Find generator block
    int block_idx = block_by_id(S("generate_wants_dashboard"));
    if (block_idx == -1) {
        prt("Error: Block #generate_wants_dashboard not found\n");
        flush_exit(1);
    }
    
    span block = state->blocks.a[block_idx];
    span comment_part = block_comment_part(block);
    span code_part = block;
    code_part.buf = comment_part.end;
    
    if (empty(code_part)) {
        prt("Error: Block #generate_wants_dashboard has no code\n");
        flush_exit(1);
    }
    
    // Write code to temp file
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/gen_dash_%d.sh", getpid());
    write_to_file_span(code_part, S(tmp_path), 1);
    
    // Make executable
    char chmod_cmd[512];
    snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s", tmp_path);
    system(chmod_cmd);
    
    // Execute and pipe to pandoc
    char gen_cmd[1024];
    snprintf(gen_cmd, sizeof(gen_cmd),
        "%s | pandoc -f markdown -t html --standalone --metadata title='All Wants Dashboard' -o %s",
        tmp_path, html_path);
    
    int result = system(gen_cmd);
    unlink(tmp_path);
    
    if (result != 0) {
        prt("Error: Dashboard generation failed\n");
        flush_exit(1);
    }
    
    prt("Generated: %s\n", html_path);
    flush();
}

/* #handle_work */
void handle_work(char *event_arg) {
    // Enable T-debug
    span debug_file = prs("%.*sT-debug", (int)len(state->cmprdir), state->cmprdir.buf);
    FILE *f = fopen((char*)debug_file.buf, "w");
    if (f) fclose(f);
    
    // If event provided, add it to T
    if (event_arg) {
        span event_span = {(u8*)event_arg, (u8*)event_arg + strlen(event_arg)};
        event_add(event_span, 255);
    }
    
    // Build the entr command like show-T does (use -n for non-interactive)
    span cmd = prs("echo \"%.*sT\" | entr -n -c sh -c \"cmpr --T\"", 
                   (int)len(state->cmprdir), state->cmprdir.buf);
    system((char*)cmd.buf);
}
/* #handle_trace */
void handle_trace() {
    char *t_path = ".cmpr/T";
    char prev_content[65536] = {0};
    
    // Load initial T content
    FILE *f = fopen(t_path, "r");
    if (f) {
        size_t n = fread(prev_content, 1, sizeof(prev_content)-1, f);
        prev_content[n] = '\0';
        fclose(f);
        prt("=== Initial T ===\n%s", prev_content);
    }
    
    // Watch for changes using poll on file mtime
    struct stat st, prev_st;
    if (stat(t_path, &prev_st) < 0) {
        prt("Error: Cannot stat %s\n", t_path);
        return;
    }
    
    prt("=== Watching T for changes (Ctrl+C to stop) ===\n");
    flush();
    
    while (1) {
        usleep(100000); // 100ms poll interval
        
        if (stat(t_path, &st) < 0) continue;
        
        if (st.st_mtime != prev_st.st_mtime || st.st_size != prev_st.st_size) {
            prev_st = st;
            
            char new_content[65536] = {0};
            f = fopen(t_path, "r");
            if (f) {
                size_t n = fread(new_content, 1, sizeof(new_content)-1, f);
                new_content[n] = '\0';
                fclose(f);
                
                // Print new content if different
                if (strcmp(new_content, prev_content) != 0) {
                    // Find what's new (simple: if content differs, show current state)
                    if (strlen(new_content) == 0) {
                        prt("--- T cleared ---\n");
                    } else {
                        prt("--- T changed ---\n%s", new_content);
                    }
                    flush();
                    strcpy(prev_content, new_content);
                }
            }
        }
    }
}
/* #handle_status */
void handle_status() {
    // Count inbox items
    int inbox_idx = block_from_arg("#INBOX");
    int end_inbox_idx = block_from_arg("#END_INBOX");
    int inbox_count = 0;
    if (inbox_idx >= 0 && end_inbox_idx > inbox_idx) {
        inbox_count = end_inbox_idx - inbox_idx - 1;
    }
    
    // Count wants (deduped, consistent with --wants)
    span seen[1024];
    int want_count = 0;
    for (int i = 0; i < state->blocks.n; i++) {
        span block = state->blocks.a[i];
        while (block.buf < block.end) {
            span line = head_line(&block);
            while (line.buf < line.end && (*line.buf == ' ' || *line.buf == '\t')) line.buf++;
            if (line.buf >= line.end || *line.buf != '"') continue;

            // Parse SN format: "event" N.
            u8 *p = line.end - 1;
            if (p < line.buf || *p != '.') continue;
            p--;
            while (p >= line.buf && *p >= '0' && *p <= '9') p--;
            if (p < line.buf || *p != ' ') continue;
            p--;
            if (p < line.buf || *p != '"') continue;

            span event_str = {line.buf + 1, p};
            span want_prefix = S("We want ");
            if (event_str.end - event_str.buf >= 8 &&
                memcmp(event_str.buf, want_prefix.buf, 8) == 0) {
                // Check if already seen
                int is_dup = 0;
                for (int j = 0; j < want_count; j++) {
                    if (span_eq(seen[j], event_str)) { is_dup = 1; break; }
                }
                if (!is_dup && want_count < 1024) {
                    seen[want_count++] = event_str;
                }
            }
        }
    }
    
    // Count anonymous blocks
    int anonymous_count = 0;
    for (int i = 0; i < state->blocks.n; i++) {
        span id = id_for_block(state->blocks.a[i]);
        if (empty(id)) {
            anonymous_count++;
        }
    }
    
    // Print status
    prt("Inbox: %d items pending\n", inbox_count);
    prt("Wants: %d total\n", want_count);
    prt("Blocks: %d total, %d anonymous\n", state->blocks.n, anonymous_count);
    
    flush();
}

/* #handle_event_report */
void handle_event_report() {
    const char *report_path = "public_html/event_activity.html";
    struct stat st;
    char file_date[9] = {0};
    char today_date[9] = {0};
    time_t t = time(NULL);
    struct tm tm_now;
    localtime_r(&t, &tm_now);
    strftime(today_date, sizeof(today_date), "%Y%m%d", &tm_now);

    int need_generate = 0;
    if (stat(report_path, &st) != 0) {
        need_generate = 1;
    } else {
        struct tm tm_mod;
        localtime_r(&st.st_mtime, &tm_mod);
        strftime(file_date, sizeof(file_date), "%Y%m%d", &tm_mod);
        if (strcmp(file_date, today_date) != 0) {
            need_generate = 1;
        }
    }

    if (need_generate) {
        system("mkdir -p public_html");
        int block_idx = block_by_id(S("generate_event_report"));
        if (block_idx < 0) {
            prt("Error: #generate_event_report block not found\n");
            flush();
            flush_exit(1);
        }
        span block = state->blocks.a[block_idx];
        span code = block_code_part(block);
        int pid = (int)getpid();
        char temp_file[64];
        snprintf(temp_file, sizeof(temp_file), "/tmp/gen_evt_%d.sh", pid);
        FILE *f = fopen(temp_file, "w");
        if (!f) {
            prt("Error: cannot create temp file\n");
            flush();
            flush_exit(1);
        }
        fwrite(code.buf, 1, len(code), f);
        fclose(f);
        char chmod_cmd[128];
        snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s", temp_file);
        if (system(chmod_cmd) != 0) {
            prt("Error: chmod failed\n");
            unlink(temp_file);
            flush();
            flush_exit(1);
        }
        char pipe_cmd[512];
        snprintf(pipe_cmd, sizeof(pipe_cmd),
            "%s | pandoc -f markdown -t html --standalone --metadata title='Event System Activity' -o %s",
            temp_file, report_path);
        int ret = system(pipe_cmd);
        unlink(temp_file);
        if (ret != 0) {
            prt("Error: report generation failed\n");
            flush();
            flush_exit(1);
        }
        prt("Generated: public_html/event_activity.html\n");
    } else {
        prt("Current: public_html/event_activity.html\n");
    }
}

void handle_export_docs() {
    int block_idx = block_by_id(S("generate_export_docs"));
    if (block_idx == -1) {
        prt("Error: Block #generate_export_docs not found\n");
        flush_exit(1);
    }
    
    span block = state->blocks.a[block_idx];
    span comment_part = block_comment_part(block);
    span code_part = block;
    code_part.buf = comment_part.end;
    
    if (code_part.buf == code_part.end) {
        prt("Error: #generate_export_docs has no code part\n");
        flush_exit(1);
    }
    
    // Write code to temp file
    char temp_file[256];
    snprintf(temp_file, sizeof(temp_file), "/tmp/export_docs_%d.sh", getpid());
    FILE* f = fopen(temp_file, "w");
    if (!f) {
        prt("Error: failed to create temp file\n");
        flush_exit(1);
    }
    fwrite(code_part.buf, 1, len(code_part), f);
    fclose(f);
    
    // Make executable and execute
    char chmod_cmd[512];
    snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s", temp_file);
    system(chmod_cmd);
    
    int result = system(temp_file);
    unlink(temp_file);
    
    if (result != 0) {
        prt("Error: export docs generator failed\n");
        flush_exit(1);
    }
    
    flush();
}



/* #handle_es */
void handle_es() {
  span es_dir = S(".cmpr/es/");
  span induced_dir = S(".cmpr/induced/");
  span surprise_dir = S(".cmpr/surprise-high/");
  span patterns_dir = S(".cmpr/patterns/");

  spans es = dir_listing(es_dir);
  spans pats = dir_listing(patterns_dir);

  prt("ES\tinduced\tsurprise\tpatterns\n");

  for (int i = 0; i < es.n; i++) {
    span name = es.a[i];

    span induced_path = concat(induced_dir, name);
    span surprise_path = concat(surprise_dir, name);

    int has_induced = readable_file(induced_path);
    int has_surprise = readable_file(surprise_path);

    char patbuf[1024];
    patbuf[0] = 0;
    int first = 1;

    for (int j = 0; j < pats.n; j++) {
      span fn = pats.a[j];

      int dash = find_char(fn, '-');
      if (dash <= 0) continue;
      if (dash >= len(fn) - 1) continue;

      span left = first_n(fn, dash);
      span right = skip_n(fn, dash + 1);

      span other = nullspan();
      if (span_eq(name, left)) other = right;
      else if (span_eq(name, right)) other = left;
      else continue;

      if (len(other) == 0) continue;

      if ((int)strlen(patbuf) + (first ? 0 : 1) + len(other) + 1 >= (int)sizeof(patbuf)) break;

      if (!first) strcat(patbuf, ",");
      first = 0;

      char tmp[512];
      int n = len(other);
      if (n >= (int)sizeof(tmp)) n = (int)sizeof(tmp) - 1;
      memcpy(tmp, other.buf, n);
      tmp[n] = 0;
      strcat(patbuf, tmp);
    }

    prt("%s\t%s\t%s\t%s\n",
        s(name),
        has_induced ? s(name) : "-",
        has_surprise ? s(name) : "-",
        patbuf[0] ? patbuf : "-");
  }

  flush();
}



/* #handle_P */
void handle_P() {
  span es_dir = S(".cmpr/es/");
  span induced_dir = S(".cmpr/induced/");
  span induced_single_dir = S(".cmpr/induced-single/");
  span surprise_dir = S(".cmpr/surprise-high/");
  span patterns_dir = S(".cmpr/patterns/");

  spans es_list = dir_listing(es_dir);
  spans induced_list = dir_listing(induced_dir);
  spans single_list = dir_listing(induced_single_dir);
  spans surprise_list = dir_listing(surprise_dir);
  spans patterns_list = dir_listing(patterns_dir);

  int es_count = es_list.n;
  int induced_count = induced_list.n;
  int single_count = single_list.n;
  int surprise_count = surprise_list.n;
  int lpp_count = patterns_list.n;

  prt("PATTERN OVERVIEW\n");
  prt("================\n\n");

  // Section 1: Event Spaces
  prt("Event Spaces (%d):\n", es_count);
  prt("  %-20s %-40s %s\n", "ES", "Example Match", "Reactive");
  prt("  ");
  for (int i = 0; i < 70; i++) prt("-");
  prt("\n");

  for (int i = 0; i < es_list.n; i++) {
    span name = es_list.a[i];

    // Check what's attached to this ES
    span induced_path = concat(induced_dir, name);
    span surprise_path = concat(surprise_dir, name);
    int has_induced = readable_file(induced_path);
    int has_surprise = readable_file(surprise_path);

    // Find LPP connections
    char lpp_buf[256];
    lpp_buf[0] = 0;
    for (int j = 0; j < patterns_list.n; j++) {
      span fn = patterns_list.a[j];
      int dash = find_char(fn, '-');
      if (dash <= 0 || dash >= len(fn) - 1) continue;
      span left = first_n(fn, dash);
      span right = skip_n(fn, dash + 1);
      span other = nullspan();
      char arrow[4] = "";
      if (span_eq(name, left)) { other = right; strcpy(arrow, "->"); }
      else if (span_eq(name, right)) { other = left; strcpy(arrow, "<-"); }
      if (len(other) > 0) {
        if (strlen(lpp_buf) > 0) strcat(lpp_buf, ",");
        strcat(lpp_buf, "LPP");
        strcat(lpp_buf, arrow);
        char tmp[64];
        int n = len(other);
        if (n >= (int)sizeof(tmp)) n = (int)sizeof(tmp) - 1;
        memcpy(tmp, other.buf, n);
        tmp[n] = 0;
        strcat(lpp_buf, tmp);
      }
    }

    // Read the ES filter to get example pattern
    span es_path = concat(es_dir, name);
    span content = read_file_into_cmp(es_path);
    char example[48] = "";
    // Extract grep pattern from filter script
    span line = content;
    while (len(line) > 0) {
      span l = next_line(&line);
      if (starts_with(l, S("grep"))) {
        // Find quoted pattern
        int q1 = find_char(l, '\'');
        if (q1 >= 0) {
          span rest = skip_n(l, q1 + 1);
          int q2 = find_char(rest, '\'');
          if (q2 > 0) {
            int n = q2;
            if (n > 40) n = 40;
            memcpy(example, rest.buf, n);
            example[n] = 0;
          }
        }
        break;
      }
    }

    // Build reactive column
    char reactive[128];
    reactive[0] = 0;
    if (has_induced) {
      strcat(reactive, "induced");
    }
    if (has_surprise) {
      if (strlen(reactive) > 0) strcat(reactive, ",");
      strcat(reactive, "surprise");
    }
    if (strlen(lpp_buf) > 0) {
      if (strlen(reactive) > 0) strcat(reactive, ",");
      strcat(reactive, lpp_buf);
    }
    if (strlen(reactive) == 0) strcpy(reactive, "-");

    prt("  %-20s %-40s %s\n", s(name), example, reactive);
  }

  // Section 2: Induced Scripts
  prt("\nInduced (%d):\n", induced_count);
  if (induced_count == 0) {
    prt("  (none)\n");
  } else {
    for (int i = 0; i < induced_list.n; i++) {
      span name = induced_list.a[i];
      span path = concat(induced_dir, name);
      prt("  %-20s -> %s\n", s(name), s(path));
    }
  }

  // Section 3: Induced-Single Triggers
  prt("\nInduced-Single (%d):\n", single_count);
  if (single_count == 0) {
    prt("  (none)\n");
  } else {
    for (int i = 0; i < single_list.n; i++) {
      span dirname = single_list.a[i];
      span name_path = concat(concat(induced_single_dir, dirname), S("/name"));
      span script_path = concat(concat(induced_single_dir, dirname), S("/script"));
      
      if (readable_file(name_path)) {
        span name_content = trim(read_file_into_cmp(name_path));
        char truncated[60];
        int n = len(name_content);
        if (n > 55) {
          memcpy(truncated, name_content.buf, 52);
          strcpy(truncated + 52, "...");
        } else {
          memcpy(truncated, name_content.buf, n);
          truncated[n] = 0;
        }
        prt("  [%s] \"%s\"\n", s(dirname), truncated);
        prt("       -> %s\n", s(script_path));
      }
    }
  }

  // Section 4: Surprise Handlers
  prt("\nSurprise (%d):\n", surprise_count);
  if (surprise_count == 0) {
    prt("  (none)\n");
  } else {
    for (int i = 0; i < surprise_list.n; i++) {
      span name = surprise_list.a[i];
      span path = concat(surprise_dir, name);
      prt("  %-20s -> %s\n", s(name), s(path));
    }
  }

  // Section 5: LPP Patterns
  prt("\nLPP Patterns (%d):\n", lpp_count);
  if (lpp_count == 0) {
    prt("  (none)\n");
  } else {
    for (int i = 0; i < patterns_list.n; i++) {
      span fn = patterns_list.a[i];
      int dash = find_char(fn, '-');
      if (dash <= 0 || dash >= len(fn) - 1) continue;
      span left = first_n(fn, dash);
      span right = skip_n(fn, dash + 1);
      
      // Count rules in pattern file
      span pat_path = concat(patterns_dir, fn);
      span content = read_file_into_cmp(pat_path);
      int rule_count = 0;
      span tmp = content;
      while (len(tmp) > 0) {
        span line = next_line(&tmp);
        if (len(trim(line)) > 0) rule_count++;
      }
      
      prt("  %-15s x %-15s  (%d rules)\n", s(left), s(right), rule_count);
    }
  }

  prt("\nSummary: %d ES, %d induced, %d induced-single, %d surprise, %d LPP\n",
      es_count, induced_count, single_count, surprise_count, lpp_count);

  flush();
}
/* #handle_E */
void handle_E() {
  span index_path = S(".cmpr/event-names");
  span events_dir = S(".cmpr/events/");
  span es_dir = S(".cmpr/es/");
  span T_path = S(".cmpr/T");

  // List memory files (sorted by name, which is timestamp-based)
  spans memories = dir_listing(events_dir);
  int memories_count = memories.n;

  // Check if index exists and is complete
  struct stat index_stat;
  int have_index = (stat(s(index_path), &index_stat) == 0);
  time_t index_mtime = have_index ? index_stat.st_mtime : 0;
  
  // Check if oldest memory is older than index - if so, index is incomplete
  if (have_index && memories.n > 0) {
    span oldest = concat(events_dir, memories.a[0]);
    struct stat oldest_stat;
    if (stat(s(oldest), &oldest_stat) == 0) {
      if (oldest_stat.st_mtime < index_mtime) {
        unlink(s(index_path));
        have_index = 0;
        index_mtime = 0;
      }
    }
  }

  // Scan memories newer than index (or all if no index)
  // Use streaming: save/restore cmp.end to reuse buffer space
  int updated = 0;
  for (int i = 0; i < memories.n; i++) {
    span memfile = concat(events_dir, memories.a[i]);
    struct stat mem_stat;
    if (stat(s(memfile), &mem_stat) != 0) continue;
    
    if (have_index && mem_stat.st_mtime <= index_mtime) continue;
    
    // Save cmp position
    u8 *saved_cmp_end = cmp.end;
    
    span content = read_file_into_cmp(memfile);
    
    while (!empty(content)) {
      span line = head_line(&content);
      if (empty(line)) continue;
      
      while (!empty(line) && (*line.buf == ' ' || *line.buf == '\t')) line.buf++;
      if (empty(line)) continue;
      if (*line.buf != '"') continue;
      line.buf++;
      
      u8 *p = line.end - 1;
      if (p < line.buf || *p != '.') continue;
      p--;
      
      u8 *digits_end = p + 1;
      while (p >= line.buf && *p >= '0' && *p <= '9') p--;
      u8 *digits_start = p + 1;
      if (digits_start >= digits_end) continue;
      
      if (p < line.buf || *p != ' ') continue;
      p--;
      if (p < line.buf || *p != '"') continue;
      
      span event_str = (span){line.buf, p};
      event_get_id(event_str);
      updated++;
    }
    
    // Restore cmp position - reuse buffer space
    cmp.end = saved_cmp_end;
  }

  // Read the index to count unique events
  int event_count = 0;
  spans event_lines = {0};
  if (readable_file(index_path)) {
    span index_content = read_file_into_cmp(index_path);
    event_lines = spans_alloc(256);
    while (!empty(index_content)) {
      span line = next_line(&index_content);
      if (len(trim(line)) > 0) {
        spans_push(&event_lines, line);
        event_count++;
      }
    }
  }

  // Get ES list and extract patterns
  spans es_list = dir_listing(es_dir);
  int *es_counts = calloc(es_list.n, sizeof(int));
  int unclassified = 0;
  
  char **es_patterns = calloc(es_list.n, sizeof(char*));
  int *es_extended = calloc(es_list.n, sizeof(int));
  
  for (int j = 0; j < es_list.n; j++) {
    span es_path = concat(es_dir, es_list.a[j]);
    span content = read_file_into_cmp(es_path);
    es_extended[j] = (strstr(s(content), "-E") != NULL) ? 1 : 0;
    int q1 = find_char(content, '\'');
    if (q1 >= 0) {
      span rest = skip_n(content, q1 + 1);
      int q2 = find_char(rest, '\'');
      if (q2 > 0) {
        char *pat = malloc(q2 + 1);
        memcpy(pat, rest.buf, q2);
        pat[q2] = 0;
        es_patterns[j] = pat;
      }
    }
  }
  
  // Match events against patterns
  for (int i = 0; i < event_lines.n; i++) {
    span event = event_lines.a[i];
    char sn_buf[4096];
    int n = len(event);
    if (n > 4000) n = 4000;
    snprintf(sn_buf, sizeof(sn_buf), "\"%.*s\" 255.", n, event.buf);
    
    int matched = 0;
    for (int j = 0; j < es_list.n; j++) {
      if (!es_patterns[j]) continue;
      
      regex_t regex;
      int flags = REG_NOSUB;
      if (es_extended[j]) flags |= REG_EXTENDED;
      
      if (regcomp(&regex, es_patterns[j], flags) == 0) {
        if (regexec(&regex, sn_buf, 0, NULL, 0) == 0) {
          es_counts[j]++;
          matched = 1;
        }
        regfree(&regex);
      }
    }
    if (!matched) unclassified++;
  }
  
  for (int j = 0; j < es_list.n; j++) {
    if (es_patterns[j]) free(es_patterns[j]);
  }
  free(es_patterns);
  free(es_extended);

  // Read current T
  int T_count = 0;
  if (readable_file(T_path)) {
    span T_content = read_file_into_cmp(T_path);
    while (!empty(T_content)) {
      span line = next_line(&T_content);
      if (len(trim(line)) > 0 && *trim(line).buf == '"') T_count++;
    }
  }

  // Print output
  prt("EVENT SYSTEM\n");
  prt("============\n\n");
  prt("|E| = %d unique events ever observed\n", event_count);
  prt("|M| = %d memories in .cmpr/events/\n", memories_count);
  if (!have_index && updated > 0) {
    prt("(rebuilt index from %d event occurrences)\n", updated);
  }
  prt("\nCurrent T: %d events\n", T_count);

  prt("\nEvent Space Coverage:\n");
  prt("  %-20s %s\n", "ES", "Indexed Events");
  prt("  ");
  for (int i = 0; i < 35; i++) prt("-");
  prt("\n");
  
  for (int i = 0; i < es_list.n; i++) {
    if (es_counts[i] > 0) {
      prt("  %-20s %d\n", s(es_list.a[i]), es_counts[i]);
    }
  }
  if (unclassified > 0) {
    prt("  %-20s %d\n", "(unclassified)", unclassified);
  }

  free(es_counts);
  flush();
}
/* #handle_induced */
void handle_induced(char *es_name) {
  // Check ES exists
  span es_path = prs(".cmpr/es/%s", es_name);
  if (!readable_file(es_path)) {
    prt("Error: Event space '%s' does not exist.\n", es_name);
    prt("Create it first or check .cmpr/es/ for available ES.\n");
    flush_exit(1);
  }
  
  // Check induced script doesn't already exist
  span induced_path = prs(".cmpr/induced/%s", es_name);
  if (readable_file(induced_path)) {
    prt("Error: Induced script already exists: %s\n", s(induced_path));
    prt("Edit it directly or remove it first.\n");
    flush_exit(1);
  }
  
  // Create induced directory if needed
  mkdir(".cmpr/induced", 0755);
  
  // Write hello-world template
  FILE *f = fopen(s(induced_path), "w");
  if (!f) {
    prt("Error: Cannot create %s\n", s(induced_path));
    flush_exit(1);
  }
  
  fprintf(f, "#!/bin/bash\n");
  fprintf(f, "# Induced script for ES: %s\n", es_name);
  fprintf(f, "# This runs when events matching this ES enter T.\n");
  fprintf(f, "# Edit this script to define the desired behavior.\n");
  fprintf(f, "\n");
  fprintf(f, "echo \"[induced/%s] fired\" >&2\n", es_name);
  fprintf(f, "cmpr --event \"Induced %s ran.\" --strength 255\n", es_name);
  fclose(f);
  
  // Make executable
  chmod(s(induced_path), 0755);
  
  prt("Created: %s\n", s(induced_path));
  prt("Edit this script to define what happens when %s events enter T.\n", es_name);
  flush();
}
/* #handle_induced_single */
void handle_induced_single(char *event_str) {
  // Find next available slot number
  mkdir(".cmpr/induced-single", 0755);
  
  int slot = 1;
  while (1) {
    span dir_path = prs(".cmpr/induced-single/%d", slot);
    struct stat st;
    if (stat(s(dir_path), &st) != 0) break;
    slot++;
    if (slot > 9999) {
      prt("Error: Too many induced-single entries.\n");
      flush_exit(1);
    }
  }
  
  // Create the slot directory
  span slot_dir = prs(".cmpr/induced-single/%d", slot);
  mkdir(s(slot_dir), 0755);
  
  // Write the name file
  span name_path = prs(".cmpr/induced-single/%d/name", slot);
  FILE *f = fopen(s(name_path), "w");
  if (!f) {
    prt("Error: Cannot create %s\n", s(name_path));
    flush_exit(1);
  }
  fprintf(f, "%s\n", event_str);
  fclose(f);
  
  // Write hello-world script
  span script_path = prs(".cmpr/induced-single/%d/script", slot);
  f = fopen(s(script_path), "w");
  if (!f) {
    prt("Error: Cannot create %s\n", s(script_path));
    flush_exit(1);
  }
  
  fprintf(f, "#!/bin/bash\n");
  fprintf(f, "# Induced-single script for exact event:\n");
  fprintf(f, "# \"%s\"\n", event_str);
  fprintf(f, "# This runs when this exact event enters T.\n");
  fprintf(f, "# Edit this script to define the desired behavior.\n");
  fprintf(f, "\n");
  fprintf(f, "echo \"[induced-single/%d] fired\" >&2\n", slot);
  fprintf(f, "cmpr --event \"Induced-single %d ran.\" --strength 255\n", slot);
  fclose(f);
  
  chmod(s(script_path), 0755);
  
  prt("Created induced-single slot %d:\n", slot);
  prt("  %s\n", s(name_path));
  prt("  %s\n", s(script_path));
  prt("Edit the script to define what happens when this event enters T.\n");
  flush();
}
/* #handle_lpp */
void handle_lpp(char *es1, char *es2) {
  // Check both ES exist
  span es1_path = prs(".cmpr/es/%s", es1);
  span es2_path = prs(".cmpr/es/%s", es2);
  
  if (!readable_file(es1_path)) {
    prt("Error: Event space '%s' does not exist.\n", es1);
    flush_exit(1);
  }
  if (!readable_file(es2_path)) {
    prt("Error: Event space '%s' does not exist.\n", es2);
    flush_exit(1);
  }
  
  // Create patterns directory if needed
  mkdir(".cmpr/patterns", 0755);
  
  span pattern_path = prs(".cmpr/patterns/%s-%s", es1, es2);
  
  if (readable_file(pattern_path)) {
    prt("LPP pattern file exists: %s\n", s(pattern_path));
    prt("Edit it directly to add or modify rules.\n");
    flush_exit(0);
  }
  
  // Write template
  FILE *f = fopen(s(pattern_path), "w");
  if (!f) {
    prt("Error: Cannot create %s\n", s(pattern_path));
    flush_exit(1);
  }
  
  fprintf(f, "# LPP Pattern: %s x %s\n", es1, es2);
  fprintf(f, "# Inference rules for when events from both ES co-occur in T.\n");
  fprintf(f, "#\n");
  fprintf(f, "# Format: Each line is a rule that fires when both ES match.\n");
  fprintf(f, "# Edit this file to define inference rules.\n");
  fprintf(f, "#\n");
  fprintf(f, "# Example rule (uncomment and modify):\n");
  fprintf(f, "# if %s and %s then infer X\n", es1, es2);
  fclose(f);
  
  prt("Created: %s\n", s(pattern_path));
  prt("Edit this file to define inference rules between %s and %s.\n", es1, es2);
  flush();
}
/* #handle_es_create */
void handle_es_create(char *name, char *pattern) {
  // Check ES doesn't already exist
  span es_path = prs(".cmpr/es/%s", name);
  if (readable_file(es_path)) {
    prt("Error: Event space '%s' already exists.\n", name);
    prt("Edit it directly or remove it first.\n");
    flush_exit(1);
  }
  
  // Create es directory if needed
  mkdir(".cmpr/es", 0755);
  
  // Write the ES filter script
  FILE *f = fopen(s(es_path), "w");
  if (!f) {
    prt("Error: Cannot create %s\n", s(es_path));
    flush_exit(1);
  }
  
  fprintf(f, "#!/bin/sh\n");
  fprintf(f, "grep -E '%s'\n", pattern);
  fclose(f);
  
  chmod(s(es_path), 0755);
  
  prt("Created: %s\n", s(es_path));
  prt("Pattern: %s\n", pattern);
  flush();
}
/* #handle_history */
void handle_history(span blockid, double log_gap_factor, int limit) {
    get_revs();
    clear_display();
    if (!empty(blockid))
        handle_history_blockid(blockid, log_gap_factor, limit);
    else
        handle_history_recent(log_gap_factor, limit);
}
/* #handle_history_blockid */
void handle_history_blockid(span blockid, double log_gap_factor, int limit) {
    struct version_entry {
        checksum ck;
        time_t ts;
        int bytes;
        int lines;
    };
    size_t cap = 256, n = 0;
    struct version_entry* versions = malloc(cap * sizeof(*versions));

    size_t scan_total = state->revs.n_revblocks;
    for (size_t i = 0; i < scan_total; ++i) {
        if (i % 1000 == 0) {
            fprintf(stderr, "\rScanning: %zu/%zu", i, scan_total);
            fflush(stderr);
        }
        spans_arena_push();
        spans ids = load_revblock_ids(i);
        int found = 0;
        for (int j = 0; j < ids.n; ++j) {
            if (span_eq(ids.a[j], blockid)) {
                found = 1;
                break;
            }
        }
        spans_arena_pop();
        if (!found)
            continue;
        rev_block* rb = &state->revs.revblocks[i];
        checksum ckval = selected_checksum(rb->contents);
        int exists = 0;
        for (size_t j = 0; j < n; ++j) {
            if (versions[j].ck.__u == ckval.__u) {
                if (rb->timestamp < versions[j].ts)
                    versions[j].ts = rb->timestamp;
                exists = 1;
                break;
            }
        }
        if (exists)
            continue;
        int linecount = 0;
        span iter = rb->contents;
        while (!empty(iter)) {
            (void)next_line(&iter);
            ++linecount;
        }
        if (n == cap) {
            cap *= 2;
            versions = realloc(versions, cap * sizeof(*versions));
        }
        versions[n].ck = ckval;
        versions[n].ts = rb->timestamp;
        versions[n].bytes = len(rb->contents);
        versions[n].lines = linecount;
        ++n;
    }
    fprintf(stderr, "\r%*s\r", 40, ""); fflush(stderr);

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i+1; j < n; ++j) {
            if (versions[j].ts > versions[i].ts) {
                struct version_entry tmp = versions[i];
                versions[i] = versions[j];
                versions[j] = tmp;
            }
        }
    }

    prt("History for %.*s\n\n", len(blockid), blockid.buf);
    int shown = 0;
    time_t last_ts = 0;
    double gap = 1.0;
    char timebuf[32];
    for (size_t i = 0; i < n; ++i) {
        if (limit > 0 && shown >= limit)
            break;
        time_t ts = versions[i].ts;
        if (log_gap_factor > 0 && shown > 0 && difftime(last_ts, ts) < gap)
            continue;
        struct tm tmres;
        localtime_r(&ts, &tmres);
        strftime(timebuf, sizeof timebuf, "%Y-%m-%d %H:%M:%S", &tmres);
        prt("  %s  %d bytes  %d lines\n", timebuf, versions[i].bytes, versions[i].lines);
        last_ts = ts;
        gap *= log_gap_factor > 0 ? log_gap_factor : 1.0;
        ++shown;
    }
    free(versions);
    flush();
}

/* #handle_history_recent */
void handle_history_recent(double log_gap_factor, int limit) {
    typedef struct { span id; checksum ck; time_t ts; } state_ent;
    typedef struct { time_t ts; span id; } event_ent;

    size_t n_revblocks = state->revs.n_revblocks;
    size_t states_cap = 1024, states_n = 0;
    state_ent* states = malloc(states_cap * sizeof(state_ent));

    size_t events_cap = 512, events_n = 0;
    event_ent* events = malloc(events_cap * sizeof(event_ent));

    int effective_limit = limit > 0 ? limit : 20;

    for (size_t i = 0; i < n_revblocks && events_n < (size_t)effective_limit * 3; i++) {
        if (i % 1000 == 0 && i > 0) {
            fprintf(stderr, "\rScanning revblock %zu/%zu...", i, n_revblocks);
            fflush(stderr);
        }
        spans_arena_push();
        rev_block* rb = &state->revs.revblocks[i];
        spans ids = load_revblock_ids((int)i);
        if (ids.n == 0) { spans_arena_pop(); continue; }
        span bid = ids.a[0];
        checksum ck = selected_checksum(bid);

        // Search state for id
        size_t state_idx = 0;
        int found = 0;
        for (; state_idx < states_n; state_idx++) {
            if (len(bid) == len(states[state_idx].id) &&
                memcmp(bid.buf, states[state_idx].id.buf, len(bid)) == 0) {
                found = 1;
                break;
            }
        }
        if (found) {
            // Compare checksums
            if (ck.__u != states[state_idx].ck.__u) {
                // Record event
                if (events_n == events_cap) { events_cap *= 2; events = realloc(events, events_cap * sizeof(event_ent)); }
                u8 *idcopy = malloc(len(bid));
                memcpy(idcopy, bid.buf, len(bid));
                events[events_n++] = (event_ent){states[state_idx].ts, (span){idcopy, idcopy + len(bid)}};
                // Update state
                free(states[state_idx].id.buf); // Free old id copy
                u8 *newcopy = malloc(len(bid));
                memcpy(newcopy, bid.buf, len(bid));
                states[state_idx].id = (span){newcopy, newcopy+len(bid)};
                states[state_idx].ck = ck;
                states[state_idx].ts = rb->timestamp;
            } else {
                // Keep oldest occurrence
                if (rb->timestamp < states[state_idx].ts)
                    states[state_idx].ts = rb->timestamp;
            }
        } else {
            if (states_n == states_cap) { states_cap *= 2; states = realloc(states, states_cap * sizeof(state_ent)); }
            u8 *idcopy = malloc(len(bid));
            memcpy(idcopy, bid.buf, len(bid));
            states[states_n++] = (state_ent){(span){idcopy, idcopy+len(bid)}, ck, rb->timestamp};
        }
        spans_arena_pop();
    }
    fprintf(stderr, "\r%*s\r", 40, ""); // Clear progress

    // Bubble sort events newest-first (small N)
    for (size_t i = 0; i + 1 < events_n; i++) {
        for (size_t j = 0; j + 1 < events_n - i; j++) {
            if (events[j].ts < events[j+1].ts) {
                event_ent tmp = events[j];
                events[j] = events[j+1];
                events[j+1] = tmp;
            }
        }
    }

    prt("Recent block changes\n\n");
    size_t printed = 0;
    time_t last_ts = 0;
    for (size_t i = 0; i < events_n && (int)printed < effective_limit; i++) {
        if (printed == 0 || log_gap_factor <= 0 ||
            last_ts == 0 || difftime(last_ts, events[i].ts) > log_gap_factor * log1p((double)(last_ts - events[i].ts))) {
            char tsbuf[32];
            struct tm tm;
            localtime_r(&events[i].ts, &tm);
            strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%d %H:%M:%S", &tm);
            prt("  %s  %.*s\n", tsbuf, (int)len(events[i].id), events[i].id.buf);
            last_ts = events[i].ts;
            printed++;
        }
    }

    for (size_t i = 0; i < states_n; i++) free(states[i].id.buf);
    for (size_t i = 0; i < events_n; i++) free(events[i].id.buf);
    free(states);
    free(events);
    flush();
}
/* #grep_blocks */
void grep_blocks(span pattern) {
    regex_t regex;
    char pattern_buf[4096];
    
    if (len(pattern) >= (int)sizeof(pattern_buf)) {
        prt("Error: pattern too long\n");
        exit(1);
    }
    memcpy(pattern_buf, pattern.buf, len(pattern));
    pattern_buf[len(pattern)] = '\0';
    
    int ret = regcomp(&regex, pattern_buf, REG_EXTENDED);
    if (ret != 0) {
        char errbuf[256];
        regerror(ret, &regex, errbuf, sizeof(errbuf));
        prt("Error: invalid regex: %s\n", errbuf);
        exit(1);
    }
    
    int first_match = 1;
    
    for (int i = 0; i < state->blocks.n; i++) {
        span block = state->blocks.a[i];
        span comment = block_comment_part(block);
        span code = block_code_part(block);
        
        // Handle nullspan returns
        if (empty(comment) || comment.buf == NULL) {
            comment = nullspan();
        }
        if (empty(code) || code.buf == NULL) {
            code = nullspan();
        }
        
        int comment_len = len(comment);
        int code_len = len(code);
        
        // Sanity check lengths
        if (comment_len < 0) comment_len = 0;
        if (code_len < 0) code_len = 0;
        
        char *comment_str = (char *)malloc(comment_len + 1);
        char *code_str = (char *)malloc(code_len + 1);
        
        if (comment_str == NULL || code_str == NULL) {
            prt("Error: out of memory\n");
            regfree(&regex);
            exit(1);
        }
        
        if (comment_len > 0 && comment.buf != NULL) {
            memcpy(comment_str, comment.buf, comment_len);
        }
        comment_str[comment_len] = '\0';
        
        if (code_len > 0 && code.buf != NULL) {
            memcpy(code_str, code.buf, code_len);
        }
        code_str[code_len] = '\0';
        
        int comment_matches = (regexec(&regex, comment_str, 0, NULL, 0) == 0);
        int code_matches = (regexec(&regex, code_str, 0, NULL, 0) == 0);
        
        free(comment_str);
        free(code_str);
        
        if (comment_matches || code_matches) {
            if (!first_match) {
                prt(" ");
            }
            first_match = 0;
            
            span id = id_for_block(block);
            if (len(id) > 0) {
                if (comment_matches) {
                    wrs(id);
                } else {
                    wrs(id);
                    prt(":code");
                }
            }
        }
    }
    
    prt("\n");
    regfree(&regex);
}


/* #after */
void after(span arg) {
    int block_idx = block_id_arg(arg);
    if (block_idx == -1) {
        prt("Block not found: %.*s\n", len(arg), arg.buf);
        flush_err();
        exit(1);
    }

    span new_content = read_stdin_into_cmp();
    dbgd(len(new_content));
    if (len(inp) + len(new_content) >= BUF_SZ) {
        prt("Too much input to insert, buffer size exceeded\n");
        flush_err();
        exit(1);
    }

    span block = state->blocks.a[block_idx];
    int file_idx = file_for_block(block);

    projfile *pf = &state->files.a[file_idx];

    u8 *after_block = block.end;
    size_t tail_len = inp.end - after_block;

    memmove(after_block + len(new_content), after_block, tail_len);
    inp.end += len(new_content);
    memcpy(after_block, new_content.buf, len(new_content));

    pf = &state->files.a[file_idx];
    pf->contents.end += len(new_content);

    for (int i = file_idx + 1; i < state->files.n; ++i) {
        state->files.a[i].contents.buf += len(new_content);
        state->files.a[i].contents.end += len(new_content);
    }

    ingest();

    new_rev(S(""), file_idx);
}




/* #replace */
void replace(span arg) {
    int block_idx = block_id_arg(arg);
    if (block_idx == -1) {
        prt("Block not found: %.*s\n", len(arg), arg.buf);
        flush_err();
        exit(1);
    }

    span new_content = read_stdin_into_cmp();

    if (len(new_content) == 0 || new_content.end[-1] != '\n') {
        if (len(inp) + len(new_content) + 1 >= BUF_SZ) {
            prt("Too much input to insert, buffer size exceeded\n");
            flush_err();
            exit(1);
        }
        *new_content.end++ = '\n';
    }

    if (len(inp) + len(new_content) >= BUF_SZ) {
        prt("Too much input to insert, buffer size exceeded\n");
        flush_err();
        exit(1);
    }

    span block = state->blocks.a[block_idx];
    int file_idx = file_for_block(block);

    projfile *pf = &state->files.a[file_idx];

    size_t old_block_len = len(block);
    u8 *block_start = block.buf;
    u8 *block_end = block.end;
    size_t tail_len = inp.end - block_end;

    ssize_t diff = (ssize_t)len(new_content) - (ssize_t)old_block_len;

    if (diff > 0) {
        if (len(inp) + diff >= BUF_SZ) {
            prt("Too much input to replace, buffer size exceeded\n");
            flush_err();
            exit(1);
        }
        memmove(block_end + diff, block_end, tail_len);
        inp.end += diff;
    } else if (diff < 0) {
        memmove(block_end + diff, block_end, tail_len);
        inp.end += diff;
    }

    memcpy(block_start, new_content.buf, len(new_content));

    pf->contents.end += diff;

    for (int i = file_idx + 1; i < state->files.n; ++i) {
        state->files.a[i].contents.buf += diff;
        state->files.a[i].contents.end += diff;
    }

    ingest();

    new_rev(S(""), file_idx);
}


/* #handle_replace_current */
void handle_replace_current(void) {
    span input = read_stdin_into_cmp();
    
    // Parse headers
    span id = nullspan();
    u64 expected_checksum = 0;
    int found_checksum = 0;
    
    span rest = input;
    span body = nullspan();
    
    while (len(rest) > 0) {
        span line = next_line(&rest);
        
        // Blank line marks end of headers
        if (len(line) == 0 || (len(line) == 1 && line.buf[0] == '\r')) {
            body = rest;
            break;
        }
        
        // Parse "Key: Value" format
        u8 *colon = memchr(line.buf, ':', len(line));
        if (!colon) continue;
        
        span key = { line.buf, colon };
        span value = { colon + 1, line.end };
        
        // Skip leading whitespace in value
        while (len(value) > 0 && (value.buf[0] == ' ' || value.buf[0] == '\t')) {
            value.buf++;
        }
        // Trim trailing whitespace/CR
        while (len(value) > 0 && (value.end[-1] == '\r' || value.end[-1] == ' ')) {
            value.end--;
        }
        
        if (span_eq(key, S("ID"))) {
            id = value;
        } else if (span_eq(key, S("Checksum"))) {
            // Parse hex checksum
            char hex[17] = {0};
            int hexlen = len(value) < 16 ? len(value) : 16;
            memcpy(hex, value.buf, hexlen);
            expected_checksum = strtoull(hex, NULL, 16);
            found_checksum = 1;
        }
    }
    
    // Validate required headers
    if (len(id) == 0) {
        prt("Error: OFRA input missing ID header\n");
        flush_exit(1);
    }
    if (!found_checksum) {
        prt("Error: OFRA input missing Checksum header\n");
        flush_exit(1);
    }
    if (len(body) == 0) {
        prt("Error: OFRA input missing body (no blank line after headers)\n");
        flush_exit(1);
    }
    
    // Look up block by ID
    int block_idx = block_id_arg(id);
    if (block_idx == -1) {
        prt("Block not found: %.*s\n", (int)len(id), id.buf);
        flush_exit(1);
    }
    
    span block = state->blocks.a[block_idx];
    
    // Compute current checksum
    checksum current_cs = selected_checksum(block);
    
    // Compare checksums
    if (current_cs.__u != expected_checksum) {
        prt("Error: Checksum mismatch - block was modified since editing began\n");
        prt("Expected: %016llX\n", (unsigned long long)expected_checksum);
        prt("Current:  %016llX\n", (unsigned long long)current_cs.__u);
        flush_exit(1);
    }
    
    // Ensure body ends with newline
    span new_content = body;
    if (len(new_content) == 0 || new_content.end[-1] != '\n') {
        if (len(inp) + len(new_content) + 1 >= BUF_SZ) {
            prt("Too much input, buffer size exceeded\n");
            flush_exit(1);
        }
        *new_content.end++ = '\n';
    }
    
    // Perform replacement (similar to replace())
    int file_idx = file_for_block(block);
    projfile *pf = &state->files.a[file_idx];
    
    size_t old_block_len = len(block);
    u8 *block_start = block.buf;
    u8 *block_end = block.end;
    size_t tail_len = inp.end - block_end;
    
    ssize_t diff = (ssize_t)len(new_content) - (ssize_t)old_block_len;
    
    if (diff > 0) {
        if (len(inp) + diff >= BUF_SZ) {
            prt("Too much input to replace, buffer size exceeded\n");
            flush_exit(1);
        }
        memmove(block_end + diff, block_end, tail_len);
        inp.end += diff;
    } else if (diff < 0) {
        memmove(block_end + diff, block_end, tail_len);
        inp.end += diff;
    }
    
    memcpy(block_start, new_content.buf, len(new_content));
    
    pf->contents.end += diff;
    
    for (int i = file_idx + 1; i < state->files.n; ++i) {
        state->files.a[i].contents.buf += diff;
        state->files.a[i].contents.end += diff;
    }
    
    ingest();
    
    new_rev(S(""), file_idx);
}
/* #replace_comment */
void replace_comment(span arg) {
    int block_idx = block_id_arg(arg);
    if (block_idx == -1) {
        prt("Block not found: %.*s\n", len(arg), arg.buf);
        flush_err();
        exit(1);
    }

    span new_comment = read_stdin_into_cmp();
    if (len(new_comment) == 0 || new_comment.end[-1] != '\n') {
        if (cmp.end + 1 >= cmp_space + BUF_SZ) {
            prt("Too much input to insert, buffer size exceeded\n");
            flush_err();
            exit(1);
        }
        *cmp.end++ = '\n';
        new_comment.end++;
    }

    span block = state->blocks.a[block_idx];
    span code_part = block_code_part(block);

    // Append code part to cmp after new comment
    if (cmp.end + len(code_part) >= cmp_space + BUF_SZ) {
        prt("Too much input to insert, buffer size exceeded\n");
        flush_err();
        exit(1);
    }
    memcpy(cmp.end, code_part.buf, len(code_part));
    cmp.end += len(code_part);

    span new_content = (span){new_comment.buf, cmp.end};

    if (len(inp) + len(new_content) >= BUF_SZ) {
        prt("Too much input to insert, buffer size exceeded\n");
        flush_err();
        exit(1);
    }

    int file_idx = file_for_block(block);
    projfile *pf = &state->files.a[file_idx];

    size_t old_block_len = len(block);
    u8 *block_start = block.buf;
    u8 *block_end = block.end;
    size_t tail_len = inp.end - block_end;

    ssize_t diff = (ssize_t)len(new_content) - (ssize_t)old_block_len;

    if (diff > 0) {
        if (len(inp) + diff >= BUF_SZ) {
            prt("Too much input to replace, buffer size exceeded\n");
            flush_err();
            exit(1);
        }
        memmove(block_end + diff, block_end, tail_len);
        inp.end += diff;
    } else if (diff < 0) {
        memmove(block_end + diff, block_end, tail_len);
        inp.end += diff;
    }

    memcpy(block_start, new_content.buf, len(new_content));

    pf->contents.end += diff;

    for (int i = file_idx + 1; i < state->files.n; ++i) {
        state->files.a[i].contents.buf += diff;
        state->files.a[i].contents.end += diff;
    }

    ingest();

    new_rev(S(""), file_idx);
}

void replace_code(span arg) {
    int block_idx = block_id_arg(arg);
    if (block_idx == -1) {
        prt("Block not found: %.*s\n", len(arg), arg.buf);
        flush_err();
        exit(1);
    }

    span new_code = read_stdin_into_cmp();
    if (len(new_code) == 0 || new_code.end[-1] != '\n') {
        if (cmp.end + 1 >= cmp_space + BUF_SZ) {
            prt("Too much input to insert, buffer size exceeded\n");
            flush_err();
            exit(1);
        }
        *cmp.end++ = '\n';
        new_code.end++;
    }

    span block = state->blocks.a[block_idx];
    span comment_part = block_comment_part(block);

    // Build new content in cmp: move new_code after comment_part
    u8 *comment_start = cmp.end;
    if (cmp.end + len(comment_part) >= cmp_space + BUF_SZ) {
        prt("Too much input to insert, buffer size exceeded\n");
        flush_err();
        exit(1);
    }
    memcpy(cmp.end, comment_part.buf, len(comment_part));
    cmp.end += len(comment_part);

    // Move new_code to be after comment
    memmove(cmp.end, new_code.buf, len(new_code));
    cmp.end += len(new_code);

    span new_content = (span){comment_start, cmp.end};

    if (len(inp) + len(new_content) >= BUF_SZ) {
        prt("Too much input to insert, buffer size exceeded\n");
        flush_err();
        exit(1);
    }

    int file_idx = file_for_block(block);
    projfile *pf = &state->files.a[file_idx];

    size_t old_block_len = len(block);
    u8 *block_start = block.buf;
    u8 *block_end = block.end;
    size_t tail_len = inp.end - block_end;

    ssize_t diff = (ssize_t)len(new_content) - (ssize_t)old_block_len;

    if (diff > 0) {
        if (len(inp) + diff >= BUF_SZ) {
            prt("Too much input to replace, buffer size exceeded\n");
            flush_err();
            exit(1);
        }
        memmove(block_end + diff, block_end, tail_len);
        inp.end += diff;
    } else if (diff < 0) {
        memmove(block_end + diff, block_end, tail_len);
        inp.end += diff;
    }

    memcpy(block_start, new_content.buf, len(new_content));

    pf->contents.end += diff;

    for (int i = file_idx + 1; i < state->files.n; ++i) {
        state->files.a[i].contents.buf += diff;
        state->files.a[i].contents.end += diff;
    }

    ingest();

    new_rev(S(""), file_idx);
}


/* #expand_block */
void expand_block(int idx) {
    if (idx < 0 || idx >= state->blocks.n) {
        prt("Invalid block index: %d\n", idx);
        flush();
        exit(1);
    }
    span block = state->blocks.a[idx];
    span result = expand_refs_2(block, S("both"));
    wrs(result);
    terpri();
    flush();
}


/* #block_by_id */
int block_by_id(span id_no_hash) {
    for (int i = 0; i < state->block_idx.n; ++i) {
        span idx = state->block_idx.a[i];
        advance1(&idx);
        if (span_eq(id_no_hash, idx)) {
            return block_for_span(state->block_idx.a[i]);
        }
    }
    return -1;
}



/* #press_any_key */
/* #ex_expand */
void ex_expand() {
    span current_block = state->blocks.a[state->curr_block_idx];
    span comment_part = block_comment_part(current_block);
    span trimmed_comment = trim(comment_part);
    span expanded = expand_refs_2(trimmed_comment, S("both"));
    
    clear_display();
    wrs(expanded);
    
    prt("Press any key to continue...");
    flush();
    getch();
}


/* #ex_reload */
void ex_reload() {
    int changed = 0;
    
    for (int i = 0; i < state->files.n; i++) {
        // Read fresh copy into inp buffer
        span fresh = read_file_S_into_span(state->files.a[i].path, inp_compl());
        checksum fresh_cksum = selected_checksum(fresh);
        
        // Compare with load_checksum
        if (fresh_cksum.__u != state->files.a[i].load_checksum.__u) {
            // File changed on disk - update
            state->files.a[i].contents = fresh;
            inp.end = fresh.end;
            state->files.a[i].load_checksum = fresh_cksum;
            state->files.a[i].cksum = fresh_cksum;
            changed++;
        }
    }
    
    if (changed > 0) {
        ingest();
        clear_display();
        prt("Reloaded %d file(s) from disk.\n", changed);
    } else {
        clear_display();
        prt("No files changed on disk.\n");
    }
    
    prt("Press any key to continue...");
    flush();
    getch();
}
/* #ex_config */
void ex_config() {
    clear_display();
    prt("Opening config file for editing...\n");
    flush();
    
    // Create a null-terminated string for the config path
    span path = state->config_file_path;
    char filename[256];
    int pathlen = len(path);
    if (pathlen >= 256) pathlen = 255;
    memcpy(filename, path.buf, pathlen);
    filename[pathlen] = '\0';
    
    int result = launch_editor(filename);
    
    if (result == 0) {
        prt("Config saved. Reloading...\n");
        flush();
        
        // Reset files array and re-parse config
        state->files.n = 0;
        parse_config();
        get_code();
        
        prt("Config reloaded successfully.\n");
    } else {
        prt("Editor exited with error, config not reloaded.\n");
    }
    
    prt("Press any key to continue...");
    flush();
    getch();
}
/* #ex_allfiles */
void ex_allfiles() {
    clear_display();
    prt("Scanning for source files...\n");
    flush();
    
    // Run find to get source files
    FILE* fp = popen("find . -maxdepth 3 -type f \\( "
                     "-name '*.c' -o -name '*.h' -o "
                     "-name '*.py' -o -name '*.js' -o -name '*.ts' -o "
                     "-name '*.go' -o -name '*.rs' -o "
                     "-name '*.java' -o -name '*.cpp' -o -name '*.hpp' "
                     "\\) 2>/dev/null | sort", "r");
    
    if (!fp) {
        prt("Error: Could not scan directory.\n");
        prt("Press any key to continue...");
        flush();
        getch();
        return;
    }
    
    int added = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        // Remove trailing newline
        int linelen = strlen(line);
        if (linelen > 0 && line[linelen-1] == '\n') line[--linelen] = '\0';
        if (linelen == 0) continue;
        
        // Check if file already in project
        span path = {(u8*)line, (u8*)line + linelen};
        int found = 0;
        for (int i = 0; i < state->files.n; i++) {
            if (span_eq(state->files.a[i].path, path)) {
                found = 1;
                break;
            }
        }
        
        if (!found && state->files.n < state->files.cap) {
            // Copy path into cmp space for persistence
            span cmp_free = cmp_compl();
            if (len(cmp_free) < linelen + 1) continue; // skip if no room
            memcpy(cmp_free.buf, line, linelen);
            span stored_path = {cmp_free.buf, cmp_free.buf + linelen};
            cmp.end = stored_path.end;
            
            // Guess language from extension
            span lang = S("C");
            if (ends_with(path, S(".py"))) lang = S("Python");
            else if (ends_with(path, S(".js"))) lang = S("JavaScript");
            else if (ends_with(path, S(".ts"))) lang = S("TypeScript");
            else if (ends_with(path, S(".go"))) lang = S("Go");
            else if (ends_with(path, S(".rs"))) lang = S("Rust");
            else if (ends_with(path, S(".java"))) lang = S("Java");
            
            state->files.a[state->files.n].path = stored_path;
            state->files.a[state->files.n].language = lang;
            state->files.a[state->files.n].contents = nullspan();
            state->files.n++;
            added++;
            prt("  Added: %s\n", line);
        }
    }
    pclose(fp);
    
    if (added > 0) {
        save_conf();
        prt("\nAdded %d file(s) to config.\n", added);
        // Reload to pick up contents
        get_code();
    } else {
        prt("\nNo new files found to add.\n");
    }
    
    prt("Press any key to continue...");
    flush();
    getch();
}
/* #ex_addfile */
void ex_addfile(span file_path) {
    clear_display();
    
    // Trim whitespace from path
    file_path = trim(file_path);
    
    if (empty(file_path)) {
        prt("Usage: :addfile <path>\n");
        prt("Press any key to continue...");
        flush();
        getch();
        return;
    }
    
    // Check if file already in project
    for (int i = 0; i < state->files.n; i++) {
        if (span_eq(state->files.a[i].path, file_path)) {
            prt("File already in project: %.*s\n", len(file_path), file_path.buf);
            prt("Press any key to continue...");
            flush();
            getch();
            return;
        }
    }
    
    // Check if we have room
    if (state->files.n >= state->files.cap) {
        prt("Error: Too many files in project.\n");
        prt("Press any key to continue...");
        flush();
        getch();
        return;
    }
    
    // Copy path into cmp space for persistence
    span cmp_free = cmp_compl();
    int pathlen = len(file_path);
    if (len(cmp_free) < pathlen + 1) {
        prt("Error: Out of memory.\n");
        prt("Press any key to continue...");
        flush();
        getch();
        return;
    }
    memcpy(cmp_free.buf, file_path.buf, pathlen);
    span stored_path = {cmp_free.buf, cmp_free.buf + pathlen};
    cmp.end = stored_path.end;
    
    // Guess language from extension
    span lang = S("C");
    if (ends_with(file_path, S(".py"))) lang = S("Python");
    else if (ends_with(file_path, S(".js"))) lang = S("JavaScript");
    else if (ends_with(file_path, S(".ts"))) lang = S("TypeScript");
    else if (ends_with(file_path, S(".go"))) lang = S("Go");
    else if (ends_with(file_path, S(".rs"))) lang = S("Rust");
    else if (ends_with(file_path, S(".java"))) lang = S("Java");
    else if (ends_with(file_path, S(".md"))) lang = S("Markdown");
    else if (ends_with(file_path, S(".sh"))) lang = S("Shell");
    
    state->files.a[state->files.n].path = stored_path;
    state->files.a[state->files.n].language = lang;
    state->files.a[state->files.n].contents = nullspan();
    state->files.n++;
    
    save_conf();
    prt("Added file: %.*s (language: %.*s)\n", len(stored_path), stored_path.buf, len(lang), lang.buf);
    
    // Reload to pick up contents
    get_code();
    
    prt("Press any key to continue...");
    flush();
    getch();
}
/* #ex_addlib */
void ex_addlib(span lib_path) {
    clear_display();
    
    lib_path = trim(lib_path);
    
    if (empty(lib_path)) {
        prt("Usage: :addlib <path>\n");
    } else {
        prt("Library support not yet implemented.\n");
        prt("Would add: %.*s\n", len(lib_path), lib_path.buf);
    }
    
    prt("Press any key to continue...");
    flush();
    getch();
}
/* #blockref_id */
span blockref_id(span ref) {
    advance(&ref, 1);  // Skip '@'
    int colon_pos = find_char(ref, ':');
    if (colon_pos == -1) {
        return ref;
    } else {
        return take_n(colon_pos, &ref);
    }
}

span blockref_fname(span ref) {
    int colon_pos = find_char(ref, ':');
    if (colon_pos == -1) {
        return S("comment");
    } else {
        advance(&ref, colon_pos + 1);
        return ref;
    }
}


/* #language_comment_starter */
span language_comment_starter(span language) {
    if (span_eq(language, S("C"))) return S("/*");
    if (span_eq(language, S("Python"))) return S("\"\"\"");
    if (span_eq(language, S("JavaScript"))) return S("/*");
    return nullspan();
}

span language_comment_ender(span language) {
    if (span_eq(language, S("C"))) return S("*/");
    if (span_eq(language, S("Python"))) return S("\"\"\"");
    if (span_eq(language, S("JavaScript"))) return S("*/");
    return nullspan();
}


/* #expand_refs_2 */
span expand_refs_2(span block, span mode) {
    span ret = {cmp.end, cmp.end};
    spans_arena_push();
    out_sav sav = out2cmp();
    spans already = spans_alloc(16);
    
    if (span_eq(mode, S("context"))) {
        expand_refs_2_rec_context(block, S("comment"), &already, 0, 0);
    } else if (span_eq(mode, S("body"))) {
        expand_refs_2_rec_body(block, S("comment"), &already, 0, 0);
    } else {
        expand_refs_2_rec_both(block, S("comment"), &already, 0, 0);
    }
    
    out_rst(sav);
    spans_arena_pop();
    ret.end = cmp.end;
    return ret;
}


/* #expand_refs_2_rec */
void expand_refs_2_rec_both(span block, span transform, spans* already, int comment_context, int depth) {
    if (depth > 512) {
        prt("block expansion depth limit (512) exceeded, possible reference cycle?");
        flush_exit(1);
    }
    expand_refs_2_rec_context(block, transform, already, comment_context, depth);
    expand_refs_2_rec_body(block, transform, already, comment_context, depth);
}


/* #expand_refs_2_rec_body_pre */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

/* #expand_refs_2_rec_body */
void expand_refs_2_rec_body(span block, span transform, spans* already, int comment_context, int depth) {
    if (depth > 512) {
        prt("block expansion depth limit (512) exceeded, possible reference cycle?\n");
        prt("while expanding the block:\n");
        wrs(block);
        flush_exit(1);
    }

    spans ids = ids_for_block(block);
    for (size_t i = 0; i < ids.n; i++) {
        if (index_of(ids.a[i], *already) != -1 && (span_eq(transform, S("comment")) || span_eq(transform, S("all")))) {
            return;
        }
    }

    if (span_eq(transform, S("comment")) || span_eq(transform, S("all"))) {
        for (size_t i = 0; i < ids.n; i++) {
            spans_push(already, ids.a[i]);
        }
    }

    span lang = language_for_block(block);
    span comment_start = language_comment_starter(lang);
    span comment_end = language_comment_ender(lang);

    span comment_part = nullspan();
    span code_part = nullspan();
    int handle_comment = 0;
    int handle_code = 0;

    if (span_eq(transform, S("comment"))) {
        comment_part = trim(block_comment_part(block));
        handle_comment = 1;
    } else if (span_eq(transform, S("code"))) {
        code_part = block_code_part(block);
        handle_code = 1;
    } else if (span_eq(transform, S("all"))) {
        comment_part = trim(block_comment_part(block));
        code_part = block_code_part(block);
        handle_comment = handle_code = 1;
    }

    if (handle_comment) {
        if (!comment_context) {
            wrs(comment_start);
            sp();
            for (size_t i = 0; i < ids.n; i++) {
                wrs(ids.a[i]);
                sp();
            }
            bksp();
            terpri();
        }

        span top_line = next_line(&comment_part);
        while (!empty(comment_part)) {
            span line = next_line(&comment_part);
            if (starts_with(line, S("@-"))) continue;
            if (line.buf[0] == '@') {
                span id = blockref_id(line);
                span fname = blockref_fname(line);
                int block_idx = block_by_id(id);
                if (block_idx == -1) {
                    prt("no block found matching reference %.*s\n", len(line), line.buf);
                    prt("Press any key to continue...\n");
                    flush();
                    getch();
                    continue;
                }
                span ref_block = state->blocks.a[block_idx];
                expand_refs_2_rec_body(ref_block, fname, already, 1, depth + 1);
            } else if (empty(comment_part)) {
                if (comment_context) {
                    shorten(&line, len(comment_end));
                    wrs(line);
                    terpri();
                } else {
                    wrs(line);
                    terpri();
                }
            } else {
                wrs(line);
                terpri();
            }
        }
    }

    if (handle_comment && handle_code) {
        terpri();
    }

    if (handle_code) {
        wrs(code_part);
    }
}

/* #expand_refs_2_rec_context */
void expand_refs_2_rec_context(span block, span transform, spans* already, int comment_context, int depth) {
    if (span_eq(transform, S("code"))) return;

    span top_line = next_line(&block);
    spans tokens = split_whitespace(top_line);

    for (size_t i = 0; i < tokens.n; i++) {
        if (tokens.a[i].buf[0] == '@') {
            span id = blockref_id(tokens.a[i]);
            span fname = blockref_fname(tokens.a[i]);
            if (index_of(id, *already) == -1) {
                int block_idx = block_by_id(id);
                if (block_idx == -1) {
                    prt("no block found matching reference %.*s\n", len(tokens.a[i]), tokens.a[i].buf);
                    prt("Press any key to continue...\n");
                    flush();
                    getch();
                    continue;
                }
                span ref_block = state->blocks.a[block_idx];
                expand_refs_2_rec_both(ref_block, fname, already, comment_context, depth + 1);
            }
        }
    }

    while (!empty(block)) {
        span line = next_line(&block);
        if (line.buf[0] == '@') {
            if (starts_with(line, S("@- "))) continue;
            span id = blockref_id(line);
            span fname = blockref_fname(line);
            if (index_of(id, *already) == -1) {
                int block_idx = block_by_id(id);
                if (block_idx == -1) {
                    prt("no block found matching reference %.*s\n", len(line), line.buf);
                    prt("Press any key to continue...\n");
                    flush();
                    getch();
                    continue;
                }
                span ref_block = state->blocks.a[block_idx];
                expand_refs_2_rec_context(ref_block, fname, already, comment_context, depth + 1);
            }
        }
    }
}



/* #chase_ref */
/* #chase_ref_2 */
span chase_ref_2(span ref_id) {
    int idx = block_by_id(ref_id);
    if (idx == -1) {
        return nullspan();
    }
    return state->blocks.a[idx];
}


/* #strip_markdown_codeblock */
span strip_markdown_codeblock(span input) {
    int count = 0;
    span ret = nullspan();
    span copy = input;

    while (!empty(copy)) {
        span line = next_line(&copy);
        if (starts_with(line, S("```"))) {
            if (count == 0) {
                ret.buf = line.end + 1;
            } else if (count == 1) {
                ret.end = line.buf;
            }
            count++;
        }
    }

    if (count != 2) {
        return input;
    }

    return ret;
}


/* #send_to_clipboard */
void send_to_clipboard(span content) {
    ensure_conf_var(&(state->cbcopy), S("The command to pipe data to the clipboard on your system. For Mac try \"pbcopy\", Linux \"xclip -i -selection clipboard\", Windows \"clip.exe\""), S(""));

    char command[2048];
    snprintf(command, sizeof(command), "%.*s", (int)(state->cbcopy.end - state->cbcopy.buf), state->cbcopy.buf);

    FILE* pipe = popen(command, "w");
    if (!pipe) {
        perror("Failed to open pipe for clipboard command");
        exit(EXIT_FAILURE);
    }

    fwrite(content.buf, sizeof(char), content.end - content.buf, pipe);

    if (pclose(pipe) != 0) {
        perror("Failed to execute clipboard command");
        exit(EXIT_FAILURE);
    }
}

/* #normalize_path_for_match */
span normalize_path_for_match(span path) {
    if (len(path) >= 2 && path.buf[0] == '.' && path.buf[1] == '/') {
        return skip_n(path, 2);
    }
    return path;
}


/* #paths_match_for_block_map */
int paths_match_for_block_map(span a, span b) {
    a = normalize_path_for_match(trim(a));
    b = normalize_path_for_match(trim(b));

    if (span_eq(a, b)) return 1;

    if (len(a) >= len(b) && ends_with(a, b)) return 1;
    if (len(b) >= len(a) && ends_with(b, a)) return 1;

    return 0;
}


/* #parse_block_map_entry */
int parse_block_map_entry(span line, span* path, int* start_line, int* end_line, span* block_id) {
    line = trim(line);
    if (empty(line)) return 0;

    spans tokens = split_whitespace(line);
    if (tokens.n < 2) return 0;

    *block_id = tokens.a[tokens.n - 1];

    span path_token = tokens.a[0];
    span range_token = tokens.n >= 3 ? tokens.a[tokens.n - 2] : nullspan();

    if (!empty(range_token)) {
        int dash = find_char(range_token, '-');
        if (dash != -1 && isdigit(*range_token.buf)) {
            *start_line = parse_int(first_n(range_token, dash));
            *end_line = parse_int(skip_n(range_token, dash + 1));
            *path = path_token;
            return 1;
        }

        if (tokens.n >= 4 && isdigit(*range_token.buf) && isdigit(*tokens.a[tokens.n - 3].buf)) {
            *start_line = parse_int(tokens.a[tokens.n - 3]);
            *end_line = parse_int(range_token);
            *path = path_token;
            return 1;
        }
    }

    int colon = find_char(path_token, ':');
    if (colon != -1) {
        span before_colon = first_n(path_token, colon);
        span after_colon = skip_n(path_token, colon + 1);
        int range_dash = find_char(after_colon, '-');
        if (range_dash != -1) {
            *start_line = parse_int(first_n(after_colon, range_dash));
            *end_line = parse_int(skip_n(after_colon, range_dash + 1));
            *path = before_colon;
            return 1;
        }
        if (!empty(after_colon) && isdigit(*after_colon.buf)) {
            *start_line = parse_int(after_colon);
            *end_line = *start_line;
            *path = before_colon;
            return 1;
        }
    }

    return 0;
}



/* #block_ids_for_file_line */
spans block_ids_for_file_line(span block_map, span file_path, int line_number) {
    if (empty(block_map)) {
        return spans_alloc(0);
    }

    int match_count = 0;
    span map_copy = block_map;
    while (!empty(map_copy)) {
        span line = next_line(&map_copy);
        span entry_path = nullspan();
        int start_line = 0, end_line = 0;
        span block_id = nullspan();

        if (!parse_block_map_entry(line, &entry_path, &start_line, &end_line, &block_id)) continue;
        if (!paths_match_for_block_map(entry_path, file_path)) continue;
        if (line_number < start_line || line_number > end_line) continue;

        match_count++;
    }

    spans result = spans_alloc(match_count);
    map_copy = block_map;
    while (!empty(map_copy)) {
        span line = next_line(&map_copy);
        span entry_path = nullspan();
        int start_line = 0, end_line = 0;
        span block_id = nullspan();

        if (!parse_block_map_entry(line, &entry_path, &start_line, &end_line, &block_id)) continue;
        if (!paths_match_for_block_map(entry_path, file_path)) continue;
        if (line_number < start_line || line_number > end_line) continue;

        spans_push(&result, block_id);
    }

    return result;
}


/* #parse_compiler_error_line */
int parse_compiler_error_line(span line, span* path, int* line_number) {
    int first_colon = find_char(line, ':');
    if (first_colon == -1) return 0;

    span path_span = first_n(line, first_colon);
    span after_path = skip_n(line, first_colon + 1);

    int second_colon = find_char(after_path, ':');
    if (second_colon == -1) return 0;

    span line_number_span = trim(first_n(after_path, second_colon));
    if (empty(line_number_span) || !isdigit(*line_number_span.buf)) return 0;

    *line_number = parse_int(line_number_span);
    *path = trim(path_span);
    return 1;
}


/* #replace_block_code_part */
void replace_block_code_part(span new_code) {
   new_code = strip_markdown_codeblock(new_code);

   span original_block = state->blocks.a[state->curr_block_idx];
   int file_idx = file_for_block(original_block);

   span comment_part = block_comment_part(original_block);
   size_t comment_len = len(comment_part);
   
   int newlines_to_add = 0;
   if (comment_len >= 2) {
       if (comment_part.end[-1] != '\n' || comment_part.end[-2] != '\n') {
           newlines_to_add = (comment_part.end[-1] != '\n') ? 2 : 1;
       }
   }

   size_t new_block_len = comment_len + newlines_to_add + len(new_code) + 1;
   size_t old_block_len = len(original_block);
   ptrdiff_t size_diff = new_block_len - old_block_len;

   if (size_diff != 0) {
       memmove(original_block.buf + new_block_len, original_block.end, inp.end - original_block.end);
       inp.end += size_diff;
   }

   u8* write_ptr = original_block.buf + comment_len;
   for (int i = 0; i < newlines_to_add; i++) {
       *write_ptr++ = '\n';
   }

   memcpy(write_ptr, new_code.buf, len(new_code));
   write_ptr += len(new_code);
   *write_ptr = '\n';

   for (int i = file_idx; i < state->files.n; i++) {
       if (i == file_idx) {
           state->files.a[i].contents.end += size_diff;
       } else {
           state->files.a[i].contents.buf += size_diff;
           state->files.a[i].contents.end += size_diff;
       }
   }

   ingest();
   new_rev(nullspan(), file_idx);
}

/* #output_design */
/* #output_save */
void output_save(span operation, span message) {
    generic_output_save(operation, message);
}

/* #make_output_saver */
llm_message_handler make_output_saver(span operation) {
    return partial_sp_sp(operation, output_save);
}


/* #generic_output_save */
void generic_output_save(span operation, span message) {
    span ret;
    ret.buf = cmp.end;
    out_sav sav = out2cmp();

    span block_id = id_for_block(state->blocks.a[state->curr_block_idx]);
    checksum cksum = selected_checksum(state->blocks.a[state->curr_block_idx]);

    prt("block_id: ");
    wrs(block_id);
    terpri();

    prt("checksum: ");
    pr_checksum(cksum);

    prt("output_of: ");
    wrs(operation);
    terpri();
    terpri();

    wrs(message);
    if (!empty(message) && message.end[-1] != '\n') {
        terpri();
    }

    ret.end = cmp.end;
    out_rst(sav);

    span filename = filename_template(S("{cmprdir}/outputs/{timestamp}"));
    write_to_file_span(ret, filename, 1);

    cmp.end = ret.buf;
}


/* #span_cmp_wrapper */
int span_cmp_wrapper(const void *a, const void *b) {
    return span_cmp(*(span *)a, *(span *)b);
}


/* #get_outputs */
void get_outputs() {
    span outdir = concat(state->cmprdir, S("outputs"));
    DIR *dir = opendir(s(outdir));
    if (!dir) {
        prt("Cannot open outputs directory: %s\n", s(outdir));
        flush();
        exit(1);
    }

    state->outputs_filenames = spans_alloc(8);
    static char buf[1 << 18];
    char *buf_ptr = buf;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strlen(entry->d_name) == 15 && isdigit(entry->d_name[0]) && isdigit(entry->d_name[1]) &&
            isdigit(entry->d_name[2]) && isdigit(entry->d_name[3]) && isdigit(entry->d_name[4]) &&
            isdigit(entry->d_name[5]) && isdigit(entry->d_name[6]) && isdigit(entry->d_name[7]) &&
            entry->d_name[8] == '-' && isdigit(entry->d_name[9]) && isdigit(entry->d_name[10]) &&
            isdigit(entry->d_name[11]) && isdigit(entry->d_name[12]) && isdigit(entry->d_name[13]) &&
            isdigit(entry->d_name[14])) {
            strcpy(buf_ptr, entry->d_name);
            span filename_span = { (u8*)buf_ptr, (u8*)buf_ptr + 15 };
            spans_push(&state->outputs_filenames, filename_span);
            buf_ptr += 16;
        }
    }

    closedir(dir);

    qsort(state->outputs_filenames.a, state->outputs_filenames.n, sizeof(span), span_cmp_wrapper);
}



/* #dir_listing */
spans dir_listing(span dirname) {
    spans ret = spans_alloc(16);
    
    DIR *dir;
    struct dirent *entry;
    
    dir = opendir(s(dirname));
    if (dir == NULL) {
        return ret;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        span filename = prs(entry->d_name);
        spans_push(&ret, filename);
    }

    closedir(dir);
    
    qsort(ret.a, ret.n, sizeof(span), span_cmp_wrapper);
    
    return ret;
}



/* #read_output_headers */
spans read_output_headers(span bname) {
    span filename = filename_template(concat(S("{cmprdir}/outputs/"), bname));
    span file_contents = read_file_into_cmp(filename);

    spans headers = spans_alloc(0);

    while (!empty(file_contents)) {
        span line = next_line(&file_contents);
        if (empty(trim(line))) break;
        
        int colon_idx = find_char(line, ':');
        if (colon_idx == -1) continue;
        
        span key = trim(first_n(line, colon_idx));
        span value = trim(skip_n(line, colon_idx + 1));
        
        spans_push(&headers, key);
        spans_push(&headers, value);
    }

    cmp.end = file_contents.buf; // Reset cmp space to keep headers but give back memory for the body

    return headers;
}

 /* read_output_body @output_design @filename_template

span read_output_body(span bname);

We read <cmprdir>/outputs/<timestamp> (where timestamp is the basename that we are given) into cmp space.
We look for the first blank line, and return everything after it.
(If there is no blank line, we just return the empty span at the end of the file's contents.)

*/

span read_output_body(span bname) {
    span path = filename_template(concat(S("{cmprdir}/outputs/"), bname));
    span content = read_file_into_cmp(path);

    while (!empty(content)) {
        span line = next_line(&content);
        if (empty(trim(line))) {
            return content;
        }
    }
    return nullspan();
}


/* #replace_block */
void replace_block(span new_block) {
    span original_block = state->blocks.a[state->curr_block_idx];
    int file_index = state->curr_file_idx;

    size_t original_len = len(original_block);
    size_t new_len = len(new_block);
    size_t rest_len = inp.end - original_block.end;
    ssize_t diff = new_len - original_len;

    if (diff != 0) {
        memmove(original_block.buf + new_len, original_block.end, rest_len);
        inp.end += diff;
    }

    memcpy(original_block.buf, new_block.buf, new_len);

    state->files.a[file_index].contents.end += diff;

    for (size_t i = file_index + 1; i < state->files.n; i++) {
        state->files.a[i].contents.buf += diff;
        state->files.a[i].contents.end += diff;
    }

    ingest();
    new_rev(nullspan(), file_index);
}


/* #cmpr_init */
void cmpr_init() {
    mkdir(".cmpr", 0755);
    mkdir(".cmpr/revs", 0755);
    mkdir(".cmpr/tmp", 0755);
    mkdir(".cmpr/api_calls", 0755);

    FILE *file = fopen(".cmpr/conf", "a");
    if (file != NULL) {
        fclose(file);
    }
}



/* #simple_message_handler */
llm_message_handler simple_message_handler(void(*f)(span)) {
    return partial_0_sp(f);
}


/* #nl2pl_rewrite */
void nl2pl_rewrite() {
    span op = S("nl2pl_rewrite");
    span template = get_prompt_template(op);
    spans vars = current_block_template_vars();
    span expanded_prompt = expand_template(template, vars);
    send_to_llm(expanded_prompt, simple_message_handler(replace_block_code_part));
}


/* #pl2nl_rewrite */
void pl2nl_rewrite() {
    span template = get_prompt_template(S("pl2nl_rewrite"));
    spans vars = current_block_template_vars();
    span expanded = expand_template(template, vars);
    send_to_llm(expanded, simple_message_handler(pl2nl_rewrite_cb));
}



/* #pl2nl_rewrite_cb */
void pl2nl_rewrite_cb(span message) {
    span received_comment = strip_markdown_codeblock(message);
    span current_block = state->blocks.a[state->curr_block_idx];
    span current_code = block_code_part(current_block);

    span first_line = next_line(&current_block);
    next_line(&received_comment); // Discard the first line of received comment

    span new_comment_part = prs("%.*s\n%.*s", len(first_line), first_line.buf, len(received_comment), received_comment.buf);
    span complete_comment;

    if (ends_with(new_comment_part, S("\n\n"))) {
        complete_comment = new_comment_part;
    } else if (ends_with(new_comment_part, S("\n"))) {
        complete_comment = prs("%.*s\n", len(new_comment_part), new_comment_part.buf);
    } else {
        complete_comment = prs("%.*s\n\n", len(new_comment_part), new_comment_part.buf);
    }

    span final_content = prs("%.*s%.*s", len(complete_comment), complete_comment.buf, len(current_code), current_code.buf);

    replace_block(final_content);
}



/* #nl2algo */
void nl2algo() {
    span op = S("nl2algo");
    span template = get_prompt_template(op);
    spans vars = current_block_template_vars();
    span expanded_template = expand_template(template, vars);
    llm_message_handler handler = make_output_saver(op);
    send_to_llm(expanded_template, handler);
}


/* #agreement_to_pl_diff */
void agreement_to_pl_diff() {
    span prompt_template = get_prompt_template(S("agreement_to_pl_diff"));
    spans template_vars = current_block_template_vars();

    output_template_var(&template_vars, S("agreement"));

    span expanded_template = expand_template(prompt_template, template_vars);
    wrs(expanded_template);
    flush();
    getch();

    llm_message_handler cb = make_output_saver(S("agreement_to_pl_diff"));
    send_to_llm(expanded_template, cb);
}



/* #summarize_block */
/* #handle_learn */
void handle_learn(span es1, span es2) {
    // Alphabetize for consistent filename
    char es1_str[64] = {0};
    char es2_str[64] = {0};
    s_buffer(es1_str, sizeof(es1_str), es1);
    s_buffer(es2_str, sizeof(es2_str), es2);
    
    char *first = es1_str;
    char *second = es2_str;
    if (strcmp(es1_str, es2_str) > 0) {
        first = es2_str;
        second = es1_str;
    }
    
    // Build pattern filepath
    char filepath[256];
    snprintf(filepath, sizeof(filepath), ".cmpr/patterns/%s-%s", first, second);
    
    // Build filter paths
    char es1_path[256], es2_path[256];
    snprintf(es1_path, sizeof(es1_path), ".cmpr/es/%.*s", len(es1), es1.buf);
    snprintf(es2_path, sizeof(es2_path), ".cmpr/es/%.*s", len(es2), es2.buf);
    
    // Check filters exist
    if (access(es1_path, X_OK) != 0) {
        prt("Error: Event space filter not found: %s\n", es1_path);
        flush_exit(1);
    }
    if (access(es2_path, X_OK) != 0) {
        prt("Error: Event space filter not found: %s\n", es2_path);
        flush_exit(1);
    }
    
    // Storage for co-occurrence counts
    #define MAX_PAIRS 10000
    #define MAX_EVENT_LEN 256
    static struct { char e1[MAX_EVENT_LEN]; char e2[MAX_EVENT_LEN]; int count; } pairs[MAX_PAIRS];
    int n_pairs = 0;
    
    // Open events directory
    DIR *dir = opendir(".cmpr/events");
    if (!dir) {
        prt("Error: Cannot open .cmpr/events\n");
        flush_exit(1);
    }
    
    // Collect snapshot filenames
    char *snapshots[4096];
    int n = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL && n < 4096) {
        if (de->d_name[0] == '.') continue;
        snapshots[n++] = strdup(de->d_name);
    }
    closedir(dir);
    
    // Process each snapshot
    for (int s = 0; s < n; s++) {
        char snap_path[512];
        snprintf(snap_path, sizeof(snap_path), ".cmpr/events/%s", snapshots[s]);
        
        // Run through ES1 filter
        char cmd1[1024];
        snprintf(cmd1, sizeof(cmd1), "cat '%s' | '%s'", snap_path, es1_path);
        FILE *fp1 = popen(cmd1, "r");
        char es1_events[100][MAX_EVENT_LEN];
        int es1_n = 0;
        if (fp1) {
            char buf[4096];
            while (fgets(buf, sizeof(buf), fp1) && es1_n < 100) {
                size_t l = strlen(buf);
                while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = 0;
                if (l > 0 && buf[0] == '"') {
                    // Parse SN line: "event" <strength>.
                    char *period = buf + l - 1;
                    if (*period != '.') continue;
                    
                    char *num_start = period - 1;
                    while (num_start > buf && *num_start >= '0' && *num_start <= '9') num_start--;
                    num_start++;
                    
                    int strength = atoi(num_start);
                    if (strength != 255) continue; // Only learn from 255
                    
                    char *close_quote = num_start - 1;
                    while (close_quote > buf && *close_quote == ' ') close_quote--;
                    if (*close_quote != '"') continue;
                    
                    int evlen = close_quote - buf + 1;
                    if (evlen >= MAX_EVENT_LEN) evlen = MAX_EVENT_LEN - 1;
                    memcpy(es1_events[es1_n], buf, evlen);
                    es1_events[es1_n][evlen] = '\0';
                    es1_n++;
                }
            }
            pclose(fp1);
        }
        
        // Run through ES2 filter
        char cmd2[1024];
        snprintf(cmd2, sizeof(cmd2), "cat '%s' | '%s'", snap_path, es2_path);
        FILE *fp2 = popen(cmd2, "r");
        char es2_events[100][MAX_EVENT_LEN];
        int es2_n = 0;
        if (fp2) {
            char buf[4096];
            while (fgets(buf, sizeof(buf), fp2) && es2_n < 100) {
                size_t l = strlen(buf);
                while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = 0;
                if (l > 0 && buf[0] == '"') {
                    char *period = buf + l - 1;
                    if (*period != '.') continue;
                    
                    char *num_start = period - 1;
                    while (num_start > buf && *num_start >= '0' && *num_start <= '9') num_start--;
                    num_start++;
                    
                    int strength = atoi(num_start);
                    if (strength != 255) continue; // Only learn from 255
                    
                    char *close_quote = num_start - 1;
                    while (close_quote > buf && *close_quote == ' ') close_quote--;
                    if (*close_quote != '"') continue;
                    
                    int evlen = close_quote - buf + 1;
                    if (evlen >= MAX_EVENT_LEN) evlen = MAX_EVENT_LEN - 1;
                    memcpy(es2_events[es2_n], buf, evlen);
                    es2_events[es2_n][evlen] = '\0';
                    es2_n++;
                }
            }
            pclose(fp2);
        }
        
        // Count co-occurrences if both have events
        if (es1_n > 0 && es2_n > 0) {
            for (int i = 0; i < es1_n; i++) {
                for (int j = 0; j < es2_n; j++) {
                    int found = -1;
                    for (int k = 0; k < n_pairs; k++) {
                        if (strcmp(pairs[k].e1, es1_events[i]) == 0 && 
                            strcmp(pairs[k].e2, es2_events[j]) == 0) {
                            found = k;
                            break;
                        }
                    }
                    if (found >= 0) {
                        pairs[found].count++;
                    } else if (n_pairs < MAX_PAIRS) {
                        memcpy(pairs[n_pairs].e1, es1_events[i], MAX_EVENT_LEN);
                        memcpy(pairs[n_pairs].e2, es2_events[j], MAX_EVENT_LEN);
                        pairs[n_pairs].count = 1;
                        n_pairs++;
                    }
                }
            }
        }
        
        free(snapshots[s]);
    }
    
    // Build output content with log-stochastic counts
    char outbuf[65536];
    int outlen = 0;
    for (int i = 0; i < n_pairs; i++) {
        int log_count = 0;
        int c = pairs[i].count;
        while (c > 1) { c >>= 1; log_count++; }
        
        outlen += snprintf(outbuf + outlen, sizeof(outbuf) - outlen,
            "%s %s %d.\n", pairs[i].e1, pairs[i].e2, log_count);
    }
    span content = (span){(u8*)outbuf, (u8*)(outbuf + outlen)};
    
    // Create directory and write file
    mkdir_p(S(".cmpr/patterns"));
    write_to_file_2(content, filepath, 1);
    
    // Print output
    fwrite(outbuf, 1, outlen, stdout);
    prt("Saved to %s\n", filepath);
    flush();
}

/* #handle_log_stochastic_count_joint */
void handle_log_stochastic_count_joint() {
    // Read all stdin
    span input = read_stdin_into_cmp();
    
    // Storage for co-occurrence counts
    #define MAX_PAIRS 10000
    static struct { span e1; span e2; int count; } pairs[MAX_PAIRS];
    int n_pairs = 0;
    
    // Parse groups
    span pos = input;
    while (!empty(pos)) {
        span line = next_line(&pos);
        if (empty(line)) continue;  // blank line between groups
        
        // If line doesn't start with ", it's a timestamp - process group
        if (line.buf[0] != '"') {
            // Collect events for this group
            span es1_events[100];
            span es2_events[100];
            int n_es1 = 0, n_es2 = 0;
            int in_es2 = 0;
            span prev_prefix = nullspan();
            
            // Collect events until blank line
            while (!empty(pos)) {
                line = next_line(&pos);
                if (empty(line)) break;  // end of group
                if (line.buf[0] != '"') continue;  // skip non-event lines
                
                // Find end of event string - look for last " before strength
                u8 *end = line.end;
                while (end > line.buf && *(end-1) != '"') end--;
                if (end <= line.buf) continue;  // malformed
                span event_str = (span){line.buf, end};
                
                // Extract prefix (up to first colon)
                span prefix = event_str;
                for (u8 *c = event_str.buf; c < event_str.end; c++) {
                    if (*c == ':') { prefix.end = c + 1; break; }
                }
                
                // Detect ES boundary by prefix change
                if (empty(prev_prefix)) {
                    prev_prefix = prefix;
                } else if (!span_eq(prefix, prev_prefix) && !in_es2) {
                    in_es2 = 1;
                }
                
                if (!in_es2 && n_es1 < 100) {
                    es1_events[n_es1++] = event_str;
                } else if (n_es2 < 100) {
                    es2_events[n_es2++] = event_str;
                }
            }
            
            // Count co-occurrences for this group
            for (int i = 0; i < n_es1; i++) {
                for (int j = 0; j < n_es2; j++) {
                    // Find or create pair
                    int found = -1;
                    for (int k = 0; k < n_pairs; k++) {
                        if (span_eq(pairs[k].e1, es1_events[i]) && 
                            span_eq(pairs[k].e2, es2_events[j])) {
                            found = k;
                            break;
                        }
                    }
                    if (found >= 0) {
                        pairs[found].count++;
                    } else if (n_pairs < MAX_PAIRS) {
                        pairs[n_pairs].e1 = es1_events[i];
                        pairs[n_pairs].e2 = es2_events[j];
                        pairs[n_pairs].count = 1;
                        n_pairs++;
                    }
                }
            }
        }
    }
    
    // Output joint events with log count
    for (int i = 0; i < n_pairs; i++) {
        int log_count = 0;
        int c = pairs[i].count;
        while (c > 1) { c >>= 1; log_count++; }
        
        prt("%.*s %.*s %d.\n", 
            len(pairs[i].e1), pairs[i].e1.buf,
            len(pairs[i].e2), pairs[i].e2.buf,
            log_count);
    }
    flush();
}

/* #compile */
void compile() {
    ensure_conf_var(&state->buildcmd, S("The build command will be run every time you hit 'B' and should build the code you are editing (typically in projfile)"), nullspan());
    
    char buf[2048] = {0};
    s_buffer(buf, sizeof(buf), state->buildcmd);
    
    prt("Running command: %s\n", buf);
    flush();
    
    int status = system(buf);
    
    if (status != 0) {
        prt("Build failed, press any key to continue...\n");
        flush();
        getch();
    } else {
        prt("Build succeeded\n");
        flush();
        sleep(1);
    }
}

