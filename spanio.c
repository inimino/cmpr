/* #libraryintro

This is the spanio library.
We ALWAYS use spanio methods when possible and never use null-terminated C strings (except when calling library functions or at other interface boundaries where it is required.)

- `empty(span)`: Checks if a span is empty (start and end pointers are equal).
- `len(span)`: Returns the length of a span.
- `init_spans()`: Initializes input, output, and comparison spans and their associated buffers.
- `prt2cmp()`, `prt2std()`: Swaps output and comparison spans.
- `prt(const char *, ...)`: Formats and appends a string to the output span.
- `w_char(char)`: Writes a single character to the output span.
- `wrs(span)`: Writes the contents of a span to the output span.
- `bksp()`: Removes the last character from the output span.
- `sp()`: Appends a space character to the output span.
- `terpri()`: Appends a newline character to the output span.
- `flush()`, `flush_err()`, `flush_to(char*)`: Flushes the output span to standard output, standard error, or a specified file.
- `write_to_file(span, const char*)`: Writes the contents of a span to a specified file.
- `read_file_into_span(char*, span)`: Reads the contents of a file into a span.
- `read_file_S_into_span(span, span)`: Ibid, but taking the filename as a span.
- `redir(span)`, `reset()`: Redirects output to a new span and resets it to the previous output span.
- `save()`, `push(span)`, `pop(span*)`, `pop_into_span()`: Manipulates a stack for saving and restoring spans.
- `advance1(span*)`, `advance(span*, int)`: Advances the start pointer of a span by one or a specified number of characters.
- `find_char(span, char)`: Searches for a character in a span and returns its index.
- `contains(span, span)`: Checks if one span contains another.
- `starts_with(span, span)`: Check if a starts with b.
- `take_n(int, span*)`: Takes the first n characters from a span, mutating it, and returns as a new span.
- `first_n(span, int)`: Returns n chars as a new span without mutating.
- `next_line(span*)`: Extracts the next line from a span and returns it as a new span.
- `span_eq(span, span)`, `span_cmp(span, span)`: Compares two spans for equality or lexicographical order.
- `S(char*)`: Creates a span from a null-terminated string.
- `s(char*,int,span)`: Creates a null-terminated string in a user-provided buffer of provided length from given span.
- `nullspan()`: Returns an empty span.
- `spans_alloc(int)`: Allocates a spans structure with a specified number of span elements.
- `span_arena_alloc(int)`, `span_arena_free()`, `span_arena_push()`, `span_arena_pop()`: Manages a memory arena for dynamic allocation of spans.
- `is_one_of(span, spans)`: Checks if a span is one of the spans in a spans.
- `spanspan(span, span)`: Finds the first occurrence of a span within another span and returns the rest of the haystack span starting from that point.
- `w_char_esc(char)`, `w_char_esc_pad(char)`, `w_char_esc_dq(char)`, `w_char_esc_sq(char)`, `wrs_esc()`: Write characters (or in the case of wrs, spans) to out, the output span, applying various escape sequences as needed.

typedef struct { u8* buf; u8* end; } span; // reminder of the type of span

typedef struct { span *s; int n; } spans; // reminder of the type of spans, given by spans_alloc(n)

We have a generic array implementation using arena allocation.

- Call MAKE_ARENA(T,E,STACK_SIZE) to define array type E for elements of type T.
  - E has .a and .n on it of type T* and int resp.
- Use E_arena_alloc(N) and E_arena_free(), typically in main() or similar.
- E_alloc(N) returns an array of type E, with .n = .cap = N.
- E_arena_push() and E_arena_pop() manage arena allocation stack.

Note that spans uses .s for the array member, not .a like all of our generic array types.

spans_alloc returns a spans which has n equal to the number passed in.
Typically the caller will either fill the number requested exactly, or will shorten n if fewer are used.
These point into the spans arena which must be set up by calling span_arena_alloc() with some integer argument (in bytes).
A common idiom is to iterate over something once to count the number of spans needed, then call spans_alloc and iterate again to fill the spans.

Common idiom: use a single span, with buf pointing to one thing and end to something else found later; return this result (from many functions returning span).

Note that we NEVER write const in C, as this only brings pain (we have some uses of const in library code, but PLEASE do not create any more).

Sometimes we need a null-terminated C string for talking to library functions, we can use this pattern (with "2048" being replaced by some reasonably generous number appropriate to the specific use case): `char buf[2048] = {0}; s(buf,2048,some_span);`.

Note that prt() has exactly the same function signature as printf, i.e. it takes a format string followed by varargs.
We never use printf, but always prt.
A common idiom when reporting errors is to call prt, flush, and exit.

To prt a span we use `%.*s` with len.
*/
/* includes */

