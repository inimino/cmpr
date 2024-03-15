/*

# CMPr

## Program in English!

Thesis: the future of programming is driving an LLM to write code.
Why?
Mostly, because it's way more fun.
You can work with any technology, so it's a good match for generalist skillsets.
When code is written by an AI, we treat it as disposable, which changes our relationship with the code in interesting ways.
It doesn't matter as much what language the code is in; what's important is the English that described that code.

## What's this then?

This is a early experimental tool to support the LLM programming workflow.

Today we just support ChatGPT, and interaction is by copy and paste.
This means you can use it with a free account, or if you have a paid account you can use GPT4, which writes better code.

(Coming soon: API usage, local models, competing LLMs, etc.)

This is a framework and represents a particular and very opinionated approach to this workflow.
We will be updating continuously as we learn.
This version was written in a week, using the workflow itself.

All code here is by ChatGPT4, and all comments by me, which is the idea of the workflow:

- You have a "block" which starts with a comment and ends with code.
- You write the comment; the LLM writes the code.
- You iterate on the comment until the code works and meets your standard.

The tool is a C program; compile it and run it locally.
The main editing loop shows you your "blocks".
You can use j/k to move from block to block.
Full text search with "/" is also supported (improvements coming soon!).
Once you find the block you want, use "e" to open it up in "$EDITOR" (or vim by default).
Then you do ":wq" or whatever makes your editor exit successfully, and a new revision of your code is automatically saved.

To get the LLM involved, when you're on a block you hit "r" and this puts a prompt into the clipboard for you.
(You won't see anything happen when you hit "r", but the clipboard has been updated.)
Then you switch over to your ChatGPT window and hit "Ctrl-V".
You could edit the prompt, but usually you'll just hit Enter.
ChatGPT writes the code, you can click "copy code" in the ChatGPT window, and then hit "R" (uppercase) back in cmpr to replace everything after the comment (i.e. the code half of the block) with the clipboard contents.
Mnemonic: "r" gets the LLM to "rewrite" (or "replace") the code to match the comment (and "R" is the opposite of "r").

Hit "q" to quit, "?" for short help, and "b" to build by running some build command that you specify.

## Quick start:

1. Get the code and build; assuming git repo at ~/cmpr and you have gcc, `gcc -o cmpr/cmpr cmpr/cmpr.c -lm`.
2. Put the executable in your path with e.g. `sudo install cmpr/cmpr /usr/local/bin`.
3. Go to directory you want to work in and run `cmpr --init`.
4. `export EDITOR=emacs` or whatever editor you use, or vi will be run by default.
5. Run `cmpr` in this directory, and it will ask you some configuration questions.
   If you want to change the answers later, you can edit the .cmpr/conf file.

## Caveats:

Developed on Linux; volunteers and bug reports on other environments gladly welcomed!
We are using "xclip" to send the prompts to the clipboard.
This really improves quality of life over manual copying and pasting of comments into a ChatGPT window.
The first time you use the 'r' or 'R' commands you will be prompted for the command to use to talk to the clipboard on your system.
For Mac you would use "pbcopy" and "pbpaste".

This tool is developed in one week; we are still light on features.
We only really support C and Python; mostly this is around syntax of where blocks start (in C we use block comments, and triple-quoted strings in Python).
It's not hard to extend the support to other languages, just ask for what you want in the discord and it may happen.
It's not hard to contribute, you don't need to know C well, but you do need to be able to read it (you can't trust the code from GPT without close examination).

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
- `s(char*,int,span)`: Creates a null-terminated string in a user-provided buffer of provided length from given span.
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

Sometimes we need a null-terminated C string for talking to library functions, we can use this pattern (with "2048" being replaced by some reasonably generous number appropriate to the specific use case): `char buf[2048] = {0}; s(buf,2048,state->buildcmd);`.

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

/* 
Below we have shell functions which we used for building in the first days of the project.
These can gradually be cleaned up as we implement the functionality directly or in other ways.
We export these functions into a separate file so that they can be sourced at the shell.
e.g. `. cmpr/functions.sh` brings the current functions into scope.
Then we use `build` to do a build, and so on.

SH_FN_START

F=cmpr/cmpr.c

prepare_commit() {
  # the .? on the line below is so this doesn't end a C block comment...
  awk 'BEGIN {block=0} /^\/\*.?/ {block++; if (block==2) exit} {print}' < cmpr/cmpr.c | tail -n +3 > cmpr/README.md
  cat cmpr/cmpr.c > cmpr/cmpr_orig.c
  rm cmpr/cmpr.c
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
  gcc -o cmpr/dist/cmpr -g cmpr/cmpr.c -lm
}

clipboard_copy() {
  xclip -i -selection clipboard
}

clipboard_paste() {
  xclip -o -selection clipboard
}

SH_FN_END

just stashed here for now:

Reply only with code. Do not simplify, but include working code. If necessary, you may define a helper function which we will implement later, but do not include comments leaving out key parts. Your code must be complete as written, but calling functions that do not yet exist is allowed.

*/

void send_to_clipboard(span prompt);

/* (Library code ends; our code begins.)

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
- the path to the config file as a span
- terminal_rows and _cols which stores the terminal dimensions

Additionally, we include a span for each of the config files, with an X macro inside the struct, using CONFIG_FIELDS defined above.
*/


