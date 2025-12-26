/* #libraryintro

The spanio library.
We use spanio methods when possible and only use null-terminated C strings at interface boundaries where there is no way around it, see the s() pattern.
When we say "print" we mean what prt() does, which is append output to the output span out; it has the printf interface, i.e. format string followed by varargs.
You must flush() before the output will be printed to stdout and be visible to the user.
A common cmpr pattern is prt, flush, getch.
To "complain and exit" means prt, flush, exit(n>0).
A span has a start pointer and an end pointer, called .buf and .end respectively.

- empty(span): If a span is empty (start and end pointers are equal).
- len(span): The length of a span. Prefer this over less clear .end minus .buf.
- init_spans(): Init global spans and buffers; called only from main().
- prt(char *, ...): Formats and appends a string to the output span, i.e. prints it. Pronounced as prt.
- prs(char *, ...): Same as prt, but returns a span (allocated in cmp space).
- w_char(char): Writes a single character.
- wrs(span): Writes the contents of a span.
- bksp(): Backspace, shortens the output span by one.
- sp(): Appends a space character to the output span, i.e. prints a space.
- terpri(): Prints a newline (name courtesy Common Lisp).
- out_sav out2cmp(), out_rst(out_sav): redirect all output functions to cmp (instead of out) and then undo (reset) back to given opaque state reprentation.
- flush(), flush_err(): Flushes the output span to standard output or standard error.
- write_to_file_span(span content, span path, int clobber): Write a span to a file, optionally overwriting.
- write_to_file(span, const char*): Deprecated.
- readable_file(span): whether a file exists as a regular file readable by us.
- read_file_into_span(char*, span): Reads the contents of a file into a span. Deprecated.
- read_file_S_into_span(span, span): Read the contents of a file $1 into a span $2. Used in new code. Returns a span prefix of $2.
- read_file_into_cmp(span): Filename as a span, returns contents as a span inside cmp space.
- read_file_into_inp(span): Filename as a span, returns contents as a span inside inp space.
- advance1(span*), advance(span*, int): Advances the start pointer of a span by one or a specified number of characters.
- shorten1(span*), shorten(span*, int): Shortens a span by one or by a given number of characters.
- find_char(span, char): Searches for a character in a span and returns its first index or -1; find_char_rev(span,char) finds the last index or -1.
- contains(span, span): Checks if one span TEXTUALLY contains another; "abc b"; O(n) string search.
- contains_ptr(span, span): Checks if one span PHYSICALLY contains another; "[[]]"; O(1) pointer comparisons.
- starts_with(span, span): Check if $2 is textual prefix of (or equal to) $1 (mnemonic for arg order: $1 starts-with $2).
- ends_with(span, span): Check if $1 ends with $2.
- consume_prefix(span, span*): Shortens a span by a prefix if present, returning that prefix or nullspan().
- first_n(span, int): Returns n leading chars of a span.
- skip_n(span, int): Returns a new span skipping n initial chars.
- take_n(int, span*): Returns as a new span the first n characters from a span, mutating it; often used when parsing.
- next_line(span*): Extracts the next line (up to \n or .end) from a span and returns it as a new span.
- span_eq(span, span), span_cmp(span, span): Compares two spans for equality or lexicographical order.
- S(char*): Creates a span from a null-terminated string.
- char* s(span): Returns a null-terminated string (in cmp space) containing the given contents.
- char* s_buffer(char*,int,span): Copies $3 into $1 (of length $2) and null-terminates it, returning $1 for convenience.
- nullspan(): Returns the empty span at address 0.
- index_of(span,spans): Return first element of $2 which is span_eq $1, or -1 if none match.
- spanspan(span, span): Finds the first occurrence of a span within another span and returns a span into haystack.
- trim(span): Gives the possibly smaller span with any isspace(3) trimmed on both sides.
- split_whitespace(span): split a span into tokens on whitespace
- concat(span,span): Returns a new span (in cmp space) containing a concatenation.
- parse_int(span): Parse an int, but without altering the span.
- parse_hex(span): Parse a hex value, without altering the span.
- scan_int(span*), scan_hex(span*): Similar, but advances the span past the parsed value.

typedef struct { u8* buf; u8* end; } span; // the type of span

*/
/* #spanio_advanced

These should probably be documented separately.

- skip_whitespace(span*): modifies a span, returning a prefix span of zero or more removed whitespace.
- split_commas_ws(span): splits a span into a spans on commas, stripping whitespace
- w_char_esc(char), w_char_esc_pad(char), w_char_esc_dq(char), w_char_esc_sq(char), wrs_esc(): Write characters (or for wrs_esc, spans) to the output span, applying various escape sequences.
*/

