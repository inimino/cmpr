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
  ls -lh cmpr/README.md
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

/* import our library code */

#include "spanio.c"
/*
We define CONFIG_FIELDS including all our known config settings, as they are used in several places (with X macros).

The known config settings are:

- projdir, location of the project directory that we are working with as a span
- revdir, a span containing a relative or absolute path to where we store our revisions
- tmpdir, another path for temp files that we use for editing blocks
- buildcmd, the command to do a build (e.g. by the "b" key)
- cbcopy, the command to pipe data to the clipboard on the user's platform
- cbpaste, the same but for getting data from the clipboard
*/

#define CONFIG_FIELDS \
X(projdir) \
X(revdir) \
X(tmpdir) \
X(buildcmd) \
X(cbcopy) \
X(cbpaste)

/*
A project can contain multiple files.

We have a projfile type which contains for each file:

- the path as a span
- the language, also a span
- the contents of the file, also a span

Here we have a typedef for the projfile, and we also call our generic macro to make a corresponding array type called projfiles, choosing 256 for the stack size.
*/

typedef struct {
    span path;
    span language;
    span contents;
} projfile;

MAKE_ARENA(projfile, projfiles, 256)
/*
We define a struct, ui_state, which we can use to store any information about the UI that we can then pass around to various functions as a single entity.
This includes, so far:
- files, a projfiles array of files in the project
- the current language, a span, used by the file/language config handler functions
- the blocks, a spans
- the current_index into them
- a marked_index, which represent the "other end" of a selected range
- the search span which will contain "/" followed by some search if in search mode, otherwise will be empty()
- the config file path as a span
- terminal_rows and _cols which stores the terminal dimensions
- scrolled_lines, the number of physical lines that have been scrolled off the screen upwards

Additionally, we include a span for each of the config files, with an X macro inside the struct, using CONFIG_FIELDS defined above.
*/

typedef struct ui_state {
    projfiles files;
    span current_language;
    spans blocks;
    int current_index;
    int marked_index;
    span search;
    span config_file_path;
    int terminal_rows;
    int terminal_cols;
    int scrolled_lines;
    #define X(name) span name;
    CONFIG_FIELDS
    #undef X
} ui_state;
/*
In main,

First we call init_spans, since all our i/o relies on it.

We call projfiles_arena_alloc near the top and and _free before we exit, with room for 1 << 14 (2^14) elements.

We call span_arena_alloc(), and at the end we call span_arena_free() just for clarity even though it doesn't matter anyway since we're exiting the process.
We allocate a spans arena of 1 << 20 or a binary million spans.

Just above main, we declare a global ui_state* called state, which will allow us to not pass around the ui_state singleton all over our program.
After declaring our ui_state variable in main, which we initialize to {0}, we will set this global pointer to it.
We set config_file_path on the state to the default configuration file path which is ".cmpr/conf", i.e. always relative to the CWD.
We projfiles_alloc room for 1024 files on the state, and set the ".n" to zero, so we can use the _push() pattern.

We call a function handle_args to handle argc and argv.
This function will also read our config file (if any).
We call check_conf_vars() once after this; we also call it in the main loop but we need it before trying to get the code.

Next we call a function get_code().
This function either handles reading standard input or if we are in "project directory" mode then it reads the files indicated by our config file.
In either case, once this returns, inp is populated and any other initial code indexing work is done.

Then we call main_loop().

The main loop reads input in a loop and probably won't return, but just in case, we always call flush() before we return so that our buffered output from prt and friends will be flushed to stdout.
*/

void handle_args(int argc, char **argv);
void get_code();
void check_conf_vars();
void main_loop();

ui_state* state;

int main(int argc, char** argv) {
    init_spans();
    projfiles_arena_alloc(1 << 14);
    span_arena_alloc(1 << 20);

    ui_state local_state = {0};
    state = &local_state;
    state->config_file_path = S(".cmpr/conf");
    state->files = projfiles_alloc(1024);
    state->files.n = 0;

    handle_args(argc, argv);
    check_conf_vars();
    get_code();
    main_loop();

    flush();
    projfiles_arena_free();
    span_arena_free();
    return 0;
}
/* #all_functions
*/

void edit_current_block();
void rewrite_current_block_with_llm();
void compile();
void replace_code_clipboard();
void toggle_visual();
void start_search();
void settings_mode();
void send_to_clipboard(span prompt);
void print_physical_lines(span, int);
int print_matching_physical_lines(span, span);
void page_down();
void page_up();
span count_physical_lines(span, int*);
void print_multiple_partial_blocks(int,int);
void print_single_block_with_skipping(int,int);
/*
Debugging helper
*/

