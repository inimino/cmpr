/*

# CMPr

## "Future programming" here now?

Thesis: the future of programming is driving an LLM to write code.
Why?
Mostly, because it's way more fun.
There are some practical benefits too.
You can work with any technology, so it's a great technique for a generalist.
When code is written by an AI, we treat it as disposable.
What's important is the English that described that code.
It doesn't matter as much what language the code is in.

## What's this then?

This is a tool to play with and support the LLM programming workflow.

Today we just support ChatGPT and interaction is by copy and paste.
This means you can use it with a free account, or if you have a paid account you can use GPT4, which writes better code.

(Coming soon: API usage, local models, competing LLMs, etc.)

This is a framework and represent a particular and very opinionated approach to this workflow.
We will be updating continuously as we learn.
(This version was written in ONE DAY, using the workflow itself.)

All code here is by ChatGPT4, and all comments by me, which is the idea of the workflow:

- You have a "block" which starts with a comment and ends with code.
- You write the comment; the LLM writes the code.
- You iterate on the comment until the code works and meets your standard.

The tool is a C program; compile it and run it locally, and it will go into a loop where it shows you your "blocks".
You can use j/k to move from block to block. (TODO: full text search with "/")
Once you find the block you want, use "e" to open it up in "$EDITOR" (or vim by default).
Then you do ":wq" or whatever makes your editor exit successfully, and a new revision of your code is automatically saved under "cmpr/revs/<timestamp>".

To get the LLM involved, when you're on a block you hit "r" and this puts a prompt into the clipboard for you.
(You won't see anything happen when you hit "r", but the clipboard has been updated.)
Then you switch over to your ChatGPT window and hit "Ctrl-V".
You could edit the prompt, but usually you'll just hit Enter.
ChatGPT writes the code, and then you copy it directly into back into the block or you can fix it up if you want.
You can click "copy code" in the ChatGPT window and then hit "R" (uppercase) in the tool to replace everything after the comment with the clipboard contents.
Mnemonic: "r" gets the LLM to "rewrite" (or "replace") the code to match the comment (and "R" is the opposite of "r").

You can currently also hit "q" to quit, "?" for short help, "b" to build, and more features are coming soon.

## Quick start:

- get the code into ~/cmpr
- assumption is that everything runs from ~, so `cd ~` with cmpr in your home dir
- `gcc -o cmpr/cmpr cmpr/cmpr.c` or use clang or whatever you like
- `export EDITOR=emacs` or whatever editor you use
- `cmpr/cmpr < cmpr/cmpr.c` now you're in the workflow (using cmpr to dev cmpr), you'll see blocks, you can try "j"/"k", "e" and "r"

Day one version only useful for developing itself, because we have "cmpr.c" and similar hard-coded everywhere.
Check back tomorrow for a way to adapt it to your own codebase, start a new project, etc.
If you want to use "b" to build your code then run it like this: `CMPR_BUILD="my build command" cmpr/cmpr < cmpr/cmpr.c`, or by default it will just run `make`.

Developed on Linux; volunteers to try on Windows/MacOS/... and submit bug reports / patches very much welcomed!
We are using "xclip" to send the prompts to the clipboard.
Either install xclip, or edit the `cbsend` and `cbpaste` strings in the source code to suit your environment (TODO: make this better).
I.e. for Mac you would use "pbcopy" and "pbpaste".

## More

Development is being [streamed on twitch](https://www.twitch.tv/inimino2).
Join [our discord](https://discord.gg/ekEq6jcEQ2).

*/

/* #libraryintro

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
- `redir(span)`, `reset()`: Redirects output to a new span and resets it to the previous output span.
- `save()`, `push(span)`, `pop(span*)`, `pop_into_span()`: Manipulates a stack for saving and restoring spans.
- `advance1(span*)`, `advance(span*, int)`: Advances the start pointer of a span by one or a specified number of characters.
- `find_char(span, char)`: Searches for a character in a span and returns its index.
- `contains(span, span)`: Checks if one span contains another.
- `take_n(int, span*)`, `head_n(int, span*)`: Takes the first n characters from a span and returns them as a new span.
- `next_line(span*)`: Extracts the next line from a span and returns it as a new span.
- `span_eq(span, span)`, `span_cmp(span, span)`: Compares two spans for equality or lexicographical order.
- `S(char*)`: Creates a span from a null-terminated string.
- `nullspan()`: Returns an empty span.
- `spans_alloc(int)`: Allocates a spans structure with a specified number of span elements.
- `span_arena_alloc(int)`, `span_arena_free()`, `span_arena_push()`, `span_arena_pop()`: Manages a memory arena for dynamic allocation of spans.
- `is_one_of(span, spans)`: Checks if a span is one of the spans in a spans.
- `spanspan(span, span)`: Finds the first occurrence of a span within another span and returns the rest of the haystack span starting from that point.
- `w_char_esc(char)`, `w_char_esc_pad(char)`, `w_char_esc_dq(char)`, `w_char_esc_sq(char)`, `wrs_esc()`: Write characters (or in the case of wrs, spans) to out, the output span, applying various escape sequences as needed.

typedef struct { u8* buf; u8* end; } span; // reminder of the type of span

typedef struct { span *s; int n; } spans; // reminder of the type of spans, given by spans_alloc(n)

spans_alloc returns a spans which has n equal to the number passed in.
Typically the caller will either fill the number requested exactly, or will shorten n if fewer are used.
These point into the spans arena which must be set up by calling span_arena_alloc() with some integer argument (in bytes).
A common idiom is to iterate over something once to count the number of spans needed, then call spans_alloc and iterate again to fill the spans.
When spans tile a span, the buf of each but the first is a duplicate of the previous .end, but we don't care about this waste.

Common idiom: use a single span, with buf pointing to one thing and end to something else found later; return this result (from many functions returning span).

Note that we NEVER write const in C, as this only brings pain (we have some uses of const in library code, but PLEASE do not create any more).
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
/* 
The inp variable is the span which writes into input_space, and then is the immutable copy of stdin for the duration of the process.
The number of bytes of input is len(inp).
*/

int empty(span);
int len(span);

void init_spans(); // init buffers

/* 
basic spanio primitives

The output is stored in span out, which points to output_space.
Input processing is generally by reading out of the inp span or subspans of it.
The output spans are mostly written to with prt() and other IO functions.
The cmp_space and cmp span which points to it are used for analysis and model data, both reading and writing.
*/

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
*/