#define _GNU_SOURCE // for memmem
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
/* convenient debugging macros */
#define dbgd(x) prt(#x ": %d\n", x),flush()
#define dbgx(x) prt(#x ": %x\n", x),flush()
#define dbgf(x) prt(#x ": %f\n", x),flush()

typedef unsigned char u8;

/* span

Think of span as our string type.
A span is two pointers, one to the first char included in the string, and the second to the first char excluded after the string's end.
If these two pointers are equal, the string is empty, but it still points to a location.
So two empty spans are not necessarily the same span, while two empty strings are.
Neither spans nor their contents are immutable.
These two pointers must point into some space that has been allocated somewhere.
Usually the spans are backed by the input buffer or the output buffer or a scratch buffer that we create.
A scratch buffer allocated for a particular phase of processing can be seen as a form of arena allocation.
A common pattern is for(;s.buf<s.end;s.buf++) { ... s.buf[0] ... } or similar.

The inp variable is the span which writes into input_space, and then is the immutable copy of stdin for the duration of the process.
The number of bytes of input is len(inp).
The output is stored in span out, which points to output_space.
Input processing is generally by reading out of the inp span or subspans of it.
The output spans are mostly written to with prt() and other IO functions.
The cmp_space and cmp span which points to it are used for analysis and model data, both reading and writing.
These are just the common conventions; specific programs may use them differently.

When writing output, we often see prt followed by flush.
Flush sends to stdout the contents of out (the output span) that have not already been sent.
Usually it is important to do this
- before any operation that blocks, when the user should see the output that we've already written,
- after printing any error message and before exiting the program, and
- at the end of main before returning.
*/

typedef struct {
  u8 *buf;
  u8 *end;
} span;

#define BUF_SZ (1 << 30)

u8 *input_space; // remains immutable once stdin has been read up to EOF.
u8 *output_space;
u8 *cmp_space;
span out, inp, cmp;

int empty(span);
int len(span);

void init_spans(); // main spanio init function

// basic spanio primitives

void prt2cmp();
void prt2std();
void prt(const char *, ...);
void w_char(char);
void wrs(span);
void bksp();
void sp();
void terpri();
void flush();
void flush_err();
void flush_to(char*);
void write_to_file(span content, const char* filename);
span read_file_into_span(char *filename, span buffer);
void redir(span);
span reset();
void w_char_esc(char);
void w_char_esc_pad(char);
void w_char_esc_dq(char);
void w_char_esc_sq(char);
void wrs_esc();
void save();
void push(span);
void pop(span*);
void advance1(span*);
void advance(span*,int);
span pop_into_span();
int find_char(span s, char c);
int contains(span, span);
span take_n(int, span*);
span next_line(span*);
span first_n(span, int);
int span_eq(span, span);
int span_cmp(span, span);
span S(char*);
span nullspan();

typedef struct {
  span *s; // array of spans (points into span arena)
  int n;   // length of array
} spans;

#define SPAN_ARENA_STACK 256

span* span_arena;
int span_arenasz;
int span_arena_used;
int span_arena_stack[SPAN_ARENA_STACK];
int span_arena_stack_n;

void span_arena_alloc(int);
void span_arena_free();
void span_arena_push();
void span_arena_pop();


/* our input statistics on raw bytes */

int counts[256] = {0};

void read_and_count_stdin(); // populate inp and counts[]
int empty(span s) {
  return s.end == s.buf;
}


int len(span s) { return s.end - s.buf; }

u8 in(span s, u8* p) { return s.buf <= p && p < s.end; }

int out_WRITTEN = 0, cmp_WRITTEN = 0;