void print_config() {
    prt("Current Configuration Settings:\n");

    #define X(name) prt(#name ": %.*s\n", len(state->name), state->name.buf);
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

void print_config();
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
Here we use "Version: $VERSION$" and the dollar-delimited variable-looking thing is replaced by a build step.)

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
            prt("Version: $VERSION$\n");
            exit_success();
        }
    }

    parse_config();

    if (print_conf) {
        print_config();
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

/* #block_sanity_check
 
In block_sanity_check, we are given a file span and blocks spans, and we check certain invariants.

First we handle the special case where the file is empty, in this case there must be exactly one empty block and nothing else, and we early exit.
In all other cases, blocks are never empty.

Then we ensure with a simple loop, that all the blocks returned together tile the file, and that none are empty.
This means:
The first block begins where our input span begins.
The last block ends where our input ends.
If either of these conditions fails, the sanity test fails.
For every other block, .buf is equal to the .end of the previous.
No block is empty (i.e. len() > 0 in every case).
If this sanity check fails, we complain and crash as usual (prt, flush, exit).
*/

void block_sanity_check(span file, spans blocks) {
    if (empty(file)) {
        if (blocks.n != 1 || !empty(blocks.s[0])) {
            prt("Error: Empty file must have exactly one empty block.\n");
            flush();
            exit(EXIT_FAILURE);
        }
        return; // Early exit for empty file
    }

    // Check if the first block begins where the input span begins
    if (blocks.s[0].buf != file.buf) {
        prt("Error: The first block does not start where input begins.\n");
        flush();
        exit(EXIT_FAILURE);
    }

    // Check if the last block ends where the input ends
    if (blocks.s[blocks.n - 1].end != file.end) {
        prt("Error: The last block does not end where input ends.\n");
        flush();
        exit(EXIT_FAILURE);
    }

    // Ensure all blocks tile the file and none are empty
    for (int i = 1; i < blocks.n; ++i) {
        if (blocks.s[i].buf != blocks.s[i - 1].end || empty(blocks.s[i])) {
            prt("Error: Blocks do not properly tile the file or a block is empty.\n");
            flush();
            exit(EXIT_FAILURE);
        }
    }
}

/*
In find_all_blocks, we find the blocks in each file.

We are going to need to know how many blocks there are in all the files, and to store all of the blocks so that we can allocate a single spans to hold all of them at the end.
Therefore we first allocate an array of the `spans` type, one per file.

Then for each of the projfiles, we call find_blocks_by_type() to find the blocks in that file, and stash those temporarily on our array.
We also call block_sanity_check on the returned blocks for each file.

Once we have all of the blocks for each of the files, we then can construct a new spans (using spans_alloc) of the right size, and we can copy all the blocks into this, which we store on the state.
(We don't store the blocks per file anywhere, since we can always determine which file a block belongs to by comparing the block's span with the contents span on the projfile.)
*/

spans find_blocks_by_type_python(span);
spans find_blocks_by_type_c(span);
spans find_blocks_by_type(span, span);

void find_all_blocks() {
    spans* file_blocks = (spans*)malloc(state->files.n * sizeof(spans));
    if (!file_blocks) {
        perror("Failed to allocate memory for file_blocks");
        exit(EXIT_FAILURE);
    }

    size_t total_blocks = 0;
    for (int i = 0; i < state->files.n; ++i) {
        file_blocks[i] = find_blocks_by_type(state->files.a[i].contents, state->files.a[i].language);
        block_sanity_check(state->files.a[i].contents, file_blocks[i]);
        total_blocks += file_blocks[i].n;
    }

    spans all_blocks = spans_alloc(total_blocks);
    size_t index = 0;
    for (int i = 0; i < state->files.n; ++i) {
        for (int j = 0; j < file_blocks[i].n; ++j) {
            all_blocks.s[index++] = file_blocks[i].s[j];
        }
    }

    state->blocks = all_blocks;
    free(file_blocks);
}
/*
In get_code, we get the code into the input buffer.

For each of the projfiles:

- we read this file into inp by read_file_S_into_span with inp_compl() as the span argument, always advancing inp as usual in this pattern so we don't overwrite the contents
- we store the contents on the projfile

Then we call find_all_blocks(), which sets up the blocks according to each file's contents and language.
*/

void get_code() {
    for (int i = 0; i < state->files.n; i++) {
        state->files.a[i].contents = read_file_S_into_span(state->files.a[i].path, inp_compl());
        inp.end = state->files.a[i].contents.end; // Advance inp to not overwrite contents
    }

    find_all_blocks();
}
/*
In find_blocks_by_type_python, we get a span containing a file.

We write two loops.
In the first one we count the blocks, then we spans_alloc our return value with the correct number, and in the second loop we assign the spans.

For a Python file, a block starts with triple double-quote at the beginning of a line.
It contains another triple double-quote at the beginning of a line somewhere in the middle of the block, ending the comment part, which we must skip over.
It ends where the next block starts (true for all blocks regardless of language).

There are some special cases:

If the file is empty, we return a single empty block.

If a file does not begin with the triple quote, then the first block will just be from the beginning of the file to the second triple quote at the beginning of a line.

The last block always goes to the end of the file.

Before the first loop we copy our argument into a new span `copy` so that we can consume it while counting, but then still have access to the original full span for the second loop.

The block-finding loop:
- While the remaining input is not empty, we get the next line. (Note that next_line will return everything left if there is no newline).
- If this line starts with the pattern, or if it is the first line in the file, then it begins a block (regardless of whether the line is empty), otherwise we simply do nothing and continue the loop. (If it starts with the pattern and we have seen an odd number of them so far, then we skip it because the pattern appears two times in each Python block.)
- If we are counting blocks, we increment our counter, otherwise we assign .buf of a span for this block, and if we had a previous block, we assign .end of that block to the same offset.
- When we reach the end of the input we will assign .end of the last block to the end of the input.

(To determine if we are at the start of the input, we can compare .buf of the line with that of the input.)

At the end of the function, we ensure with a simple loop, that all the blocks returned together tile the file, and that none are empty.
This means:
The first block begins where our input span begins.
The last block ends where our input ends.
If either of these conditions fails, the sanity test fails.
For every other block, .buf is equal to the .end of the previous.
If this sanity check fails, we complain and crash as usual (prt, flush, exit).
*/


spans find_blocks_by_type_python(span file) {
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
            blocks.s[index].buf = line.buf;
            previous_block = &blocks.s[index++];
        }
    }
    if (previous_block != NULL) {
        previous_block->end = file.end;
    }

    // Sanity check
    for (int i = 0; i < blocks.n; ++i) {
        if (i == 0 && blocks.s[i].buf != file.buf) {
            prt("Error: First block does not start where input begins.\n");
            flush();
            exit(EXIT_FAILURE);
        }
        if (i == blocks.n - 1 && blocks.s[i].end != file.end) {
            prt("Error: Last block does not end where input ends.\n");
            flush();
            exit(EXIT_FAILURE);
        }
        if (i > 0 && blocks.s[i].buf != blocks.s[i - 1].end) {
            prt("Error: Block start does not match previous block end.\n");
            flush();
            exit(EXIT_FAILURE);
        }
    }

    return blocks;
}
/*
In find_blocks_by_type_c, we get a span containing a file.

We write two loops.
In the first one we count the blocks, then we spans_alloc our return value with the correct number, and in the second loop we assign the spans.

For a C file, a block starts with the slash-star pattern at the beginning of a line.
It ends where the next block starts.

There are some special cases:

If the file is empty, we return a single empty block.
We handle this as a special case, since it doesn't really work with our two-loop approach.
We also care about where the blocks are, even if they are empty, so in this case we ensure that the empty block we return is the same as the empty span of the file itself (i.e. .buf and .end are equal to each other, and to those of the file: we can NOT use nullspan() here).

If a file does not begin with a block comment, then the first block will just be from the beginning of the file to the first block comment.

The last block always goes to the end of the file.

Before the first loop we copy our argument into a new span `copy` so that we can consume it while counting, but then still have access to the original full span for the second loop.

The block-finding loop:
- While the remaining input is not empty, we get the next line. (Note that next_line will return everything left if there is no newline).
- If this line starts with the pattern, or if it is the first line in the file, then it begins a block (regardless of whether the line is empty), otherwise we simply do nothing and continue the loop.
- If we are counting blocks, we increment our counter, otherwise we assign .buf of a span for this block, and if we had a previous block, we assign .end of that block to the same offset.
- When we reach the end of the input we will assign .end of the last block to the end of the input.

(To determine if we are at the start of the input, we can compare .buf of the line with that of the input.)
*/

