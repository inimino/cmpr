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