/**/

#define _GNU_SOURCE
#include "siphash/siphash.h"
#include <dirent.h>

typedef uint64_t u64; // we should probably put all these in one place

#define flush_exit(n) flush(); exit(n) // used only by handle_args; let's do this differently
/* #span_ret
The span ret pattern is a common idiom in functions returning span.
Instead of collecting the start and end of the span in separate variables and then constructing a span value to return at the end, we instead declare a span variable called "ret" at the top, and then set the .buf and .end separately, wherever it is convenient to do so (not necessarily in that order), and whenever both have been set, the value is ready and can be returned or used.
*/
/* #const
Note that we NEVER write const in C, as this feature doesn't pull its weight.
There's some existing contamination around library functions but try to minimize the spread.
*/

/* #prt_usage
Note that prt() has exactly the same function signature as printf, i.e. it takes a format string followed by varargs.
We never use printf, but always prt.
A common idiom when reporting errors is to call prt, flush_err, and exit.

To prt a span x we use %.*s with len(x) and x.buf.
If the span would be the only thing in the formatting string, just use wrs(x) (maybe with terpri() after if you need a newline).
*/
/* #span_usage

A span is only two u8* elements, .buf and .end.

Always use len() to get the length of a span.

A common idiom is next_line() in a loop with !empty().

As next_line() leaves out the newline, you can implement cat by using a next_line + empty loop with wrs and terpri in the body.

In a next_line loop, to tell if you are on the last line, since next_line() has already removed the line you are processing, you can test whether the thing you are consuming is empty; if it is, the line next_line gave you was the last line.
*/
/* #spanio_initialization
@- TODO: fill this out (with arenas and whatever else).

In main() or similar it is common to call init_spans and often also read_and_count_stdin.
*/

/* #thran
@- experimental, may go away

A thran has three pointers and can be addressed as two spans which share an endpoint; it is naturally used internally for things like buffers, pipes, and in general anywhere where information is being consumed linearly (usually left-to-right, i.e. ascending addresses in memory, but could be in reverse), for example in parsing.
You can think of it as a span with a progress bar.

typedef struct { u8* buf; u8* end; u8* p; } thran; // a thran holds buf and end but also .p (pointer (or progress))

- thran_of(span): the pointer always refers to some location in between buf and end, here it will be set equal to buf.
- thran_a(thran): returns the "a" part of a thran, i.e. the part up to the pointer (e.g. empty(thran_a(thran_of(x))) for any x).
- thran_b(thran): returns the "b" part, after the pointer, (span_eq (thran_b (thran_of x)) x) is true for any span x.
- thran_full(thran): the dual of thran_of, returns both parts of the thran as a span (discarding the .p information).
*/
/* #generic_array

We have a generic array implementation using arena allocation.

- T will have .a of type E*, and .n, and .cap of type size_t.
- T_alloc(N) returns an array of type T, with .n = 0, .cap = N.
- T_arena_push() and T_arena_pop() manage arena allocation stack; use them as directed.
- Use T_push(T*,E) to push an element onto an array.

*/