typedef struct {
    spans blocks;          // Array of spans representing blocks of text/code
    int current_index;     // Current index into the blocks
    int marked_index;      // Marked index, representing the other end of a selected range
    span search;           // Search span, starts with "/" for search mode, empty otherwise
    span config_file_path;
    int terminal_rows;     // Terminal rows
    int terminal_cols;     // Terminal columns

    // Configuration spans
    #define X(name) span name;
    CONFIG_FIELDS
    #undef X
} ui_state;
/*
In main,

First we call init_spans, since all our i/o relies on it.

We declare a ui_state variable `state` which we'll pass around for ever after.

Just above main, we declare a global ui_state* called state, which will allow us to not pass around the ui_state singleton all over our program.
After declaring our ui_state variable in main, we will set this global pointer to it.
We set config_file_path on the state to the default configuration file path which is ".cmpr/conf", i.e. always relative to the CWD.

We call a function handle_args to handle argc and argv.
This function will also read our config file (if any).
We call check_conf_vars() once after this; we also call it in the main loop but we need it before trying to get the code.

We call span_arena_alloc(), and at the end we call span_arena_free() just for clarity even though it doesn't matter anyway since we're exiting the process.
We allocate a spans arena of 1 << 20 or a binary million spans.

Next we call a function get_code().
This function either handles reading standard input or if we are in "project directory" mode then it reads the files indicated by our config file.
In either case, once this returns, inp is populated and any other initial code indexing work is done.

Then we call main_loop().

The main loop reads input in a loop and probably won't return, but just in case, we always call flush() before we return so that our buffered output from prt and friends will be flushed to stdout.
*/

void handle_args(int argc, char **argv);
void get_code(ui_state* state);
void check_conf_vars();
void main_loop();

ui_state* state;

int main(int argc, char **argv) {
  init_spans(); // Initialize the global span buffers
  ui_state statevar = {0}; // Declare the ui_state variable
  state = &statevar;

  state->config_file_path = S(".cmpr/conf");

  handle_args(argc, argv);

  check_conf_vars();

  span_arena_alloc(1 << 20); // Allocate memory for a binary million spans

  get_code(state); // Populate inp and perform initial indexing

  main_loop(); // Enter the main interaction loop

  flush(); // Ensure all buffered output is flushed to stdout
  span_arena_free(); // Free the allocated spans arena

  return 0;
}

void print_config(ui_state* state) {
    prt("Current Configuration Settings:\n");

    #define X(name) prt(#name ": %.*s\n", (int)(state->name.end - state->name.buf), state->name.buf);
    CONFIG_FIELDS
    #undef X
    flush();
}

/*
We have exit_success and exit_with_error for convenience.

Because it is such a common pattern, we include a exit_success() function of no arguments, which calls flush and then exit(0).
*/

void exit_success() {
    flush();
    exit(0);
}

void print_config(ui_state*);
void parse_config();
void exit_with_error(char*);
void cmpr_init();
spans find_blocks(span);
void reset_stdin_to_terminal();
/*
In handle_args we handle any command-line arguments.

If "--conf <alternate-config-file>" is passed, we update config_file_path on the state.
Once we know the conf file to read from, we call parse_config before we do anything else.

If "--print-conf" is passed in, we print our configuration settings and exit.
This is only OK to write because we have already called parse_config, so the configuration settings have already been read in from the file.
In other words, this function will always call parse_config, always before printing the config if "--print-conf" is used, and always after updating the config file if "--conf" is used.
We do this by setting an indicator print_conf while parsing the flags, and then printing the conf at the end; this handles both the case where both flags are used together and the case where just --print-conf is used with the default conf file.

With the "--help" flag we just prt a short usage summary and exit_success().
We include argv[0] as usual and indicate that typical usage is by providing a project directory as the argument, and summarize all the flags that we support.

With "--init" we call a function, cmpr_init(), which performs some initialization of a new directory to be used with the tool.

With "--version" we print the version number.
(The version is always a natural number, and goes up when a release significantly increases usability.
Current version: 2)

void handle_args(int argc, char **argv);
*/

void handle_args(int argc, char **argv) {
    int print_conf = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--conf") == 0 && i + 1 < argc) {
            state->config_file_path = S(argv[++i]);
        } else if (strcmp(argv[i], "--print-conf") == 0) {
            print_conf = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            prt("Usage: %s [--conf <config-file>] [--print-conf] [--init] [--help] [--version]\n", argv[0]);
            prt("       --conf <config-file>   Use an alternate configuration file.\n");
            prt("       --print-conf           Print the current configuration settings.\n");
            prt("       --init                 Initialize a new directory for use with the tool.\n");
            prt("       --help                 Display this help message and exit.\n");
            prt("       --version              Print the version number and exit.\n");
            exit_success();
        } else if (strcmp(argv[i], "--init") == 0) {
            cmpr_init();
            exit_success();
        } else if (strcmp(argv[i], "--version") == 0) {
            prt("Version: 2\n");
            exit_success();
        }
    }

    parse_config(state);

    if (print_conf) {
        print_config(state);
        exit_success();
    }
}
/*
In clear_display() we clear the terminal by printing some escape codes (with prt and flush as usual).
*/


void clear_display() {
    prt("\033[2J\033[H"); // Escape codes to clear the screen and move the cursor to the top-left corner
    flush();
}

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
    state->blocks = find_blocks(inp);
}
/*
In find_blocks, we take a span, and we return a spans which contains blocks that fully cover the input.

A block is defined as follows:

There is a special pattern that defines the start of a block (this is configurable).
For a C file this might be slash star, for a Python file, triple-quote, and so on.
This pattern is always recognized at the start of a line, so a block always starts at the start of a line.
Then the block simply runs to the start of the next block if any, or the end of the file otherwise.
So blocks always end with a newline, unless it's the last block in a file and the file doesn't end with a newline.

In other words, the first block starts on the first line of the file unconditionally, so it actually doesn't matter if the pattern is there or not.
Therefore, before both of our loops, we can simply throw away the first line with next_line().

Starting after the first line, to find the rest of the blocks, we simply scan each line, and call a helper function, is_line_block_pattern_start.
This predicate function takes a span and returns 0 or 1.

We perform the loop twice, the first time we just count the number of blocks that are in the file, then we can allocate our spans of the correct size using spans_alloc(n_blocks), and then we loop the same way again but now populating the spans which we will return.
Before the first loop we copy our argument into a new spans so that we can consume it while counting, but then still have access to the original full span for the second loop.

Here we can use a common pattern which is that we declare a span that we are going to return or assign, and then we set .buf first (here when we find the starting line) and then set .end later (when we find the closing brace).

At the end of find_blocks, we ensure with a simple loop, that all the blocks returned together tile the file, and that none are empty.
This means:
The buf of the first block is the same as our input span.
The end of the last block is the same as that of our input.
The buf of each other block is equal to the end of the previous one.
If this sanity check fails, we complain and crash as usual.

*/




