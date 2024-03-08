/*

# CMPr

## "Future programming" here now?

Thesis: the future of programming is driving an LLM to write code; this is a tool to support and play with this workflow.

Today we just support ChatGPT and interaction is by copy and paste.
This means you can use it with a free GPT account, or the paid account with GPT4, which writes better code.

(Later: API usage, local models, competing LLMs, etc.)

This is a framework and represent a particular and very opinionated approach to this workflow.
We will be updating continuously as we learn.
(This version was written in ONE DAY, using the workflow itself.)

All code by ChatGPT4, and all comments written by me, which is the idea of the workflow:

- You have a "block" which starts with a comment and ends with code.
- You write the comment; the LLM writes the code.
- You iterate on the comment until the code works and meets your standard.

The tool is a C program; compile it and run it locally, and it will go into a loop where it shows you your "blocks".
You can use j/k to move from block to block. (TODO: full text search with "/")
Once you find the block you want, use "e" to open it up in "$EDITOR" (or vim by default).
Then you do ":wq" or whatever makes your editor exit successfully, and a new revision of your code is automatically saved under "cmpr/revs/<timestamp>".

To get the LLM involved, when you're on a block you hit "r" and this puts a prompt into the clipboard for you.
Then you switch over to your ChatGPT window and hit "Ctrl-V" or whatever it is on your platform to paste.
ChatGPT writes the code, and you copy it directly into the block or you can fix it up if you want.
Mnemonic: "r" means "rewrite", where the LLM is rewriting the code, based on the comment, which is the human's work.

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
If on Linux, make sure you have xclip, or edit the `cbpaste` string in the source code to suit your environment (TODO: make this better).

}

*/

#define _GNU_SOURCE // for memmem
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <limits.h>
#include <termios.h>
#include <errno.h>
#include <time.h>

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


/* spans_alloc returns a spans which has n equal to the number passed in.
Typically the caller will either fill the number requested exactly, or will shorten n if fewer are used.
*/

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
  inp.end = input_space + BUF_SZ;
  cmp.buf = cmp_space;
  cmp.end = cmp_space;
}

void bksp() {
  out.end -= 1;
}

void sp() {
  w_char(' ');
}

/* we might have a generic take() which would take from inp */
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