void init_spans() {
  input_space = malloc(BUF_SZ);
  output_space = malloc(BUF_SZ);
  cmp_space = malloc(BUF_SZ);
  out.buf = output_space;
  out.end = output_space;
  inp.buf = input_space;
  inp.end = input_space;
  cmp.buf = cmp_space;
  cmp.end = cmp_space;
}

void bksp() {
  out.end -= 1;
}

void sp() {
  w_char(' ');
}

/* we might have the same kind of redir_i() as we have redir() already, where we redirect input to come from a span and then use standard functions like take() and get rid of these special cases for taking input from streams or spans. */
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
  span ret = {(u8*)s, (u8*)s + strlen(s) };
  return ret;
}

void s(char* buf, int n, span s) {
  size_t l = (n - 1) < len(s) ? (n - 1) : len(s);
  memmove(buf, s.buf, l);
  buf[l] = '\0';
}

void read_and_count_stdin() {
  int c;
  while ((c = getchar()) != EOF) {
    //if (c == ' ') continue;
    assert(c != 0);
    counts[c]++;
    *inp.buf = c;
    inp.buf++;
    if (len(inp) == BUF_SZ) exit(1);
  }
  inp.end = inp.buf;
  inp.buf = input_space;
}

span saved_out[16] = {0};
int saved_out_stack = 0;

void redir(span new_out) {
  assert(saved_out_stack < 15);
  saved_out[saved_out_stack++] = out;
  out = new_out;
}

span reset() {
  assert(saved_out_stack);
  span ret = out;
  out = saved_out[--saved_out_stack];
  return ret;
}

// set if debugging some crash
const int ALWAYS_FLUSH = 0;

void swapcmp() { span swap = cmp; cmp = out; out = swap; int swpn = cmp_WRITTEN; cmp_WRITTEN = out_WRITTEN; out_WRITTEN = swpn; }
void prt2cmp() { /*if (out.buf == output_space)*/ swapcmp(); }
void prt2std() { /*if (out.buf == cmp_space)*/ swapcmp(); }

void prt(const char * fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  out.end += vsprintf((char*)out.end, fmt, ap);
  if (out.buf + BUF_SZ < out.end) {
    printf("OUTPUT OVERFLOW (%ld)\n", out.end - output_space);
    exit(7);
  }
  va_end(ap);
  if (ALWAYS_FLUSH) flush();
}

void terpri() {
  *out.end = '\n';
  out.end++;
  if (ALWAYS_FLUSH) flush();
}

void w_char(char c) {
  *out.end++ = c;
}

void w_char_esc(char c) {
  if (c < 0x20 || c == 127) {
    out.end += sprintf((char*)out.end, "\\%03o", (u8)c);
  } else {
    *out.end++ = c;
  }
}

void w_char_esc_pad(char c) {
  if (c < 0x20 || c == 127) {
    out.end += sprintf((char*)out.end, "\\%03o", (u8)c);
  } else {
    sp();sp();sp();
    *out.end++ = c;
  }
}

void w_char_esc_dq(char c) {
  if (c < 0x20 || c == 127) {
    out.end += sprintf((char*)out.end, "\\%03o", (u8)c);
  } else if (c == '"') {
    *out.end++ = '\\';
    *out.end++ = '"';
  } else if (c == '\\') {
    *out.end++ = '\\';
    *out.end++ = '\\';
  } else {
    *out.end++ = c;
  }
}

void w_char_esc_sq(char c) {
  if (c < 0x20 || c == 127) {
    out.end += sprintf((char*)out.end, "\\%03o", (u8)c);
  } else if (c == '\'') {
    *out.end++ = '\\';
    *out.end++ = '\'';
  } else if (c == '\\') {
    *out.end++ = '\\';
    *out.end++ = '\\';
  } else {
    *out.end++ = c;
  }
}

void wrs(span s) {
  for (u8 *c = s.buf; c < s.end; c++) w_char(*c);
}

void wrs_esc(span s) {
  for (u8 *c = s.buf; c < s.end; c++) w_char_esc(*c);
}

void flush() {
  if (out_WRITTEN < len(out)) {
    printf("%.*s", len(out) - out_WRITTEN, out.buf + out_WRITTEN);
    out_WRITTEN = len(out);
    fflush(stdout);
  }
}

void flush_err() {
  if (out_WRITTEN < len(out)) {
    fprintf(stderr, "%.*s", len(out) - out_WRITTEN, out.buf + out_WRITTEN);
    out_WRITTEN = len(out);
    fflush(stderr);
  }
}