int python_hack = 0;

int is_line_block_pattern_start(span line);

spans find_blocks(span input) {
    span copy = input; // Copy of the input span to consume while counting
    is_line_block_pattern_start(next_line(&copy)); // Discard the first line

    int n_blocks = 1; // Start with 1 for the first block

    // First loop: count blocks
    while (!empty(copy)) {
        span line = next_line(&copy);
        if (is_line_block_pattern_start(line)) {
            n_blocks++;
        }
    }

    // Allocate spans for the blocks
    spans blocks = spans_alloc(n_blocks);

    // Reset copy for second loop
    copy = input;
    /* *** VERY manual fixup *** */
    python_hack = 0;
    is_line_block_pattern_start(next_line(&copy)); // Discard the first line again

    int block_index = 0;
    blocks.s[block_index].buf = input.buf; // First block starts at the beginning of the input

    // Second loop: populate blocks
    while (!empty(copy)) {
        span line = next_line(&copy);
        if (is_line_block_pattern_start(line) || empty(copy)) {
            blocks.s[block_index].end = line.buf; // End of the current block
            block_index++;
            if (!empty(copy)) { // Prepare the start of the next block
                blocks.s[block_index].buf = line.buf;
            }
        }
    }

    // Ensure the last block ends correctly
    if (n_blocks > 0) {
        blocks.s[n_blocks - 1].end = input.end;
    }

    return blocks;
}
/*
In is_line_block_pattern_start, we get a span which represents a line.

We return 1 if it starts a block and 0 otherwise.
Here we define the following functions:

- the C predicate, which returns 1 iff the first two chars are slash star.
- the Python version, which matches triple-quote (three double quotes) at the start of a line.

These helper functions take a span and return an int.

Finally we write the is_line_block_pattern_start, which takes ui_state* and span.
This blockdef on the state for equality with "Python", and dispatches to the Python version.
Otherwise it dispatches to the C version (which is therefore also our default).
*/

// *** manual fixup ***

int is_line_block_pattern_start_C(span line) {
  // Check if the line starts with "/*"
  if ((line.end - line.buf) >= 2 && strncmp(line.buf, "/*", 2) == 0) {
    return 1;
  }
  return 0;
}

int is_line_block_pattern_start_python(span line) {
  /* *** VERY manual fixup *** */
  int matches = span_eq(S("\"\"\""), first_n(line, 3));
  if (!matches) return 0;
  python_hack = python_hack ? 0 : 1;
  return python_hack;
}