spans find_blocks_by_type_c(span file) {
    if (empty(file)) {
        // Handle special case for empty file
        spans single_empty_block = spans_alloc(1);
        single_empty_block.s[0].buf = file.buf;
        single_empty_block.s[0].end = file.end;
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
            blocks.s[index].buf = line.buf;
            previous_block = &blocks.s[index++];
            is_first_line = 0;
        }
    }
    if (previous_block != NULL) {
        previous_block->end = file.end;
    }

    return blocks;
}
/*
In `spans find_blocks_by_type(span,span)`, we take a span and a language, which is currently either C or Python.

We dispatch to another function that handles either language, and return the resulting spans.

If the language is not known, we prt, flush, exit as per usual.
*/

spans find_blocks_by_type(span source, span language) {
    if (span_eq(language, S("C"))) {
        return find_blocks_by_type_c(source);
    } else if (span_eq(language, S("Python"))) {
        return find_blocks_by_type_python(source);
    } else {
        prt("Error: Unknown language\n");
        flush();
        exit(EXIT_FAILURE);
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
In main_loop, we initialize:

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
/* span count_physical_lines(span, int*)

In this function we are given a span and a (pointer to a) maximum number of physical lines to print or count.

The span may be of arbitrary length, which is why we do not simply count the lines in the span; we do not want unbounded runtime.

Definitions:
A logical line is an actual line terminated by newline or by the end of the span.
A physical line is a row of terminal output, which is reached either when terminal_cols characters of output reaches the end of the line, causing wrapping, or when a literal newline is printed (or both).
The physical line is defined such that after printing a physical line (starting at the beginning of a terminal row), the next character printed to the terminal will appear on the subsequent terminal row.

We begin counting physical lines off of the input span according to this definition.
Specifically, we count off either terminal_cols chars, followed by a newline (which will have no effect if it comes after exactly that many chars, i.e. after counting off terminal_cols chars, we need to check if the *next* char is a newline, because if it is we should skip over it), or we count off a shorter line followed by an actual newline.

We decrement the int of remaining lines each time we have counted off a physical line.
Our return value is the span of physical lines that we have counted off.
The int will go to zero if the span passed in is long enough, otherwise the span returned will contain the entire input and the int may remain positive.

(The intended typical use of this function is to provide a span and a desired number of physical lines of terminal space to occupy, then to print the returned span, and perhaps to prepare a suffix of the original span to keep printing, and perhaps to use the int to track remaining lines of terminal output to fill or similar.)

(Note: I don't think the below is correct when a logical line has exactly terminal_cols chars in it, however I don't want to iterate further or test right now.)
*/

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
    int lines_to_skip = state->scrolled_lines;
    int content_rows = state->terminal_rows - 2;
    span block_copy = state->blocks.s[state->current_index];
    span scrolled_off = count_physical_lines(block_copy, &lines_to_skip);

    block_copy.buf = scrolled_off.end;
    int lines_for_screen = content_rows;
    span remainder = count_physical_lines(block_copy, &lines_for_screen);

    if (lines_for_screen > 0) {
        state->scrolled_lines -= lines_for_screen;
    } else {
        state->scrolled_lines += content_rows;
        lines_to_skip = state->scrolled_lines;
        block_copy = state->blocks.s[state->current_index];
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

void toggle_visual() {
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
This function will update the state directly.

If marked_index != -1 and != current_index, then we have a "visual" selected range of more than 1 block.

First we determine which of marked_index and current_index is lower and make that our block range start, and then one past the other is our block range end (considered as an exclusive endpoint).

The difference between the two is then the number of selected blocks.
At this point we know how many blocks we are displaying.
Finally we pass state, inclusive start, and exclusive end of range to another function handles rendering.

Helper functions:

- render_block_range(int,int) -- also supports rendering a single block (if range includes only one block).
*/

void get_screen_dimensions() {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  state->terminal_rows = w.ws_row;
  state->terminal_cols = w.ws_col;
}

void render_block_range(int, int);

void print_current_blocks() {
  get_screen_dimensions();

  if (state->marked_index != -1 && state->marked_index != state->current_index) {
    int start = state->current_index < state->marked_index ? state->current_index : state->marked_index;
    int end = state->current_index > state->marked_index ? state->current_index + 1 : state->marked_index + 1;

    render_block_range(start, end);
  } else {
    // Normal mode or visual mode with only one block selected.
    render_block_range(state->current_index, state->current_index + 1);
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
/* #handle_keystroke

In handle_keystroke, we support the following single-char inputs:

- j/k Go up or down one block. If we are at the first or last block, these are no-ops.
- g/G Go to the first or last block resp.
- e, Edit the current block in $EDITOR (or vi by default)
- r, Request an LLM rewrite the code part of the block based on the comment part; silently updates clipboard
- R, kin to "r", which reads current clipboard contents back into the block, replacing the code part
- space/b, paginate down or ("back") up within a block
- B, do a build by running the build command you provide
- v, sets the marked point to the current index, switching to "visual" selection mode, or leaves visual mode if in it
- /, switches to search mode
- S, (likely to change) goes into settings mode
- ?, display brief help about the keyboard shortcuts available
- q, exits (with prt("goodbye\n"); flush(); exit(0))

To get the help text we can basically copy the lines above, except formatted nicely for terminal output.
We split out j/k and g/G onto their own lines though.
Include all relevant details about usage that might be non-obvious, e.g.:
- r "puts a prompt on the clipboard to rewrite the code part based on comment part"
Include mnemonic hints where given (e.g. "back" for b).
We can call clear_display first, and flush, getch after, so the user has time to read the help (we prompt them about this).

We call terpri() on the first line of this function (just to separate output from any handler function from the ruler line).

Implemented inline: j,k,g,G,?,q
All others call helper functions already declared above (B -> compile()).
*/

void handle_keystroke(char input) {
    terpri();

    switch (input) {
        case 'j':
            if (state->current_index < state->blocks.n - 1) {
                state->current_index++;
                state->scrolled_lines = 0;
            }
            break;
        case 'k':
            if (state->current_index > 0) {
                state->current_index--;
                state->scrolled_lines = 0;
            }
            break;
        case 'g':
            state->current_index = 0;
            state->scrolled_lines = 0;
            break;
        case 'G':
            state->current_index = state->blocks.n - 1;
            state->scrolled_lines = 0;
            break;
        case 'e':
            edit_current_block();
            break;
        case 'r':
            rewrite_current_block_with_llm();
            break;
        case 'R':
            replace_code_clipboard();
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
            toggle_visual();
            break;
        case '/':
            start_search();
            break;
        case 'S':
            settings_mode();
            break;
        case '?':
            clear_display();
            prt("j/k: Move up/down one block.\n");
            prt("g/G: Go to the first/last block.\n");
            prt("e: Edit current block in $EDITOR (default: vi).\n");
            prt("r: Rewrite code part based on comment, puts prompt on clipboard.\n");
            prt("R: Read clipboard contents back into block, replacing code part.\n");
            prt("space/b: Paginate down/back up within a block.\n");
            prt("B: Execute build command.\n");
            prt("v: Mark current index, toggle visual selection mode.\n");
            prt("/: Enter search mode.\n");
            prt("S: Enter settings mode.\n");
            prt("?: Display this help.\n");
            prt("q: Exit (goodbye).\n");
            flush();
            prt("Press any key to return...\n");
            getch();
            break;
        case 'q':
            prt("goodbye\n");
            flush();
            exit(0);
            break;
        default:
            // Unhandled key
            break;
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

- perform_search()
- finalize_search()

OF COURSE, we use getch() which we carefully defined above, NEVER getchar().

We write declarations for the helper functions (which we define below).
*/

void perform_search();
void finalize_search();

void start_search() {
    static char search_buffer[256] = {"/"}; // Static buffer for search, pre-initialized with "/"
    state->search = (span){.buf = search_buffer, .end = search_buffer + 1}; // Initialize search span to contain just "/"

    perform_search(); // Perform initial search display/update

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

        perform_search(); // Update search results after each modification
    }

    finalize_search(); // Finalize search on Enter
}

/*
In perform_search(), we get the state after the search string has been updated.

The search string (span state.search) will always start with a slash.
We remove this (there is no library method for this so just directly construct the span) and take the rest of it as the actual string to search for.
We iterate through the blocks and use spanspan to find the first block that matches, along with the number of other blocks that match.
If the search span is empty (as when only "/" was typed) then we match every block, so we can use empty() on the result of spanspan to detect a match, but we also match if the search span is empty().
(This will store the empty span at the beginning of the first block as the match span, which gives the behavior we want when printing the match later.)
We store both the index of the first block that matched and a copy of the span given by spanspan for this first block only, as we will need both of them later.

This tells us where the first block was that matched, and how many total blocks matched, and also the location in the span that contains the match.

Once we have our search results, we call clear_display().

Also at the top, we declare a local variable of remaining lines from the top of the terminal window, since we want to print something at the bottom later.
Every time we print a line or multiple lines, we decrement this value with the number of lines we printed, no more and no less.
In particular, we never decrement this "in advance," for lines to printed later, as that would violate the invariant that the number of actual remaining lines is the number in our variable.

Next, we print "Block N:" on a line (adding one as usual).
Then we decide how many initial lines of the block to print.
Without changing the terminal lines remaining value (since we indeed want our last line to be on the last line) we subtract 8 from this remaining number (leaving space for other output) and divide this by two to get the number of initial block physical lines to print.
Then we print this many physical lines of the block, by calling a function print_physical_lines(span,int).
(Note: physical lines means terminal rows used, as opposed to logical lines actually ending in newline.)

If nothing matched, then we do not print this part, but we still print the "N blocks matched" part later (0 in that case, of course) and the search string itself on the last line.

After the first lines of the block, we print a blank line, then "Match:" on a line, and then the line that contains the matched span.
We call a helper function, print_matching_physical_lines, which takes the block and the actual matched span (which we have from before), and handles finding and printing the match, and returns the number of physical lines that it used.

We print another blank line and then "N blocks matched".

Finally, we will add empty lines until we are at the bottom of the screen as indicated by terminal_rows on the state.
On the last line we will print the entire search string (including the slash).
(We can do this with wrs(), we don't need to add a newline as we are already on the last line of the window anyway.)
Before returning from the function we call flush() as we are responsible for updating the display.
*/

void perform_search() {
    int remaining_lines = state->terminal_rows;
    span search_span = {state->search.buf + 1, state->search.end};
    int match_count = 0;
    int first_match_index = -1;
    span first_match_span = nullspan();

    for (int i = 0; i < state->blocks.n; i++) {
        span match = spanspan(state->blocks.s[i], search_span);
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
        print_physical_lines(state->blocks.s[first_match_index], initial_lines_to_print);
        remaining_lines -= initial_lines_to_print;

        prt("\n");
        remaining_lines -= 1;

        prt("Match:\n");
        remaining_lines -= 1;

        int lines_printed = print_matching_physical_lines(state->blocks.s[first_match_index], first_match_span);
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
In print_ruler we use prt to show

- the number of blocks,
- the currently selected block,
- the scrolled lines plus one (i.e. the one-based index of the top visible line)

all on a line without a newline.
*/

void print_ruler() {
    prt("%d blocks, Block %d, Line %d", state->blocks.n, state->current_index + 1, state->scrolled_lines + 1);
}
/*
In print_single_block_with_skipping we get a block index and a pagination index in the form of a number of lines already "scrolled off" above the top of the screen (skipped_lines).

First, we call count_physical_lines, which gives us a span of skipped lines and alters an int, subtracting the number of physical lines which this span represents.
We make a copy of the block and adjust this copy to the suffix which is meant to be aligned to the top of our content area, by setting the .buf of the copy to the .end of the scrolled-off span.

We set a variable remaining_rows which we initialize with state->terminal_rows and decrement as we print lines of output.
First we print a line "Block N" and decrement this variable by one.

We have a ruler line at the bottom that we need to leave room for, so we make another variable, remaining_content_lines, that is one less than remaining lines, and call count_physical_lines again with this variable, letting us determine how many lines are actually printed, and more importantly, giving us a span of the appropriate content to at-most fill the screen.

We then print this content by wrs().
Note that the int passed by reference into count_physical_lines will be DECREMENTED by the number of actual physical lines of content in the returned span.
Therefore, the value of this variable after the call is the number of REMAINING lines of content area yet to be filled, thus, while this remains positive, we print blank lines, filling the content area.
Finally, we call print_ruler to handle the last line of the terminal.
*/

void print_single_block_with_skipping(int block_index, int skipped_lines) {
    span block = state->blocks.s[block_index];
    int remaining_rows = state->terminal_rows;

    span skipped_span = count_physical_lines(block, &skipped_lines);
    block.buf = skipped_span.end;

    prt("Block %d\n", block_index + 1);
    remaining_rows--;

    int remaining_content_lines = remaining_rows - 1;
    span content_to_print = count_physical_lines(block, &remaining_content_lines);

    wrs(content_to_print);
    remaining_rows -= (state->terminal_rows - 1 - remaining_content_lines);

    while (remaining_rows > 1) {
        terpri();
        remaining_rows--;
    }

    print_ruler();
}
/*
In print_physical_lines, we get a span, and a number of lines.
We can use next_line on the span to get each logical line, and then we use the terminal_cols on the state to determine the number of physical lines that each one will require.
However, note that a blank line will still require one physical line (because we will print an empty line).
If the physical lines would be more than we need, then we only print enough characters (with wrapping) to fill the lines.
Otherwise we print the full line, followed by a newline, and then we decrement the number of lines that we still need appropriately.
*/


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
/*
In print_matching_physical_lines, we get the span of a block and of a match.

We must print the physical lines from the block which contain the match, and then return the number of physical lines that we have printed.

We happen to know, because of how our search currently works, that a match will never cross a logical line boundary (since we don't have a way of entering newlines in the search mode).

We will loop over the logical lines using next_line as usual.

When we find the logical line that contains the match (which we can do by comparing .buf and .end), then we determine where to start printing the physical lines.
(Note: we compare .buf and .end because we already have found a match, we do not use contains() which would search the strings again!)

(A logical line contains physical lines, which are runs of characters that fill the terminal_cols (on state), and therefore cause wrapping.)

Specifically, we want to skip any physical lines at the start of the logical line until we get to the first physical line that contains the match.
Then we print this physical line, and keep printing physical lines until we have reached the end of the match.
Whether or not we have printed the full logical line, we then print a newline.

We return the number of physical lines that we have printed.
*/

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
/*
In finalize_search(), we update the current_index to point to the first result of the search given in the search string.
We ignore the first character of state.search which is always slash, and find the first block which contains the rest of the search string (using contains()).
Then we set current_index to that block, also resetting scrolled_lines.
We then reset state.search to an empty span to indicate that we are not in search mode any more.
Finally we call print_current_blocks to refresh the display given the block that is now the current one (replacing the search screen).
*/


void finalize_search() {
    span search_span = {state->search.buf + 1, state->search.end}; // Ignore the leading slash

    for (int i = 0; i < state->blocks.n; i++) {
        if (contains(state->blocks.s[i], search_span)) {
            state->current_index = i; // Update current_index to the first match
            state->scrolled_lines = 0;
            break; // Exit the loop once the first match is found
        }
    }

    state->search = nullspan(); // Reset search to indicate exit from search mode
    //print_current_blocks(); // Refresh the display
}

/* Settings.

When the tool starts, we ask about specific configuration settings that must exist.
Otherwise, for settings like the buildcmd we only ask the first time the feature is used.

As settings are changed, we write a configuration file, which is always .cmpr/conf in the current working directory.

The contents of this file will be similar to RFC822-style headers.
We will have a key name followed by colon, space and then a value to the end of the line.

We have functions to read and write this format, which we do in the cmp space.

In handle_conf_{language,file} we are called with a span in each case containing either a pathname or a language, in the order that these config keys occur in the config file.

The idea is that when a language key occurs, it sets the language which is then used on subsequent files.
However, as the one exception to this pattern, when files are included before any language is given, then we will add the files without a language.

Therefore:

- when we see a language line, we will set the current_language on the state
- when we see a file line, we set the language on the file to current_language and add the file to the state
- only in the case where we see a language line and the language was not already set, then we know that this is the first language line in the file; in this case we iterate over any files that we already added, and set the language on them.

Below we write both functions handle_conf_{language,file}.
*/

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

/*
In the first function, parse_config, we read the contents of our config file (at state->config_file_path) into the cmp space, parse it, and set on the ui_state all the appropriate values.

We have a library method cmp_compl() which gives us the complement of cmp in cmp_space, which is the space that we can safely read into.
We'll read our configuration file into that space with read_file_S_into_span().

After we read the file into cmp, we get a span back from our library method containing the file contents.
We still have to manually update cmp.end to match the end of this span so that later uses of the cmp space don't clobber our configs.

We then can use next_line() in a loop to process them one by one.
First we look for ":" with find_char, and if it is not found, we skip the line.
After the colon we'll skip any whitespace with an isspace() loop and then consider the rest of the line to be the value.
(Note that we don't trim whitespace off the end, something maybe worth documenting elsewhere for config file users).
Next_line has already stripped the newline from the end so we don't need to do that.

Next, we compare the key part before the colon with our list of configuration values above, and if it matches any of them, we set the corresponding member of the ui state, which is always named the same as the config key name.
The values of the config keys will always be spans.
We have CONFIG_FIELDS defined above, we use that here with an X macro.

However, there is an exception to the handling of config fields for some values that might be handled specially.
These are not included in our CONFIG_FIELDS, but instead we handle them with custom code here.
These are:

- language
- file

These are handled by custom code, so we have functions handle_conf_{language,file} (already written above) that we call with the value span for either of these each time they occur in the config file.
*/

void parse_config() {
    span cmp_free_space = cmp_compl();
    span config_content = read_file_S_into_span(state->config_file_path, cmp_free_space);
    cmp.end = config_content.end; // Update cmp to avoid overwriting config

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
            handle_conf_language(value);
        } else if (span_eq(key, S("file"))) {
            handle_conf_file(value);
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
}

void settings_mode(){}
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
/* save_conf_files()

Here we write the language and file lines into the conf file.

The structure of this data in the conf file is that a language line should be included every time the language of the next file is different from the previous file.
For example, if there are three files, with languages C, Python, Python, then we would have a language line of C, then the first file, then language Python and both the other files.

Therefore, we maintain a local variable indicating the last-written language, initially empty of course.
For each file, if the language is already equal to this, then we just print the file line, otherwise we print a language line first.

As mentioned elsewhere, each conf line includes the key, a colon, space and the value, followed by newline.
*/

void save_conf_files() {
    span last_written_language = nullspan();
    for (int i = 0; i < state->files.n; i++) {
        if (!span_eq(last_written_language, state->files.a[i].language)) {
            last_written_language = state->files.a[i].language;
            prt("language: %.*s\n", len(last_written_language), last_written_language.buf);
        }
        prt("file: %.*s\n", len(state->files.a[i].path), state->files.a[i].path.buf);
    }
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
Our X macro handles the "normal" config fields, but then we call another function, save_conf_files, that handles the file and language lines that are special.
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

revdir: we save a revision every time you edit a block, this is where the revisions will be stored (.cmpr/revs is a good choice).
tmpdir: we store a copy of a block for you to edit in your editor with the "e" key, this is where those live, can be .cmpr/tmp or a regular tmpdir like /tmp.

TODO: probably all the above "config-related metadata" should be in one place in a table for all the conf vars; we'll do that later.
For now, a local macro cleans up this code quite a bit.

For each missing variable in the order given, we print a suitable message explaining what it is and why it is required (inspired by the above).
We try to include all the relevant information from the above about what the conf var is and why it's required, but fitting the explanation in a sentence or two at most. 

We then call read_line to get the new value from the user.
For the buffer space to use we will first call cmp_compl() to get the complement of cmp space as a span.
After read_line returns we will always set cmp.end to point to the end of the returned span; this makes sure nothing else uses that space later.

Additional to the "normal" conf vars, we also have the language setting for each file.
If there is a file in projfiles that doesn't have a language set, then we tell the user that this is required and set the config on the projfile in this case.
We tell them that the language must be either "Python" or "C", and determines how blocks start and also where the comment part ends and code part starts.
For Python, both are triple-quote, for C the block comment syntax is used.

Once we have set all the required conf vars, if any of them were missing, including any languages on the projfiles, then we call save_conf() which just rewrites the conf file.

TODO: use the ensure_conf_var thing that we added after this
*/



void check_conf_vars() {
    int missing_vars = 0;

    #define CHECK_AND_PROMPT(var, msg) \
        if (empty(state->var)) { \
            printf(#var ": " msg "\n"); \
            span cmp_space = cmp_compl(); \
            read_line(&cmp_space); \
            state->var = cmp_space; \
            cmp.end = cmp_space.end; \
            missing_vars = 1; \
        }

    CHECK_AND_PROMPT(revdir, "Revision directory required (.cmpr/revs recommended). Stores block revisions.");
    CHECK_AND_PROMPT(tmpdir, "Temporary directory required (.cmpr/tmp or /tmp recommended). Stores temporary block files.");

    #undef CHECK_AND_PROMPT

    for (int i = 0; i < state->files.n; i++) {
        if (empty(state->files.a[i].language)) {
            printf("Language for file %.*s is required. Must be \"Python\" or \"C\".\n", len(state->files.a[i].path), state->files.a[i].path.buf);
            span cmp_space = cmp_compl();
            read_line(&cmp_space);
            state->files.a[i].language = cmp_space;
            cmp.end = cmp_space.end;
            missing_vars = 1;
        }
    }

    if (missing_vars) {
        save_conf();
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

char* tmp_filename();
int launch_editor(char* filename);
void handle_edited_file(char* filename);

void edit_current_block() {
  if (state->current_index >= 0 && state->current_index < state->blocks.n) {
    // Get a temporary filename based on the current UI state
    char* filename = tmp_filename();

    // Write the current block to the temporary file
    write_to_file(state->blocks.s[state->current_index], filename);

    // Launch the editor on the temporary file
    int editor_exit_code = launch_editor(filename);

    // Check if the editor exited normally
    if (editor_exit_code == 0) {
      // Handle the edited file (read changes, update the block, etc.)
      handle_edited_file(filename);
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
We append a file extension: we use file_for_block and current_block to get the language for the current block and if it's C we add ".c" and if it's Python we add ".py" to the filename; otherwise we don't add anything.
We return a char* which the caller must free.
Since state->tmpdir may or may not include a trailing slash we test for and handle both cases.
*/

int file_for_block(span);

char* tmp_filename() {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[16]; // Enough to hold YYYYMMDD-HHMMSS
    strftime(time_str, sizeof(time_str), "%Y%m%d-%H%M%S", tm);

    int file_index = file_for_block(state->blocks.s[state->current_index]);
    char* extension = "";
    if (span_eq(state->files.a[file_index].language, S("C"))) {
        extension = ".c";
    } else if (span_eq(state->files.a[file_index].language, S("Python"))) {
        extension = ".py";
    }

    int need_slash = state->tmpdir.end[-1] != '/';
    int total_length = (state->tmpdir.end - state->tmpdir.buf) + strlen(time_str) + strlen(extension) + 1 + need_slash;
    char* filename = malloc(total_length);
    if (!filename) {
        perror("Failed to allocate memory for filename");
        exit(EXIT_FAILURE);
    }

    snprintf(filename, total_length, "%.*s%s%s%s", 
             (int)(state->tmpdir.end - state->tmpdir.buf), state->tmpdir.buf, 
             need_slash ? "/" : "", 
             time_str, 
             extension);

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
/* int file_for_block(span)

Here we're given the span of a block and we must find out the index of the file that contains that block.

If .buf of the span is >= .buf of the contents of the file (recall that file contents are spans into inp and so are blocks), and the .end of the block is similarly <= .end of the file, then the file contains the block.

If the block overlaps a file boundary, something has gone badly wrong and we complain as exit as usual (prt, flush, exit).

Similarly if it's outside all the files.
In short, we always return an int n, 0 <= n <= state->files.n, or don't return at all.
*/

int file_for_block(span block) {
    for (int i = 0; i < state->files.n; i++) {
        if (block.buf >= state->files.a[i].contents.buf && block.end <= state->files.a[i].contents.end) {
            return i;
        }
    }
    prt("Error: Block does not belong to any file.\n");
    flush();
    exit(EXIT_FAILURE);
}
/* handle_edited_file

Here we are given a tmp file containing new contents of a block.

Every block belongs to a projfile, and the blocks are in order of the projfiles (set up in the conf file).

What we must do is replace the existing block contents with the new contents of that tmp file, and then write everything out to a new file on disk called a rev for the particular projfile that contains that block.

We already have on state all the current values of the blocks, which are spans that point into inp.
First we want to fix inp to reflect the new reality, and then we will write out the new disk file from inp (our global input span).
We will also fix the contents spans of all of the files starting with this one, and including all later ones.

We get the span corresponding to the current block, which is also the edited block, in a local variable for convenience.
We can do the same for the .contents of the file, calling file_for_block to get the index.

The contents of inp are already correct, up to the start of this block, which is what we want to replace.
Now we get the size of the tmp file and compare it to the len of the existing block (recall that len(span) exists and is one of our most commonly used library methods).
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

However, the file[i].contents for the files including and subsequent to this one may be incorrect.
Specifically, the difference in length of this block must be added to the .end of the file contents span for this file, and to both the .buf and .end of every subsequent file.

As a sanity check, after this step, we could validate that the .end of the last file is equal to the .end of inp itself.

We need to update the blocks, since any blocks after and including this one may have moved, so we call find_all_blocks().

Then we call a helper function, new_rev, which takes the filename and the file index for the projfile that was altered.
This function is responsible for storing a new rev, cleaning up the tmp file, and any reporting to the user that we might do.
We include a forward declaration here for new_rev.

Relevant helper functions:
void new_rev(char*,int);
*/

void new_rev(char*, int);

void handle_edited_file(char* filename) {
    int file_index = file_for_block(state->blocks.s[state->current_index]);
    span original_block = state->blocks.s[state->current_index];
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        prt("Error: Failed to open edited file.\n");
        flush();
        exit(EXIT_FAILURE);
    }
    off_t new_size = lseek(fd, 0, SEEK_END);
    ssize_t size_diff = new_size - (original_block.end - original_block.buf);
    close(fd);

    // Adjusting the memory in inp for new content size
    memmove(original_block.buf + new_size, original_block.end, inp.end - original_block.end);
    inp.end += size_diff;

    // Reading the new block content into the adjusted space in inp
    span gap = {original_block.buf, original_block.buf + new_size};
    span result = read_file_into_span(filename, gap);
    if (len(result) != new_size) {
        prt("Error: Unexpected file size after reading edited content.\n");
        flush();
        exit(EXIT_FAILURE);
    }

    // Adjusting file contents spans for this and subsequent files
    state->files.a[file_index].contents.end += size_diff;
    for (int i = file_index + 1; i < state->files.n; ++i) {
        state->files.a[i].contents.buf += size_diff;
        state->files.a[i].contents.end += size_diff;
    }

    // Sanity check
    if (state->files.a[state->files.n - 1].contents.end != inp.end) {
        prt("Error: Inconsistent state after updating file contents.\n");
        flush();
        exit(EXIT_FAILURE);
    }

    // Updating the blocks representation
    find_all_blocks();

    // Creating a new revision and cleaning up
    new_rev(filename, file_index);
}

void update_link(char* new_filename);
/*
Here we store a new revision, given a filename which contains a block that was edited and the index of the projfile that contains that block.
The file contents have already been processed, we just get the name so that we can clean up the file when done.

Our conf var revdir tells us where to put the new file that we are going to write.
We don't assume that the conf var ends in a slash, so we test for that and handle both cases.

First we construct a path starting with revdir and ending with an ISO 8601-style compact timestamp like 20240501-210759.
We then write the contents of the projfile (which has already been updated) into this file (the "rev") using write_to_file().

We create a copy of this file at the path for the projfile.
The file there which may have been edited and contain unsaved changes by some other process.
So we have a helper function, update_projfile, which takes the projfile index, the path of the rev file, and handles all of this.

Finally we unlink the filename that was passed in, since we have now fully processed it.
The filename is now optional, since we sometimes also are processing clipboard input, so if it is NULL we skip this step.

Reminder: we never write `const` in C as it only brings pain.
*/

void update_projfile(int, char*);

void new_rev(char* filename, int file_index) {
    char rev_path[1024];
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    int need_slash = state->revdir.end[-1] != '/';
    snprintf(rev_path, sizeof(rev_path), "%.*s%s%04d%02d%02d-%02d%02d%02d", 
             (int)(state->revdir.end - state->revdir.buf), state->revdir.buf,
             need_slash ? "/" : "",
             tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
             tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);

    write_to_file(state->files.a[file_index].contents, rev_path);

    update_projfile(file_index, rev_path);

    if (filename) {
        unlink(filename);
    }
}

void update_projfile(int file_index, char* rev_path) {
    char projfile_path[1024];
    snprintf(projfile_path, sizeof(projfile_path), "%.*s", 
             (int)(state->files.a[file_index].path.end - state->files.a[file_index].path.buf), 
             state->files.a[file_index].path.buf);

    // Check if the projfile exists and handle accordingly
    struct stat statbuf;
    if (lstat(projfile_path, &statbuf) == 0) {
        if (S_ISREG(statbuf.st_mode)) {
            // If it's a regular file, we might want to back it up before overwriting
            char backup_path[1024];
            snprintf(backup_path, sizeof(backup_path), "%s.bak", projfile_path);
            rename(projfile_path, backup_path);
        }
    }

    // Create or overwrite the projfile with the new revision
    link(rev_path, projfile_path);
}
/*
SH_FN_START

update_symlink() {
  rm -r cmpr/cmpr.c; ln -s $(cd cmpr; find revs | sort | tail -n1; ) cmpr/cmpr.c
}

SH_FN_END
*/

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

Then we dispatch based on the language on the projfile which we lookup using file_for_block().
If this span contains "Python" we call the Python version, and otherwise we call the C version (which therefore is our default).

The helper function takes a span and returns an index offset to the location of the "block comment part terminator".
For Python this is the second occurrence of the triple doublequote in the block, and for C it is star slash.

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
    int quote_count = 0;
    for (int i = 0; i < len(block) - 2; ++i) {
        if (block.buf[i] == '"' && block.buf[i + 1] == '"' && block.buf[i + 2] == '"') {
            quote_count++;
            if (quote_count == 2) { // Second occurrence
                return i + 3; // Include the """ in the index
            }
            i += 2; // Skip past this quote
        }
    }
    return len(block);
}

span block_comment_part(span block) {
    int file_index = file_for_block(block);
    span language = state->files.a[file_index].language;
    int index;

    if (span_eq(language, S("Python"))) {
        index = find_block_comment_end_python(block);
    } else { // Default to C
        index = find_block_comment_end_c(block);
    }

    // Include any newlines and whitespace after the terminator
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
Finally we write "```\n\n" and an instruction, which is usually "Write the code. Reply only with code. Do not include comments.".

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
  int index = file_for_block(comment);
  span lang = state->files.a[index].language;
  if (span_eq(lang, S("Python"))) {
    prt("```python\n");
    wrs(comment);
    prt("```\n\nWrite the code. Reply only with code. Avoid using the code_interpreter.\n");
  } else {
    prt("```c\n"); // Begin code block
    wrs(comment);  // Write the comment span
    prt("```\n\nWrite the code. Reply only with code. Do not include comments.\n"); // End code block and add instruction
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
(Remember to call flush() before getch() so the user sees the prompt (which is "Build failed, press any key to continue...").)

On the other hand, if the build succeeds, we don't need the extra keystroke and go back to the main loop after a 1s delay so the user has time to read the success message before the main loop refreshes the current block.
In this case we prt "Build succeeded" on a line.

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
        prt("Build failed, press any key to continue...\n");
        flush();
        getch();
    } else {
        prt("Build succeeded\n");
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

void replace_code_clipboard() {
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
/* #replace_block_code_part(span)

Here we get a span (into cmp space) which contains a new code part for the current block.

Note: To get the length of a span, use len().
Note: Do this every time, not the just the first time.

Similar to handle_edited_file() above, we are given a span (instead of a file) and we must update the current block, moving data around in memory as necessary.

We put the original block's span in a local variable for convenience.

The end result of inp should contain:
- the contents of inp currently, up to the start (the .buf) of the original block.
- the comment part of the current block, which we can get from block_comment_part on the current block.
- up to two newlines unless the block comment part already ends with them
- the code part coming from the clipboard in our second argument
- current contents of inp from the .end of the original block to the inp.end of original input.

So, first we check whether we are adding one, zero, or two newlines, being careful about SEGV.
We get the index of the projfile from file_for_block.
Then we check the length of what the new block will be (comment part + opt. newlines + new part).
We then compare this to the old block length and do a memmove if necessary on the "rest" of inp, so that we have a gap to accommodate the new block's len.
Then we simply copy any newlines and the new code into inp.
(We do not need to copy the comment part, as it is already there in the original block.)

We then must update the .end of the current file contents, and both the .buf and .end of all subsequent projfiles, since the block length may have changed and therefore the file contents lengths will have also changed.

As before we then find the current locations of the blocks.

Once all this is done, we call new_rev, passing NULL for the filename argument, since there's no filename here.
*/

void replace_block_code_part(span new_code) {
    int file_index = file_for_block(state->blocks.s[state->current_index]);
    span original_block = state->blocks.s[state->current_index];
    span comment_part = block_comment_part(original_block);

    int newlines_needed = 2;
    if (len(comment_part) > 0 && comment_part.end[-1] == '\n') {
        newlines_needed = 1;
        if (comment_part.end - comment_part.buf > 1 && comment_part.end[-2] == '\n') {
            newlines_needed = 0;
        }
    }

    size_t new_block_length = len(comment_part) + newlines_needed + len(new_code);
    ssize_t size_diff = new_block_length - (original_block.end - original_block.buf);

    if (size_diff != 0) {
        memmove(original_block.end + size_diff, original_block.end, inp.end - original_block.end);
    }

    unsigned char* current_pos = original_block.buf + len(comment_part);
    for (int i = 0; i < newlines_needed; ++i) {
        *current_pos++ = '\n';
    }

    memcpy(current_pos, new_code.buf, len(new_code));

    // Update inp.end to reflect the new size
    inp.end += size_diff;

    // Update the contents span of the current and subsequent files
    state->files.a[file_index].contents.end += size_diff;
    for (int i = file_index + 1; i < state->files.n; ++i) {
        state->files.a[i].contents.buf += size_diff;
        state->files.a[i].contents.end += size_diff;
    }

    // Re-find all the blocks since inp has changed
    find_all_blocks();

    // Store a new revision, no filename required
    new_rev(NULL, file_index);
}
/* cmpr_init
We are called without args and set up some configuration and empty directories to prepare the CWD for use as a cmpr project.

In sh terms:

- mkdir -p .cmpr/{,revs,tmp}
- touch .cmpr/conf

Note that if the CWD is already initialized this is a no-op i.e. the init is idempotent (up to file access times and similar).
*/

void cmpr_init() {
    mkdir(".cmpr", 0755);
    mkdir(".cmpr/revs", 0755);
    mkdir(".cmpr/tmp", 0755);

    FILE *file = fopen(".cmpr/conf", "a");
    if (file != NULL) {
        fclose(file);
    }
}