// Function to print error messages and exit
void exit_with_error(const char *error_message) {
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
We assume (or stipulate) that in our C code, we follow a specific pattern, called the "block" pattern:

Each function is preceeded by a block comment, and this always begins with slash star on a line by itself.

Each function also ends with a closing curly brace on a line by itself.

Furthermore, we assume there are no closing braces alone on a line between the start of the block comment and the end of the function.

Given these conventions we can partition the bytes or lines of a file into blocks by looking for and recording the lines where block comments start and where functions end.

First, we index our file to find these blocks.
We use an awk script to find all block comment start lines, and all closing-brace-only lines, and print for each block, the start and end lines separated by comma.
Each of these pairs is separated by a newline.

Here is an example C file:
*/

// #include <stdio.h>
// 
// /*
// Here is our main function!
// */
// 
// int main(int argc, char** argv) {
//   printf("hello world!\n");
// }

/* In the above example C file, we would only index one block, which starts on line 3 and ends on line 9.
So the output of our AWK script should be "3,9" and no other lines.
*/

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
*/

void send_to_clipboard(span prompt);

/*
In main,

We call a function handle_args to handle argc and argv.

We do our standard library setup, starting with init_spans() and read_and_count_stdin(), which reads stdin into the input space and the span inp.
Our standard input will always be our own source code.
We also call span_arena_alloc(), and at the end we call span_arena_free() just for clarity even though it doesn't matter anyway since we're exiting the process.
We allocate a spans arena of 1 << 20 or a binary million spans.

Next, we call find_blocks() to find the blocks in our input, which we will be using for block-oriented editing flow after this point.
This function takes the entire input span inp as an argument and returns a spans containing the blocks that it finds.

Then we call main_loop() which takes our blocks as the only arg.
Before we call main_loop, though, we must call another function reset_stdin_to_terminal, which re-opens the terminal on stdin, as our stdin typically will have been redirected from a file by the shell.

The main loop reads input in a loop and probably won't return, but just in case, we always call flush() before we return so that our buffered output from prt and friends will be flushed to stdout.
*/

void handle_args(int, char**);
spans find_blocks(span);
void main_loop(spans);
void reset_stdin_to_terminal();

int main(int argc, char **argv) {
  handle_args(argc, argv);
  init_spans();
  read_and_count_stdin();
  span_arena_alloc(1 << 20); // Allocate a spans arena of a binary million spans
  spans blocks = find_blocks(inp);
  reset_stdin_to_terminal();
  main_loop(blocks);
  flush();
  span_arena_free(); // Free the spans arena, for clarity
  return 0;
}

/*
In handle_args we handle any command-line arguments.

We don't have any command-line flags yet so this function is empty.
*/

void handle_args(int argc, char**argv) {
}

/*
We scan the input by lines and record all of the blocks into a spans.

The definition of a block is that it starts with "/*" on a line by itself without whitespace, and ends with the next "}" on a line by itself without whitespace.
Therefore we can just use next_line in a loop, and if it is span_eq to either one of these literal strings then we are either starting or ending a block.

We perform the loop twice, the first time we just count the number of blocks that are in the file, then we can allocate our spans of the correct size using spans_alloc(n_blocks), and then we loop the same way again but now populating the spans which we will return.
Before the first loop we copy our argument into a new spans so that we can consume it while counting, but then still have access to the original full span for the second loop.

Here we can use a common pattern which is that we declare a span that we are going to return or assign, and then we set .buf first (here when we find the starting line) and then set .end later (when we find the closing brace).
*/

spans find_blocks(span inp) {
    int n_blocks = 0;
    span copy = inp; // Copy to preserve original input for second iteration

    // First pass: Count the number of blocks
    while (!empty(copy)) {
        span line = next_line(&copy);
        if (span_eq(line, S("/*"))) {
            while (!empty(copy) && !span_eq(next_line(&copy), S("}"))) {
                // Loop until the end of the block is found
            }
            n_blocks++;
        }
    }

    // Allocate spans for the number of blocks found
    spans blocks = spans_alloc(n_blocks);
    copy = inp; // Reset copy to original input for second pass
    int i = 0;

    // Second pass: Populate the spans with blocks
    while (!empty(copy)) {
        span line = next_line(&copy);
        if (span_eq(line, S("/*"))) {
            blocks.s[i].buf = line.buf; // Start of the block
            while (!empty(copy) && !span_eq((line = next_line(&copy)), S("}"))) {
                // Loop until the end of the block is found
            }
            blocks.s[i].end = line.end; // End of the block
            i++;
        }
    }

    return blocks;
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

void main_loop(spans blocks);

/*
In main_loop, we are going to maintain a current index into the blocks, which will start at 0.
We will always print the current block to the terminal, after clearing the terminal.
Then we will wait for a single keystroke of keyboard input using our getch() above.

Before main_loop we define a struct, ui_state, which we can use to store any information about the UI that we can then pass around to various functions as a single entity.
This includes, so far, the blocks and the current index into them.
The only argument to main_loop is the blocks, so we first assign these and start at index 0.

We also define a helper function which takes this struct and prints the current block.

Once we have a keystroke, we will call another function, handle_keystroke, which will take the char that was entered and a pointer to our struct with the state.
This function will be responsible for updating anything about the display or handling other side effects.
When it returns we will always be ready for another input keystroke.
*/

typedef struct {
  spans blocks;
  int current_index;
} ui_state;

void print_current_block(const ui_state* state);
void handle_keystroke(char keystroke, ui_state* state);

void main_loop(spans blocks) {
  ui_state state = {blocks, 0}; // Initialize UI state with the blocks and start at index 0

  while (1) {
    // Clear the terminal
    printf("\033[H\033[J");
    // Print the current block
    print_current_block(&state);

    // Wait for a single keystroke of keyboard input
    char input = getch();

    // Handle the input keystroke
    handle_keystroke(input, &state);
  }
}

/*
Print the current block.
*/

void print_current_block(const ui_state* state) {
  if (state->current_index >= 0 && state->current_index < state->blocks.n) {
    span current_block = state->blocks.s[state->current_index];
    fwrite(current_block.buf, 1, len(current_block), stdout);
    printf("\n"); // Print a newline after the block
  }
}

void handle_keystroke(char keystroke, ui_state* state);

/*
In handle_keystroke, we support the following single-char inputs:

- j/k Increment or decrement respectively the index of the currently displayed block.
  If we are at 0 or the max, these are no-ops.
  We are also responsible for updating the display before returning to the main loop, so after either of these we call print_current_block.
- e, which calls a function that will let the user edit the current block
- r, which calls a function that will let an LLM rewrite the code part of the current block based on the comment
- R, kin to "r", which reads current clipboard contents back into the block, replacing the code part
- b, do a build (runs the command in CMPR_BUILD envvar, or "make")
- ?, which displays some help about the keyboard shortcuts available
- q, which exits the process cleanly (with prt("goodbye\n"); flush(); exit(0))

Before the function itself we write function declarations for all helper functions needed.
- edit_current_block(ui_state*)
- rewrite_current_block_with_llm(ui_state*)
- compile(ui_state*)
- replace_code_clipboard(ui_state*)

To get the help text we can basically copy the lines above, except formatted nicely for terminal output.
*/

void print_current_block(const ui_state* state);
void edit_current_block(ui_state* state);
void rewrite_current_block_with_llm(ui_state* state);
void replace_code_clipboard(ui_state* state);
void compile(ui_state* state);
void display_help(void);
char getch(void); // Assuming getch is defined elsewhere as per previous discussions

void handle_keystroke(char keystroke, ui_state* state) {
  switch (keystroke) {
    case 'j':
      if (state->current_index < state->blocks.n - 1) {
        state->current_index++;
        print_current_block(state);
      }
      break;
    case 'k':
      if (state->current_index > 0) {
        state->current_index--;
        print_current_block(state);
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
    case '?':
      display_help();
      break;
    case 'q':
      prt("goodbye\n");
      flush();
      exit(0);
      break;
    default:
      prt("Unknown command. Press ? for help.\n");
      flush();
      break;
  }
} // keep going

void display_help() {
  prt("Keyboard shortcuts:\n");
  prt("- j: Next block\n");
  prt("- k: Previous block\n");
  prt("- e: Edit current block\n");
  prt("- r: Rewrite current block with LLM\n");
  prt("- R: Replace code part from clipboard\n");
  prt("- b: Build project\n");
  prt("- ?: Display this help\n");
  prt("- q: Quit\n");
  flush();
}










void edit_current_block(ui_state* state);

/*
To edit the current block we first write it out to a file, which we do with a helper function write_to_file(span, char*).

Once the file is written, we then launch the user's editor of choice on that file, which is handled by another helper function.

That function will wait for the editor process to exit, and will indicate to us by its return value whether the editor exited normally.

If so, then we call another function which will then read the edited file contents back in and handle storing the new version.
Wheee!
*/

char* tmp_filename(ui_state* state);
int launch_editor(const char* filename);
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


char* tmp_filename(ui_state* state);

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




int launch_editor(const char* filename);

/*
In launch_editor, we are given a filename and must launch the user's editor of choice on that file, and then wait for it to exit and return its exit code.

We can look in the env for an EDITOR environment variable and use that if it is present, otherwise we will use "vim" "vi" "nano" or "ed" in that order.
*/

int launch_editor(const char* filename) {
  const char* editor = getenv("EDITOR");
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
	state->blocks = find_blocks(inp);

	// Create a new revision and perform clean-up
	new_rev(state, (char*)filename);
}




/*
Here we store a new revision, given the ui state and a filename which contains a block that was edited.
The file contents have already been processed, we just get the name so that we can clean up the file when done.

First we construct a path starting with "cmpr/revs/" and ending with an ISO 8601-style compact timestamp like 20240501-210759.
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
  char *end_comment = "*/\n"; // The end of comment pattern to search for
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




/*
To do a build, we check if there is an environment variable CMPR_BUILD set.
If it is, we run that, otherwise we just run "make".
For now we just wait for another keystroke before returning, so the user can see any compiler errors (later we'll handle them better).
(Remember to call flush() before getch() so the user sees the prompt.)

On the other hand, if the compile command returns 0 we don't need the extra keystroke and will go back to the main loop.
We aren't using ui_state for anything yet here, but later we'll put something on it to provide more status info to the user.
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

void flush(void);
char getch(void); // Assuming getch is defined as per previous discussions

void compile(ui_state* state) {
  (void)state; // Unused parameter placeholder to avoid compiler warnings

  const char* build_cmd = getenv("CMPR_BUILD");
  if (!build_cmd) {
    build_cmd = "make";
  }

  prt("Compiling...\n");
  flush();

  pid_t pid = fork();
  if (pid == -1) {
    perror("Failed to start compile process");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    // Child process: Execute build command
    execlp("sh", "sh", "-c", build_cmd, (char*)NULL);
    // If execlp returns, an error occurred
    perror("execlp");
    exit(EXIT_FAILURE);
  } else {
    // Parent process: Wait for build command to complete
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      prt("Compile succeeded\n");flush();
    } else {
      prt("Compile failed, press any key to continue...\n");flush();
      getch(); // Wait for a keystroke before returning
    }
    flush();
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
  state->blocks = find_blocks(inp);

  // Store a new revision, now allowing NULL as filename argument
  new_rev(state, NULL);
}