/* #spans @generic_array

Our generic array is used to declare a spans type and the associated functions.
*/
/* #s_pattern

Note that in general our spans are NOT null-terminated, so casting a span.buf to a char* and hoping for the best in calling C library functions would be very wrong.

When we need a null-terminated C string for talking to library functions, we can use s_buffer().
We use a local buffer of some suitably generous size, according to the use case.
For example, when used for a path name we should use PATH_MAX.

Here is an example using a size of 2048:

```
char buf[2048] = {0};
s_buffer(buf,2048,some_span);
... use 'buf' ...
```
*/
/* #jsonlib

JSON support in the spanio library.

- json_s(span): prt a JSON string (double-quoted and escaped appropriately).
- json_n(f64): prt a double in JSON format.
- json_b(int): prt a true or false (only 0 is false).
- json_0(): prt a json_null value ("null").
- json_o(): prt an empty json object.
- json_o_extend(json*,span,json): extends $1 with key $2 and value $3.
- json_a(): prt an empty json array.
- json_a_extend(json*,json): extends $1 with key $2.
- all the above json constructor functions return the json type (which they also prt, usually this is sent to cmp space) as in the _{s,n,b,0,o,a} constructors.
- json_{s,n,b,0,o,a}p: full list of json_?p predicate funcs, used to distinguish types of json values.
  - (for example) json_sp(json): 1 if $1 is a string, otherwise 0.
- json_key(span, json): lookup on json object.
- json_index(int, json): lookup on json array.
- above lookup functions return a "nullable json".
- mnemonic: the argument order was inspired by partial application.
- int json_is_null(json): returns 0 or 1.
- json_un_s(json): return a span containing the actual value of a json string value (e.g. from json_key or json_index).
- json_parse(span): parse a span into a json and return it; may be shorter only by trimmed whitespace; commonly used.
- make_json(span): return a json wrapper of the span in O(1); the span must be known to be valid json already; rarely used.
- json_s2s(json,span*,u8*): converts json string $1 into an unquoted string in $2 (not exceeding buffer end $3); returns a span.
- json_parse_prefix(span*): not usually called directly, but can be used to parse a json value off the front of a buffer, shortening it.

The json type is a wrapped span which actually contains a JSON-formatted string, allocated in cmp space.
Every json value wraps a span .s, which can be accessed directly whenever the string value of the json is needed, for example when sending as JSON over the wire.

There are constructor functions for all the primitive types, and for the collection types, array and object, there are constructors for the empty collections and extend functions to extend them.
These extend them in place, and are intended for relatively simple applications like building a message for an API call.

There are predicate functions for the json type that distinguish between numbers, arrays, and so on.
(Since the json type just wraps an actual JSON string, these work by looking at the first character of that string, which is definitive; this implies that the json type doesn't include leading or trailing whitespace.)

JSON defines a literal "null" value, but we define a separate "nulljson" distinguished signal value, testable by json_is_null, which indicates some kind of hard failure.
It is returned by all the json-returning functions that can fail, such as indexing an array or object, or parsing a JSON string.
It is simply the json type wrapping a nullspan (the span having .buf = .end = 0).

The function make_json is rarely used and is for "casting" a span to a json object in constant time.
It is normally only used internally in library methods, but can be used if you know you have a JSON string and don't want to parse it again.

The json indexing functions return the json type; that is, the contents are still valid JSON.
In particular, a JSON string will contain JSON string escaping.
If you want the actual string value, you can use json_un_s, which returns a new span in cmp space.
(This uses the lower-level json_s2s, which has a less convenient interface.)

(We should probably have a similar function for getting a number out, but it hasn't been added yet.)
*/
/* #json_design

- all the json constructor functions trim whitespace, so that all the predicate functions follow a pointer and examine one byte.
- the json parser and constant-time wrapper functions are the low-trust and high-trust ways to make a json from a string.
- if the json parser indicates that your span is valid json, that means that one of the json value-type predicates will return true for that json.
- the json value returned from the parser will match the input span except that any whitespace will have been trimmed.
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
#include <stddef.h>
#include <regex.h>
/* convenient debugging macros */
#define dbgd(x) prt(#x ": %d\n", x),flush()
#define dbgx(x) prt(#x ": %x\n", x),flush()
#define dbgf(x) prt(#x ": %f\n", x),flush()
#define dbgp(x) prt(#x ": %p\n", x),flush()
#define dbgs(x) prt(#x ": %.*s\n", len(x), x.buf),flush()

typedef unsigned char u8;