void write_to_file(span content, const char* filename) {
  // Attempt to open the file with O_CREAT and O_EXCL to ensure it does not already exist
  int fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd == -1) {
    prt("Error opening file for writing: File already exists or cannot be created.\n");
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

// Function to print error messages and exit (never write "const" in C)
void exit_with_error(char *error_message) {
  perror(error_message);
  exit(EXIT_FAILURE);
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

// END LIBRARY CODE

/* 
Below we have shell functions which we use for building.
We export these functions into a separate file so that they can be sourced at the shell.
e.g. `. cmpr/functions.sh` brings the current functions into scope.
Then we use `build` to do a build, and so on.

SH_FN_START

F=cmpr/cmpr.c

prepare_commit() {
  awk '/^}$/ {exit} {print}' < cmpr/cmpr.c > cmpr/README.md
  rm -r cmpr/cmpr.c
  cp $(cd cmpr; find revs | sort | tail -n 1 ) cmpr/cmpr_orig.c
  ln -s cmpr_orig.c cmpr/cmpr.c
  ls -lh cmpr/cmpr.c cmpr/README.md
}

export_functions() {
  awk 'BEGIN{EMIT=0} /^SH_FN_END$/{EMIT=0} {if(EMIT)print} /^SH_FN_START$/{EMIT=1}' <"$F" >cmpr/functions.sh;
}

spanio_pre() {
  echo "Here's our library code:"
  echo
  echo '```c'
}

spanio_post() {
  echo '```'
  cat <<EOF
You are an expert code writer, writing complete and accurate code yourself, NOT telling someone else how to write the code.
Your code will be integrated into an existing system, so just reply with the code that was asked for.
Never silently consume errors, either don't test for something and assume that it's correct, or if you must test for it and it is not correct, then fail loudly and exit.
Answer briefly and focus on code.
Usually you will be asked to write a function that is described by a comment.
In this case just reply with a single function.
Feel free to write function declarations for any helper or library funtions that we need but don't already have.
Don't implement the later helper functions, as we will do that in a subsequent step.
EOF
}

spanio_prompt() {
  ( spanio_pre
    awk '/END LIBRARY CODE/ {exit} {print}' "$F"
    spanio_post
  ) | xclip -i -selection clipboard
}

code_blocks() {
  awk '/^\/\*$/ { start = NR; } /^}$/ { if (start != 0) { print start "," NR; start = 0; } }'
}

code_prompt() {
  ( echo "Here's our code so far. Reply with \"OK\"."
    echo '```'
    awk '/END LIBRARY CODE/,0' "$F" | tail -n +2
    echo '```'
  ) | xclip -i -selection clipboard
}

index() {
  ( cd cmpr
    ctags "$F"
    code_blocks < "$F" > blocks
  )
}

build() {
  ( cd cmpr
    gcc -o cmpr cmpr.c
  )
}

clipboard_copy() {
  xclip -i -selection clipboard
}

clipboard_paste() {
  xclip -o -selection clipboard
}

SH_FN_END

Reply only with code. Do not simplify, but include working code. If necessary, you may define a helper function which we will implement later, but do not include comments leaving out key parts. Your code must be complete as written, but calling functions that do not yet exist is allowed.

*/

void send_to_clipboard(span prompt);

/*
We define CONFIG_FIELDS including all our known config settings, as they are used in several places (with X macros).

The known config settings are:

- projdir, location of the project directory that we are working with as a span
- revdir, a span containing a relative or absolute path to where we store our revisions
- tmpdir, another path for temp files that we use for editing blocks
- projfile, the symlink that points to the current rev, and might be used in a build process, etc
- buildcmd, the command to do a build (e.g. by the "b" key)
- cbcopy, the command to pipe data to the clipboard on the user's platform
- cbpaste, the same but for getting data from the clipboard
- blockdef, the name of the block definition configuration (e.g. "C" or "Python")
*/

#define CONFIG_FIELDS \
X(projdir) \
X(revdir) \
X(builddir) \
X(tmpdir) \
X(projfile) \
X(buildcmd) \
X(cbcopy) \
X(cbpaste) \
X(blockdef)

/*
We define a struct, ui_state, which we can use to store any information about the UI that we can then pass around to various functions as a single entity.
This includes, so far:
- the blocks
- the current index into them
- a "marked" index, which represent the "other end" of a selected range
- the search span which will contain "/" followed by some search if in search mode, otherwise will be empty()
- terminal_rows and _cols which stores the terminal dimensions
-    int block_rows;        // Rows to spend on a block

Additionally, we include a span for each of the config files, which we can do with an X macro inside the struct, using our CONFIG_FIELDS define above.
*/


typedef struct {
    spans blocks;          // Array of spans representing blocks of text/code
    int current_index;     // Current index into the blocks
    int marked_index;      // Marked index, representing the other end of a selected range
    span search;           // Search span, starts with "/" for search mode, empty otherwise
    int terminal_rows;     // Terminal rows
    int terminal_cols;     // Terminal columns
    int block_rows;        // Rows to spend on a block

    // Configuration spans
    #define X(name) span name;
    CONFIG_FIELDS
    #undef X
} ui_state;
/*
In main,

First we call init_spans, since all our i/o relies on it.

We declare a ui_state variable `state` which we'll pass around for ever after.

We call a function handle_args to handle argc and argv, and pass a pointer to our state into this.
This function will also read our config file (if any).

We call span_arena_alloc(), and at the end we call span_arena_free() just for clarity even though it doesn't matter anyway since we're exiting the process.
We allocate a spans arena of 1 << 20 or a binary million spans.

Next we call a function get_code(), which also takes a pointer to our ui state.
This function either handles reading standard input or if we are in "project directory" mode then it reads the files indicated by our config file.
In either case, once this returns, inp is populated and any other initial code indexing work is done.

Then we call main_loop() which takes our ui state as the only arg.

The main loop reads input in a loop and probably won't return, but just in case, we always call flush() before we return so that our buffered output from prt and friends will be flushed to stdout.
*/

void handle_args(int argc, char **argv, ui_state* state);
void get_code(ui_state* state);
void main_loop(ui_state* state);

int main(int argc, char **argv) {
  init_spans(); // Initialize the global span buffers
  ui_state state; // Declare the ui_state variable

  handle_args(argc, argv, &state); // Pass a pointer to state for handling arguments and config

  span_arena_alloc(1 << 20); // Allocate memory for a binary million spans

  get_code(&state); // Populate inp and perform initial indexing

  main_loop(&state); // Enter the main interaction loop

  flush(); // Ensure all buffered output is flushed to stdout
  span_arena_free(); // Free the allocated spans arena

  return 0;
}


void print_config(ui_state* state) {
    printf("Current Configuration Settings:\n");

    #define X(name) printf(#name ": %.*s\n", (int)(state->name.end - state->name.buf), state->name.buf);
    CONFIG_FIELDS
    #undef X
}

/*
We have exit_success and exit_with_error for convenience.

Because it is such a common pattern, we include a exit_success() function of no arguments, which calls flush and then exit(0).
*/

void exit_success() {
    flush(); // Assuming flush() is defined elsewhere
    exit(0);
}

void print_config(ui_state*);
void parse_config(ui_state*,char*);
void exit_with_error(char*);

/*
In handle_args we handle any command-line arguments.

If "--conf <alternate-config-file>" is passed, we read our configuration file from that instead of ".cmpr/conf", which is the default.
Once we know the conf file to read from, call parse_config before we do anything else.

If "--print-conf" is passed in, we print our configuration settings and exit.
This is only OK to write because we have already called parse_config, so the configuration settings have already been read in from the file.

With the "--help" flag we just prt a short usage summary and exit_success().
We include argv[0] as usual and indicate that typical usage is by providing a project directory as the argument, and summarize all the flags that we support.

helper functions, declared here before any other code, and called here, but defined elsewhere:

- void parse_config(ui_state*, char*)
- void print_config(ui_state*)
- void exit_with_error(char*)

main function in this block:

- void handle_args(int argc, char **argv, ui_state* state);
*/

void handle_args(int argc, char **argv, ui_state* state) {
    char* config_file_path = ".cmpr/conf"; // Default configuration file path

    // Check for "--conf <alternate-config-file>" argument
    for (int i = 1; i < argc - 1; i++) { // Ensure there's a next argument after "--conf"
        if (strcmp(argv[i], "--conf") == 0) {
            config_file_path = argv[i + 1]; // Use the alternate configuration file
            break; // No need to look further
        }
    }

    // Parse configuration from the determined file path
    parse_config(state, config_file_path); // Assuming parse_config takes the state and file path

    // Now handle other command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--print-conf") == 0) {
            print_config(state);
            exit_success();
        } else if (strcmp(argv[i], "--help") == 0) {
            prt("Usage: %s [options]\n", argv[0]);
            prt("Options:\n");
            prt("  --help               Display this help message and exit.\n");
            prt("  --print-conf         Print the current configuration settings and exit.\n");
            prt("  --conf <config file> Specify an alternate configuration file.\n");
            prt("Typical usage involves providing a project directory as an argument or specifying an alternate configuration file.\n");
            exit_success();
        }
    }
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

void reset_stdin_to_terminal();
spans find_blocks(ui_state*,span);

/*
In get_code, we get the code into the input buffer.

If the configuration file is being used, typically state->projfile will be set to a non-empty() span.
In this case, we will read this file into inp by read_file_into_span with inp_compl() as the span argument.

If the configuration file didn't specify projfile, then we are expecting input on stdin.
So in this case we call read_and_count_stdin().
If we call read_and_count_stdin, then we must call another function reset_stdin_to_terminal, which re-opens the terminal on stdin, as our stdin typically will have been redirected from a file by the shell.

Finally, if we are "project directory" mode then we will read all the files in the project into the input space.
This is not supported yet.

Next, we call find_blocks() to find the blocks in our input, which we will be using for block-oriented editing flow after this point.
This function takes the entire input span inp as an argument and returns a spans containing the blocks that it finds, which we store on our ui state.
*/


void get_code(ui_state* state) {
    if (!empty(state->projfile)) {
        // Read the project file into inp
        span inp_complement = inp_compl();
        char projfile_buf[1024] = {0};
        s(projfile_buf, 1024, state->projfile);
        inp = read_file_into_span(projfile_buf, inp_complement);
    } else {
        // Read code from stdin
        read_and_count_stdin();
        reset_stdin_to_terminal();
    }

    // Placeholder for future "project directory" mode implementation
    // if (!empty(state->projdir)) {
    //     // Read all files in the project directory into the input space
    // }

    // Find blocks in the input
    state->blocks = find_blocks(state, inp);
}
/*
In find_blocks_1, we take a span, and we return a spans which contains blocks that fully cover the input.

A block is defined as follows:

There is a special pattern that defines the start of a block.
For a C file this might be slash star, for a python file, triple-quote, and so on.

From the start of the file to the first occurence of this pattern is a block, unless this block would be empty.

Every subsequent block starts with the pattern, and ends with a newline.

To find the blocks, we simply scan each line, and call a helper function, is_line_block_pattern_start.
This predicate function takes a span and returns 0 or 1.

We perform the loop twice, the first time we just count the number of blocks that are in the file, then we can allocate our spans of the correct size using spans_alloc(n_blocks), and then we loop the same way again but now populating the spans which we will return.
Before the first loop we copy our argument into a new spans so that we can consume it while counting, but then still have access to the original full span for the second loop.

Here we can use a common pattern which is that we declare a span that we are going to return or assign, and then we set .buf first (here when we find the starting line) and then set .end later (when we find the closing brace).

*/

// Prototype of the helper function
int is_line_block_pattern_start(ui_state* state, span line);

// Function to find blocks
spans find_blocks(ui_state* state, span input) {
  span copy = input; // Copy of the input span to consume while counting
  int n_blocks = 0;

  // First loop: count blocks
  while (!empty(copy)) {
    span line = next_line(&copy);
    if (is_line_block_pattern_start(state, line) || (n_blocks == 0 && !empty(line))) {
      n_blocks++;
    }
  }

  // Allocate spans for the blocks
  spans blocks = spans_alloc(n_blocks);

  // Reset copy for second loop
  copy = input;
  int block_index = 0;
  span current_block_start = nullspan();

  // Second loop: populate blocks
  while (!empty(copy) && block_index < n_blocks) {
    span line = next_line(&copy);
    int is_start = is_line_block_pattern_start(state, line);

    if (block_index == 0 && !empty(line) && is_start) {
      current_block_start = line;
      blocks.s[block_index].buf = current_block_start.buf;
      block_index++;
    } else if (is_start) {
      blocks.s[block_index - 1].end = line.buf;
      current_block_start = line;
      blocks.s[block_index].buf = current_block_start.buf;
      block_index++;
    } else if (empty(copy)) { // End of input, close last block
      blocks.s[block_index - 1].end = copy.buf;
    }
  }

  // Correctly close the last block if not already done
  if (n_blocks > 0 && blocks.s[n_blocks - 1].end == NULL) {
    blocks.s[n_blocks - 1].end = input.end;
  }

  return blocks;
}

/*
In is_line_block_pattern_start, we get a span which represents a line.

If it starts with slash star, we return 1.
*/

// *** manual fixup ***

int is_line_block_pattern_start_0(span line) {
  // Check if the line starts with "/*"
  if ((line.end - line.buf) >= 2 && strncmp(line.buf, "/*", 2) == 0) {
    return 1;
  }
  return 0;
}

int is_line_block_pattern_start_python(span line) {
  // Check if the line starts with """
  return span_eq(S("\"\"\""), first_n(line, 3));
}

int is_line_block_pattern_start(ui_state* state, span line) {
  if(span_eq(state->blockdef,S("Python"))) {
    return is_line_block_pattern_start_python(line);
  } else {
    return is_line_block_pattern_start_0(line);
  }
}

/*
Function to read a single character without echoing it to the terminal.
*/

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




/*
In reset_stdin_to_terminal, we use the technique of opening /dev/tty for direct keyboard input with dup2 to essentially "reset" stdin to the terminal, even if it was originally redirected from a file.
This approach allows us to switch back to reading from the terminal without having to specifically manage a separate file descriptor for /dev/tty in the rest of the program.
First we open the terminal device, dup2 to stdin (fd 0), and finally close the tty fd as it's no longer needed.
*/

void reset_stdin_to_terminal() {
  int tty_fd = open("/dev/tty", O_RDONLY);
  if (tty_fd < 0) {
    perror("Failed to open /dev/tty");
    exit(EXIT_FAILURE);
  }

  if (dup2(tty_fd, STDIN_FILENO) < 0) {
    perror("Failed to duplicate /dev/tty to stdin");
    close(tty_fd);
    exit(EXIT_FAILURE);
  }

  close(tty_fd);
}

/*
In main_loop, we declare a ui_state variable to hold the current state of the UI.

The only argument to main_loop is the blocks, so we first assign these and we start with current_index 0, i.e. looking at the first block.
We assign marked index to -1 indicating that there is no mark (i.e. we are not in visual selection mode).

We will always print the current block to the terminal, after clearing the terminal.
Then we will wait for a single keystroke of keyboard input using our getch() above.

We also define a helper function print_current_blocks which takes the ui state and prints the current block.
(Reminder: we never write `const` in C.)

Once we have a keystroke, we will call another function, handle_keystroke, which will take the char that was entered and a pointer to our struct with the state.
This function will be responsible for updating anything about the display or handling other side effects.
When it returns we will always be ready for another input keystroke.
*/

void print_current_blocks(ui_state* state);
void handle_keystroke(char keystroke, ui_state* state);
char getch(void);

void main_loop(ui_state* state) {
  state->current_index = 0; // Start with the first block
  state->marked_index = -1;

  while (1) {
    // Clear the terminal
    printf("\033[H\033[J");

    // Print the current block
    print_current_blocks(state);

    /* *** manual fixup ***/
    flush();

    // Wait for a single keystroke of keyboard input
    char input = getch();

    // Handle the input keystroke
    handle_keystroke(input, state);
    //print_current_blocks(state);
  }
}

/* Helper function to print the first n lines of a block. */

void print_first_n_lines(span block, int n) {
  int line_count = 0;
  span copy = block; // Make a copy to avoid modifying the original span
  while (!empty(copy) && line_count < n) {
    span line = next_line(&copy);
    wrs(line); // Write the line to the output span
    terpri();  // Append a newline character
    line_count++;
  }
}

/*
In toggle_visual, we test if we are in visual selection mode.
If the marked index is not -1 then we are in visual mode, and we leave the mode (by setting it to -1).
Otherwise we enter it by setting marked index to be the current index.
In either case we then reflect the new state in the display by calling print_current_blocks()
*/


void toggle_visual(ui_state *state) {
    if (state->marked_index != -1) {
        // Leave visual mode
        state->marked_index = -1;
    } else {
        // Enter visual mode
        state->marked_index = state->current_index;
    }
    // Reflect the new state in the display
    print_current_blocks(state);
}
/*
In print_current_blocks, we print either the current block, if we are in normal mode, or the set of selected blocks if we are in visual mode.

Before this, we write a helper function that gets the screen dimensions (rows and cols) from the terminal.
This function will take the ui_state* and update it directly.

If marked_index != -1 and != current_index, then we have a "visual" selected range of more than 1 block.

First we determine which of marked_index and current_index is lower and make that our block range start, and then one past the other is our block range end (considered as an exclusive endpoint).

The difference between the two is then the number of selected blocks.
At this point we know how many blocks we are displaying.
Finally we pass state, inclusive start, and exclusive end of range to another function handles rendering.

Helper functions:

- render_block_range(ui_state*,int,int) -- also supports rendering a single block (if range includes only one block).
*/

void get_screen_dimensions(ui_state* state) {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  state->terminal_rows = w.ws_row;
  state->terminal_cols = w.ws_col;
}

void render_block_range(ui_state*, int, int);

void print_current_blocks(ui_state* state) {
  get_screen_dimensions(state);

  if (state->marked_index != -1 && state->marked_index != state->current_index) {
    int start = state->current_index < state->marked_index ? state->current_index : state->marked_index;
    int end = state->current_index > state->marked_index ? state->current_index + 1 : state->marked_index + 1;

    render_block_range(state, start, end);
  } else {
    // Normal mode or visual mode with only one block selected.
    render_block_range(state, state->current_index, state->current_index + 1);
  }
}

/*
In render_block_range, we get the state and a range of blocks (first endpoint inclusive, second exclusive).

The state tells us (with terminal_rows and _cols) what the dimensions of the terminal are.

First we calculate the length in lines of each of the blocks that we have.

More specifically, we calculate a mapping from logical lines to physical lines.
First we identify the logical lines by finding the newlines in the block, and we store the offset of each (as an offset from .buf on the block).
We find out how many "physical" lines (i.e. terminal rows) each logical line uses by taking the ceil of quotient of line length by terminal cols.

Then we decide, based on the number of blocks, the layout information, and the terminal dimensions, how many lines from each we can fit on the screen.
For this we declare a separate helper function which we will write later.
We call this function (with the ui state and our logical-physical mapping) and then we free our own resources.

We create a struct line_mapping which is a return type for calculate_line_mappings(span block, int terminal_cols), which we write next, and then free_line_mappings().
Finally we implement render_block_range itself in terms of these.

In calculate_line_mappings, we start with memory for 1024 physical and logical lines, and then realloc, doubling every time, as necessary.

helper functions:

- void decide_layout_and_render(ui_state* state, line_mapping* mappings, int start_block, int end_block);

We include a forward declaration for our helper functions.
*/

typedef struct {
    int* line_offsets;    // Array of offsets for the start of each logical line
    int* physical_lines;  // Array indicating how many physical lines each logical line occupies
    int total_logical_lines; // Total number of logical lines in the block
    int total_physical_lines; // Total sum of physical lines required for the block
} line_mapping;

void decide_layout_and_render(ui_state* state, line_mapping* lm, int start_block, int end_block);

line_mapping calculate_line_mappings(span block, int terminal_cols) {
    int capacity = 1024; // Starting capacity for logical and physical lines
    line_mapping lm = {
        .line_offsets = (int*)malloc(capacity * sizeof(int)),
        .physical_lines = (int*)malloc(capacity * sizeof(int)),
        .total_logical_lines = 0,
        .total_physical_lines = 0
    };

    int current_offset = 0;
    while (current_offset < block.end - block.buf) {
        if (lm.total_logical_lines == capacity) {
            capacity *= 2; // Double the capacity
            lm.line_offsets = (int*)realloc(lm.line_offsets, capacity * sizeof(int));
            lm.physical_lines = (int*)realloc(lm.physical_lines, capacity * sizeof(int));
        }

        lm.line_offsets[lm.total_logical_lines] = current_offset;
        span current_line = { .buf = block.buf + current_offset, .end = block.end };
        int line_length = 0;
        while (current_line.buf + line_length < block.end && current_line.buf[line_length] != '\n') {
            ++line_length;
        }

        int physical_line_count = (int)ceil((double)line_length / terminal_cols);
        lm.physical_lines[lm.total_logical_lines] = physical_line_count;
        lm.total_physical_lines += physical_line_count;
        ++lm.total_logical_lines;

        current_offset += line_length + 1; // Skip newline character
    }

    return lm;
}

void free_line_mappings(line_mapping lm) {
    free(lm.line_offsets);
    free(lm.physical_lines);
}

//* *** manual fixup ***/

void print_multiple_partial_blocks(ui_state*,line_mapping*,int,int);
void print_single_block_with_skipping(ui_state*,line_mapping*,int);
void render_block_range(ui_state* state, int start, int end) {
    line_mapping* mappings = (line_mapping*)malloc((end - start) * sizeof(line_mapping));
    line_mapping* lm = mappings;
    for (int i = start; i < end; ++i) {
        mappings[i - start] = calculate_line_mappings(state->blocks.s[i], state->terminal_cols);
    }

    if (end - start == 1) {
      print_single_block_with_skipping(state, lm, start);
    } else {
      print_multiple_partial_blocks(state, lm, start, end);
    }

    //decide_layout_and_render(state, mappings, start, end);

    for (int i = 0; i < end - start; ++i) {
        free_line_mappings(mappings[i]);
    }
    free(mappings);
}

/*
In print_multiple_partial_blocks, we get a state and we should print as much as we can of the blocks.
Currently, we just print the number of blocks that there are.
*/

// Placeholder for print_multiple_partial_blocks, assuming it's defined elsewhere
void print_multiple_partial_blocks(ui_state* state, line_mapping *lm, int start_block, int end_block) {
  prt("%d blocks (printing multiple blocks coming soon!)\n", end_block - start_block);
}

/*
In print_last_lines, a helper function, we get the state, the line mapping, a block, and a number of lines to show.
To print the last lines, since we already have physical line counts for each logical line, we just sum up in one loop, working backward from the end, to find out how many logical lines we can include.
Then we actually print starting from that logical line, skipping some physical lines if necessary, so we print the correct information in the correct order but without scrolling the terminal.
For example, if there were space for 3 more physical lines, and we had one physical line in the last logical line and 5 physical lines in the penultimate logical line as per the line mapping, then we would start printing from the penultimate logical line, but skip the first three physical lines of that output (by skipping over terminal_cols * 3 bytes of that logical line) and then we would print the last two physical lines of that logical line, and then print the remaining logical (and physical) last line.
*/

void print_last_lines(ui_state* state, line_mapping* lm, span block, int bottom_lines_to_show) {
    int physical_lines_needed = bottom_lines_to_show;
    int logical_line_start_index = lm->total_logical_lines - 1;
    int physical_lines_counted = 0;

    // Backtrack to find the logical start line for printing bottom lines
    for (int i = lm->total_logical_lines - 1; i >= 0 && physical_lines_needed > 0; --i) {
        physical_lines_needed -= lm->physical_lines[i];
        logical_line_start_index = i;
        physical_lines_counted += lm->physical_lines[i];
    }

    // Adjust the starting byte if we're skipping physical lines within the first logical line to print
    int skip_physical_lines = physical_lines_counted - bottom_lines_to_show;
    int skip_bytes = skip_physical_lines * state->terminal_cols; // This simplification assumes uniform line length, adjust as needed

    for (int i = logical_line_start_index; i < lm->total_logical_lines; ++i) {
        int line_start_offset = lm->line_offsets[i];
        int line_end_offset = (i < lm->total_logical_lines - 1) ? lm->line_offsets[i + 1] : (block.end - block.buf);
        
        // Adjust start offset for the first line if we're skipping bytes
        if (i == logical_line_start_index) {
            line_start_offset += skip_bytes;
        }

        // Ensure we don't exceed the block bounds when adjusting
        line_start_offset = line_start_offset < (block.end - block.buf) ? line_start_offset : (block.end - block.buf);
        
        int line_length = line_end_offset - line_start_offset;
        char* line_buffer = (char*)malloc(line_length + 1); // +1 for null-terminator
        memcpy(line_buffer, block.buf + line_start_offset, line_length);
        line_buffer[line_length] = '\0'; // Ensure string is null-terminated

        prt("%s", line_buffer); // Print the line; prt handles formatting and appending

        free(line_buffer); // Clean up
    }
}

/* #printsingle

In printsingle, we get the ui state and a limit of lines to print.
We call printsinglehelper(), which takes:

- the ui state,
- a max number of lines to print phy_row_lim,
- a number of lines to skip phy_row_off,
- an indicator (1) to print or (0) to count the lines,
- an indicator (1) to split or (0) favor the 
- the address of a decrement counter for lines remaining (non-negative),
- the address of a span of unprinted input.

We call an offset (the number of rows to skip) perfect if it perfectly fills the limit, which means that a call to the helper sends the lines remaining to zero and the len of unprinted input to zero on the same call.
In the helper, if 

The limit must always be less than or equal to state.terminal_rows, otherwise it will be ignored, since we are a paging output mode.
The block to print from is given by the second argument, always a zero-based integer < state->blocks.n.

If we are in splitting mode, then we first need to determine whether we have more than

Specifically:

- block_row_limit sets a hard limit of vertical lines to be occupied by the printing of the block
- block_row_offset skips a number of vertical lines of the block before it begins printing at the beginning of a (physical) line.

The caller sets block_row_limit to a desired limit before the call to this function.
If block_row_limit is zero, we return 0 (the number of lines we have successfully printed, as always).
If it is negative we prt() a short complaint with newline and exit_failure().

We copy the block into a local span for convenience.

In our first loop, we count physical lines until we know that we have skipped block_row_offset lines.
We perform this by calling a function, span decrement_physical_skip(int*,int,span), described later.
The arguments to this function are, in order, the address of block_row_limit, terminal_cols, and our copy of the block, and we assign the return value back to our copy of the block as the block copy always contains data not yet printed (or in this loop, skipped).
If the len() of the remaining block contents goes to zero in this loop, then we return zero.

We then skip more physical lines until we know whether we can overfill block_row_limit.

If the number of physical lines printed equals the block_row_limit, and the block_row_limit was positive, we can say the offset is "perfect".

*/
/*
In handle_keystroke, we support the following single-char inputs:

- j/k Increment or decrement respectively the index of the currently displayed block.
  If we are at 0 or the max, these are no-ops.
  We are also responsible for updating the display before returning to the main loop, so after either of these we call print_current_blocks.
- e, which calls a function that will let the user edit the current block
- r, which calls a function that will let an LLM rewrite the code part of the current block based on the comment
- R, kin to "r", which reads current clipboard contents back into the block, replacing the code part
- b, do a build (runs the command in CMPR_BUILD envvar, or "make")
- v, sets the marked point to the current index, switching to "visual" selection mode, or leaves visual mode if in it
- /, switches to "search" mode by calling a helper function.
- S, (likely to change) goes into settings mode
- ?, which displays some help about the keyboard shortcuts available
- q, which exits the process cleanly (with prt("goodbye\n"); flush(); exit(0))

Before the function itself we write function declarations for all helper functions needed.
- edit_current_block(ui_state*)
- rewrite_current_block_with_llm(ui_state*)
- compile(ui_state*)
- replace_code_clipboard(ui_state*)
- toggle_visual(ui_state*)
- start_search(ui_state*)
- settings_mode(ui_state*)

To get the help text we can basically copy the lines above, except formatted nicely for terminal output.
*/


#include <stdlib.h> // For exit

// Forward declarations of helper functions
void edit_current_block(ui_state*);
void rewrite_current_block_with_llm(ui_state*);
void compile(ui_state*);
void replace_code_clipboard(ui_state*);
void toggle_visual(ui_state*);
void start_search(ui_state*);
void settings_mode(ui_state*);
void print_help(void);
void print_current_blocks(ui_state*);

void handle_keystroke(char input, ui_state* state) {
    switch (input) {
        case 'j':
            if (state->current_index < state->blocks.n - 1) {
                state->current_index++;
            }
            break;
        case 'k':
            if (state->current_index > 0) {
                state->current_index--;
            }
            break;
        case 'e':
            edit_current_block(state);
            break;
        case 'r':
            rewrite_current_block_with_llm(state);
            break;
        case 'R':
            replace_code_clipboard(state);
            break;
        case 'b':
            compile(state);
            break;
        case 'v':
            toggle_visual(state);
            break;
        case '/':
            start_search(state);
            break;
        case 'S':
            settings_mode(state);
            break;
        case '?':
            print_help();
            break;
        case 'q':
            prt("goodbye\n");
            flush();
            exit(0);
            break;
        default:
            // No operation for other keys
            break;
    }
}

void print_help(void) {
    prt("Keyboard shortcuts:\n");
    prt("- j: Move to next block\n");
    prt("- k: Move to previous block\n");
    prt("- e: Edit the current block\n");
    prt("- r: Rewrite the current block with LLM\n");
    prt("- R: Replace code part of the block with clipboard contents\n");
    prt("- b: Build the project\n");
    prt("- v: Toggle visual mode\n");
    prt("- /: Search mode\n");
    prt("- S: Settings mode\n");
    prt("- ?: Display this help\n");
    prt("- q: Quit the application\n");
    flush(); // Ensure the help text is displayed immediately
}
/*
In clear_display() we clear the terminal by printing some escape codes (with prt and flush as usual).
*/


void clear_display() {
    prt("\033[2J\033[H"); // Escape codes to clear the screen and move the cursor to the top-left corner
    flush();
}
/*
In print_block, similar to our earlier print_current_block, we clear the display and print the block, the only difference is that in print_block the index is given by an argument instead of print_current_block always printing the current index from the state.
*/


void print_block(ui_state* state, int index) {
    clear_display(); // Clears the display before printing the block
    if (index >= 0 && index < state->blocks.n) { // Check if index is within bounds
        wrs(state->blocks.s[index]); // Write the content of the block to the output span
        flush(); // Flush the output to display the block
    }
}
/*
To support search mode, we have a static buffer of length 256 which can be used to search and which the user types into when in search mode.

In start_search(), to indicate that we are in search mode, we set the search on the ui state to be equal to a span which points at this static buffer.
And we set the span to contain the slash "/" that was typed.
We can do this using S(), but we prefer to just directly construct the span.
When we first go into search mode, we point the span at the start of the static buffer, we make it length 1 and the static buffer always starts with slash.

Then we enter our own loop where we call getch and handle basic line editing.
On any backspace character we will simply shorten the span (.end--) and on any other input at all we will extend it.
However, if we've deleted the initial slash, that means the user doesn't want to be in search mode any more.
Therefore, if the search span on the ui state has zero length, this means search mode is off.
So after every backspace, if the search span length goes to zero then we call print_current_blocks() to update the display and then return.

Every time the contents of the line changes, or when we first enter search mode, we will call another function, perform_search.
This will implement search and also displays the search results and indicates that we're in search mode to the user.
If we hit enter, we call another helper to finish the search successfully.

helper functions:

- perform_search(ui_state*)
- finalize_search(ui_state*)

OF COURSE, we use getch() which we carefully defined above, NEVER getchar().

We write declarations for the helper functions (which we define below).
*/

void perform_search(ui_state* state);
void finalize_search(ui_state* state);

void start_search(ui_state* state) {
    static char search_buffer[256] = {"/"}; // Static buffer for search, pre-initialized with "/"
    state->search = (span){.buf = search_buffer, .end = search_buffer + 1}; // Initialize search span to contain just "/"

    perform_search(state); // Perform initial search display/update

    char input;
    while ((input = getch()) != '\n') { // Continue until Enter is pressed
        if (input == '\b' || input == 127) { // Handle backspace (ASCII DEL on some systems)
            if (state->search.buf < state->search.end) {
                state->search.end--; // Shorten the span
                if (state->search.end == state->search.buf) {
                    // If we've deleted the initial "/", exit search mode
                    print_current_blocks(state);
                    return;
                }
            }
        } else if ((state->search.end - state->search.buf) < sizeof(search_buffer) - 1) {
            // Ensure there's space for more characters
            *state->search.end++ = input; // Extend the span
        }

        perform_search(state); // Update search results after each modification
    }

    finalize_search(state); // Finalize search on Enter
}

/*
In count_physical_lines(int,span) we are given a terminal width in cols and a logical line.
The line will not contain newlines, so we just divide the len of the line by the cols (and add one if there is a remainder).
Recall there is a len() in our spanio.
*/


int count_physical_lines(int cols, span line) {
    int length = len(line);
    return (length / cols) + (length % cols != 0); // Add one if there is a remainder
}
/*
In perform_search(), we get the state after the search string has been updated.

The search string (span state.search) will always start with a slash.
We remove this and take the rest of it as the actual string to search for.
We iterate through the blocks and use spanspan to find the first block that matches, along with the number of other blocks that match.
We store both the index of the first block that matched and a copy of the span given by spanspan for this first block only, as we will need both of them later.

This tells us where the first block was the matched, and how many total blocks matched, and also the location in the span that contains the match.

Once we have our search results, we call clear_display().

Also at the top, we declare a local variable of remaining lines from the top of the terminal window, since we want to print something at the bottom later.
So every time we print something, we'll decrement this value appropriately.

Next, we print "Block N:" on a line (adding one as usual).
Then we print the first four physical lines of the block.
We do this by calling a function print_physical_lines(ui_state*,span,int).
Here we write the forward declaration of this function (we define it later).

If nothing matched, then we do not print this part, but we still print the "N blocks matched" part later (0 in that case, of course) and the search string itself on the last line.

After the first four lines of the span, we print a blank line and then the line that contains the matched span.
Because we saved the spanspan value from the first matching block, we can use this to locate the lines in the block span where the match is.
Specifically, we can use next_line on a copy of the span until we find a line that has the .buf of the match returned by spanspan inside it.
(We don't have a library method for this, but you can just compare with buf and end. Recall that buf is inclusive and end exclusive.)
We can call wrs() and terpri() to print the line (as next_line does not include the newline, we must add it back).
We call a helper function, count_physical_lines(int, span) which takes the terminal_cols and a span and returns the number of physical lines that will be occupied by the span.

We print another blank line and then "N blocks matched".

Finally, we will add empty lines until we are at the bottom of the screen as indicated by terminal_rows on the state.
On the last line we will print the entire search string (including the slash).
(We can do this with wrs(), we don't need to add a newline as we are already on the last line of the window anyway.)
Before returning from the function we call flush() as we are responsible for updating the display.
*/


void perform_search(ui_state* state) {
    clear_display();
    int remaining_lines = state->terminal_rows; // Track remaining lines on the screen

    // Forward declaration
    void print_physical_lines(ui_state*, span, int);
    int count_physical_lines(int, span);

    span search_span = {state->search.buf + 1, state->search.end}; // Actual search string, excluding the leading slash
    int first_match_index = -1;
    span first_match_location = nullspan();
    int total_matches = 0;

    for (int i = 0; i < state->blocks.n; i++) {
        span match = spanspan(state->blocks.s[i], search_span);
        if (!empty(match)) {
            total_matches++;
            if (first_match_index == -1) {
                first_match_index = i;
                first_match_location = match;
            }
        }
    }

    if (first_match_index != -1) {
        // Print details of the first matched block
        prt("Block %d:\n", first_match_index + 1);
        remaining_lines -= 1; // Decrement for the header line

        print_physical_lines(state, state->blocks.s[first_match_index], 4);
        int lines_printed = 4; // Assuming print_physical_lines handles line counting accurately
        remaining_lines -= lines_printed;

        // Print the matching line
        span temp_block = state->blocks.s[first_match_index];
        while (!empty(temp_block) && temp_block.buf < first_match_location.buf) {
            span line = next_line(&temp_block);
            if (line.buf <= first_match_location.buf && line.end > first_match_location.buf) {
                prt("\n"); // Blank line before matched line
                remaining_lines -= 1;

                wrs(line); // Print matched line
                terpri(); // Add the newline back
                remaining_lines -= 1 + count_physical_lines(state->terminal_cols, line); // Account for physical lines taken by the matched line
                break;
            }
        }

        prt("\n"); // Another blank line after matched line
        remaining_lines -= 1;
    }

    // Print the "N blocks matched" message
    prt("%d blocks matched.\n", total_matches);
    remaining_lines -= 1;

    // Fill up to the bottom of the screen
    while (remaining_lines > 1) {
        terpri();
        remaining_lines--;
    }

    // Print the search string on the last line
    wrs(state->search);

    flush(); // Ensure display is updated
}

void print_top_lines(ui_state* state, line_mapping* lm, span block, int lines_to_print) {
    int lines_printed = 0;
    for (int i = 0; i < lm->total_logical_lines && lines_printed < lines_to_print; i++) {
        int line_length = (i < lm->total_logical_lines - 1) ? lm->line_offsets[i + 1] - lm->line_offsets[i] : block.end - (block.buf + lm->line_offsets[i]);
        span line_span = {block.buf + lm->line_offsets[i], block.buf + lm->line_offsets[i] + line_length};
        wrs(line_span); // Writes the span to the output
        lines_printed += lm->physical_lines[i];
    }
}

void print_single_block_with_skipping(ui_state* state, line_mapping* lm, int block_index) {
    prt("Block %d\n", block_index + 1); // Print block header

    int half_screen_height = (state->terminal_rows / 2) - 10; // Half the screen height, minus one for the header

    // Print the top lines
    print_top_lines(state, lm, state->blocks.s[block_index], half_screen_height);

    // Calculate total and skipped lines
    int total_lines = lm->total_physical_lines;
    int lines_to_skip = total_lines - (2 * half_screen_height);
    prt("[...skipping %d of %d lines...]\n", lines_to_skip, total_lines);

    // Print the last lines
    print_last_lines(state, lm, state->blocks.s[block_index], half_screen_height);
}

/*
In print_physical_lines, we get the ui state, a span, and a number of lines (in that order).
We can use next_line on the span (which is passed to us by value, so we can freely modify it without side effects to the caller) to get each logical line, and then we use the terminal_cols on the state to determine the number of physical lines that each one will require.
However, note that a blank line will still require one physical line (because we will print an empty line).
If the physical lines would be more than we need, then we only print enough characters (with wrapping) to fill the lines.
Otherwise we print the full line, followed by a newline, and then we decrement the number of lines that we still need appropriately.
*/


void print_physical_lines(ui_state* state, span block, int lines_to_print) {
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
/*
In finalize_search(ui_state*), we update the current_index to point to the first result of the search given in the search string.
We ignore the first character of state.search which is always slash, and find the first block which contains the rest of the search string (using contains()).
Then we set current_index to that block.
We then reset state.search to an empty span to indicate that we are not in search mode any more.
Finally we call print_current_blocks to refresh the display given the block that is now the current one (replacing the search screen).
*/


void finalize_search(ui_state* state) {
    span search_span = {state->search.buf + 1, state->search.end}; // Ignore the leading slash

    for (int i = 0; i < state->blocks.n; i++) {
        if (contains(state->blocks.s[i], search_span)) {
            state->current_index = i; // Update current_index to the first match
            break; // Exit the loop once the first match is found
        }
    }

    state->search = nullspan(); // Reset search to indicate exit from search mode
    print_current_blocks(state); // Refresh the display
}

/* Settings mode.

In settings mode, we display a list of settings, letting the user choose and set each one.

As settings are changed, we write a configuration file, which is always .cmpr/conf in the current working directory.

The contents of this file will be similar to RFC822-style headers.
We will have a key name followed by colon space and then a value to the end of the line.

We may as well support double-quoted strings at the outset, probably using JSON string conventions.

We will need functions to read and write this format, which we can do in the cmp space.

In the first function, parse_config, we read the contents of our config file into the cmp space, parse it, and set on the ui_state all the appropriate values.
We get the state pointer passed in, and a char* for config_filename, and return void.

We have a library method read_file_into_span.
To use this we will construct a span with the complement of cmp in cmp_space, representing the space that we can safely read into.
In other words, we take .buf of this span to be cmp.end, and the end to be cmp_space + BUF_SZ.
We'll read our configuration file into that space.

After we read the file into cmp, we get a span back from our library method containing the file contents.
We then can use next_line() in a loop to process them one by one.
First we look for ":", which we can do with spanspan, and if it is not found, we skip the line.
Spanspan returns the entire rest of the haystack input, from the start of the match to the end.
After the colon we'll skip any whitespace, and then consider of the line to be the value.
Next_line has already stripped the newline from the end so we don't need to do that.
Then we can compare the key part before the colon with our list of configuration values above.
If it matches any of them, then we set the corresponding member of the ui state, which always named the same as the config key name.
The values of the config keys will always be spans.
We have CONFIG_FIELDS defined above, we use that here with an X macro.
*/

void parse_config(ui_state* state, char* config_filename) {
    span cmp_free_space = {cmp.end, cmp_space + BUF_SZ};

    // Read the configuration file into cmp space
    span config_content = read_file_into_span(config_filename, cmp_free_space);

    // Process each line of the configuration
    while (!empty(config_content)) {
        span line = next_line(&config_content);
        span key_span = nullspan();
        span value_span = nullspan();

        // Find the position of the first colon
        /* *** manual fixup *** */
        //span colon = spanspan(line, S(":"));
        int colon = find_char(line,':');
        if (colon != -1) {
            key_span = (span){line.buf, line.buf + colon};
            // Skip colon and any whitespace following it
            value_span.buf = key_span.end + 1;
            //while (*value_span.buf == ' ' || *value_span.buf == '\t') value_span.buf++;
            while(isspace(*value_span.buf))value_span.buf++;
            value_span.end = line.end;
        }

        if (!empty(key_span)) {
            #define X(name) if (span_eq(key_span, S(#name))) state->name = value_span;
            CONFIG_FIELDS
            #undef X
        }
    }
}

void settings_mode(ui_state* state){}
/*
To edit the current block we first write it out to a file, which we do with a helper function write_to_file(span, char*).

Once the file is written, we then launch the user's editor of choice on that file, which is handled by another helper function.

That function will wait for the editor process to exit, and will indicate to us by its return value whether the editor exited normally.

If so, then we call another function which will then read the edited file contents back in and handle storing the new version.
*/

char* tmp_filename(ui_state* state);
int launch_editor(char* filename);
void handle_edited_file(char* filename, ui_state* state);

void edit_current_block(ui_state* state) {
  if (state->current_index >= 0 && state->current_index < state->blocks.n) {
    // Get a temporary filename based on the current UI state
    char* filename = tmp_filename(state);

    // Write the current block to the temporary file
    write_to_file(state->blocks.s[state->current_index], filename);

    // Launch the editor on the temporary file
    int editor_exit_code = launch_editor(filename);

    // Check if the editor exited normally
    if (editor_exit_code == 0) {
      // Handle the edited file (read changes, update the block, etc.)
      handle_edited_file(filename, state);
    } else {
      // Optionally handle abnormal editor exit
      printf("Editor exited abnormally. Changes might not be saved.\n");
    }

    // Cleanup: Free the filename if allocated dynamically
    free(filename);
  }
}

/*
To generate a tmp filename for launching the user's editor, we return a string starting with "cmpr/tmp/", assuming that the tool is being used from the home directory as intended, with cmpr/ as a subdirectory of ~.
We get the ui state as an argument, which we can use later (for example, we might store some metadata about the block or some version information to help recover from crashes), but we aren't using it yet.
For the filename part, we construct a timestamp in a compressed ISO 8601-like format, as YYYYMMDD-hhmmss with just a single dash as separator.
*/

char* tmp_filename(ui_state* state) {
  // Allocate memory for the filename
  // The path will have the format "cmpr/tmp/YYYYMMDD-hhmmss"
  // Assuming the maximum length for "cmpr/tmp/" prefix and a null terminator
  char* filename = malloc(sizeof(char) * (9 + 15 + 1)); // "cmpr/tmp/" + "YYYYMMDD-hhmmss" + '\0'
  if (!filename) {
    perror("Failed to allocate memory for filename");
    exit(EXIT_FAILURE);
  }

  // Get the current time
  time_t now = time(NULL);
  struct tm *tm_now = localtime(&now);
  if (!tm_now) {
    perror("Failed to get local time");
    free(filename);
    exit(EXIT_FAILURE);
  }

  // Format the timestamp into the allocated string
  if (strftime(filename, 9 + 15 + 1, "cmpr/tmp/%Y%m%d-%H%M%S", tm_now) == 0) {
    fprintf(stderr, "Failed to format the timestamp\n");
    free(filename);
    exit(EXIT_FAILURE);
  }

  return filename;
}

/*
In launch_editor, we are given a filename and must launch the user's editor of choice on that file, and then wait for it to exit and return its exit code.

We can look in the env for an EDITOR environment variable and use that if it is present, otherwise we will use "vim" "vi" "nano" or "ed" in that order.
*/

int launch_editor(char* filename) {
  char* editor = getenv("EDITOR");
  if (editor == NULL) {
    editor = "vim"; // Default to vim if EDITOR is not set
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    // Child process: replace with the editor process
    execlp(editor, editor, filename, (char *)NULL);

    // execlp only returns if there's an error
    perror("execlp");
    exit(EXIT_FAILURE);
  } else {
    // Parent process: wait for the editor to exit
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
      return WEXITSTATUS(status); // Return the exit status of the editor
    } else {
      return -1; // Indicate failure if the editor didn't exit normally
    }
  }
}

/*
Here in handle_edited_file we are given a file containing new contents of a block, and the ui state.
What we must do is replace the existing block contents with the new contents of that file, and then write everything out to a new file on disk called a rev.

We already have on ui_state all the current values of the blocks, which are spans that point into inp.
First we want to fix inp to reflect the new reality, and then we will write out the new disk file from inp (our global input span).

We get the span corresponding to the current block, which is also the edited block, in a local variable for convenience.

The contents of inp are already correct, up to the start of this block, which is what we want to replace.
Now we get the size of the file and compare it to length of the existing block.
If it is larger, we need to move the contents after it (in inp) to the right in memory, if smaller, to the left.
We can do this using memmove.

Once we have done this, we have everything correct in inp except for the contents of the file itself, and we have a space that's sufficient for the file contents to be read into.
We construct a span that represents this space.
This span starts at the .buf of the original block, but has the length of the file.

We have a library function, span read_file_into_span(char*,span), which takes a span and reads a file into a prefix of that span, and then returns that prefix as another span.
We can pass our "gap" span representing the part of inp that we want to replace to this function, along with the filename.
We can confirm that the span that is returned is in fact the identical span as the buffer we passed in, since we are expecting that the file length has not changed.

As usual, if this expectation is violated we will complain and exit (using our exit_with_error(char*) convenience function).

Now inp is representing the new state of the project.
We need to update the blocks, since any blocks after and including this one may have moved, so we call find_blocks and assign the result back to ui_state->blocks.
Then we call a helper function, new_rev, which takes the ui state and the filename.
This function will be responsible for storing a new rev if anything was changed, cleaning up the tmp file, and any reporting to the user that we might do.
We include a forward declaration here for new_rev.

Relevant helper functions:
void new_rev(ui_state*, char*);
*/

void new_rev(ui_state* state, char* filename);

void handle_edited_file(char* filename, ui_state* state) {
  int fd = open(filename, O_RDONLY);
  if (fd == -1) exit_with_error("Failed to open edited file");

  struct stat file_stat;
  if (fstat(fd, &file_stat) == -1) {
    close(fd);
    exit_with_error("Failed to get file size");
  }
  close(fd);

  span current_block = state->blocks.s[state->current_index];
  size_t new_size = file_stat.st_size;
  size_t old_size = current_block.end - current_block.buf;
  ssize_t size_difference = new_size - old_size;

  // Adjust memory if the new size is different from the old size
  if (size_difference != 0) {
    memmove(current_block.end + size_difference, current_block.end, inp.end - current_block.end);
    inp.end += size_difference;
  }

  // Prepare a span for the new content
  span gap = {current_block.buf, current_block.buf + new_size};

  // Read the new content into the span
  span result = read_file_into_span((char*)filename, gap);
  if (len(result) != new_size) {
    exit_with_error("Unexpected size difference after reading file");
  }

  // Now inp correctly represents the new state
  // Update the blocks in ui_state to reflect potential changes
  state->blocks = find_blocks(state,inp);

  // Create a new revision and perform clean-up
  new_rev(state, (char*)filename);
}




/*
Here we store a new revision, given the ui state and a filename which contains a block that was edited.
The file contents have already been processed, we just get the name so that we can clean up the file when done.

On ui_state we have a span revdir, which would be "cmpr/revs/" for cmpr development itself, but will typically differ between projects.
We assume this span is set correctly and that the directory exists.

First we construct a path starting with revdir and ending with an ISO 8601-style compact timestamp like 20240501-210759.
We then write the contents of inp (our global input span) into this file using write_to_file().
We create a symlink as cmpr/cmpr.c that points to this new rev.
There might be an existing symlink there, which we want to replace, but there also might be a file there which may have been edited and contain unsaved changes.
So we have a helper function, update_link, which takes the ui_state, and the filename, and handles all of this.
We forward declare this function below; all others we already have above.

Finally we unlink the filename that was passed in, since we have now fully processed it.
The filename is now optional, since we sometimes also are processing clipboard input, so if it is NULL we skip this step.

Reminder: we never write `const` in C as it only brings pain.
*/

void update_link(ui_state* state, char* new_filename);

void new_rev(ui_state* state, char* edited_filename) {
  char new_rev_path[256]; // Buffer to hold the new revision path
  time_t now = time(NULL);
  struct tm *tm_now = localtime(&now);
  if (tm_now == NULL) {
    perror("Failed to get local time");
    exit(EXIT_FAILURE);
  }

  // Constructing the path for the new revision
  if (strftime(new_rev_path, sizeof(new_rev_path), "cmpr/revs/%Y%m%d-%H%M%S", tm_now) == 0) {
    fprintf(stderr, "Failed to format timestamp for new revision\n");
    exit(EXIT_FAILURE);
  }

  // Write the contents of inp into the newly constructed revision file
  write_to_file(inp, new_rev_path);

  // Update the symlink "cmpr/cmpr.c" to point to this new revision file
  update_link(state, new_rev_path);

  // If a filename is provided (not NULL), unlink the temporary file used for editing
  if (edited_filename != NULL) {
    if (unlink(edited_filename) == -1) {
      perror("Failed to unlink the temporary edit file");
      exit(EXIT_FAILURE);
    }
  }
}





/*
We have a shell function that does what we want here, with a few changes:

SH_FN_START

update_symlink() {
  rm -r cmpr/cmpr.c; ln -s $(cd cmpr; find revs | sort | tail -n1; ) cmpr/cmpr.c
} # don't end block

SH_FN_END

- update_link(ui_state*, char*); is a C function not a shell function
- instead of the unconditional rm -r, we want to check and see if the existing cmpr/cmpr is a symlink; if not we complain and exit to avoid data loss
- instead of the find | sort | tail trick to get the latest revision, we are given it directly as an argument

So in fact all we do here is just check if the symlink exists, remove if it does and is only a symlink, and then recreate it.
Also, the symlink will be created in cmpr/ so it must be relative to cmpr/, meaning that we must strip the leading "cmpr/" from the filename that we are given.

Note that we never write "const" in C code, as this feature simply does not pull its weight.
*/

void update_link(ui_state* state, char* new_filename) {
  // Assuming new_filename format is "cmpr/revs/YYYYMMDD-hhmmss"
  char* symlink_path = "cmpr/cmpr.c";
  char relative_filename[256]; // To store the relative path

  // Strip the leading "cmpr/" from new_filename for the symlink
  if (sscanf(new_filename, "cmpr/%s", relative_filename) != 1) {
    exit_with_error("Failed to strip leading 'cmpr/' from filename");
  }

  // Check if cmpr/cmpr.c is a symlink
  struct stat statbuf;
  if (lstat(symlink_path, &statbuf) == 0) {
    if (S_ISLNK(statbuf.st_mode)) {
      // It's a symlink, safe to remove
      if (unlink(symlink_path) == -1) {
        exit_with_error("Failed to remove existing symlink");
      }
    } else {
      // The path exists but it's not a symlink; exiting to avoid data loss
      fprintf(stderr, "Error: '%s' exists and is not a symlink. Operation aborted to prevent data loss.\n", symlink_path);
      exit(EXIT_FAILURE);
    }
  } else if (errno != ENOENT) {
    // An error other than "not existing" occurred
    exit_with_error("Unexpected error checking for existing symlink");
  }

  // Create the new symlink
  if (symlink(relative_filename, symlink_path) == -1) {
    exit_with_error("Failed to create new symlink");
  }
}








/*
void rewrite_current_block_with_llm(ui_state *state);

Here we call a helper function with the block contents which returns the top part of the block, which is usually a comment, stripping the rest which is usually code.
It calls another function to turn this comment part into a prompt.

We then call another function which writes that span into a file and calls a user-provided command that places the contents of that file on the clipboard.

Helper functions:
span block_comment_part(span);
span comment_to_prompt(span);
void send_to_clipboard(span);
*/

span block_comment_part(span block);
span comment_to_prompt(span comment);
void send_to_clipboard(span prompt);

void rewrite_current_block_with_llm(ui_state *state) {
  if (state->current_index < 0 || state->current_index >= state->blocks.n) {
    fprintf(stderr, "Invalid block index.\n");
    return;
  }

  // Extract the comment part of the current block
  span current_block = state->blocks.s[state->current_index];
  span comment = block_comment_part(current_block);

  if (comment.buf == NULL || len(comment) == 0) {
    fprintf(stderr, "No comment found in the current block.\n");
    return;
  }

  // Convert the comment to a prompt
  span prompt = comment_to_prompt(comment);
  if (prompt.buf == NULL || len(prompt) == 0) {
    fprintf(stderr, "Failed to create a prompt from the comment.\n");
    fflush(stderr);
    exit(1);
    return;
  }

  // Send the prompt to the clipboard
  send_to_clipboard(prompt);
}

/*
To split out the block_comment_part of a span, we simply look for the first occurrence of star slash on a line by itself in the block.
We return the span up to this point and including the newline after.
*/

span block_comment_part(span block) {
  span result = { .buf = block.buf, .end = block.buf }; // Initialize result span to start of block

  for (unsigned char *p = block.buf; p < block.end - 2; ++p) {
    // Check if we've found the end_comment pattern
    if (*p == '*' && *(p + 1) == '/' && *(p + 2) == '\n') {
      result.end = p + 3; // Include the newline after "*/"
      break;
    }
  }
  return result;
}

/*
In comment_to_prompt, we use the cmp space to construct a prompt around the given block comment.

First we should create our return span and assign .buf to the cmp.end location.

Then we call prt2cmp.

Next we prt the literal string "```c\n".
Then we use wrs to write the span passed in as our argument.
Finally we write "```\n\n" and an instruction, which is usually "Write the function.".

Next we call prt2std to go back to the normal output mode.

Then we can get the new end of cmp and make that the end of our return span so that we return everything written into the cmp space.
*/

span comment_to_prompt(span comment) {
  // Prepare the return span to capture the start of new content in cmp
  span return_span;
  return_span.buf = cmp.end;

  // Switch to writing in the cmp space
  prt2cmp();

  // Start writing the prompt
  prt("```c\n"); // Begin code block
  wrs(comment);  // Write the comment span
  prt("```\n\nWrite the function. Reply only with code.\n"); // End code block and add instruction

  // Switch back to standard output space to avoid side-effects
  prt2std();

  // Capture the end of the new content written into cmp
  return_span.end = cmp.end;

  return return_span;
}


char *cbsend = "xclip -i -selection clipboard";

/*
In send_to_clipboard, we are given a span and we must send it to the clipboard using a user-provided method, since this varies quite a bit between environments.

We assume there is a global char* variable "cbsend" available.
An example of what this string might contain is "xclip -i -selection clipboard" or "pbcopy".
We run this as a command and pass the span data to its stdin.
*/

void send_to_clipboard(span content) {
  FILE* pipe = popen(cbsend, "w");
  if (pipe == NULL) {
    perror("Failed to open clipboard send command");
    exit(EXIT_FAILURE);
  }

  fwrite(content.buf, sizeof(char), len(content), pipe);

  if (pclose(pipe) == -1) {
    perror("Failed to close clipboard send command");
    exit(EXIT_FAILURE);
  }
}




/* #compile(ui_state*)

In compile(), we take the state and execute buildcmd, which is a config parameter.

First, we prt "Compiling" with a newline and flush so the user sees something before the compiler process, which may be slow to produce output.

Next we print the command that we are going to run.

Then we use system(3) on a 2048-char buf which we allocate and statically zero.
Our s() interface (s(char*,int,span)) lets us set buildcmd as a null-terminated string beginning at buf.

We wait for another keystroke before returning if the compiler process fails, so the user can read the compiler errors (later we'll handle them better).
(Remember to call flush() before getch() so the user sees the prompt (which is "Compile failed, press any key to continue...\n").)

On the other hand, if the build succeeds, we don't need the extra keystroke and go back to the main loop after a 1s delay so the user has time to read the success message before the main loop refreshes the current block.
In this case we prt "Compile succeeded" on a line.
We aren't using ui_state for anything yet here, but later we'll put something on it to provide more status info to the user.
*/

void compile(ui_state* state) {
  prt("Compiling...\n");
  flush();

  char buf[2048] = {0};
  s(buf,2048,state->buildcmd);

  int status = system(buf);

  if (status != 0) {
    prt("Compile failed, press any key to continue...\n");
    flush();
    getch(); // Wait for a keystroke before returning
  } else {
    prt("Compile succeeded\n");
    flush();
    sleep(1); // 1s delay for the user to read the message
  }
}


/*
In replace_code_clipboard, we pipe in the result of running the global variable "cbpaste".

This will contain a command like "xclip -o -selection clipboard" (our default) or "pbpaste" on Mac, etc. depending on platform or user settings.

We use the cmp buffer to store this data, starting from cmp.end (which is always somewhere before the end of the cmp space big buffer).
We set aside a reference to cmp.end before we do anything, so we can reset it at the end.

The space that we can use is the difference between cmp_space + BUF_SZ, which locates the end of the cmp_space, and cmp.end, which is always less than this limit.

We then create a span, which is pointing into cmp, capturing the new data we just captured.

We call another function, replace_block_code_part(state, span) which takes this new span and the current state.
It is responsible for all further processing, notifications to the user, etc.
Once this function returns we then reset cmp.end back to what it was.

(This invalidates the span we created, but that's fine since it's about to go out of scope and we're done with it.)
*/

char* cbpaste = "xclip -o -selection clipboard";

void replace_block_code_part(ui_state* state, span new_code);

void replace_code_clipboard(ui_state* state) {
  unsigned char* original_cmp_end = cmp.end; // Save the original cmp.end

  FILE* pipe = popen(cbpaste, "r");
  if (pipe == NULL) {
    perror("Failed to execute cbpaste command");
    exit(EXIT_FAILURE);
  }

  // Calculate the maximum space available in cmp buffer
  size_t max_space = (cmp_space + BUF_SZ) - cmp.end;
  size_t bytes_read = fread(cmp.end, 1, max_space, pipe);
  if (ferror(pipe)) {
    perror("Failed to read from cbpaste command");
    pclose(pipe);
    exit(EXIT_FAILURE);
  }

  pclose(pipe);

  // Create a span that captures the new data from clipboard
  span clipboard_data = {cmp.end, cmp.end + bytes_read};

  // Process the new data, replacing the current block's code part
  replace_block_code_part(state, clipboard_data);

  // Reset cmp.end back to what it was before we added the clipboard data
  cmp.end = original_cmp_end;
}

/*
Here we get the ui state and a span (into cmp space) which contains the new code part of the current block.

Note: to get the length of a span, use len().

Similar to handle_edited_file() above, we are given a span (instead of a file) and we must update the current block, moving data around in memory as necessary.

We put the original block's span in a local variable for convenience.

The end result of inp should contain:
- the contents of inp currently, up to the start (the .buf) of the original block.
- the comment part of the current block, which we can get from block_comment_part on the current block.
- two newlines
- the code part coming from the clipboard in our second argument
- current contents of inp from the .end of the original block to the inp.end of original input.

So, first we check the length of what the new block will be (comment part + two newlines + new part).
We then compare this to the old block length and do a memmove if necessary on the "rest" of inp, so that we have a gap to accommodate the new block's len.
Then we simply copy the newlines and the new code into inp.
(We do not need to copy the comment part, as it is already there in the original block.)

As before we then find the current locations of the blocks and update the state.

Once all this is done, we call new_rev.
We'll assume that function has been updated to allow NULL for the filename argument, since there's no filename here.
*/

void replace_block_code_part(ui_state* state, span new_code) {
  span original_block = state->blocks.s[state->current_index];
  span comment_part = block_comment_part(original_block);

  size_t new_block_length = len(comment_part) + 2 + len(new_code); // +2 for two newlines
  size_t old_block_length = original_block.end - original_block.buf;

  ssize_t length_difference = new_block_length - old_block_length;

  // Adjust memory for the rest of inp, if necessary
  if (length_difference != 0) {
    memmove(original_block.buf + new_block_length, original_block.end, inp.end - original_block.end);
    inp.end += length_difference;
  }

  // Current position in inp to start copying the new content
  unsigned char* current_pos = original_block.buf + len(comment_part);

  // Copy two newlines after the comment part
  *current_pos++ = '\n';
  *current_pos++ = '\n';

  // Copy the new code part into inp
  memcpy(current_pos, new_code.buf, len(new_code));

  // Update the blocks since the contents of inp have changed
  state->blocks = find_blocks(state,inp);

  // Store a new revision, now allowing NULL as filename argument
  new_rev(state, NULL);
}