void flush_to(char *fname) {
  int fd = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0666);
  dprintf(fd, "%*s", len(out) - out_WRITTEN, out.buf + out_WRITTEN);
  //out_WRITTEN = len(out);
  // reset for constant memory usage
  out_WRITTEN = 0;
  out.end = out.buf;
  //fsync(fd);
  close(fd);
}

/* 
In write_to_file we open a file, which must not exist, and write the contents of a span into it, and close it.
If the file exists or there is any other error, we prt(), flush(), and exit as per usual.

In write_to_file_span we simply take the same two arguments but the filename is a span.
We build a null-terminated string and call write_to_file.
*/

void write_to_file_2(span, const char*, int);

void write_to_file(span content, const char* filename) {
  write_to_file_2(content, filename, 0);
}

void write_to_file_2(span content, const char* filename, int clobber) {
  // Attempt to open the file with O_CREAT and O_EXCL to ensure it does not already exist
  /* clobber thing is a manual fixup */
  int flags = O_WRONLY | O_CREAT;
  if (!clobber) flags |= O_EXCL;
  int fd = open(filename, flags, 0644);
  if (fd == -1) {
    prt("Error opening %s for writing: File already exists or cannot be created.\n", filename);
    flush();
    exit(EXIT_FAILURE);
  }

  // Write the content of the span to the file
  ssize_t written = write(fd, content.buf, len(content));
  if (written != len(content)) {
    // Handle partial write or write error
    prt("Error writing to file.\n");
    flush();
    close(fd); // Attempt to close the file before exiting
    exit(EXIT_FAILURE);
  }

  // Close the file
  if (close(fd) == -1) {
    prt("Error closing file after writing.\n");
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

// Function to print error messages and exit (never write "const" in C)
void exit_with_error(char *error_message) {
  perror(error_message);
  exit(EXIT_FAILURE);
}

span read_file_S_into_span(span filename, span buffer) {
  char path[2048];
  s(path,2048,filename);
  return read_file_into_span(path, buffer);
}

span read_file_into_span(char* filename, span buffer) {
  // Open the file
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    exit_with_error("Failed to open file");
  }

  // Get the file size
  struct stat statbuf;
  if (fstat(fd, &statbuf) == -1) {
    close(fd);
    exit_with_error("Failed to get file size");
  }

  // Check if the file's size fits into the provided buffer
  size_t file_size = statbuf.st_size;
  if (file_size > len(buffer)) {
    close(fd);
    exit_with_error("File content does not fit into the provided buffer");
  }

  // Read file contents into the buffer
  ssize_t bytes_read = read(fd, buffer.buf, file_size);
  if (bytes_read == -1) {
    close(fd);
    exit_with_error("Failed to read file contents");
  }

  // Close the file
  if (close(fd) == -1) {
    exit_with_error("Failed to close file");
  }

  // Create and return a new span that reflects the read content
  span new_span = {buffer.buf, buffer.buf + bytes_read};
  return new_span;
}

u8 *save_stack[16] = {0};
int save_count = 0;

void save() {
  push(out);
}

span pop_into_span() {
  span ret;
  ret.buf = save_stack[--save_count];
  ret.end = out.end;
  return ret;
}

void push(span s) {
  save_stack[save_count++] = s.buf;
}

void pop(span *s) {
  s->buf = save_stack[--save_count];
}

/* take_n is a mutating function which takes the first n chars of the span into a new span, and also modifies the input span to remove this same prefix.
After a function call such as `span new = take_n(x, s)`, it will be the case that `new` contatenated with `s` is equivalent to `s` before the call.
*/

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

int starts_with(span a, span b) {
  return len(a) >= len(b) && 0 == memcmp(a.buf, b.buf, len(b));
}

span first_n(span s, int n) {
  span ret;
  if (len(s) < n) n = len(s); // Ensure we do not exceed the span's length
  ret.buf = s.buf;
  ret.end = s.buf + n;
  return ret;
}

int find_char(span s, char c) {
  for (int i = 0; i < len(s); ++i) {
    if (s.buf[i] == c) return i;
  }
  return -1; // Character not found
}

/* next_line(span*) shortens the input span and returns the first line as a new span.
The newline is consumed and is not part of either the returned span or the input span after the call.
I.e. the total len of the shortened input and the returned line is one less than the len of the original input.
If there is no newline found, then the entire input is returned.
In this case the input span is mutated such that buf now points to end.
This makes it an empty span and thus a null span in our nomenclature, but it is still an empty span at a particular location.
This convention of empty but localized spans allows us to perform comparisons without needing to handle them differently in the case of an empty span.
*/

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

/* 
In consume_prefix(span*,span) we are given a span which is typically something being parsed and another span which is expected to be a prefix of it.
If the prefix is found, we return 1 and modify the span that is being parsed to remove the prefix.
Otherwise we leave that span unmodified and return 0.
Typical use is in an if statement to either identify and consume some prefix and then continue on to handle what follows it, or otherwise to skip the if and continue parsing the unmodified input.
*/

int consume_prefix(span *input, span prefix) {
  if (len(*input) < len(prefix) || !span_eq(first_n(*input, len(prefix)), prefix)) {
    return 0; // Prefix not found or input shorter than prefix
  }
  input->buf += len(prefix); // Remove prefix by advancing the start
  return 1;
}

/*
The spans arena implementation.
*/

spans spans_alloc(int n);
int bool_neq(int, int);
span spanspan(span haystack, span needle);
int is_one_of(span x, spans ys);

void span_arena_alloc(int sz) {
  span_arena = malloc(sz * sizeof *span_arena);
  span_arenasz = sz;
  span_arena_used = 0;
  span_arena_stack_n = 0;
}
void span_arena_free() {
  free(span_arena);
}
void span_arena_push() {
  assert(span_arena_stack_n < SPAN_ARENA_STACK);
  span_arena_stack[span_arena_stack_n++] = span_arena_used;
}
void span_arena_pop() {
  assert(0 < span_arena_stack_n);
  span_arena_used = span_arena_stack[--span_arena_stack_n];
}
spans spans_alloc(int n) {
  assert(span_arena);
  spans ret = {0};
  ret.s = span_arena + span_arena_used;
  ret.n = n;
  span_arena_used += n;
  assert(span_arena_used < span_arenasz);
  return ret;
}

/* Generic arrays.

Here we have a macro that we can call with two type names (i.e. typedefs) and a number.
One is an already existing typedef and another will be created by the macro, and the number is the size of a stack, described below.

For example, to create the spans type we might call this macro with span and spans as the names.
We call these the element type and array type names resp.
(We may use "T" and "E" as variables in documentation to refer to them.)

This macro will create a typedef struct with that given name that has a pointer to the element type called "a", a number of elements, which is always called "n", and a capacity "cap", which are size_t's.

We don't implement realloc for simplicity and improved memory layout.
We use an arena allocation pattern.
Because we don't realloc, a common pattern is to make an initial loop over something to count some required number of elements and then to call the alloc function to get an array of the precise size and then to have a second loop where we populate that array.

For every generic array type that we make, we will have:

- A setup function T_arena_alloc(N) for the arena, which takes a number (as int) and allocates (using malloc) enough memory for that many of the element type, where T is the array type name.
- A corresponding T_arena_free().
- A pair T_arena_push() and T_arena_pop().
- A function T_alloc(N) which returns T, always having "n" already set.
- T_push(E) which increments n, complains and exits if cap is reached, and stores the element provided.

The implementation makes a single global struct (both the typedef and the singleton instance) that holds the arena state for the array type.
This includes the arena pointer, the arena size in elements, the number of allocated elements, and a stack of such numbers.
The stack size is also an argument to the macro.
We do not support realloc on the entire arena, rather the programmer needs to choose a big enough value and if we exceed at runtime we will always crash.
The programmer has to call the T_arena_alloc(N) and _free methods themselves, usually in a main() function or similar, and if the function is not called the arena won't be initialized and T_alloc() will always complain and crash (using prt, flush, exit as usual).

The main entry point is the MAKE_ARENA(T,E) macro, which sets up everything and must be called before any references to E in the source code.
Then the arena alloc and free functions must be called somewhere, and everything is ready to use.
*/

#define MAKE_ARENA(T, E, STACK_SIZE) \
typedef struct { \
    T* a; \
    size_t n, cap; \
} E; \
\
static struct { \
    T* arena; \
    size_t arena_size, allocated, stack[STACK_SIZE], stack_top; \
} E##_arena = {0}; \
\
void E##_arena_alloc(int N) { \
    E##_arena.arena = (T*)malloc(N * sizeof(T)); \
    if (!E##_arena.arena) { \
        prt("Failed to allocate arena for " #E "\n", 0); \
        flush(); \
        exit(1); \
    } \
    E##_arena.arena_size = N; \
    E##_arena.allocated = 0; \
    E##_arena.stack_top = 0; \
} \
\
void E##_arena_free() { \
    free(E##_arena.arena); \
} \
\
void E##_arena_push() { \
    if (E##_arena.stack_top == STACK_SIZE) { \
        prt("Exceeded stack size for " #E "\n", 0); \
        flush(); \
        exit(1); \
    } \
    E##_arena.stack[E##_arena.stack_top++] = E##_arena.allocated; \
} \
\
void E##_arena_pop() { \
    if (E##_arena.stack_top == 0) { \
        prt("Stack underflow for " #E "\n", 0); \
        flush(); \
        exit(1); \
    } \
    E##_arena.allocated = E##_arena.stack[--E##_arena.stack_top]; \
} \
\
E E##_alloc(size_t N) { \
    if (E##_arena.allocated + N > E##_arena.arena_size) { \
        prt("Arena overflow for " #E "\n", 0); \
        flush(); \
        exit(1); \
    } \
    E result; \
    result.a = E##_arena.arena + E##_arena.allocated; \
    result.n = N; \
    result.cap = N; \
    E##_arena.allocated += N; \
    return result; \
} \
\
void E##_push(E* e, T elem) { \
    if (e->n == e->cap) { \
        prt("Capacity reached for " #E "\n", 0); \
        flush(); \
        exit(1); \
    } \
    e->a[e->n++] = elem; \
}
/*
Other stuff.
*/

span nullspan() {
  return (span){0, 0};
}

int bool_neq(int a, int b) { return ( a || b ) && !( a && b); }

/* 
C string routines are a notorious cause of errors.
We are adding to our spanio library as needed to replace native C string methods with our own safer approach.
We do not use null-terminated strings but instead rely on the explicit end point of our span type.
Here we have spanspan(span,span) which is equivalent to strstr or memmem in the C library but for spans rather than C strings or void pointers respectively.
We implement spanspan with memmem under the hood so we get the same performance.
Like strstr or memmem, the arguments are in haystack, needle order, so remember to call spanspan with the thing you are looking for as the second arg.
We return a span which is either NULL (i.e. nullspan()) or starts with the first location of needle and continues to the end of haystack.
Examples:

spanspan "abc" "b" -> "bc"
spanspan "abc" "x" -> nullspan
*/

span spanspan(span haystack, span needle) {
  // If needle is empty, return the full haystack as strstr does.
  if (empty(needle)) return haystack;

  // If the needle is larger than haystack, it cannot be found.
  if (len(needle) > len(haystack)) return nullspan();

  // Use memmem to find the first occurrence of needle in haystack.
  void *result = memmem(haystack.buf, len(haystack), needle.buf, len(needle));

  // If not found, return nullspan.
  if (!result) return nullspan();

  // Return a span starting from the found location to the end of haystack.
  span found;
  found.buf = result;
  found.end = haystack.end;
  return found;
}

// Checks if a given span is contained in a spans.
// Returns 1 if found, 0 otherwise.
// Actually a more useful function would return an index or -1, so we don't need another function when we care where the thing is.
int is_one_of(span x, spans ys) {
  for (int i = 0; i < ys.n; ++i) {
    if (span_eq(x, ys.s[i])) {
      return 1; // Found
    }
  }
  return 0; // Not found
}

/*
Library function inp_compl() returns a span that is the complement of inp in the input_space.
The input space is defined by the pointer input_space and the constant BUF_SZ.
As the span inp always represents the content of the input (which has been written so far, for example by reading from stdin), the complement of inp represents the portion of the input space after inp.end which has not yet been written to.

We have cmp_compl() and out_compl() methods which do the analogous operation for the respective spaces.
*/

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

// END LIBRARY CODE