/* #span

Basic types and function declarations.

A span is two pointers.
Buf points to the first char included in the string.
End points to the first char excluded after the string's end.

These two pointers must point into some space that has been allocated somewhere.

If these two pointers are equal, the string is empty, but it still points to a location.
(So two empty spans are not necessarily the same span, while two empty strings are.)

Neither spans nor their contents are immutable; everything depends on intended use.

Spans frequently point into one of three large buffers, namely inp, out, and cmp.

The inp variable is the span which writes into input_space, and then is the immutable copy of stdin for the duration of the process.
This input may come from stdin or from the filesystem or network, etc.
The number of bytes of input is len(inp).
The output is stored in span out, which points to output_space.
Input processing is generally by reading or parsing inp or subspans of inp.
The output spans are mostly written to with prt() and other IO functions.
The cmp_space and cmp span which points to it are used for model data.
This includes reading and writing data that is synthesized during the program runtime.
These are just the common conventions; your program may use inp, out, and cmp differently.

When writing output, we often see prt followed by flush.
Flush sends to stdout the contents of out (the output span) that have not already been sent.
Usually it is important to do this
- before any operation that blocks, when the user should see the output that we've already written,
- generally immediately after prt when debugging anything,
- after printing any error message and before exiting the program, and
- at the end of main.

If you want to write to stderr, you can use flush_err(), which also flushes from the output_space but to stderr instead of stdout.
(You may need to do a flush() before the call to prt() if you already have pending output that needs to go to stdout.)
*/

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
/* #spanio_basics

input statistics on raw bytes; span basics

This hand-written C code implements most of our span I/O basics.
If we can get an LLM to match this style it's a good result.

*/

int counts[256] = {0};

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
    counts[c]++;
    *inp.buf = c;
    inp.buf++;
    if (len(inp) == BUF_SZ) { prt("input overflow\n"); flush_err(); exit(1); }
  }
  inp.end = inp.buf;
  inp.buf = input_space;
}

 /*
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
*/

// set if debugging some crash
const int ALWAYS_FLUSH = 0;

// Note: this doesn't swap output_space, which means manual comparisons with output_space + BUF_SZ will be broken?
// probably an argument for the "thran"
// actually we should just be using out.buf + BUF_SZ anyway I suppose
//void swapcmp() { span swap = cmp; cmp = out; out = swap; int swpn = cmp_WRITTEN; cmp_WRITTEN = out_WRITTEN; out_WRITTEN = swpn; }
//void prt2cmp() { if (out.buf == output_space) swapcmp(); }
//void prt2std() { if (out.buf == cmp_space) swapcmp(); }

//span prt_cmp_stack[1024] = {0};
//int prt_cmp_stack_n = 0;
//void prt_cmp() { assert(prt_cmp_stack_n < 1023); prt_cmp_stack[prt_cmp_stack_n++] = out; out = cmp; }
//void prt_pop() { assert(0 < prt_cmp_stack_n); out = prt_cmp_stack[--prt_cmp_stack_n]; }

/* C convenience methods

We have a copy_file already here.

We add mkdir_p and pathpart just to simplify out2atp.

*/

/* #copy_file
The copy_file function copies the contents from one file to another.
It operates by opening the source file for reading and the destination file for writing.
The function reads chunks of data into a buffer and writes them out to the destination file, handling potential interruptions due to signals.
It also performs error checks at each step, including during file opening, reading, and writing.
If an error occurs, the function closes any open file descriptors and returns a negative error code corresponding to the step where the failure occurred.
*/

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
/* #mkdir_p

void mkdir_p(span dir) {
  // find the first occurrence, if any, of the char "/", which ends the leading path component of dir
  // if there is no such occurrence, we are done, return
  // use chdir(2) to change the cwd
  // if this doesn't work because the directory does not exist, then create it (and then cd into it after all)
  // continue to loop over the remaining part of the span after the "/"
  // finally we return to the directory which we were originally in (which we must have saved earlier in `old_cwd` using getcwd and a PATH_MAX-sized buffer).
}

First we store the u8* cmp.end, so we don't leak cmp space.
On the last line of the function (or before any early return) we must remember to reset cmp.end = end.
@- probably we should have a very unsafe s() version that re-uses a single static buffer, since this is library stuff it's ok if it's hard to use

If any of our system calls fails, we will immediately print any filename argument (using prt, flush_err) and then print the OS error message using perror("mkdir_p") and finally exit(1).
*/

void mkdir_p(span dir) {
    u8* end = cmp.end;
    fprintf(stderr, "%p", end);
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
    fprintf(stderr, "%p", cmp.end);
}