int is_line_block_pattern_start(span line) {
  if(span_eq(state->blockdef,S("Python"))) {
    return is_line_block_pattern_start_python(line);
  } else {
    return is_line_block_pattern_start_C(line);
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
In main_loop, we get a ui_state pointer where all UI state is stored.

We initialize:

- the current index to 0,
- the marked index to -1 indicating that we are not in visual selection mode.

In our loop, we call check_conf_vars(), which just handles the case where some essential configuration variables aren't set.

Next we clear the terminal, then print the current block or blocks.
Then we will wait for a single keystroke of keyboard input using our getch() above.
Just before we call this function, we also call flush(), which just prevents us having to call it an a lot of other places all over the code.

We also define a helper function print_current_blocks; we include just the declaration for that function below.
(Reminder: we never write `const` in C.)

Once we have a keystroke, we will call another function, handle_keystroke, which takes the char that was entered.
*/

void check_conf_vars();
void print_current_blocks();
void handle_keystroke(char keystroke);

void main_loop() {
    state->current_index = 0;
    state->marked_index = -1;

    while (1) {
        check_conf_vars(); // Ensure essential configuration variables are set

        clear_display(); // Clear the terminal screen
        print_current_blocks(); // Print the current block or blocks
        flush(); // Flush the output before waiting for input

        char input = getch(); // Wait for a single keystroke
        handle_keystroke(input); // Handle the input keystroke
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
    print_current_blocks();
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

void print_current_blocks() {
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

We include a forward declaration for our helper functions.
*/

//* *** manual fixup ***/

void print_multiple_partial_blocks(ui_state*,int,int);
void print_single_block_with_skipping(ui_state*,int);
void render_block_range(ui_state* state, int start, int end) {

    if (end - start == 1) {
      print_single_block_with_skipping(state, start);
    } else {
      print_multiple_partial_blocks(state, start, end);
    }
}

/*
In print_multiple_partial_blocks, we get a state and we should print as much as we can of the blocks.
Currently, we just print the number of blocks that there are.
*/

// Placeholder for print_multiple_partial_blocks, assuming it's defined elsewhere
void print_multiple_partial_blocks(ui_state* state, int start_block, int end_block) {
  prt("%d blocks (printing multiple blocks coming soon!)\n", end_block - start_block);
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
- g/G Go to the first or last block resp.
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
- edit_current_block()
- rewrite_current_block_with_llm()
- compile()
- replace_code_clipboard()
- toggle_visual()
- start_search()
- settings_mode()

To get the help text we can basically copy the lines above, except formatted nicely for terminal output.
We call terpri() on the first line of this function (just to separate output from any handler function from the ruler line).
*/

void edit_current_block(ui_state*);
void rewrite_current_block_with_llm();
void compile();
void replace_code_clipboard(ui_state*);
void toggle_visual(ui_state*);
void start_search(ui_state*);
void settings_mode(ui_state*);

void handle_keystroke(char input) {
  terpri(); // Separate output from any handler function from the ruler line

  switch (input) {
    case 'j':
      if (state->current_index < state->blocks.n - 1) state->current_index++;
      break;
    case 'k':
      if (state->current_index > 0) state->current_index--;
      break;
    case 'g':
      state->current_index = 0;
      break;
    case 'G':
      state->current_index = state->blocks.n - 1;
      break;
    case 'e':
      edit_current_block(state);
      break;
    case 'r':
      rewrite_current_block_with_llm();
      break;
    case 'R':
      replace_code_clipboard(state);
      break;
    case 'b':
      compile();
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
      prt("Keyboard shortcuts:\n");
      prt("- j/k: Navigate blocks\n");
      prt("- g/G: Go to the first/last block\n");
      prt("- e: Edit the current block\n");
      prt("- r: Rewrite the current block with LLM\n");
      prt("- R: Replace code part of the block with clipboard contents\n");
      prt("- b: Build the project\n");
      prt("- v: Toggle visual mode\n");
      prt("- /: Search mode\n");
      prt("- S: Settings mode\n");
      prt("- ?: Display this help\n");
      prt("- q: Quit the application\n");
      break;
    case 'q':
      prt("goodbye\n");
      flush();
      exit(0);
      break;
  }
}
/*
In print_block, similar to our earlier print_current_block, we clear the display and print the block, the only difference is that in print_block the index is given by an argument instead of print_current_block always printing the current index from the state.


void print_block(ui_state* state, int index) {
    clear_display(); // Clears the display before printing the block
    if (index >= 0 && index < state->blocks.n) { // Check if index is within bounds
        wrs(state->blocks.s[index]); // Write the content of the block to the output span
        flush(); // Flush the output to display the block
    }
}
*/
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
                    print_current_blocks();
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
/*
In next_line_limit(span* s, int n) we are given a span and a length limit.
We return a span which is a prefix of s.
If the first n characters do not contain newline, and the n+1th character is also not a newline, then we return a span, not ending in newline, of n characters.
If the first n characters do not contain newline but n+1 does, we include n+1 characters, including the newline.
Otherwise, there is a newline within the first n characters, we return n or fewer characters with the last one being the newline.
We also set s->buf to contain the rest of the span not in the returned prefix.
*/


span next_line_limit(span* s, int n) {
    span result = {s->buf, s->buf}; // Initialize result span to start of input span
    if (s->buf == NULL || n <= 0) return result; // Check for null pointer or non-positive length

    int i;
    for (i = 0; i < n && (s->buf + i) < s->end; i++) {
        if (s->buf[i] == '\n') {
            result.end = s->buf + i + 1; // Include the newline in the result
            break;
        }
    }

    if (i == n) { // Reached the length limit without encountering a newline
        if ((s->buf + n) < s->end && s->buf[n] == '\n') {
            result.end = s->buf + n + 1; // Include the next character if it is a newline
        } else {
            result.end = s->buf + n; // Just include up to the limit
        }
    }

    s->buf = result.end; // Advance the start of the input span past the returned prefix
    return result;
}
/*
In print_ruler we use prt to show the number of blocks and the currently selected block, on a line without a newline.
*/

void print_ruler(ui_state* state) {
    prt("Total Blocks: %d, Current Block: %d", state->blocks.n, state->current_index + 1);
    // No newline as specified
}
/*
In print_single_block_with_skipping we get ui state and a block index.
For now, we first calculate whether the block fits on the screen.
First, we set a variable remaining_rows which we initialize with state->terminal_rows and decrement as we print lines of output.
First we print a line "Block N" and decrement the remaining rows by one.
Invariant: remaining_rows always is the number of untouched rows of remaining terminal area.
Then, in a loop we call next_line_limit(span*,int) with a copy of the block and state->terminal_cols until remaining rows is one, or the copy of the block is empty.
Next_line_limit() returns a prefix span representing the next physical line.
It is either exactly terminal_cols chars, not containing a newline, or up to terminal_cols + 1 chars, ending with a newline.
The only exception may be the last block in a file, which may not end in a newline if the file does not end in one.
We always print the line, using wrs().
We test if the line is shorter than terminal cols. WE PUT THIS RESULT IN A VARIABLE "is_short".
If the line is a short, then we test if the last char of the line is a newline (i.e. len() > 0 and end[-1] == '\n').
We put this result in a variable `contains_newline`.
If is_short is true and contains_newline is false, we call terpri().
Now that we have printed the line, we decrement remaining_rows, and if it is 1, we stop and call print_ruler().
If we run out of block before we run out of remaining rows, then we add blank lines up to the end of the screen anyway, and still print our ruler line in the same place.
*/

void print_single_block_with_skipping(ui_state* state, int block_index) {
    if (block_index < 0 || block_index >= state->blocks.n) return; // Validate block index

    int remaining_rows = state->terminal_rows - 1;
    prt("Block %d\n", block_index + 1);
    remaining_rows--;

    span block = state->blocks.s[block_index];
    span block_copy = block; // Copy of the block span to be consumed by next_line_limit

    while (remaining_rows > 0 && !empty(block_copy)) {
        span line = next_line_limit(&block_copy, state->terminal_cols);
        int is_short = len(line) < state->terminal_cols;
        int contains_newline = len(line) > 0 && line.end[-1] == '\n';

        wrs(line);
        if (is_short && !contains_newline) {
            terpri(); // Print newline if the line is short and doesn't end with a newline
        }
        remaining_rows--;

        if (remaining_rows == 0) {
            print_ruler(state);
            break;
        }
    }

    // If we have remaining rows after printing the block, fill them with blank lines
    while (remaining_rows > 0) {
        terpri();
        remaining_rows--;
        if (remaining_rows == 0) {
            print_ruler(state);
        }
    }
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
    print_current_blocks(); // Refresh the display
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
First we look for ":" with find_char, and if it is not found, we skip the line.
After the colon we'll skip any whitespace with an isspace() loop and then consider the rest of the line to be the value.
(Note that we don't trim whitespace off the end, something maybe worth documenting elsewhere for config file users).
Next_line has already stripped the newline from the end so we don't need to do that.

Next, we compare the key part before the colon with our list of configuration values above, and if it matches any of them, we set the corresponding member of the ui state, which is always named the same as the config key name.
The values of the config keys will always be spans.
We have CONFIG_FIELDS defined above, we use that here with an X macro.
*/

void parse_config() {
    span cmp_free_space = {cmp.end, cmp_space + BUF_SZ};
    //prt("conf: %s\n", config_filename);flush();

    // Read the configuration file into cmp space
    span config_content = read_file_S_into_span(state->config_file_path, cmp_free_space);

    /* *** manual fixup *** */
    cmp.end = config_content.end; // avoid later uses of cmp space clobbering our conf vars

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
            while (*value_span.buf == ' ' || *value_span.buf == '\t') value_span.buf++;
            //while(isspace(*value_span.buf))value_span.buf++;
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
In read_line, we get a span pointer to some space that we can use to store input from the user.

We first print our prompt, which is "> ".

Recall that we use prt() as always.

We handle some basic line editing using our getch() in a loop, until enter is hit.
At that point we return a span containing the user's input.
The span which was passed in will have been shortened such that .end of our return value is now the .buf of the passed-in span.
(In this way, the caller can perform other necessary adjustments, or use the remaining buffer area in a loop etc.)

The line editing we support:

- all kinds of backspaces shorten the span by one and redraw the line (using ANSI escapes and \r).
- enter returns.
- everything else just gets appended to our span, which it extends.
*/



#include <stdio.h>

span read_line(span* buffer) {
    prt("> ");
    flush();

    span input = {buffer->buf, buffer->buf}; // Initialize input span to start at buffer start

    while (1) {
        char ch = getch(); // Read a single character from input

        if (ch == '\n') {
            prt("\n"); // Newline for visual separation of input
            break; // Return on enter
        } else if ((ch == '\b' || ch == 127) && input.buf < input.end) {
            // Handle backspace (ASCII BACKSPACE and DEL)
            input.end--; // Shorten the span by one
            prt("\033[2K\r> %.*s", (int)(input.end - input.buf), input.buf); // Redraw the line
        } else {
            *input.end++ = ch; // Append character to span and extend it
            prt("%c", ch); // Echo the character
        }
        flush(); // Ensure output is updated immediately
    }

    buffer->buf = input.end; // Update the original buffer to point to the end of the input
    return input; // Return the span containing the user's input
}
/*
In save_conf(), we simply rewrite the conf file to reflect any settings that may have been changed.

First we will write into the cmp space the current configuration.
Next, we will write that into the file named by state.config_file_path.
Finally we can shorten the cmp space back to what it was.

First we store a span that has buf pointing to the current cmp.end.
Then we call prt2cmp() and use prt to print a line for each config var as described below.
Then we call prt2std().
Next we set the .end of that span to be the current cmp.end.

Then we call write_to_file_span with the span and the configuration file name.
Finally we shorten cmp back to what it was, since the contents have been written out we no longer need them around.

To print a config var, we print the name, a colon and single space, and then the value itself followed by newline.
(We currently assume that none of our conf vars contain newlines (a safe assumption, as if they did we'd also have no way to read them in).)
*/


void save_conf() {
    span start = {cmp.end, cmp.end};
    prt2cmp();

    #define X(name) prt(#name ": %.*s\n", len(state->name), state->name.buf);
    CONFIG_FIELDS
    #undef X

    prt2std();
    start.end = cmp.end;

    write_to_file_span(start, state->config_file_path, 1);
    cmp.end = start.buf;
}
/*
In check_conf_vars() we test the state for all essential configuration variables and if any is missing we prompt the user to set that.

The essential configuration variables, along with the reason why each is required, are:

blockdef: must be either "Python" or "C", determines how blocks start and also where the comment part ends and code part starts. For Python, both are triple-quote, for C the block comment syntax is used.
revdir: we save a revision every time you edit a block, this is where the revisions will be stored (.cmpr/revs is a good choice).
tmpdir: we store a copy of a block for you to edit in your editor with the "e" key, this is where those live, can be .cmpr/tmp or a regular tmpdir like /tmp.
projfile: this is the main file that you are editing, all your blocks will live in this file (until we have directory-mode support coming soon).

TODO: probably all the above "config-related metadata" should be in one place in a table for all the conf vars; we'll do that later.
For now, a local macro cleans up this code quite a bit.

For each missing variable in the order given, we print a suitable message explaining what it is and why it is required (inspired by the above).
We try to include all the relevant information from the above about what the conf var is and why it's required, but fitting the explanation in a sentence or two at most. 

We then call read_line to get the new value from the user.
For the buffer space to use we will first call cmp_compl() to get the complement of cmp space as a span.
After read_line returns we will always set cmp.end to point to the end of the returned span; this makes sure nothing else uses that space later.

Once we have set all the required conf vars, if any of them were missing, the we call save_conf() which just rewrites the conf file.
*/

 /*
void check_conf_vars() {
    int need_save = 0;
    span buffer = cmp_compl();

    if (empty(state->blockdef)) {
        prt("blockdef is missing: must be either \"Python\" or \"C\".\n");
        state->blockdef = read_line(&buffer);
        need_save = 1;
    }
    if (empty(state->revdir)) {
        prt("revdir is missing: where revisions are stored (.cmpr/revs is recommended).\n");
        state->revdir = read_line(&buffer);
        need_save = 1;
    }
    if (empty(state->tmpdir)) {
        prt("tmpdir is missing: where temporary edit files are stored (.cmpr/tmp or /tmp are recommended).\n");
        state->tmpdir = read_line(&buffer);
        need_save = 1;
    }
    if (empty(state->projfile)) {
        prt("projfile is missing: the main file you are editing.\n");
        state->projfile = read_line(&buffer);
        need_save = 1;
    }

    cmp.end = buffer.buf; // Update cmp.end to the end of the last read_line call

    if (need_save) {
        save_conf(state);
    }
}
*/

void check_conf_vars() {
    int missing_vars = 0;

    #define CHECK_AND_SET(var, message) \
        if (empty(state->var)) { \
            prt(message "\n"); \
            span buffer = cmp_compl(); \
            assert(buffer.buf == cmp.end); \
            span input = read_line(&buffer); \
            state->var = input; \
            cmp.end = input.end; \
            missing_vars = 1; \
        }

    CHECK_AND_SET(blockdef, "blockdef is required to determine block start/end. Set to \"Python\" or \"C\".");
    CHECK_AND_SET(revdir, "revdir is where revisions are stored. Suggest setting to \".cmpr/revs\".");
    CHECK_AND_SET(tmpdir, "tmpdir is where temporary block files are stored. Can be \".cmpr/tmp\" or system tmp like \"/tmp\".");
    CHECK_AND_SET(projfile, "projfile is the main file you are editing. This is required for editing.");

    #undef CHECK_AND_SET

    if (missing_vars) {
        save_conf(state);
    }
}

/*
In ensure_conf_var() we are given a span, which must be one of the conf vars on the state, a message for the user to explain what the conf setting does and why it is required, and a default or current value that we can pass through to read_line.

If the conf var is not empty, we return immediately.

We call read_line to get the new value from the user.
For the buffer space to use we will first call cmp_compl() to get the complement of cmp space as a span.
After read_line returns we will always set cmp.end to point to the end of the returned span; this makes sure nothing else uses that space later.

Then we call save_conf() which just rewrites the conf file.
*/

void ensure_conf_var(span* var, span message, span default_value) {
    if (!empty(*var)) return; // If the configuration variable is already set, return immediately

    prt("%.*s\n", len(message), message.buf); // Print the message explaining the configuration setting
    if (!empty(default_value)) {
        prt("Default: %.*s\n", len(default_value), default_value.buf); // Show default value if provided
    }

    span buffer = cmp_compl(); // Get complement of cmp space as a span for input
    *var = read_line(&buffer); // Read new value from user

    cmp.end = buffer.buf; // Update cmp.end to the end of the returned span from read_line

    save_conf(); // Rewrite the configuration file with the updated setting
}
/*
To edit the current block we first write it out to a file, which we do with a helper function write_to_file(span, char*).

Once the file is written, we then launch the user's editor of choice on that file, which is handled by another helper function.

That function will wait for the editor process to exit, and will indicate to us by its return value whether the editor exited normally.

If so, then we call another function which will then read the edited file contents back in and handle storing the new version.

If not, we print a short message to let the user know their changes were ignored.
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
      prt("Editor exited abnormally. Changes might not be saved.\n");
    }

    // Cleanup: Free the filename if allocated dynamically
    free(filename);
  }
}

/*
To generate a tmp filename for launching the user's editor, we return a string starting with state->tmpdir.
For the filename part, we construct a timestamp in a compressed ISO 8601-like format, as YYYYMMDD-hhmmss with just a single dash as separator.
We return a char* which the caller must free.
Since state->tmpdir may or may not include a trailing slash we test for and handle both cases.
*/

char* tmp_filename(ui_state* state) {
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char timestamp[16]; // YYYYMMDD-hhmmss
    snprintf(timestamp, sizeof(timestamp), "%04d%02d%02d-%02d%02d%02d",
             tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
             tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);

    int tmpdir_len = state->tmpdir.end - state->tmpdir.buf;
    int need_slash = state->tmpdir.buf[tmpdir_len - 1] != '/';
    int total_len = tmpdir_len + need_slash + sizeof(timestamp); // +1 for potential slash, sizeof includes null terminator

    char* filename = (char*)malloc(total_len);
    if (!filename) {
        perror("Failed to allocate memory for filename");
        exit(EXIT_FAILURE);
    }

    if (need_slash) {
        snprintf(filename, total_len, "%.*s/%s", tmpdir_len, state->tmpdir.buf, timestamp);
    } else {
        snprintf(filename, total_len, "%.*s%s", tmpdir_len, state->tmpdir.buf, timestamp);
    }

    return filename;
}

/*
In launch_editor, we are given a filename and must launch the user's editor of choice on that file, and then wait for it to exit and return its exit code.

We look in the env for an EDITOR environment variable and use that if it is present, otherwise we will use "vi".

As always, we never write const anywhere in C.
*/

int launch_editor(char* filename) {
    char* editor = getenv("EDITOR");
    if (editor == NULL) {
        editor = "vi"; // Default to vi if EDITOR is not set
    }

    pid_t pid = fork();
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
  state->blocks = find_blocks(inp);

  // Create a new revision and perform clean-up
  new_rev(state, (char*)filename);
}




void update_link(char* new_filename);

/*
Here we store a new revision, given the ui state and a filename which contains a block that was edited.
The file contents have already been processed, we just get the name so that we can clean up the file when done.

On ui_state we have a span revdir, which would be "cmpr/revs/" for cmpr development itself, but will typically differ between projects.
We assume this span is set correctly and that the directory exists.
However, we don't assume that the conf var ends in a slash, so we test for that and handle both cases.

First we construct a path starting with revdir and ending with an ISO 8601-style compact timestamp like 20240501-210759.
We then write the contents of inp (our global input span) into this file using write_to_file().
We create a symlink as cmpr/cmpr.c that points to this new rev.
There might be an existing symlink there, which we want to replace, but there also might be a file there which may have been edited and contain unsaved changes.
So we have a helper function, update_link, which takes the filename, and handles all of this.

Finally we unlink the filename that was passed in, since we have now fully processed it.
The filename is now optional, since we sometimes also are processing clipboard input, so if it is NULL we skip this step.

Reminder: we never write `const` in C as it only brings pain.
*/

void new_rev(ui_state* state, char* edited_filename) {
    char new_rev_path[1024]; // Buffer for the new revision path
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);

    // Check if revdir ends with a slash and construct the new revision path accordingly
    if (state->revdir.end[-1] == '/') {
        snprintf(new_rev_path, sizeof(new_rev_path), "%.*s%04d%02d%02d-%02d%02d%02d", 
                 (int)(state->revdir.end - state->revdir.buf), state->revdir.buf,
                 tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
                 tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    } else {
        snprintf(new_rev_path, sizeof(new_rev_path), "%.*s/%04d%02d%02d-%02d%02d%02d", 
                 (int)(state->revdir.end - state->revdir.buf), state->revdir.buf,
                 tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
                 tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    }

    write_to_file(inp, new_rev_path);
    update_link(new_rev_path);

    if (edited_filename != NULL) {
        unlink(edited_filename);
    }
}

/*
SH_FN_START

update_symlink() {
  rm -r cmpr/cmpr.c; ln -s $(cd cmpr; find revs | sort | tail -n1; ) cmpr/cmpr.c
}

SH_FN_END
*/

/*
In update_link, we get a filename which contains a new version of the project file.

We have in state->projfile the path to a main project file, which will typically be a symlink into the state->revdir directory of revisions.
So in fact all we do here is just check if the symlink exists, remove if it does and is only a symlink, and then recreate it.
Otherwise we complain and exit to avoid data loss.

Note that we never write "const" in C code, as this feature simply does not pull its weight.
Since we will need to use some library functions here we use the s pattern with a local buffer to construct a null-terminated string first.
*/

void update_link(char* new_filename) {
    char projfile_path[1024];
    snprintf(projfile_path, sizeof(projfile_path), "%.*s", (int)(state->projfile.end - state->projfile.buf), state->projfile.buf);

    struct stat sb;
    if (lstat(projfile_path, &sb) == 0) {
        if (S_ISLNK(sb.st_mode)) {
            if (unlink(projfile_path) != 0) {
                perror("Failed to remove existing symlink");
                exit(EXIT_FAILURE);
            }
        } else {
            fprintf(stderr, "Existing file is not a symlink, operation aborted to prevent data loss.\n");
            exit(EXIT_FAILURE);
        }
    }

    if (symlink(new_filename, projfile_path) != 0) {
        perror("Failed to create new symlink");
        exit(EXIT_FAILURE);
    }
}

/* #rewrite_current_block_with_llm

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

void rewrite_current_block_with_llm() {
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
To split out the block_comment_part of a span, we first write two helper functions, one for C and one for Python.

Then we dispatch based on state->blockdef.
If this span contains "Python" we call the Python version, and otherwise we call the C version (which therefore is our default).

The helper function takes a span and returns an index offset to the location of the "block comment part terminator".
For Python this is the triple-quote, and for C it is star slash.

In the main function block_comment_part, we return the span up to and including the comment part terminator, and also including any newlines and whitespace after it.
*/

int find_block_comment_end_c(span block) {
    for (int i = 0; i < len(block) - 1; ++i) {
        if (block.buf[i] == '*' && block.buf[i + 1] == '/') {
            return i + 2; // Include the "*/" in the index
        }
    }
    return len(block);
}

int find_block_comment_end_python(span block) {
  /* *** manual fixup *** */
    int first = 0;
    for (int i = 0; i < len(block) - 2; ++i) {
        if (first && (block.buf[i] == '"' && block.buf[i + 1] == '"' && block.buf[i + 2] == '"')) {
            return i + 3; // Include the """ in the index
        }
        if (block.buf[i] == '"' && block.buf[i + 1] == '"' && block.buf[i + 2] == '"') first = 1;
    }
    return len(block);
}

span block_comment_part(span block) {
    int index;
    if (span_eq(state->blockdef, S("Python"))) {
        index = find_block_comment_end_python(block);
    } else {
        index = find_block_comment_end_c(block);
    }

    // Find the end of whitespace following the comment terminator
    while (index < len(block) && isspace(block.buf[index])) {
        ++index;
    }

    return (span){block.buf, block.buf + index};
}
/*
In comment_to_prompt, we use the cmp space to construct a prompt around the given block comment.

First we should create our return span and assign .buf to the cmp.end location.

Then we call prt2cmp.

Next we prt the literal string "```c\n".
Then we use wrs to write the span passed in as our argument.
Finally we write "```\n\n" and an instruction, which is usually "Write the function. Reply only with code. Do not include comments.".

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
  /* *** manual fixup *** */
  if (span_eq(state->blockdef, S("Python"))) {
    prt("```python\n");
    wrs(comment);
    prt("```\n\nWrite the function. Reply only with code. Avoid using the code_interpreter.\n");
  } else {
    prt("```c\n"); // Begin code block
    wrs(comment);  // Write the comment span
    prt("```\n\nWrite the function. Reply only with code. Do not include comments.\n"); // End code block and add instruction
  }

  // Switch back to standard output space to avoid side-effects
  prt2std();

  // Capture the end of the new content written into cmp
  return_span.end = cmp.end;

  return return_span;
}
/*
In send_to_clipboard, we are given a span and we must send it to the clipboard using a user-provided method, since this varies quite a bit between environments.

There is a global ui_state* variable "state" with a span cbcopy on it.

Before we do anything else we ensure this is set by calling ensure_conf_var with the message "The command to pipe data to the clipboard on your system. For Mac try \"pbcopy\", Linux \"xclip -i -selection clipboard\", Windows please let me know and I'll add something here".

We run this as a command and pass the span data to its stdin.
We use the `s()` library method and heap buffer pattern to get a null-terminated string for popen from our span conf var.

We complain and exit if anything goes wrong as per usual.
*/

void send_to_clipboard(span content) {
    ensure_conf_var(&(state->cbcopy), S("The command to pipe data to the clipboard on your system. For Mac try \"pbcopy\", Linux \"xclip -i -selection clipboard\", Windows please let me know and I'll add something here"), S(""));

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
/* #compile()

In compile(), we take the state and execute buildcmd, which is a config parameter.

First we call ensure_conf_var(state->buildcmd), since we are about to use that setting.

Next we print the command that we are going to run and flush, so the user sees something before the compiler process, which may be slow to produce output.

Then we use system(3) on a 2048-char buf which we allocate and statically zero.
Our s() interface (s(char*,int,span)) lets us set buildcmd as a null-terminated string beginning at buf.

We wait for another keystroke before returning if the compiler process fails, so the user can read the compiler errors (later we'll handle them better).
(Remember to call flush() before getch() so the user sees the prompt (which is "Compile failed, press any key to continue...").)

On the other hand, if the build succeeds, we don't need the extra keystroke and go back to the main loop after a 1s delay so the user has time to read the success message before the main loop refreshes the current block.
In this case we prt "Compile succeeded" on a line.

(We aren't doing this yet, but later we'll put something on the state to provide more status info to the user.)
*/

void compile() {
    ensure_conf_var(&state->buildcmd, S("The build command will be run every time you hit 'b' and should build the code you are editing (typically in projfile)"), nullspan());
    
    char buf[2048] = {0};
    s(buf, sizeof(buf), state->buildcmd);
    
    prt("Running command: %s\n", buf);
    flush();
    
    int status = system(buf);
    
    if (status != 0) {
        prt("Compile failed, press any key to continue...\n");
        flush();
        getch();
    } else {
        prt("Compile succeeded\n");
        flush();
        sleep(1); // Give time for the user to read the message
    }
}
/*
In replace_code_clipboard, we pipe in the result of running state->cbpaste.

First we call ensure_conf_var with the message "Command to get text from the clipboard on your platform (Mac: pbpaste, Linux: try xclip -o -selection clipboard, Windows: ???)".

This will contain a command like "xclip -o -selection clipboard" (our default) or "pbpaste" on Mac, and comes from our conf file.

We use the cmp buffer to store this data, starting from cmp.end (which is always somewhere before the end of the cmp space big buffer).

The space that we can use is the difference between cmp_space + BUF_SZ, which locates the end of the cmp_space, and cmp.end, which is always less than this limit.

We then create a span, which is pointing into cmp, capturing the new data we just captured.

We call another function, replace_block_code_part(span) which takes this new span and the current state.
It is responsible for all further processing, notifications to the user, etc.

Note: when this function returns, cmp.end is unchanged.
This invalidates the span we created, but that's fine since it's about to go out of scope and we're done with it.
*/

void replace_block_code_part(span new_code);

void replace_code_clipboard(ui_state* state) {
    ensure_conf_var(&(state->cbpaste), S("Command to get text from the clipboard on your platform (Mac: pbpaste, Linux: try xclip -o -selection clipboard, Windows: \?\?\?)"), S(""));
    char command[2048];
    snprintf(command, sizeof(command), "%.*s", (int)(state->cbpaste.end - state->cbpaste.buf), state->cbpaste.buf);

    FILE* pipe = popen(command, "r");
    if (!pipe) {
        perror("Failed to open clipboard paste command");
        exit(EXIT_FAILURE);
    }

    span buffer = cmp_compl();
    size_t bytes_read = fread(buffer.buf, 1, buffer.end - buffer.buf, pipe);
    if (ferror(pipe)) {
        perror("Failed to read clipboard content");
        pclose(pipe);
        exit(EXIT_FAILURE);
    }
    pclose(pipe);

    span new_code = {buffer.buf, buffer.buf + bytes_read};
    replace_block_code_part(new_code);
}
/*
Here we get a span (into cmp space) which contains the new code part of the current block.

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

Once all this is done, we call new_rev, passing NULL for the filename argument, since there's no filename here.
*/

void replace_block_code_part(span new_code) {
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
  state->blocks = find_blocks(inp);

  // Store a new revision, now allowing NULL as filename argument
  new_rev(state, NULL);
}

/* cmpr_init
We are called without args and set up some configuration and empty directories to prepare the CWD for use as a cmpr project.

In sh terms:

- mkdir -p .cmpr/{,revs,tmp}
- touch .cmpr/conf

Note that if the CWD is already initialized this is a no-op i.e. the init is idempotent (up to file access times and similar).
*/


#include <sys/stat.h>

void cmpr_init() {
    mkdir(".cmpr", S_IRWXU);
    mkdir(".cmpr/revs", S_IRWXU);
    mkdir(".cmpr/tmp", S_IRWXU);

    FILE* conf = fopen(".cmpr/conf", "a");
    if (conf != NULL) {
        fclose(conf);
    }
}