/* #pathpart

This is just a convenience method getting the longest known-path component of a span.

This is simply the prefix of dir that ends with the last slash it contains.

span pathpart(span dir) {
  // find the last slash in dir
  // if none, return the empty span located at dir.buf
  // return a span starting from dir.buf and ending with the slash offset plus one (so that it is included)
}

(Note that this function always returns a subspan of dir, and only a null span if dir is the null span.)
*/

span pathpart(span dir) {
    int last_slash = find_char_rev(dir, '/');
    if (last_slash == -1) {
        return (span){ .buf = dir.buf, .end = dir.buf };
    }
    return (span){ .buf = dir.buf, .end = dir.buf + last_slash + 1 };
}

/* spanio basics
*/

out_sav out2cmp() { out_sav ret = {0}; ret.outp = outp; outp = &cmp; return ret; }
//out_sav out2atp(span p) { out_sav ret = {0}; ret.outp = outp; outp = &cmp; mkdir_p(pathpart(p)); char buffer[4096]; s_buffer(buffer, 4096, pathpart(p)); ret.fcls = open(buffer, O_WRONLY | O_CREAT | O_APPEND); return ret; }
//void out_rst(out_sav sav) { outp = sav.outp; /*if (sav.fcls) close(sav.fcls);*/ if (sav.prev_target) flush_target = sav.prev_target; }

 /*
out_sav out2atp(span p) {
  out_sav ret = {0};
  ret.outp = outp;
  outp = &cmp;
  //fprintf(stderr,"before: %p\n",cmp.end);
  mkdir_p(pathpart(p));
  //fprintf(stderr,"after: %p\n",cmp.end);
  char buffer[4096];
  s_buffer(buffer, 4096, p);
  int fd = open(buffer, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd < 0) {
    perror("out2atp");
    exit(1);
  }
  ret.prev_target = flush_target;
  flush_target = fdopen(fd, "a");
  if (!flush_target) {
    perror("out2atp");
    exit(1);
  }
  return ret;
}
*/

void out_rst(out_sav sav) {
  //flush();
  outp = sav.outp;
  //if (sav.fcls) close(sav.fcls);
  //if (sav.prev_target) {
    //fclose(flush_target);
    //flush_target = sav.prev_target;
  //}
}

void prt(const char * fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *buffer;
  // we used to use vsprintf here, but that adds a null byte that we don't want
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
  // we used to use vsprintf here, but that adds a null byte that we don't want
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

 /* not really any better for usability I think
span read_f_into_span(span filename, span* buffer) {
  span ret = read_file_S_into_span(filename, *buffer);
  buffer->buf = ret.end;
}
*/

 /* #readable_file @s_buffer

int readable_file(span path);

We use s_buffer pattern with PATH_MAX and do a stat.

If the file doesn't exist, isn't a normal file, or isn't readable by us we return 0, otherwise 1.
*/

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

 /*
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
*/

   /*
take_n is a mutating function which takes the first n chars of the span into a new span, and also modifies the input span to remove this same prefix.
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
If the prefix is found, we return it and modify the span that is being parsed to remove the prefix.
Otherwise we leave that span unmodified and return nullspan().
Typical use is in an if statement to either identify and consume some prefix and then continue on to handle what follows it, or otherwise to skip the if and continue parsing the unmodified input.
We return the span that points into the input in case the caller has some use for it.
*/

span consume_prefix(span prefix, span *input) {
  if (len(*input) < len(prefix) || !span_eq(first_n(*input, len(prefix)), prefix)) {
    return nullspan();
  }
  span ret = {.buf = input->buf};
  input->buf += len(prefix);
  ret.end = input->buf;
  return ret;
}
/* #generic_array_implementation

Generic arrays.

Here we have a macro that we can call with two type names (i.e. typedefs) and a number.
One is an already existing typedef and another will be created by the macro, and the number is the size of a stack, described below.

For example, to create the spans type we call this macro with span and spans as the names.
We call these the element type and array type names resp.
We use "E" and "T" as variables in documentation.
We also use E and T and STACK_SIZE as the names of the macro arguments.

This macro will create a typedef struct with that given name that has a pointer to the element type called "a", a number of elements, which is always called "n", and a capacity "cap", which are size_t's.

We use an arena allocation pattern.

For every generic array type that we make, we will have:

- A setup function T_arena_alloc(N) for the arena, which takes a number (as int) and allocates (using malloc) enough memory for that many of the element type, where T is the array type name.
- A corresponding T_arena_free().
- A pair T_arena_push() and T_arena_pop().
- A function T_alloc(N) which returns a T, having cap of N.
- T_push(T*,E) which increments n and stores the element provided.

The T_push method may relocate the memory in the arena if necessary.
It will only move it to a later position.
When the cap would be exceeded, it uses the pointer and cap of the array, and the allocated memory on the arena to determine whether the end of this array is at the end of the allocated region of the arena.
If it is, then it simply increases each of .n and .cap (and the arena's allocated count) by one.
Otherwise, it doubles the capacity and moves the memory to be after all currently allocated memory in the arena.
(As a special case, if the cap was zero, it sets it to 2 rather than doubling it.)
Note that our reallocation strategy does not free the original allocated memory back to the pool, so we cannot subtract the original capacity from the allocated memory---memory is only given back to the pool by using T_arena_pop().

The implementation makes a single global struct (both the typedef and the singleton instance) that holds the arena state for the array type.
This includes the arena pointer, the arena size in elements, the number of allocated elements, and a stack of such numbers.
The stack size is also an argument to the macro.
We do not support realloc on the entire arena, rather the programmer needs to choose a big enough value and if we exceed at runtime we will always crash.
The programmer has to call the T_arena_alloc(N) and _free methods themselves, usually in a main() function or similar, and if the function is not called the arena won't be initialized and T_alloc() will always complain and crash (using prt, flush, exit as usual).

The main entry point is the MAKE_ARENA(E,T) macro, which sets up everything and must be called before any references to T in the source code.
Then the arena alloc and free functions must be called somewhere, and everything is ready to use.

The global arena variable, while not technically part of the interface, is read directly for debugging memory usage, so we also make it part of the interface.
The name should be T##_global_arena, and it should have members `arena_size` and `allocated`.
@- Or we could just add getters to the actual interface.
*/

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

/* #generic_array_initialization

Generic arrays are given an array type T and an element type E.

Memory is managed in an arena by setting a high-water mark and restoring to it with a push/pop function pair.

We set the size of the stack used by this push/pop pair when we set up the generic array.

To set up the generic array, we use the MAKE_ARENA macro with T, E, and the stack size as arguments.

Later, before using the generic array, we must call the T_arena_alloc function.
This also takes a size_t parameter, but in this case it is the number of elements to allocate memory for in the arena (which is fixed size).
Finally, a T_arena_free function can be called, though we often do not need to do this as our arenas will be used until the process exits.
*/
/* #generic_array_usage
@- We have a problem with abstraction here, the LLM isn't smart enough to make use of this documentation without it being specialized to the type in question.
@- We can fix this by actually making the documentation take type names as variables, and expand the documentation as a template.
@- This lets us generate documentation that's more explicit for the LLM while maintaining the documentation at the higher abstraction level of the generic implementation.

Generic arrays use an arrena allocation pattern.

Each generic array type is created by a macro with E and T type variables.

The E type is the element type of the array, and the T type is the type of the array itself.

In the spanio library itself the spans array type is already created, where the element type E is `span`, and the array type T is `spans`.

For every generic array type, we get the following functions available, with E and T being placeholders:

- T_arena_push()                       pushes the current arena allocation size onto a stack
- T T_alloc(size_t)                    returns a newly allocated array with the given capacity
- T_push(T*,E)                         pushes an element (type E) onto an array (type pointer to T)
- T_arena_pop()                        sets the arena allocation point to the previous call to T_arena_push, freeing memory

When pushing onto an array, it will be extended in place if nothing has been allocated after it in the arena, otherwise it will be doubled in capacity and moved.
The only way to free memory is with T_arena_pop(), which invalidates anything allocated since the last T_arena_push().
Anything that was pushed onto may also be invalidated.
So caution must be used when deciding where to put the T_arena_push and T_arena_pop calls.

When iterating over a T array type, the .n member (a size_t) can be used to get the number of elements in the array.
The .a member is the array itself, so for(size_t i = 0; i < x.n; i++) { ... x.a[i] ... } is a common pattern.
*/

/* #spans_usage @generic_array_usage

The spans array has T = spans and E = span.

spans_arena_push, spans_alloc, spans_push(spans*, span), and spans_arena_pop are the main methods used.

index_of finds a span's location in a spans (or -1).

@- TODO: this can be generated from the #generic_array_usage as a template
*/
/*
Our first generic array is spans, which has a stack depth of 256.

*/

MAKE_ARENA(span,spans,256);

/*
Other stuff.
*/

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
/* #json

JSON library

*/

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
/*
The `json_parse` function takes a `span` representing the JSON data and returns a `json` type object, unless the parse failed or did not consume the entire input (excepting whitespace) in which case it returns nulljson().
*/

json json_parse(span s) {
  skip_whitespace(&s);
  json ret = json_parse_prefix(&s);
  skip_whitespace(&s);
  if (empty(s)) return ret;
  return nulljson();
}
/*
The json_parse_prefix function takes a span and parses as much of it as it can as a json, then leaves the rest, and returns a json which is null only if the parse failed.

In fact, the span is passed in by reference and we modify it, shortening it from the front.
If the parse fails the input span may be modified.

We declare a json return value `ret`.
This will include the first non-whitespace byte that we consume up to the end of the complete JSON value by the time we return it, or we will indicate failure and not return it.

First we strip any whitespace by calling skip_whitespace on the input span.
Then we set ret.s.buf from the input as if we succeed this is the only value it can have.
After this point, if we return successfully we will always set ret.s.end to be the same as .buf of the input span when we are returning.
I.e. the json that we return always covers the prefix that we have parsed up to what is left in the input.

Then we switch on the first non-ws char of the input, and then we either:

- call json_parse_prefix_string
- call json_parse_prefix_number
- directly parse a true/false/null
- directly parse an object or array

To directly parse an object, we first consume the "{", then call json_parse_prefix_string.
If this returns a null json it means we failed and we return the null json.
Otherwise we continue by skipping whitespace, consuming the ":" and then recursively calling json_parse_prefix to consume the value.
Once again, if the value is the null json then we failed and return the null json.
Otherwise, we consume (whitespace and) either another comma, going around the loop again, we exit the loop, and then outside that consume (any whitespace and) the final "}" and return successfully.

To directly parse an array we do something similar but without the keys, just handling the commas and values.

To directly parse true/false/null, we call consume_prefix with the appropriate string.
This returns a span, which we put in a variable; if it is the null span we return nulljson().
Otherwise, it will be a span pointing into the span that we are parsing, for this reason we return the same span returned from consume_prefix, just wrapping it with a call to make_json first.
*/

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
/*

In json_s2s we get a json with .s being a JSON string, and a span pointer to a buffer area, which we will extend, and a max u8* giving the end of the buffer region.

First we assert that the string starts with the double quote, which we advance past.

Then we iterate over the input until it ends or we reach the closing quote.

We unescape the input into the buffer, advancing the end of the buffer.

We handle all the escaping in JSON strings.

Specifically, we can see a backslash followed by:

- b,f,n,r,t
- ",\,/
- u followed by four hex digits with either A-F or a-f

In every case we write the unescaped character into the buffer, advancing .end, and we also copy all non-escaped characters over directly.
In the case of \u we encode as UTF-8.

Finally we return a span covering the area that .end advanced over.
I.e. the length of the returned span is also the length that was added to the buffer span.

If we would ever advance the buf past the max we also crash the program (prt, flush, exit).
*/

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

/*
In json_parse_prefix_string we consume a JSON string from the input span and wrap it as a json.

We handle all the escaping in JSON strings.

Specifically, we can see a backslash followed by:

- b,f,n,r,t
- ",\,/
- u followed by four hex digits with either A-F or a-f

Here we just consume the initial and final double quotes, parse all the escaping to be sure it is a valid JSON string, and return a json with .s that points to the string (including the quotes) or nulljson() if there is any parsing error.
If we return successfully ret.s.end and input->buf will be equal at the end.
*/

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

/*
In json_parse_prefix_number, we handle the JSON number format, namely:

We either return nulljson() if we could not parse the number for any reason or a json which includes the number that was parsed.

Manually written.
*/

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
/*
Manually written for now.
*/

json json_parse_prefix_littok(span *input) {
  span inner;
  if (!empty(inner = consume_prefix(S("true"), input))) return (json){inner};
  if (!empty(inner = consume_prefix(S("false"), input))) return (json){inner};
  if (!empty(inner = consume_prefix(S("null"), input))) return (json){inner};
  return nulljson();
}

/* #sio

Spanio basics.

Instead of null-terminated strings, we use spans.
Functions that take string input or output should always use the span type, never char*.
(We only rarely use char* for talking to C library functions.)

To create a span from a char* use S(): `span s = S("hello world")`.

A span is a struct having a .buf and .end, both u8 pointers.

To get the length of a span, use `len(s)`, or to just check if it is empty use `empty(s)`.

To compare equality of strings, use span_eq(a,b).

for a lexicographic comparison use span_cmp(a,b), returning an int {-,0,+} according to whether {a<b, a==b, a>b}.

You can tell if a span `a` starts or ends with another `b` using starts_with(a,b) and ends_with(a,b).

To get a prefix of a span use first_n(span,int).
*/
/* #parserpattern

In parser functions, we are given a span pointer, usually called input.

We either return a value indicating success of some parsed result, and shorten the span (by advancing .buf), or we leave the span untouched and return some distinguished value indicating failure.

Thus the basic parser pattern is a function that takes a span pointer as input, and returns something.
The success or failure is indicated by the returned value.
The progress of the parse of the input is indicated by the modification, if any, made to the span that is passed in by reference.
The input span is gradually consumed by the various parser functions until it is empty, or until the parse fails.

We often use consume_prefix(span,span*) in parsers which consumes a literal prefix, if present, and returns it as span, and shortens the second argument accordingly, or if it is not present, returns an empty span and leaves the second argument unmodified.

We may also use take_n(int,span*), which is similar to first_n in the main spanio functions, but specialized for modifying a span as we do when parsing.
It takes a bite off the front of the string being parsed.
It returns a span of the given length pointing to the beginning of the input span, and advances the .buf of that span by the same amount.
*/

/* #jsonparser

The json parser (and indexing methods) returns a parsed json object that wraps a span containing the actual bytes of input.

This means that if the span parses cleanly as json, the .s of the returned value will be a subset of the input span (in the sense of contains_ptr), i.e. it will point to the same bytes.
This means that we don't allocate any new memory when parsing, and it also means that (if desired) the consuming function can index the JSON as it is being parsed.

This means that we only wrap spans of the given input, and manually construct json objects to return, rather than calling other constructor functions (e.g. json_b), which will typically point to static strings.
*/
/* #json_parse_prefix_littok @sio @jsonlib @parserpattern @jsonparser

Here we parse the three literal tokens, which are "true" "false" and "null".

Write the json json_parse_prefix_littok(span*) function.


json json_parse_prefix_littok(span* input) {
    if (consume_prefix(input, S("true"))) return json_b(1);
    if (consume_prefix(input, S("false"))) return json_b(0);
    if (consume_prefix(input, S("null"))) return json_0();
    return json_is_null(json_0()); // or some other error indication
}

*/
/* 
We do not use null-terminated strings but instead rely on the explicit end point of our span type.
Here we have spanspan(span,span) which is equivalent to strstr or memmem in the C library but for spans rather than C strings or void pointers respectively.
We implement spanspan with memmem under the hood so we get the same performance.
Like strstr or memmem, the arguments are in haystack, needle order, so remember to call spanspan with the thing you are looking for as the second arg.
We return either the empty span at the end of haystack or the span pointing to the first location of needle in haystack.
If needle is empty we return the empty span at the beginning of haystack.
(This allows you to distinguish between a non-found needle and an empty needle, as both give an empty match.)
Maybe we should actually return the nullspan here; considering the obvious extension to regexes or more powerful patterns, the difference between a matching empty span and a non-match would become significant.
Examples:

spanspan "abc" "b" -> "b"
spanspan "abc" "x" -> "" (located after "c")
spanspan "abc" ""  -> "" (located at "a")
*/

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

