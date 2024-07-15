/* #source_intro

Source code introduction
----

The cmpr source is organized into blocks.

Each block starts with a block comment, which is often followed by some code.

The blocks can be read in sequence from start to end inside this file.
*/

/* #example_block @a @b @c

Function:
int add(int a, int b)

Purpose:
Define a function that adds two integers.

Algorithm:
- Return the sum of integers a and b.
*/

int add(int a, int b) {
    return a + b;
}

/* #example_refs @test_block @config_fields

Here's a block that contains block references as an example of how these features work.

The id of this block itself is #test_refs, and the #test_block and #config_fields blocks are also pulled in as context.

These context blocks typically are used to provide context that is relevant to the current block.

The projfiles reference below is an inline reference and will be expanded inline.
It must be on a line by itself to count, so you can write @projfiles in a sentence, like this one, and it will not be replaced.
You can also use #hashtag syntax like we use above to refer to a block in NL text, and this will not be replaced either.
However, the #hashtag block id part of the block's top line will be included when expanding context blocks.
This means if you do use #hashtag to refer to some previous block, and you also include the same @hashtag block as context, then the expansion will include the same token sequence starting with the hash in both the place where the block is provided as context and later in the place where it is mentioned.
Both humans and LLMs can use this to connect the two locations quickly.

@projfiles

Here's our add function, as an example of a transformation applied to a block---in this case, getting the code part instead of the comment part:
@example_block:code
Note that the code reference is on a line by itself, but here we didn't include blank lines around it.
The blank lines are optional, and when we perform the replacement, we will maintain whitespace that you added.

You can also have ":code" or ":all" suffixes on context block references, for example we could have written "@test_block:code" above to bring in the code part of the text block (only) into the context or "@test_block:all" for the full block.

The ex command :expand shows the expansion of the block.
*/
/* import libraries 
*/

#include "spanio.c"
/* #langtable

Here we have a table of languages that we support.

(The columns are written as numbered items, for easier editing.)

1. Supported Language:

C, Python, JavaScript, Markdown

2. Filename extension:

C: .c
Python: .py
JavaScript: .js
Markdown: .md

3. Find blocks implementation:

C: find_blocks_language_c
Python: find_blocks_language_python
JavaScript: find_blocks_language_c
Markdown: find_blocks_language_markdown

4. Find blocks description string:

C: Blocks start with a C-style block comment at the beginning of a line (no leading whitespace).
Python: Blocks start with a triple-quoted string, also at the beginning of a line.
JavaScript: Uses the same rules as C (block comment flush left starts a block).
Markdown: Blocks start with a heading of any level.

5. Block comment end description:

C: Comment part ends with a C-style block comment that can end anywhere on a line.
Python: The triple-quote end also has to be at the start of a line.
JavaScript: Same as C.
Markdown: There is no comment part, markdown blocks are often all prose.

6. Markdown code block tag (langtag):

C: c
Python: py
JavaScript: js
Markdown: md
*/
/* #config_fields #conftable ->gpt3.5

Here we define the CONFIG_FIELDS macro, a list of X macro calls, e.g. X(cmprdir), for each config setting.

The config settings are:

- cmprdir, the directory for project state, usually .cmpr
- buildcmd, the command to do a build (e.g. by the "B" key)
- bootstrap, a command that creates an initial prompt (see README)
- cbcopy, the command to pipe data to the clipboard on the user's platform
- cbpaste, the same but for getting data from the clipboard
- curlbin, the path to curl (or just "curl" if unspecified)
- ollamas, a comma-separated list of ollama models to use
- model, the LLM currently in use, or "clipboard" for browser chat models
- debug, a set of single-character flags that turn on debugging features when present
*/

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

/* #pragmas

A new feature called pragmas lets us give some blocks a special function.

A pragma must appear in the top line of a block, just like a block id, but uses "!" instead of "#".

We give our pragmas long and descriptive names so they can be interpreted easily when reading source code that uses them.
We can have a :pragmas ex command to quickly select from a menu of defined pragmas and add them to the current block, so it won't be necessary to remember them in order to use them.

Here are the defined pragmas:

1. Pragma name.

use_as_global_context

2. Description.

use_as_global_context: Causes this block to be a context reference of every other block, without having to be explicitly referenced as such.

@- context_of:block_id: injection

*/
/* #checksums

We have a checksum type and a checksums generic array type.

To get the checksum for a block, we use selected_checksum with the block contents.

Checksums are used as an opaque id for the contents of a span, such as a file or a block.
*/
/* #checksum_setup @checksums

Similar to projfiles, we have a generic arena-allocated array type for checksums, which we make by MAKE_ARENA with E = checksum, T = checksums, and 256 for the stack size.

Prior to this we typedef checksum as a struct containing only a u64.
Usually this will not be accessed, so we call it __u.
*/

typedef struct {
    u64 __u;
} checksum;

MAKE_ARENA(checksum, checksums, 256)

/* #projfiles

A project usually contains multiple files.

We have a projfile type which contains for each file:

- the path as a span
- the language, also a span
- the contents of the file, also a span
- a checksum of the contents, called cksum

Here we have a typedef for the projfile.

We also call our generic macro MAKE_ARENA with E = projfile, T = projfiles, choosing 256 for the stack size.
*/

typedef struct {
    span path;
    span language;
    span contents;
    checksum cksum;
} projfile;

MAKE_ARENA(projfile, projfiles, 256)
/* #rope

To support a particular memory allocation and access pattern we have the following interface:
@- Note: currently a very simple version of a "rope", since the use case is read-only data; it's really just a linked list of blocks of memory.

rope rope_new(size_t);
int rope_isnull(rope); // 1 if initialized, 0 otherwise
void rope_release(rope*);
span rope_alloc_atleast(rope*,size_t);

This was added to support our undo feature, which reads in and indexes all the revs.

The rope is opaque from the perspective of the caller, but it will have a u8 pointer to the allocated memory and two sizes, "used" and "cap", and a pointer to the next segment of the rope.

The memory is only ever extended in our use case until it is freed, so we have a way to ensure that there is at least some quantity of memory available in a contiguous block.
We only ever write into the last segment, so this method simply scans the segments until it reaches the last one (which has the next pointer set to NULL) and then checks if it has enough unused space for the request.
If not, it allocates a new segment and extends the rope.
Regardless, the call then returns a span that points to the unitialized memory.
(This is different from our usual use of spans as strings.)

Each segment will be a minimum of 32MiB, so when allocating we either allocate a block of this size, or if a larger size was requested, we allocate the requested size.

When the rope is no longer needed, we walk the list and free all the allocated segments in turn.
*/

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

/* #rev_info

Revs are older revisions of files in the project.
They are stored in <cmprdir>/revs, and used to support the undo feature.

The rev_info structure holds metadata on our revs, and has the following elements:

- filenames, a spans holding paths (under revs/) in lexicographic order (also chronological order)
- fnbuf, a char * holding allocated memory for the filenames
- revblocks, a chronologically partially-ordered list of historical blocks with metadata
- n_revblocks, the size of that array
- cap_revblocks, the allocated capacity of revblocks, also in rev_block-sized units (i.e. not in bytes).
- revrope, a rope holding the rev contents

For the revblocks we also need a type, so we'll have a rev_block struct as well.
Then revblocks can be a pointer to a rev_block, and we'll manually manage the memory for it, doubling the cap when necessary.

On the rev_block struct we need:

- a span for the actual block contents
- a checksums, sorted_line_cksums, for the sorted line checksums which we use for similarity determination
- a spans for the zero or more IDs this block may have
- a timestamp, which we will store in seconds since the epoch as a time_t

Because of the inclusion, we declare rev_block first, then rev_info.
*/

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

/* #ui_state @config_fields:all

We define a struct, ui_state, which we can use to store any information about the UI and the project data model in a single place.
This includes, so far:

- files, a projfiles array holding the files in the project
- current language, a span, used by the file/language config handler functions (and so probably shouldn't be here)
- the blocks, a spans
- the curr_block_idx (into .blocks), the number of blocks prior to the current one, or -1 if there is no current block (e.g. empty file state)
- the curr_file_idx (into .files), the number of files prior to the current one, or -1 if there is no current file (i.e. empty project state)
- a marked_index, which represent the "other end" (from curr_block_idx) of a selected range
@- block_cksums, a checksums holding checksums for each block -- currently unused
- the lines, a spans
@- line_cksums, a checksums for the lines
- revs, a rev_info structure which stores metadata about our revision history
- block_idx, a spans which is an index of block ids
- the search span which will contain "/" followed by some search if in search mode, otherwise will be empty()
- the previous_search span, used for n/N
- the ex_command which similarly contains ":" if in ex command entry mode, otherwise empty()
- config_file_path, a span
- terminal_rows and _cols which stores the terminal dimensions
- scrolled_lines, the number of physical lines that have been scrolled off the screen upwards, supporting pagination of blocks
- openai_key, an OpenAI API key, or an empty span
- bootstrapprompt, either empty or contains the bootstrap prompt (set by :bootstrap)
- ollama_models, a spans of the configured ollama model names if any
- now, a struct timespec, used by main_loop to give a consistent timestamp per loop iteration
- outputs_filenames, a spans, temporary place to hold outputs filenames until the feature is further along

Additionally, we include a span for each of the config fields, with an X macro inside the struct, using CONFIG_FIELDS defined above.

Below the ui_state struct/typedef, we declare a global ui_state* state, which will be initialized below by main().
*/

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
    span bootstrapprompt;
    spans ollama_models;
    struct timespec now;
    spans outputs_filenames;
    #define X(name) span name;
    CONFIG_FIELDS
    #undef X
} ui_state;

ui_state* state;

/* #network_ret network return type, used by LLM API functions

Contains a response, generally json, if success; an error, a human readable string, otherwise.

@- This is really an Either type, we might generalize the name of it if we want the same interface anywhere else.
*/

typedef struct {
  int success;
  span response;
  span error;
} network_ret;

/* #sbv_state @SBV_design

The state struct for the SBV feature.

@- SBV is the "select block version" feature available by the "U" keybinding.
@- The rest of the code and documentation comes later, but we need the struct here.
*/

typedef struct {
    int *revblock_indices;
    int max_index;
    int current_index;
    spans curr_block_ids;
    checksums sorted_line_cksums;
} sbv_state;

/* #partials

Needed by the LLM callback machinery.
See #make_output_saver, below, for more.

Here we set up a tagged union type for partial applications.

We have an enum with the tagged union types, PARTIAL_SP_SP and PARTIAL_SP, called PartialType.

The tagged union type, Partial, holds one of:

PARTIAL_SP_SP

This holds a function pointer and a span.
The name indicates that when it is partially applied, a span is stored, and then when it is called, another span is provided.
The underlying function takes both spans and returns void.
So the struct has two members, f and a, of types void(*)(span,span) and span.

PARTIAL_0_SP

This struct is similar, but has no partially-applied arguments, as indicated by the 0 in the first position.
The underlying function just takes one span, and the tagged union only holds the function pointer.

In addition to the PartialType and Partial typedefs we also write some functions:

Partial partial_sp_sp(span, void(*)(span,span));
Partial partial_0_sp(void(*)(span));

void apply_partial(Partial,span);

Finally, we typedef llm_message_handler as an alias for Partial.
*/

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
/* #all_functions

Our functions are declared, usually in comments, and we have a Python script that extracts those decls into a header file.
This is convenient, because it means we can write a block as a self-contained unit, but call that function from anywhere, without having to manually maintain a header file.

Below these we have a list that we used to manually maintain; these should gradually be moved into their actual blocks.
(Note that even though the below is commented out, because of the mentioned Python script, it does affect our build!)
*/

#include "fdecls.h"

 /* #op_includes @optable
We will generate these includes from optable later.
Currently, this wouldn't work, since not all our ops actually have corresponding ops/ files.
*/
#include "ops/nl2pl_rewrite.c"
#include "ops/pl2nl_rewrite.c"
#include "ops/nl2algo.c"
#include "ops/summarize_block.c"
#include "ops/agreement_to_pl_diff.c"

 /*
// search
void start_search();
void perform_search();
void finalize_search();
void search_forward();
void search_backward();
int find_block(span); // find first block containing text
int block_by_id(span); // find a block by id (without hash char)

// ex commands
void start_ex();
void handle_ex_command();
void bootstrap();
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
/* #ingest_functions

When we start, get_code handles everything in the current project files, and get_revs handles all the historical revisions in revs/.

The ingest() function is used whenever code is changed and it re-does everything.

Finally, get_revs handles all the revs, which can be a significant amount of data, so this only happens when it is needed (currently on the "U" feature only).
*/

void get_code(); // read and index current code
void get_revs(); // read and index revs
spans find_blocks(span); // find the blocks in a file
spans find_blocks_language(span file, span language); // find_blocks helper function dispatching on language
void find_all_lines(); // like find_all_blocks, but for lines; applies to the whole project
void index_block_ids();
void ingest(); // updates everything that needs to be updated after code has changed

/* #set_default_clipboard_commands

*/

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

/*
In the following block we write the main function, but here, we surround it by cpp directives so that if -D PROMPT_LIST is used, this main() function won't be included.
(Later we might have more alternate main functions if we continue generating different binaries from the same file, and we'll do something fancier here.)

Here we just write the directive that comes before the main function.
*/

#ifndef PROMPT_LIST
/* #main

In main,

First we initialize, then we read, then we go into the main loop.

To initialize, we will set up our i/o library, memory areas, and globals.
These decisions are recorded in the init() function which we define later.
There we group things that happen the same way every time the program is run.

In read_(), which we also define later, we read in project data and input that is different from run to run.
This starts with the arguments and configuration files, so we pass our argc and argv to it.
Then we read in the project's code.
@- The underscore in the name avoids collision with the C library function read()

Finally, we go into main_loop().
This function does not return, so that's also the end of main.
However, since main returns int, we indicate to the compiler that the line after main_loop() is unreachable.
This prevents us having to add a return 0 line which will never be used.

Below we write just the main function itself.

int main(int, char**);
*/

int main(int argc, char** argv) {
    ui_state stack_state = (ui_state){0};
    state = &stack_state;

    init();
    read_(argc, argv);
    main_loop();
    return 0;
}

/*
...and here we simply close the ifndef.
*/

#endif

/* #init @generic_array_initialization


void init();

We call init_spans_ioc which takes three size_t arguments, i, o, and c, the sizes of the input, output, and cmp spaces.

We use input space for the current contents of the files in the project.

We use output space for buffering output that is intended for stdout or stderr, including all our terminal output.

We use cmp space for all other miscellaneous uses, including other small files that we read and write to, such as conf files.

However, the rev files which we also read in have their own "rope" data structure.
We might use this for input files as well in the future, as we only need each file to be in contiguous memory.

Current sizes here:

inp space:  2^30
out space:  2^30
cmp space:  2^30

@- these are just the previous defaults that we brought over into direct arguments here; we might give some thought to the actual numbers in the future.

implementation limits
----

projfiles arena:               2^14
spans arena:                   2^20
checksums arena:               2^30

These are all generic array types with separate arena allocation for each one.
We may replace this with some other approach in the future.

T and E types for the generic arrays:

projfiles projfile
spans     span
checksums checksum

So here we call each T_arena_alloc(size_t) function (each of these has already been created by our generic array macro), with T replaced by the T type (which is the name of the array type).

@- projfiles doesn't need any of the machinery, since there's only ever one projfiles array; could be specialized away like we did for revs

Our spans arena size is a binary million.
We will hit this limit soon with prompt template expansion and other features, so we need to add instrumentation and give back spans memory.

Our checksums size a "binary billion," is an overestimate, but we will are still adding checksum related features.


globals
----

Above (in #ui_state), we have declared a global ui_state* called state.
After declaring our ui_state variable, which we call stack_state since it's on the stack, and which we initialize to {0}, we then set this global pointer to its address.

Our other globals are inp, out, and cmp, which are set up by the spanio library already.


summary
----

In init() we set up spanio, our arenas, and our global ui_state* state.
*/

void init() {
    init_spans_ioc(1UL<<30, 1UL<<30, 1UL<<30);

    projfiles_arena_alloc(1UL<<14);
    spans_arena_alloc(1UL<<20);
    checksums_arena_alloc(1UL<<30);

    state->config_file_path = S(".cmpr/conf");
    state->files = projfiles_alloc(1024);
    state->files.n = 0;

    set_default_clipboard_commands();
    
    read_openai_key();
}

/* #read_

This is one of the setup functions called from main().

void read_(int argc, char** argv);

In init() we set up things that are the same on every run, but here, we read information from the environment that could change every time.

This starts with the current working directory.
We call find_cmprdir which checks ".cmpr" to see if we are in the top level directory of a cmpr project.
If not, it will chdir up the filesystem hierarchy to see if a parent directory is a cmpr project, and will leave us in that directory if so.

We projfiles_alloc the files array on the state.
We just set the capacity to the full capacity of the projfiles arena, which we can find in the init() function, above, since there won't be any other projfiles arrays allocated.

We call a function handle_args to handle argc and argv.
This function will not do anything directly, but sets indicators on state (possibly spans in cmp space) that define what we should do next.
@- This function will also read our config file (if any).

We call check_conf_vars() once after this; we will also call it in the main loop but we need it before trying to get the code.

We call find_cmprdir, which looks for the .cmpr directory that makes a directory into a cmpr project, and sets it on state.

We set config_file_path on the state to the default configuration file path which is ".cmpr/conf", i.e. always relative to the CWD.
This is important because in handle_args we will already want to either print the args in the default file, or otherwise in a non-standard file location, so this is really where we set the default configuration.

We call check_dirs() which creates any missing directories.

Next we call a function get_code().
This function reads the files indicated by our config file, populates inp, and handles any code indexing steps.
*/

void read_(int argc, char** argv) {
    handle_args(argc, argv);
    check_conf_vars();
    check_dirs();
    get_code();
}
/* delete feature

20240707

Deleting a block is a feature we already have, by using 'e' and simply deleting the block's contents.

So the 'd' feature just needs to do this exact same operation.

However, what we really want is undelete, especially since we now have 'U'ndo.
It would be strange if we had undo for changes to a block, but not for block deletions.
So we can have 'D' for a menu of deleted blocks.
This will be any block that was completely removed, unless it was so similar to a current block that it is an undo match for it.
In other words, if you delete a function in another editor, we will create a new rev without that function.
If you open the delete list, we will then show you that function as of the time that it was deleted.
However, if a block was a duplicate of another block, then we would not consider it as deleted.
Also, if it has the same block id as an existing block, we would consider that the block still exists and has not been deleted.
However, we could have lists of changed (or deleted) lines, just like we currently do for blocks.

We could have, in fact, a continuous unified-diff-like display of the updates to the project, as timestamps and unified diffs for each rev.
In other words, as we go back through the revs, we could use diff itself to tell us which current file the rev is closest to.
We can then represent the revs as this compressed representation.
It is strange that we don't have any way to actually know the filename that the rev corresponds to.
Probably we should have some kind of manifest representation, which can be a Merkle tree, something like a hash of (path, hash) pairs, one per file in the project, similar to how git represents trees.
Or, we should just start keeping the conf file in revs every time it is modified.
(Which we actually currently do if it is edited with 'e', but most people won't have added it.)

If a file is removed from the project, we will detect all the blocks in that file as not currently in the project (unless they were moved into other files).
However, we will not know what file it was.

There is another clever way to get back a deleted block.
If you know the block id, you can simply create a new block, give it that id, and then use 'U' to select the previous version of that block.

As Enter resets the content of the block with 'U', with 'D' it might set a register, like what 'dd' or 'yy' does in vi.
Then 'p' or 'P' can be used to put the block in place in the current project.

As a side note, we imagine a "C natural" filetype, intended for existing C code that doesn't use our style of blocks defined by block comments.
This should blockize in a deterministic way similar to what a human would do, and should make use of ctags.
Alternatively, we could have a single filetype that would automatically detect the language as it goes and blockize accordingly.

In existing codebases, we should get blocks for free and also ids for most of those blocks for free from ctags.
This means we should allow using tags such as "main()" to identify the block that defines the main function in the current namespace as per ctags.
That means we can do block references as "@main()" or "@main():all", without needing to do any extra work to define block ids.
*/
/* #call_llm

void call_llm(span model, json messages, llm_message_handler cb);

Here we get a model name, a json object containing chat messages, and a callback function to handle LLM output from a successful API call.

We dispatch on model name.
If it starts with "gpt" or matches "llama.cpp" we use the call_gpt function, otherwise we call_ollama.
In either case we get back a network_ret object.

In the case of error we report the error to the user, prompt them to hit any key, and wait with getch so they can read the error.

Otherwise we pass the .response and the callback on to either handle_openai_response, for a gpt/llama.cpp model, or handle_ollama_response otherwise.
*/

void call_llm(span model, json messages, llm_message_handler cb) {
    network_ret result;
    if (starts_with(model, S("gpt")) || span_eq(model, S("llama.cpp"))) {
        result = call_gpt(messages, model);
    } else {
        result = call_ollama(messages, model);
    }

    if (!result.success) {
        prt("Error: %.*s\n", len(result.error), result.error.buf);
        prt("Hit any key to continue...\n");
        flush();
        getch(); // User acknowledgment to move past error
    } else {
        if (starts_with(model, S("gpt")) || span_eq(model, S("llama.cpp"))) {
            handle_openai_response(result.response, cb);
        } else {
            handle_ollama_response(result.response, cb);
        }
    }
}

/* #read_openai_key

void read_openai_key();

In this function, we check that the file ~/.cmpr/openai-key exists and can be read.

We use getenv to get the HOME directory, and construct the path from there (using a char buffer of size PATH_MAX).

If the file does not exist at all, we assume that the user is not using the feature, and we want to return silently in that case.
Therefore, we use stat to check if the file exists, and if not, return.
(This will leave openai_key empty, indicating to the rest of the code that the API is not available.)

Otherwise, we will read the file into cmp space using our library method.
(This will complain and exit on any failure to read the file, as we intend.)

We set state.openai_key to point to the file contents.

However, we actually want to trim whitespace (such as a newline that must end the file) in case we print the key as a string (such as in an HTTP header), so we call trim() on it.
*/

void read_openai_key() {
    char path[PATH_MAX];
    struct stat st;
    char *home = getenv("HOME");
  
    if (!home) return;

    snprintf(path, PATH_MAX, "%s/.cmpr/openai-key", home);

    if (stat(path, &st) != 0) return;

    state->openai_key = trim(read_file_into_cmp(S(path)));
}

/* #filename_template @template_language_design @assoc_spans

The filename_template pattern lets us reliably generate file paths across the codebase using a simple syntax.
@- Usage: with @filename_template as a context reference to simply use the filename_template function.
@- Or, include @filename_template:all and special instructions to inline the interpretation of a static (compile-time) pattern into direct concat() calls, etc.

span filename_template(span);

We use helper functions to parse the template, set up a context (or variable dictionary) and then evaluate the template.

We return a span in cmp space, and we only extend cmp space by the length of this span.

Note that users of this function will often write a pattern like this: "<cmprdir>/revs/<timestamp>", as an example.
However, our template language uses curly braces, so we would translate this into S("{cmprdir}/revs/{timestamp}") before calling filename_template (or expand_template).

The difference between filename_template and expand_template is that filename_template is specialized to already include the variables in scope, and therefore only takes one argument (the template) instead of expand_template, which requires the caller to also provide the variable dictionary.

@- We can do this either by only using spans that were already allocated, or if we allocate new spans, by copying our final output into place at the previous end of cmp space.
@- This can also be abstracted into a library pattern, where we push cmp.end onto a stack, and then pop with a span argument.
@- The span argument will be copied to the previous end of cmp unless it is already at that location, and will be returned from the pop call, which can then be the return value of the function that's returning the cmp-allocated span.
@- In this case, all our filename components are going to be static (at least per main loop iteration) which makes our filename templates have the properties we want.
@- In particular, the {timestamp} variable will be set once per main loop iteration, which means that we can rely on multiple files (as under api_calls) having the same timestamp, even though we are making separate calls to the filename_template function.

Implementation:

We call filename_variables to get a spans containing a dictionary.

Then we simply return the result of expand_template on the template with this dictionary.

@- Partial inlined implementation:
@- In cases where the pattern is partly inlined, instead of calling filename_template, we can expand the filename pattern into a static spans.
@- We can then call expand_template directly inline, with the static spans and a filename_variables() call as arguments.

@- Fully inlined implementation:
@- Using concat and the NL part of filename_variables, we could inline the simplest cases directly as spanio library calls and references to state values.
@- This pattern would match most of the current places where <cmprdir> is used.
*/

span filename_template(span template) {
    spans vars = filename_variables();
    return expand_template(template, vars);
}

/* #assoc_spans @gcb

Similar to assoc lists in Lisp.
An "assoc spans" is just a regular spans (which is a generic array of span elements) with a particular intepretation.

Much like in Lisp, we use them to represent maps for small, code-like datasets such as variable environments.
To set one up, we simply create a spans and append variable name, value pairs onto it.
To look up values in it, we just examine the even-offset spans until we find a match and then return or otherwise use the value at the following offset.
I.e. (i*2) and (i*2 + 1) are the offsets (0-based array indices) of the i'th key and value, respectively.

We have a function, assoc_spans_lookup(spans, span) which returns the value for a given key, or the null span if it is not found.
*/

/* #assoc_spans_lookup @assoc_spans

span assoc_spans_lookup(spans, span);

*/

span assoc_spans_lookup(spans assoc_list, span key) {
    for (size_t i = 0; i < assoc_list.n / 2; ++i) {
        if (span_eq(assoc_list.a[i*2], key)) {
            return assoc_list.a[i*2 + 1];
        }
    }
    return nullspan();
}

/* #filename_variables @assoc_spans

Here we define the variables that are available to filename_template and where they come from.

- cmprdir: this is a configuration parameter, which is usually .cmpr/ in the project directory. It might or might not contain a trailing slash, which is probably something we should fix; but for now we always include the slash after cmprdir when using it to construct a path (extra slashes in paths are harmless).
- timestamp: this is a representation of the current time down to the second, constructed such that lexicographic order is also chronological order (YYYYMMDD-hhmmss). It is consistent per main loop iteration.

spans filename_variables();

Implementation:
We get cmprdir from state, and timestamp comes from state->now via strftime, in a compact ISO 8601-like format, like 20240501-210759.

We strftime into a local char buffer, but then we use prs() so that the value we push will not point into our stack frame.
*/

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

/* #call_gpt

network_ret call_gpt(json messages, span model);

Here we talk to an OpenAI model via the API.

We are given a json (the type) which contains an array of messages, and a span which contains a model string, and return network_ret.

Next we set up a json object.
We use prt_cmp() so that the json object will be written to cmp space.
Using the json api functions, we make a json object and extend it with "messages" as the messages and with "model" as json_s of the model string, then go back to normal with prt_pop().

We will write the request body and response or error to disk, so first we set up three filenames.
The filename is <cmprdir>/api_calls/<timestamp>-{req,resp,err} where cmprdir comes from state and the timestamp is in the format YYYYMMDD-hhmmss.
The three filenames (-req, -resp, and -err) all have the same timestamp, which lets us correlate them later.
To construct them we set up a base filename first and then append the req, resp, err to it in turn.

We write the request body to disk, without clobbering as it should not exist, then we call call_gpt_curl to handle the HTTP request, passing in the three filename spans, and we return what it returns.
*/

network_ret call_gpt(json messages, span model) {
    span base_filename, req_filename, resp_filename, err_filename;
    char timestr[20];
    struct timespec ts;
    network_ret net_result;

    // Use current time to generate unique filenames
    clock_gettime(CLOCK_REALTIME, &ts);
    strftime(timestr, sizeof(timestr), "%Y%m%d-%H%M%S", localtime(&ts.tv_sec));

    // Set up filenames for request, response, error
    base_filename = concat(state->cmprdir, S("/api_calls/"));
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

/* #call_gpt_curl

network_ret call_gpt_curl(span req, span resp, span err);

This is the network part of call_gpt().

We get three filenames, req, resp, and err, respectively, and we return a network_ret.

The HTTP request body as a JSON object has already been written into the req file.

We handle the communication by calling curl.
We put the binary name in a span, either state->curlbin, or just "curl" if that is empty.
If state->model is "llama.cpp" then we set is_gpt to 0, which we will use in a couple places.

If we are in gpt mode, we need the openai key.
If the openai_key is empty, we return a network_ret with success = 0 and .error of "No API key provided.".
Otherwise we are using a llama.cpp model and the key doesn't matter so we can use "[unused]".
We put this in a span and include the header all the same; the llama.cpp server will ignore it and it simplifies the code.

Next we construct a curl command.

We need to tell curl:
- to be silent except in case of error with -sS,
- the name of the file with the JSON payload,
- to set the content-type header,
- to use the API key (Bearer token) which is in state->openai_key, something like "Authorization: Bearer <key>",
- to put the output into the resp file,
- the API endpoint,
- and finally to redirect stderr to the err file.

We put the command together with snprintf.
As always, to print any span x we use `%.*s`, with corresponding arguments len(x) and x.buf.
Make everything a span before calling snprintf so that this is easier.

Don't forget to quote HTTP headers when composing the curl command with -H to protect them from being split by the shell.

Our return value is a network_ret, declared above, which either has success = 1 and the .response contains the body of the API response or success = 0 and .error contains a human-readable error message.

We read the contents of the resp file with read_file_into_cmp and set this on .response.
If the curl command returned non-zero, we also read the contents of the err file into .err.
We always read the resp file, even in cases of error.

API endpoint for gpt: "https://api.openai.com/v1/chat/completions"
for llama.cpp: "http://localhost:8080/v1/chat/completions"

Notes:

Prefer span functions to C strings.
Never write const in C.
Use the %.*s, len(x), x.buf pattern when using a span with a format string.
DO NOT EVER write %.*s, len(x) and s(x); use x.buf directly, where x is any span.
*/

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

/* #call_ollama

network_ret call_ollama(json messages, span model);

Here we talk to an Ollama local model via the API.

We are given a json which contains an array of messages, and a span which contains a model string, and return a network_ret.

We set up a json object for the message body.
Using the json api functions, we make a json object j and extend it with "messages" as the messages, with "model" as json_s of the model string, and with "stream" set to "false".

We will write the request body and response or error to disk, so first we set up three filenames.
The filename is <cmprdir>/api_calls/<timestamp>-{req,resp,err} where cmprdir comes from state and the timestamp is in the current time in the format YYYYMMDD-hhmmss.
To construct the three filenames (-req, -resp, and -err) we set up a base filename first and then append the req, resp, err to it in turn.

We write the request body j.s to disk, without clobbering as it should not exist, then we call call_ollama_curl to handle the HTTP request, passing in the three filename spans, and we return what it returns.

Functions: clock_gettime, strftime, prt_cmp, json_o, json_o_extend, json_o, S, json_b, prt_pop, prs, len, concat, write_to_file_span, call_ollama_curl
*/

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

/* #call_ollama_curl

network_ret call_ollama_curl(span req, span resp, span err);

This is the network part of call_ollama().

We get three filenames, req, resp, and err, respectively, and we return a network_ret.

The HTTP request body as a JSON object has already been written into the req file.

We handle the communication by calling curl.
We put the binary name in a span, either state->curlbin, or just "curl" if that is empty.

Next we construct a curl command.

We need to tell curl:
- to be silent except in case of error with -sS,
- the name of the file with the JSON payload,
- to set the content-type header,
- to put the output into the resp file,
- the API endpoint,
- and finally to redirect stderr to the err file.

We put the command together with snprintf.
As always, to print any span x we use `%.*s`, with corresponding arguments len(x) and x.buf.

Don't forget to quote HTTP headers when composing the curl command with -H to protect them from being split by the shell.

Our return value is a network_ret, declared above, which either has success = 1 and the .response contains the body of the API response or success = 0 and .error contains a human-readable error message.

We read the contents of the resp file with read_file_into_cmp and set this on .response.
If the curl command returned non-zero, we also read the contents of the err file into .err.
We always read the resp file, even in cases of error.

API endpoint: "http://localhost:11434/api/chat"
*/

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

/* #print_config @config_fields:all

Debugging helper.

Here we print all the config values using an X macro, CONFIG_FIELDS, and state.
*/

void print_config() {
    #define X(name) prt(#name ": %.*s\n", len(state->name), state->name.buf);
    CONFIG_FIELDS
    #undef X
    flush();
}

/* #argtable

We present the supported arguments and flags in a tabular form (as with langtable previously).

Command syntax summary:

cmpr [--conf <filepath>] [--print-conf|--help|--init|--version] [(--print-block|--print-code|--print-comment) <index>] [find-block <search>] [--count-blocks]

Command argument and flag table:

1. Supported arguments and flags:

--conf <filepath>
--print-conf
--help
--init
--version
--print-block <index>
--print-comment <index>
--print-code <index>
--find-block <search>
--count-blocks

2. Behavior of arguments and flags:

conf:
  Use an alternate configuration file given by <filepath>; otherwise proceed normally.

print-conf:
  Print configuration settings and exit.

help:
  Print usage summary, help on command-line flags and exit.

init:
  Initialize .cmpr/ in the current directory.

version:
  Print the version number and build timestamp and exit.

print-{block,comment,code}:
  Print the revelant part (or whole) of the block given by the one-based index.

3. Implementation notes:

conf:
  update config_file_path on the state; we must do this before calling parse_config

print-conf:
  we print the configuration (print_config()) and exit; we must have called parse_config (and set an alt conf file if any) prior

help:
  we just prt a short usage summary, flush(), and exit(0)
  we include argv[0] as usual and summarize everything we support.

init:
  we just call cmpr_init; we can't combine this with --conf, because --init creates the configuration file and we don't want to create configuration files in random places (the user can still move the file themselves and use --conf later if they are doing something exotic)

version:
  we prt "Version: $VERSION$" here; the dollar-delimited variable-looking thing is replaced by a build step

--help, --init, --version:
  these three flags all are "action args"; if one is provided, any other flags will have no effect
  if more than one is given, any one of them may take effect (we don't care which), but not more than one

print-{code,comment,block}, count-blocks, find-block:
  all of these require the code be loaded, which normally happens after we are called
  so if any of these flags are used we call get_code() first, then we call the appropriate function, then flush and exit successfully
  we always use one-based indexes for anything user-visible, so we must add or subtract one when calling our functions (find_block, print_block, print_comment, print_code)

4. Help strings:

conf:
  Use alternate configuration file <filepath>.

print-conf:
  Print the current configuration settings.

init:
  Initialize a new directory for use with cmpr.

help:
  Display this help message.

version:
  Display the version number / build string.

print-block, -comment, -code:
  Print a complete block (or comment or code part) given by index.

find-block:
  Print index of first block matching search string by full-text search, or -1.

count-blocks:
  Print number of blocks in project.

*/
/* #handle_args @argtable

In handle_args we handle any command-line arguments.

void handle_args(int argc, char **argv);

Below we implement void handle_args(int argc, char **argv);

Here are some random further implementation notes on that:

There are things that have to be handled in specific orderings.
Our basic technique here is to set indicators (0- or 1-valued ints) in our arg-handling loop.
These are used directly in if statements, like `if (ind_print_block) { ...`.
Below the loop we then handle the necessaries in the correct order.
We use "int ind_*" for these variables so they don't conflict with functions or anything else we already have.
We also have an int "action_arg" which tracks whether one of the action flags has been set, char pointers for string arguments like conf filepath or find-block search, and a block index which is shared by print-{block,comment,code}.

None of these flags can be combined: print-block, print-comment, print-code, find-block, count-blocks.
If more than one is set, we print an error message and exit.
If any of these are set then we exit successfully, but if none of them is, then we will return from this function and enter our main loop.

Because --init is used to set up the config file, it cannot be combined with --conf, if it is, we also print an error and exit.
Also, if we are doing an --init, we need to not try to parse the config file, because that will definitely fail.

If "--conf <alternate-config-file>" is passed, we update config_file_path on the state.
Once we know the conf file to read from, we call parse_config before we do anything else.

If "--print-conf" is passed in, we print our configuration settings and exit.
This is only OK to do once we have already called parse_config, so the configuration settings have already been read in from the file.

This function will always call parse_config, always before printing the config if "--print-conf" is used, and always after updating the config file if "--conf" is used.
In particular, even if no alternate conf file was set, we still need to read the default conf file.
*/

void handle_args(int argc, char **argv) {
    int ind_conf = 0, ind_print_conf = 0, ind_help = 0, ind_init = 0, ind_version = 0;
    int ind_print_block = 0, ind_print_comment = 0, ind_print_code = 0, ind_find_block = 0, ind_count_blocks = 0;
    int action_arg = 0;
    char *conf_filepath = NULL, *find_block_search = NULL;
    int block_index = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--conf") == 0 && i + 1 < argc) {
            conf_filepath = argv[++i];
            ind_conf = 1;
        } else if (strcmp(argv[i], "--print-conf") == 0) {
            ind_print_conf = 1;
            //action_arg = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            ind_help = 1;
            action_arg = 1;
        } else if (strcmp(argv[i], "--init") == 0) {
            ind_init = 1;
            action_arg = 1;
        } else if (strcmp(argv[i], "--version") == 0) {
            ind_version = 1;
            action_arg = 1;
        } else if (strcmp(argv[i], "--print-block") == 0 && i + 1 < argc) {
            block_index = atoi(argv[++i]) - 1;
            ind_print_block = 1;
        } else if (strcmp(argv[i], "--print-comment") == 0 && i + 1 < argc) {
            block_index = atoi(argv[++i]) - 1;
            ind_print_comment = 1;
        } else if (strcmp(argv[i], "--print-code") == 0 && i + 1 < argc) {
            block_index = atoi(argv[++i]) - 1;
            ind_print_code = 1;
        } else if (strcmp(argv[i], "--find-block") == 0 && i + 1 < argc) {
            find_block_search = argv[++i];
            ind_find_block = 1;
        } else if (strcmp(argv[i], "--count-blocks") == 0) {
            ind_count_blocks = 1;
        }
    }

    if (action_arg) {
        if (ind_conf && ind_init) {
            prt("Error: --conf and --init cannot be combined.\n");
            flush_exit(1);
        }
        if (ind_help) {
            prt("Usage: cmpr [--conf <filepath>] [--print-conf|--help|--init|--version] [(--print-block|--print-code|--print-comment) <index>] [find-block <search>] [--count-blocks]\n");
            flush_exit(0);
        }
        if (ind_version) {
            prt("Version: $VERSION$\n");
            flush_exit(0);
        }
        if (ind_init) {
            cmpr_init();
            flush_exit(0);
        }
    } else {
        if (ind_conf) {
            state->config_file_path = S(conf_filepath);
        }
        parse_config();
        if (ind_print_conf) {
            print_config();
            flush_exit(0);
        }
        if (ind_print_block + ind_print_comment + ind_print_code + ind_find_block + ind_count_blocks > 1) {
            prt("Error: --print-block, --print-comment, --print-code, --find-block, and --count-blocks cannot be combined.\n");
            flush_exit(1);
        }
        if (ind_print_block + ind_print_comment + ind_print_code + ind_find_block + ind_count_blocks) {
          get_code();
        }
        if (ind_print_block) {
            print_block(block_index);
            flush_exit(0);
        } else if (ind_print_comment) {
            print_comment(block_index);
            flush_exit(0);
        } else if (ind_print_code) {
            print_code(block_index);
            flush_exit(0);
        } else if (ind_find_block) {
            int index = find_block(S(find_block_search));
            prt("%d\n", index + 1);
            flush_exit(0);
        } else if (ind_count_blocks) {
            int count = count_blocks();
            prt("%d\n", count);
            flush_exit(0);
        }
    }
}

/* #clear_display
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

@- TODO: probably inp_sanity_check should absorb all the uses of this one
*/

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

/* #inp_sanity_checks @gcb @complain_and_exit

void inp_sanity_checks();

In several places we do some terrifying surgery on the contents of memory, moving chunks of memory around and adjusting various spans into that memory in ways that must be consistent.
(For example, whenever the user or the LLM modifies the contents of a block, or when a file is modified externally.)

As a backstop against bugs, we here specify and check certain invariants.
@- We can later take these invariants as a test case for our AI-assisted proof approaches.

This also makes a nice documentation in one place of the things we expect to always be true about our memory layout of files and blocks.

All our code goes into the input space, in the global span inp, and it's the only thing we use inp for.
We have two sequences of spans that tile this space, one for files and one for blocks.
Files can be empty but blocks cannot.

Here we test that blocks tiles inp.

To test that a spans tiles a span, we check that .buf of the first span matches the .buf of the container span, that the .end of the last span matches the .end of the container, and that the .buf of every other span matches the .end of the previous one.

We also want to test that no block is empty, which we can do at the same time.

We also test that files tiles inp.
However, the files are not stored in a spans array type like the blocks are, rather they are in projfiles, so we have to iterate over them a little differently.

We also want to ensure that every file is tiled by some contiguous set of blocks.
For this, we can loop over all the files, while maintaining a separate index into the blocks.
In this loop, we skip any empty files, since they will contain no blocks.
For the first non-empty file, we check that it matches the start of the first block (i.e. equal .buf on each).
Then we advance through the blocks as long as the start of each is inside the file we're on.
Because we already know the blocks and files both tile inp, it's not necessary to test the .end of the blocks.
We can simply leave the block index pointed at the first block that's not in this file, and then let the file loop handle every later file in the exact same way.

Algorithm:
@- (output of nl2algo, lightly modified)

- **Check that blocks tile inp:**
  - If there are no blocks, check that inp is empty, and otherwise complain and exit.
  - Get the first block in the array:
    - Check that it starts at the beginning of inp.
  - For every block from the second one onwards:
    - Check that its start matches the end of the previous block.
    - Check that the block is not empty.
  - Get the last block in the array:
    - Check that it ends at the end of inp.

- **Check that files tile inp:**
  - If there are no files, check that inp is empty, and otherwise complain and exit.
  - Get a list of current files from state->files.
  - For the first file:
    - Check that it starts at the beginning of inp.
  - For every file from the second one onwards:
    - Check that its start matches the end of the previous file.
  - For the last file:
    - Check that it ends at the end of inp.

- **Check that every file is tiled by some contiguous set of blocks:**
  - Maintain an index into the blocks array.
  - Iterate over each file:
    - If the file is empty, skip it.
    - Check that the file starts at the start of the block at the current index.
    - Advance through the blocks for as long as the start of each block is within the file.
      - Ensure each block is within the file.
    - After exiting the block loop, leave the block index pointing at the first block that is not inside the current file, so the next iteration starts correctly.

The checks should now ensure that blocks tile inp, no block is empty, files tile inp, and that every non-empty file contains a contiguous set of blocks.

Messages:

Because the messages are user-visible (in the event of a bug) and certainly programmer-visible during development, we consider them part of the interface.

Messages that this function can produce are therefore added to the below list as necessary.
All messages should begin with "file/block consistency error: ", and continue with one of the following as appropriate.

- "Input space should be empty if there are no blocks."
- "The first block should start at the beginning of inp."
- "Blocks are not contiguous."
- "No block should be empty."
- "The last block should end at the end of inp."
- "Input space should be empty when there are no files."
- "The first file should start at the beginning of inp."
- "Files are not contiguous."
- "The last file should end at the end of inp."
- "File does not align with the start of a block."
- "Block exceeds the end of the file."
*/

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

/* #jk_order

We maintain two current index values, one for blocks, as we have today, and a new one for files.
Then on j/k we either advance to the next block, or we advance to the next file, if that file is empty.
@- We'll rename these to curr_block_idx and curr_file_idx, both ints.

These two index values will either be in range of state->blocks or state->files, or they will be -1 to indicate some special situation.

The curr_file_idx will be -1 only when the entire project is empty, i.e. there are no files in it.
In this empty project state, the current block index will also be -1.

The curr_block_idx can also be -1 when we are on an empty file, in which case the file index will be the index of some file.
We call this the empty file state.

So in summary there are three kinds of states:

File index and block index are both -1: the empty project state.

Block index is -1, file index is in range: the empty file state.

Both are in range: the normal state where we are in some block in some file.
*/

/* #jk_implementation @jk_order @blocks @files

When j/k or g/G are used to navigate between blocks, this is what we do:

If the project is empty, none of these do anything so we simply return.

Otherwise, if the block index is -1, it means we are on an empty file.
Then for j/k we need to go to the previous or next file, if there is one.
(If there isn't one, we just return.)
If there was a previous or next file, once the file index is updated, we check if that file is empty.
If it is, then the block index remains at -1, and we return.
If not, then if we are going up (k), we set the block index to the last block in that file, and if going down (j), we set it to the first block in the file.
To get the last block in a file, we can iterate backwards over the blocks until file_for_block is a match for the current file index, and similarly for the first block.

If we're in the normal state (not an empty file or an empty project), then on j/k we are either going to go to an empty file state, or to a normal state.
Specifically, if we're going down, and this block is the last block in the file, and the next file is empty, we go to the empty file state.
(We can check for this by looking at whether the contents of the next file is empty, and its .buf is equal to .end of the current block.)
For going up, we do something similar.
If we're already at the maximal block in the direction we're going (i.e. either 0 or state->blocks.n - 1), we do nothing.

For g/G, we do something similar.
First, we check if the first (or last) file is empty.
If it is, we go to the "empty file state" (i.e. block index will be -1, file index is the index of that first or last file).
If not, then we set both the file and block index to the first or last one.

For all of these navigation commands, whenever we change the current block index to something in range, we also reset the pagination by setting scrolled_lines to 0.
*/
/* #find_all_blocks @gcb @projfiles:all @files @blocks

In find_all_blocks, we find the blocks in each file that is not empty.

We set up state->blocks with a new spans array.

Then for each of the projfiles, we call find_blocks_language() to find the blocks in that file by language, if that file's contents is not empty, and push them onto state->blocks.

Finally, if the blocks is empty, we set the current block index to -1.
Otherwise we check to see if it is too big (as the number of blocks may have changed) and set it to the max block index if so.
*/

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
/* #find_all_lines

Similar to find_all_blocks, but much simpler.

All the files have been read into inp, so we simply need to iterate over it once, count all the lines, allocate a lines spans and then iterate again and populate it.

We can make a copy of inp and use next_line once to count the lines, and then the next time to populate state->lines.
Note that next_line doesn't include the newline, so while our blocks tile the input, our lines index has gaps of one byte between each line and the next.
*/

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

/* #selected_checksum

checksum selected_checksum(span);

The currently selected checksum function is siphash, which we wrap here.

This is the function we are calling:

    Computes a SipHash value
    *in: pointer to input data (read-only)
    inlen: input data length in bytes (any size_t value)
    *k: pointer to the key data (read-only), must be 16 bytes 
    *out: pointer to output data (write-only), outlen bytes must be allocated
    outlen: length of the output in bytes, must be 8 or 16
int siphash(const void *in, const size_t inlen, const void *k, uint8_t *out,
            const size_t outlen);

Our input is a span, the in and inlen args to siphash are determined by the input span.

We use static buffers local to the function for the key (which is 16 bytes, given below) and the out buffer, which is 8 bytes.

The key is "ABCDEFGHIJKLMNOP".

Our output is a u64.
We pass a pointer to this int into siphash.
We return this wrapped in our checksum struct.
*/

checksum selected_checksum(span input) {
    static const char key[16] = "ABCDEFGHIJKLMNOP";
    u64 result;
    siphash(input.buf, len(input), key, (uint8_t*)&result, sizeof(result));
    return (checksum){result};
}

/* #get_code

In get_code, we get the code into the input buffer.

For each of the projfiles:

- we read this file into inp by read_file_S_into_span with inp_compl() as the span argument, always advancing inp as usual in this pattern so we don't overwrite the contents
- we store the contents on the projfile

If there are no files in the project, we set curr_file_idx to -1, otherwise we set it to 0.

Then we call ingest, which handles everything downstream of getting the bytes into memory.
*/

void get_code() {
    for (int i = 0; i < state->files.n; i++) {
        state->files.a[i].contents = read_file_S_into_span(state->files.a[i].path, inp_compl());
        inp.end = state->files.a[i].contents.end; // Advance inp to not overwrite contents
    }

    if (state->files.n == 0) state->curr_file_idx = -1;
    else state->curr_file_idx = 0;

    ingest();
}

/* #ingest

void ingest();

Here we handle all indexing operations that have to be done or re-done every time the data in inp (i.e. all the code in the project) changes.

We call:
- find_all_blocks(), which sets up the blocks according to each file's contents and language, 
- find_all_lines(), which simply finds the newlines and creates an index of lines,
- index_block_ids(), which sets up the index of block ids,
- inp_sanity_checks(), which tests some invariants on blocks and files,
*/

void ingest() {
    find_all_blocks();
    find_all_lines();
    index_block_ids();
    inp_sanity_checks();
}

/* #blocks

Block basics.

Blocks live on state, and state->blocks has type spans, so .n gives their number and state->blocks.a[i] accesses the ith block as a span.


Block IDs:

The ID of a block is defined as any string starting with '#', up to the next whitespace character, on the top line of a block.
We always tokenize the top line by whitespace.

Despite the name, blocks can have more than one id, for example (using "[[" as stand-in for the actual block comment start delimiter):

[[ #id1 #id2 @ref1 @ref2

This block would have two ids, as well as two "context" references to other blocks.

As a special case, markdown blocks always begin with "#" and they do not have ids.

The current block is always set.
The index of the block is curr_block_idx on the state.
To get the block as a span the current index is used with state->blocks.
It is never necessary to bounds-check curr_block_idx as it is known to always be in range (and if not, we should crash anyway).
When displaying block numbers to the user, we add one so that the first block is Block 1 not Block 0.
To set the current block, we call set_current_block, which also handles pagination, the file index, etc.
*/

/* #files @projfiles:all

Files basics.

Files live on state, and state->files is a generic array with projfile elements.

To get the span for a file at index i, use state->files.a[i].contents.

To determine if a file is empty use empty() on the file contents.

The current file is determined by UI navigation; see #jk_order for details.
*/
/* #index_block_ids @blocks

void index_block_ids();

We set up the block index on state by iterating over blocks and finding all block ids, defined above.

We need two loops, since spans_push does not reallocate, so we need the exact count before we call spans_alloc.

Markdown blocks have a first line starting with "#" and we do not assign them any IDs at all.

Hints: next_line, split_whitespace
*/

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

/* #ids_for_block @index_block_ids:all

spans ids_for_block(span);

This is similar to index_block_ids, except that we are not doing it for all blocks, but for a single block passed in.
*/

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

/* #block_idx @blocks

Our block id indexing strategy is a bit clever.

Because spans have a location, we simply store a spans containing all block ids, where those spans are in-place, i.e. they actually point to the location in the block of the id string itself (e.g. the "#block_idx" string in the first line of this block).

This is defined as any string starting with '#', up to the next whitespace character, on the top line of a block.

Then to look up a block by id, we simply scan the block_idx (on state), and find the first match, and then we use block_for_span to find the block index of that block.

Our contains_ptr library function facilitates this kind of usage of spans.

Despite the name, blocks can have more than one id, for example (using "[[" as stand-in for the actual block comment start delimiter):

[[ #id1 #id2 @ref1 @ref2

So to find the ids for a block we tokenize the first line by whitespace and then examine all the tokens.
*/

/* #block_for_span

int block_for_span(span);

Of the blocks on state, we find the one containing a given span, using contains_ptr.
(This is not textual inclusion, but rather we are finding the block, if any, that literally contains the memory the span is pointing at.)

If no match we return -1.
*/

int block_for_span(span s) {
    for (int i = 0; i < state->blocks.n; i++) {
        if (contains_ptr(state->blocks.a[i], s)) {
            return i;
        }
    }
    return -1;
}

/* #id_for_block

span id_for_block(span);

Though blocks may have more than one id, here we just return the first one.
@- (This is used for the "#" block id jump list.)
A block id is defined as a token on the top line of the block that starts with '#'.

Hints: next_line, split_whitespace
*/

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

/* #current_block_checksum @gcb @blocks @checksums

checksum current_block_checksum();

*/

checksum current_block_checksum() {
    return selected_checksum(state->blocks.a[state->curr_block_idx]);
}

/* #set_current_block @blocks

Here we set the current block to a given index, for example, when jumping to a known block.
The index is zero-indexed.

void set_current_block(int);

If the index is out of range, calling this function is an error.

@complain_and_exit

If we set curr_block_idx, we also must reset the pagination by zeroing scrolled_lines.

Also, every block is inside a file, so once we have set curr_block_idx, we always get the file for the block.
Then we set curr_file_idx as well.
*/

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

/* #block_id_jump @block_idx @id_for_block @blocks

void block_id_jump();

To support the "#" jump list we use select_menu on the block_idx itself.

We get the block id for the current block, if there is one, find it on block_idx by a linear search and pass that index into select_menu, otherwise we use 0.

If the user cancels the change, select_menu will return -1, and we do nothing.
Otherwise we index back into block_idx, and use block_for_span to get the matching block, and use set_current_block to jump to it.
*/

void block_id_jump() {
    span current_block = state->blocks.a[state->curr_block_idx];
    span id = id_for_block(current_block);
    int idx = 0;

    if (!empty(id)) {
        idx = index_of(id, state->block_idx);
        if (idx == -1) idx = 0;
    }

    idx = select_menu(state->block_idx, idx);
    if (idx != -1) {
        span selected_id = state->block_idx.a[idx];
        set_current_block(block_for_span(selected_id));
    }
}

/*
How we handle files and blocks:

- Non-empty files are tiled by blocks and the block location unambiguously identifies the file it is part of.
- Empty files contain no blocks.
*/

/* #get_revdir

span get_revdir();

We take the cmprdir from the state and append "/revs" to it.
Use concat and s_buffer, with a `static char buf[2048]`.
*/

span get_revdir() {
    static char buf[2048] = {0};
    span revs = S("/revs");
    span revdir = concat(state->cmprdir, revs);
    s_buffer(buf, 2048, revdir);
    return S(buf);
}
/* #complain_and_exit

@- This is a "hint"-style block, include it inline in other blocks when relevant.

"Complain and exit" is NOT the name of a function.
(If you ever write complain_and_exit(), you are fired!)
@- (Note: GPT4, for some reason, loves to hallucinate that a complain_and_exit function exists, hence the above increasingly strongly worded warning.)

Instead, it is a name for a common pattern of prt, flush_err, exit(n>0).

Generally, when things go wrong with C library functions, such as not being able to open files, allocate memory, or something that indicates programmer (not user) error, we "complain and exit".
When files or other resources are involved, always report the relevant filename or path in the message, not only the OS error.
(This is why we use prt directly, so that relevant information is more easily included.)
*/

/* #complain_and_prompt @complain_and_exit
@- Sometimes we have recoverable errors, usually due to user error or misconfiguration.
Similar to the #complain_and_exit pattern, but sometimes we want to continue.

When something goes wrong, we want to allow the user to see what happened but still continue.
We always report the filename, path, or other relevant variables in the message.
We use the "Press any key to continue..." message on its own line.
Then we flush and getch before returning.
*/
/* #get_revs @rev_info @get_revdir:code @complain_and_exit

This is similar to get_code, but for revs instead of the current versions of each file.

First, using the usual C functions opendir, readdir, and closedir with a DIR*, we open the revs directory, and iterate over it once to get the number of files.

We add 8 to this number, and allocate a spans of that size to hold the filenames.
@- (We add 8 in case new revs are created while we are scanning the directory, unlikely as that may be.)

(The files have the format YYYYMMDD-hhmmss, which is 15 chars, plus a null terminator.)

Then we allocate a char buffer of size 16 times this number to hold the files, using malloc.
We store the address of this buffer on the rev info struct.
(All our spans will point into this memory.)

Then we iterate over the directory again.
For each file, we ensure that it matches the pattern of 8 digits, a dash, and 6 more digits.
(If not, we simply ignore it and continue iterating.)
We write the filename into our filenames buffer, and then construct a span that points to that filename, including the null terminator, and add that to our filename spans array on state->revs.

Next we write a comparator function to use with qsort, which compares to spans using span_cmp.
(Note that span_cmp does not have the required signature for use with qsort; we must write a wrapper function directly before the call to qsort that does.)
We use qsort to sort all the filename spans lexicographically (which is also chronologically, due to our naming scheme).
Also, clang does not allow functions to be nested, even though gcc does, so we declare this function immediately before get_revs itself.

Finally, we call get_revs_2, which handles the rest of the work.
*/

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

/* #read_file_into @rope:code

span read_file_into(span filename, rope*);

We implement read_file_into using C library functions.
We open the file, get the size, call the rope_alloc_atleast function, and read the file into the span provided.
We expect that the files that we are reading are quiescent, so we do not expect that they will grow inbetween these calls; if they do, it is OK for us to crash with an informative error message.

@s_pattern

The rope will have already indicated that the memory is used, so all we need to do is adjust the span size, if necessary, to conform to the data read from the file, and return the span.
*/

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

/* #get_revs_2 @rope @rev_info
@- the rest of the get_revs work happens here

void get_revs_2();

This function populates, or partly populates, state->revs.

First we clear the display.

We have already a sorted list of revisions' filenames' basenames already populated on the rev_info struct on state.
However, the revblocks, which are metadata about these revs, are not set up yet, and that is what we must do here.

Or rather, we set up revblocks only for revisions that haven't already been set up.
First, we look at state->revs.revblocks[0] (if there is at least one revblock) and we get the timestamp of that block.
We can call this the latest rev timestamp.
We also store the current number of revblocks, as we will need this later to fix up the order.

Also, if the state->revs.revrope has not been set, rope_isnull() of it will be true; then we set that up with an initial size of 32MiB.

We want, for each rev, a sorted list of line hashes for that rev, which we will then use to identify the best matching file, to pick the language to blockize, and then create our chronological sequence of block revs, which we will in turn use for the "U" feature.
We will later probably optimize this to lazily access all revs as needed.
However, for now we will process them all at once.

First we take the current set of files and for each we generate a sorted list of line hashes.
To hold this we will malloc an array of n of the checksums type, where n is the number of projfiles.
We will maintain this list as we go backwards, correlating each of the projfile indices with the best current match from the revs.
We call this the "working set".
We will free it when we return, since we don't need these checksums outside this function.

So in get_revs_2 we first allocate memory for our sorted checksum arrays, one checksums array per projfile.
Then we use sorted_line_checksums to get the sorted checksums and populate this.
This is called the "working set" and is only used as an argument to get_revs_cache_put (which uses it to blockize revs by language).

Then we iterate over the revs themselves, in reverse order.
For each rev, in reverse order, we first read into the rope (on state->revs.revrope, already initialized) using read_file_into.
We print "rev hashing: n/N", where the numbers come from our iteration, and with an \033[H escape code to always appear in the top left (with prt and flush).
We have the basenames (on `filenames`) but we need to prepend the revdir using prs with "%.s/revs/%.s" with state->cmprdir and the given basename.
First we put the basename in a variable, bname, as we know we will also need it later if we do a cache put.
Our rev block spans will point into the memory of the rope (which main will later release).

We take the bname and call a helper function to get the timestamp from it.
If this timestamp is less than (earlier than) or equal to the latest rev timestamp, then we have already handled everything and we break out of the revs loop.

For each non-empty rev we now need the metadata about the rev blocks.
(If the rev file is empty, it contains no blocks and we skip it.)
This metadata we will either read from cache, or calculate and write to cache.
We call a function get_revs_cache_get(), which returns 0 if the item is not found in the cache.
Otherwise, the cache read also sets up everything in memory, so we are done with this rev and continue to the next one.

If the cache was a miss, then we both calculate and cache the data for next time with get_revs_cache_put().
(This takes the working set, the bname, and the contents of the rev as arguments.)

After our loop, we will have added new revblocks, in reverse chronological order.
However, if there were any existing revblocks, we will have added our new ones after them.
Therefore, we will use the number of previous revblocks that we stored, the current number of revblocks, and memmove to fix the order.
Specifically, we allocate some memory for the newly added revblocks, copy them into it, then copy the previously existing blocks to the later positions in the revblocks array, and then finally copy the new revblocks to the front of the array and free the temporary malloc'd memory.
Since everything is a contiguous array in state->revs.revblocks, this is straightforward.
*/

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
        prt("\033[Hrev hashing: %d/%d", state->revs.filenames.n - i, state->revs.filenames.n);
        flush();

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

/* #revs_cache_design @rev_info

This cache is written in an ASCII format on disk.

This format describes the contents of a single rev file, and contains the following:

- a short header
- one or more sections, each with a short textual header followed by lines containing some data
  - each one is preceded by a blank line (blank lines are separators, not terminators; N sections uses N-1 blank lines)
- the section headers are "blocks", "block <n> scs", and "block <n> ids", and may appear in any order, with n being in [1, N], where N is the number of blocks in the rev
- the "blocks" section is followed by pairs of integers, one per line, defined as start and end offsets (relative to rev file contents) for each block
- the "blocks" section is the only place where we include per-block data (the length and location) without a block number
- the "block <n> scs" section contains sorted checksums (scs), one checksum per line, printed in their sorted order as 16-byte hex values printed as %X
- the "block <n> ids" section contains the ids of block n if it has any (otherwise this block is not printed), printed as integer pairs one per line with the offsets this time being relative to the block itself, i.e. the block "abc" with "b" as block id would have "1,2" as the block id line.
- the timestamp (also part of the rev block structure, see @rev_info) is excluded from the cache, as it is derived from the bname itself (i.e. it's the key)

The rev cache filenames are <cmprdir>/cache/v8/revs/<bname>, where <bname> corresponds to the rev file <cmprdir>/revs/<bname>.

The header contains lines of the form "<key>: <value>".
The expected keys:

"Language", "Blocks".

This makes the files more human-readable, recording the language used to find the blocks, and the number found.

The number of blocks listed under the "blocks" line should be equal to the count given by the Blocks header.

For a C file containing a single block, with length 20 bytes, we might see the following cache file:

Language: C
Blocks: 1

blocks
0,20

block 1 scs
0123456789ABCDEF

block 1 ids
5,12

This indicates that there is only one unique line in the block, having the given checksum (64 bits written as 16 bytes of ASCII hex using 0-F).

There is one id in the block, which can be found by skipping 5 bytes and then reading 7 more (12 - 5).

Note that the offset pairs (e.g. 0,20) under the "blocks" line are relative to the rev (e.g. the rev file's contents span).
However, the offset pairs under the "block N ids" lines are always relative to the block.
*/
/* #get_revs_cache_get @revs_cache_design

int get_revs_cache_get(span bname, span rev_contents);

Dual to get_revs_cache_put, we are given a basename of a rev and the contents of the rev file.

First we store cmp.end, and before returning we will reset it.

We look up the rev cache file using prs to construct the path.

We check if the file is readable, and return 0 if not (which will cause this rev to be cached again).

We read the cache file into cmp space.

Then we parse the file by calling a helper function parse_revfile_cache, and return its result.

*/

int get_revs_cache_get(span bname, span rev_contents) {
    u8* cmp_end_backup = cmp.end;
    span rev_cache_path = prs("%.*s/cache/v8/revs/%.*s", len(state->cmprdir), state->cmprdir.buf, len(bname), bname.buf);
    if (!readable_file(rev_cache_path)) return 0;
    span rev_cache_contents = read_file_into_cmp(rev_cache_path);
    int result = parse_revfile_cache(bname, rev_cache_contents, rev_contents);
    cmp.end = cmp_end_backup;
    return result;
}

/* #scan_checksum @checksums

checksum scan_checksum(span);

Here we scan 16 hex digits of ASCII input at the start of our input into our checksum type.

This function only returns if it is successful.

@complain_and_exit
*/

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

/* #scan_int

int scan_int(span*);

Here we read an int from the initial part of a span.
The span is modified to indicate how much of the span was consumed.

First we scan for an initial run of digits (only 0-9).
Then we use atoi to get the value.
(The .buf can be used directly due to how atoi works).
We the update the .buf and return.

If the initial chars are not digits, it is an error (in particular, we do not skip whitespace).
@complain_and_exit
*/

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

/* #parse_int

int parse_int(span); // parse int without mutating

Here we parse an int off the front of a span.

If the initial chars are not digits, it is an error (in particular, we do not skip whitespace).
@complain_and_exit

We use atoi to get the value.
(The .buf can be used directly due to how atoi works).
*/

int parse_int(span s) {
    if (empty(s) || !isdigit(*s.buf)) {
        prt("Error: initial characters are not digits\n");
        flush();
        exit(1);
    }
    return atoi((char *)s.buf);
}

/* #scan_hex @scan_int

int scan_hex(span*);

This is the same as #scan_int, except that hex input (0-9, a-f, A-F) is handled, and strtol is used.
*/

int scan_hex(span* s) {
    span orig = *s;
    while (s->buf < s->end && 
           ((*s->buf >= '0' && *s->buf <= '9') || 
            (*s->buf >= 'a' && *s->buf <= 'f') || 
            (*s->buf >= 'A' && *s->buf <= 'F'))) {
        advance1(s);
    }
    if (s->buf == orig.buf) {
        prt("Invalid hex input\n");
        flush();
        exit(1);
    }
    char* endptr;
    int result = strtol((char*)orig.buf, &endptr, 16);
    s->buf = (u8*)endptr;
    return result;
}

/* #parse_hex @parse_int

int parse_hex(span);

Like #parse_int, except that hex input (0-9, a-f, A-F) is handled, and strtol is used.
*/

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

/* #parse_revfile_cache @revs_cache_design @rev_info:code

int parse_revfile_cache(span bname, span rev_cache, span rev_contents);

@- (Manually written.)

*/

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

  //spans_arena_push();

  while (!empty(rev_cache)) {
    int section_type, block_number;
    int failure;
    parse_section_header_line(&failure, &section_type, &block_number, &rev_cache);
    if (section_type < 0) return 0;
    int rev_block_idx = n_existing_revblocks + block_number - 1;
    time_t timestamp = parse_rev_fname(bname);
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

  //spans_arena_pop();
  return 1;
}

/* #parse_section_header_line @revs_cache_design @parse_revfile_cache:code

void parse_section_header_line(int *failure, int *section_type, int *block_number, span *rev_cache);

Here we parse a single "section header line", by which we mean one of "blocks", "block <n> scs", or "block <n> ids".

First we read the first line off rev_cache.

We then parse it, handling each type of line.

If anything goes wrong or the line does not exactly match one of these patterns, we set *failure to a non-zero value and return.

If it works, we set *section_type to one of the SECTION_* constants, and *block_number to the <n> if appropriate.

*/

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

/* #parse_blocks_lines @revs_cache_design @rev_info:code

void parse_blocks_lines(int *failure, time_t timestamp, int n_blocks, span rev_contents, span* rev_cache);

All errors we report by setting *failure to a non-zero value and returning.

Here we parse "blocks" section integer pairs off the front of rev_cache until meeting a blank line.

We iterate over lines, until finding a blank one.
We also keep track of how many lines we have handled.

In each line, we split it at the comma.

If it does not contain a comma, that is an error.

We parse_int both the ints, before and after the comma.
We interpret the ints relative to rev_contents, which gives us a new span.

We use revblocks, n_revblocks, and cap_revblocks (on state->revs) to manage the revblocks array.
Specifically, if necessary, we double the cap and use realloc.
If it is zero, doubling won't work so we set it to 256 instead.

We then extend revblocks, with the new span being the .contents of the new rev_block.

We also set the timestamp.

If the number of lines we have handled is not equal to n_blocks, that is an error.
*/

void parse_blocks_lines(int *failure, time_t timestamp, int n_blocks, span rev_contents, span* rev_cache) {
    span line;
    int lines_handled = 0;

    while (!empty(*rev_cache)) {
        line = next_line(rev_cache);
        if (empty(line)) break;

        int comma_pos = find_char(line, ',');
        if (comma_pos == -1) {
            *failure = 1;
            return;
        }

        span first_int_span = take_n(comma_pos, &line);
        span second_int_span = skip_n(line, 1);

        int start_offset = parse_int(first_int_span);
        int end_offset = parse_int(second_int_span);

        if (lines_handled >= n_blocks) {
            *failure = 1;
            return;
        }

        if (state->revs.cap_revblocks == 0) {
            state->revs.cap_revblocks = 256;
            state->revs.revblocks = realloc(state->revs.revblocks, state->revs.cap_revblocks * sizeof(rev_block));
        } else if (state->revs.n_revblocks >= state->revs.cap_revblocks) {
            state->revs.cap_revblocks *= 2;
            state->revs.revblocks = realloc(state->revs.revblocks, state->revs.cap_revblocks * sizeof(rev_block));
        }

        span block_span = (span){ .buf = rev_contents.buf + start_offset, .end = rev_contents.buf + end_offset };

        state->revs.revblocks[state->revs.n_revblocks].contents = block_span;
        state->revs.revblocks[state->revs.n_revblocks].timestamp = timestamp;
        // XXX bugfix!!
        state->revs.revblocks[state->revs.n_revblocks].ids = spans_alloc(0);
        state->revs.n_revblocks++;
        lines_handled++;
    }

    if (lines_handled != n_blocks) {
        *failure = 1;
    }
}

/* #parse_scs_lines @rev_cache_design @rev_info:code @checksums @scan_checksum

void parse_scs_lines(int *failure, int rev_block_idx, span* rev_cache);

Here we parse a section body of sorted checksums lines off the front of rev_cache.

We first iterate over next lines of a copy until we find a blank line, so we know how many there are.

Then we allocate a checksums of the right number.
We iterate over the next_lines of rev_cache itself, and use scan_checksum on each line, pushing each one onto the sorted checksums.
Finally we assign it onto the appropriate revblock on (state->revs.revblocks).
*/

void parse_scs_lines(int *failure, int rev_block_idx, span* rev_cache) {
    span copy = *rev_cache;
    int num_lines = 0;

    while (!empty(copy)) {
        span line = next_line(&copy);
        if (len(line) == 0) break;
        num_lines++;
    }

    checksums cksums = checksums_alloc(num_lines);
    for (int i = 0; i < num_lines; i++) {
        span line = next_line(rev_cache);
        checksum cs = scan_checksum(line);
        checksums_push(&cksums, cs);
    }

    state->revs.revblocks[rev_block_idx].sorted_line_cksums = cksums;
}

/* #parse_ids_lines @revs_cache_design @parse_int

void parse_ids_lines(int *failure, int rev_block_idx, span* rev_cache);

Here we parse ids lines off the front of rev_cache until meeting a blank line.

If there is any parse failure we will indicate that by setting *failure to 1.

First count the non-blank lines, by making a copy of rev_cache and iterating until we find a blank line.
Then we alloc a spans to hold the ids, and we assign it directly in place onto state->revs.revblocks with the given index.
We put the contents span in a variable for use later.

Next we loop over the non-blank next lines again, but this time, directly using rev_cache rather than a copy (since the intention is actually to consume the lines).

For each line we split on the comma; if there is no comma it is an error.
Then we parse the ints before and after the comma.
We iterpret them as offsets into the contents of the revblock.
Adding them to the .buf of the contents gives us a new span, which we push onto the ids for the revblock.
Note that we must push them in place on the revblocks, not onto a local copy of the spans, as that won't update .n.
*/

void parse_ids_lines(int *failure, int rev_block_idx, span* rev_cache) {
    span cache_copy = *rev_cache;
    int id_count = 0;
    while (!empty(cache_copy)) {
        span line = next_line(&cache_copy);
        if (empty(trim(line))) break;
        id_count++;
    }

    // XXX: same bugfix!!
    //state->revs.revblocks[rev_block_idx].ids = spans_alloc(id_count);
    span contents = state->revs.revblocks[rev_block_idx].contents;

    while (!empty(*rev_cache)) {
        span line = next_line(rev_cache);
        if (empty(trim(line))) break;

        int comma_idx = find_char(line, ',');
        if (comma_idx == -1) {
            *failure = 1;
            return;
        }

        span before_comma = first_n(line, comma_idx);
        span after_comma = skip_n(line, comma_idx + 1);

        int start = parse_int(before_comma);
        int end = parse_int(after_comma);

        span id_span = { contents.buf + start, contents.buf + end };
        spans_push(&state->revs.revblocks[rev_block_idx].ids, id_span);
    }
}

/* #get_revs_cache_put @revs_cache_design @rev_info:code

void get_revs_cache_put(checksums* working_set, span bname, span content);

This function takes a working set, which we use to determine the best matching language for blockizing the rev.
This is an array of sorted line checksums, indexed congruently with state->files.

We also get the basename of the rev's file on disk, and the contents of the rev, as a span which points into the rev rope.

We set up a size for the working set by getting the .n of projfiles into a variable.

If the content is empty, there is nothing to do for this rev, so we do nothing and return.

We compare this file against the working set.
We use sorted_line_checksums on the file contents; this gives us a "fingerprint" of the lines in the file.
Whichever of the working set has the best match (the highest intersection) with this fingerprint we take as indicating the language.
In the event of a tie, we do not care which is used.
If there is no match, we just do nothing.

Since the working set indices match the projfiles, we use the .language on the projfile with the same index.

We then use find_blocks_language to get the blocks, assuming that language, for the rev.

Finally, we will populate the rev_info structure with each of the blocks from that rev, also writing to the cache.
Once we have gotten the blocks, we can realloc the revblocks if necessary to have room for the blocks from that rev, doubling the capacity.

For each rev block we must find:
- a timestamp,
- a spans of ids,
- sorted checksums of lines,
- the contents.

The timestamp for each block comes from the base filename for the rev.
We have another function to get a time_t from the filenames (which follow a consistent format).

Note that as we are going through the revs in reverse chronological order, that is the order our rev blocks are also stored in.

We use sorted_line_checksums on each block, as this will be used to compare the blocks for equality.

We also have a function to get the ids for the block.

Once we have the data we then write it into a cache file in an ASCII format as given by #revs_cache_design above.

@- TODO: fix the rest of this comment (the out2atp stuff was a bad idea and didn't work)

The name of this file is "<cmprdir>/cache/v8/revs/<bname>".
We expand this using prs and the cmprdir (from state) and bname (one of our arguments).
Then we call out2atp to redirect the output to append to that constructed path.
We call pr_revinfo(), which outputs the revinfo data, and then out_rst.

@- The idea of the name out2atp is that we are appending to a path; if we appended to a file, the file would need to exist, but a path is more abstract and it will be created; this is very similar to redirection of expressions into files as seen in awk and similar.

@- TODO: mmap the files into the rope; this lets the OS page out the memory which we mostly won't be needing to access
*/

void get_revs_cache_put(checksums* working_set, span bname, span content) {
    if (empty(content))
        return;

    size_t projfile_count = state->files.n;
    int best_match_index = -1;
    int max_intersection = -1;
    checksums rev_cksums = sorted_line_checksums(content);

    for (size_t i = 0; i < projfile_count; ++i) {
        int intersection = cksums_intersection(rev_cksums, working_set[i]);
        if (intersection > max_intersection) {
            max_intersection = intersection;
            best_match_index = i;
        }
    }

    if (best_match_index == -1)
        return;

    span language = state->files.a[best_match_index].language;
    spans blocks = find_blocks_language(content, language);

    //
    int prev_n_revblocks = state->revs.n_revblocks;

    if (state->revs.n_revblocks + blocks.n > state->revs.cap_revblocks) {
        state->revs.cap_revblocks = 2 * (state->revs.n_revblocks + blocks.n);
        state->revs.revblocks = realloc(state->revs.revblocks, state->revs.cap_revblocks * sizeof(rev_block));
    }

    time_t timestamp = parse_rev_fname(bname);
    rev_block *revblocks = state->revs.revblocks + state->revs.n_revblocks;

    for (size_t i = 0; i < blocks.n; ++i) {
        revblocks[i].contents = blocks.a[i];
        revblocks[i].sorted_line_cksums = sorted_line_checksums(blocks.a[i]);
        revblocks[i].ids = ids_for_block(blocks.a[i]);
        assert(revblocks[i].ids.n >= 0);
        revblocks[i].timestamp = timestamp;
    }

    state->revs.n_revblocks += blocks.n;

    span cmprdir = state->cmprdir;
    u8* end = out.end;
    u8* ce = cmp.end;
    //span cache_path = prs("%s/cache/v8/revs/%s", s(cmprdir), s(bname));
    //char *cache_path;
    //asprintf(&cache_path, "%.*s/cache/v8/revs/%.*s", len(cmprdir), cmprdir.buf, len(bname), bname.buf);
    //discard();
    //cmp.end = end; // this can't be here, must improve the library API
    //out_sav out_state = out2atp(S(cache_path));
    pr_revinfo(language, blocks, prev_n_revblocks, content);
    //flush();
    //out_rst(out_state);
    span output = (span){end, out.end};
    span cache_path = prs("%.*s/cache/v8/revs/%.*s", len(cmprdir), cmprdir.buf, len(bname), bname.buf);
    write_to_file_span(output, cache_path, 1);
    out.end = end;
    cmp.end = ce;
}

/* #pr_revinfo @revs_cache_design

void pr_revinfo(span language, spans blocks, int prev_n_revblocks, span contents);

For each section mentioned in #revs_cache_design, we output the data in the given ASCII format, calling out to appropriate helper functions as necessary.
This populates the cache for an entire rev, handling all the blocks in a given rev at once.

void pr_checksum(checksum);
void pr_relative_span(span,span);

In pr_checksum we print a checksum as 16 ASCII digits, using the alphabet 0-9 and A-F.
Note that we specifically print uppercase hex digits, e.g. using %X rather than %x in C format strings.

In pr_relative_span we print two non-negative integers separated by a comma, i.e. "%ld,%ld".
The integers are the .buf and the .end offsets of $2, both expressed relative to the .buf of $1.
In other words, the function prints the location of the second span relative to the first one, as a pair of integers both in [0,len($1)].
To rehydrate this data back into a span, it must be scanned relative to a containing span, e.g. a memmapped file.

In pr_revinfo we print the block ids relative to the block, for example, `pr_relative_span(rb.contents, rb.ids.a[k])` for some revblock rb and 0 < k < n ids.

The pr_revinfo function takes language and blocks as arguments.
The reason the blocks are passed in as an argument (rather than looking them up on state->revs) is that we only want to handle (or iterate over) one rev's worth of blocks.
First we print the header lines with the given language and the .n of the blocks.
Then we iterate over the blocks three times.
The first time we are printing the offsets (from the rev) for each block below the "blocks" line.
The second time we are printing a "block N scs" line for each block followed by the sorted checksums, with the help of pr_checksum.
Finally we are printing a "block N ids" line for each block if it has at least one id, followed by the offset pairs (this time block-relative), one per line of the ids for that block (note that a block is allowed to have zero or more ids).

Here we write all three functions.
*/

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
        checksum* scs = sorted_line_checksums(block).a;
        size_t scs_count = sorted_line_checksums(block).n;

        prt("\nblock %ld scs\n", i + 1);
        for (size_t j = 0; j < scs_count; j++) {
            pr_checksum(scs[j]);
        }
    }

    for (size_t i = 0; i < blocks.n; i++) {
        rev_block rb = state->revs.revblocks[prev_n_revblocks + i];
        //rev_block rb = blocks.a[i];
        if (rb.ids.n > 0) {
            prt("\nblock %ld ids\n", i + 1);
            for (size_t j = 0; j < rb.ids.n; j++) {
                pr_relative_span(rb.contents, rb.ids.a[j]);
            }
        }
    }
    //flush();
}

/* #prs_checksum

span prs_checksum(checksum);

manually written
*/

span prs_checksum(checksum c) {
  out_sav sav = out2cmp();
  span ret = {cmp.end};
  pr_checksum(c);
  bksp();
  ret.end = cmp.end;
  out_rst(sav);
  return ret;
}
/* #SBV_design @rev_info

We will maintain some state for the "select block version" (SBV) ui that will be shared among several routines.

Specifically:

- we have a list of blocks that are judged as similar enough to the current one to be prior versions of it.
    - these are just indices into the revblocks, so maintained as an array of ints
    - this array is malloc'd in select_block_version and freed at the end, and has size equal to state->revs.n_revblocks, the theoretical max
    - called revblock_indices (since they are indices into the revblocks on state)
- we track the highest index on this block list that has already been assigned, `max_index`
- we also track the currently displayed index, initially zero, affected by j/k, `current_index`
- we have the block ids of the current block, used for similarity determination, `curr_block_ids` as a spans.
- we have a checksums, `sorted_line_cksums`, for the current block's fingerprint.

As the user goes through the undo history with j/k, the revblocks will be searched to find the next sufficiently similar block (when moving past the ones already found).
The max_index represents the "high water mark" of how far back we have gone with j/k; only hitting "j" can ever bump max_index.

So we have several functions below:
- to find the next match and add it if not already added
- to display the current state of things
- to actually handle keyboard input and return a "keyboard event".

All of these take a pointer to our state struct for this feature.
The entry point is select_block_version, which sets up this struct and loops.

We call the struct the sbv_state after the name of the feature.
*/
/* #getkey

int getkey();

Here we handle the keyboard input.

This may be abstracted later to replace getch.

First we declare some constants for non-ASCII and non-UTF8 keyboard events, such as arrow keys.
Since ordinary 8-bit char inputs will be in range 0-255, we start with 256.
These are ints and we define for now just the four arrow keys ARROW_{U,D,L,R}.
(Ordinary ASCII (and UTF-8) input will just be returned as itself.)

@- We turn off canonical mode and echo on the terminal, and set the minimum characters to 0 and the timeout to 1/10 of a second.
We turn off canonical mode and echo on the terminal, and set the minimum characters to 1 and the timeout to 1/10 of a second.

Then we read a character, handling EAGAIN until we get input.

If the input is an escape char, we read the next two bytes and handle them with an int used as a state machine.
If they are one of the arrow key escape sequences then we will store our arrow key constant as the ret value.
All other non-escape characters will be sent as themselves of course.
Any sequence starting with escape that isn't followed by two more bytes of input without timeout will just be sent as an escape itself ('\033').
Finally, an escape sequence that's not an arrow key will be ignored (returning to the EAGAIN loop).

Once we have some input that we are ready to return, we set the terminal back the way it was before doing so.

Here we define the four arrow key constants and the getkey function.
*/

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

/* #sbv_display @SBV_design @blocks

void sbv_display(sbv_state* sbvs);

We have (from the feature-specific state) the index of the current rev block to be displayed, and here we display it.

To actually get this rev block index, we take current_index on the sbv state as an index into the revblock indices.
That revblock index in turn indexes the state->revs.revblocks, which contains the block contents and the timestamp, both of which we need here.

First we clear the display.

We print on the top line, "Block N, ver: <offset>, <timestamp>, <help>", where N is the current block number, <offset> is either "curr" if it is the current version (which is the 0th current_index on the sbv_state, which is the current state of the block (as there is always a rev for the current state)), or a negative number, which is just "-" followed by the current index on the sbv state, i.e. the number of revs that the user has gone back in the history, and the timestamp is the timestamp for that revblock, in the format "YYYY-MM-DD hh:mm:ss", in the local time zone and accounting for DST.
(For the negative numbers or "curr", we set up a char* first.)

The "help" part is a short keyboard gloss, "j/k, Enter, q", summarizing the basic keyboard commands in this mode.

Then we print the contents with wrs, and then below those we print the same "Block..." line again, but without a newline this time.

@- Then we use count_physical_lines to print the terminal_rows - 2 rows of the actual content of the block.
@- (Later we will support pagination in this mode but not yet.)
*/

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

/* #sbv_populate @SBV_design @blocks @checksums:code

void sbv_populate(sbv_state* sbvs);
int block_id_match(spans curr_ids, spans rev_ids);
int rev_block_match(sbv_state* sbvs, rev_block* current_revblock);

(Here we write all three functions.)

There is a current index (on the feature-specific state) which changes in response to j/k.
Here we need to make sure that if the current index is greater than the max index (which records the amount of the revblock_indices that we have populated already), that we populate the corresponding array element.

If the current index is greater than the max index, it will only ever by by one.

As a special case, if the max index is -1, the current_index is 0, then we populate the 0th revblock index and also update max_index, by iterating over the revblocks until we find one which is span_eq to the current value of the block that we are actually on (i.e. the value of state->blocks indexed by state->curr_block_idx), and setting the 0th revblock index to that first matching revblock in the history.

So we get the current max index, get the corresponding revblock index, so that we can start iterating over the revblocks from that point.
Then we go back through the revblocks (on state->revs).

(That is, actually, we are going forward through the list, but as they are in reverse chronological order, this is backwards in time.)

We find the next revblock that matches our similarity criteria, as follows:
- if the revblock has the same contents (as per span_eq) as any of the revblocks already indexed (by the revblock_indices) as a version of this block, then we don't want to see it again, so we consider this a non-match.
- if any of the block ids for the current block match any of the block ids for the revblock, then we consider it a match
- otherwise we find three numbers: the number of unique lines from the current block, the number of unique lines from the rev block, and the intersection (which we have a function to find).
(The unique line counts are just .n on the respective sorted checksums.)
(The sorted checksums are also unique.)
If each of these numbers is greater than 8 we consider this a match.

Finally, if there is no other match, we will leave max_index as it originally was, and decrement current_index (essentially indicating that the 'j' or 'Down' failed because there is nothing further down).

*/

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
                if (rev_block_match(sbvs, &state->revs.revblocks[i])) {
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

int rev_block_match(sbv_state* sbvs, rev_block* current_revblock) {
    for (int i = 0; i <= sbvs->max_index; i++) {
        if (span_eq(current_revblock->contents, state->revs.revblocks[sbvs->revblock_indices[i]].contents)) {
            return 0;
        }
    }
    if (block_id_match(sbvs->curr_block_ids, current_revblock->ids)) {
        return 1;
    }
    int curr_uniq = sbvs->sorted_line_cksums.n;
    int rev_uniq = current_revblock->sorted_line_cksums.n;
    int intersection = cksums_intersection(sbvs->sorted_line_cksums, current_revblock->sorted_line_cksums);
    return (curr_uniq > 8 && rev_uniq > 8 && intersection > 8);
}

/* #select_block_version @SBV_design @getkey

void select_block_version();

If curr_block_idx is -1, this operation makes no sense, so we just return.

First we call get_revs.
(Since this is somewhat slow, it is only called when needed, rather than from main.)

We set up an sbv_state struct sbvs.
The max_index begins at -1, the current_index at 0, and we allocate (and later free) the memory for the revblock indices.
We get the block ids for the current block by calling a function and store them on the feature state struct.
We calculate the sorted checksums for the current block.
Then we enter our loop, which:
- calls sbv_populate to find the currently displayed block revision if necessary
- displays the current state
- gets a key, and then:
  - if it is q or Esc, do nothing and simply return (except our cleanup)
  - if it is j/k or an up/down arrow, we adjust the current index on the sbv state
    - if k or up, we increment current_index unconditionally
    - however if j or down, we only decrement if 0 < current_index
  - if it is Enter, we replace the block with the new contents using replace_block, and cleanup and return.

The mapping of j/k here is the opposite of navigating blocks: j (or up arrow) decrements current_index, and k or down arrow increments.
That's because it is more natural to have earlier times represented physically above later times, and our index is reverse chronological, so earlier times have higher indices.

The reason we increment unconditionally, rather than only up to some maximum, is because sbv_populate needs to have a chance to see whether the next revision of this block exists before we know if we are at the end.
*/

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

/* #sorted_line_checksums @checksums
checksums sorted_line_checksums(span);

In a first pass we count the lines in the input.
We allocate a checksums of this many.
Then we take lines one by one, get the selected checksum for each one, and add it to the checksums.

We sort the checksums using qsort.
For this we need to declare a comparator function, handling the void* type that qsort expects.

Though gcc does allow this function to be declared inline, for compatibility with clang we declare it immediately before sorted_line_checksums itself.

We remove duplicates by keeping track of an offset and overwriting forward in a single pass over the sorted list.
*/

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

/* #cksums_intersection @checksums

int cksums_intersection(checksums,checksums);

We are given two checksums, a and b, which will be sorted.

We return in linear time the count of checksums appearing in both lists, i.e. the intersection.

To do this we iterate over both of the arrays in parallel, advancing whichever is lower when they are not equal, and when they are equal, advancing our intersection count and advancing both of them.
*/

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

/* @new_rev
time_t parse_rev_fname(span);

We get a filename, only the basename part, in the format described above, and parse it into a time_t.

@- maybe this is where the DST bug is?
*/

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

/*
In find_blocks_language_python, we get a span containing a file.

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

/*
In find_blocks_language_c, we get a span containing a file.

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

/*
In find_blocks_language_markdown, we get a span containing a file.

We write two loops.
In the first one we count the blocks, then we spans_alloc our return value with the correct number, and in the second loop we assign the spans.

For a Markdown file, a block starts with any heading, which we define as a line starting with a "#" flush left.
It ends where the next block starts.

There are some special cases:

If the file is empty, we return a single empty block.
We handle this as a special case, since it doesn't really work with our two-loop approach.
We also care about where the blocks are, even if they are empty, so in this case we ensure that the empty block we return is the same as the empty span of the file itself (i.e. .buf and .end are equal to each other, and to those of the file: we can NOT use nullspan() here).

If a file does not begin with a block, then the first block will just be from the beginning of the file to the first "block" proper.
For markdown, this means that if the file doesn't begin with a heading, the first block will be everything before the first heading.

The last block always goes to the end of the file.

Before the first loop we copy our argument into a new span `copy` so that we can consume it while counting, but then still have access to the original full span for the second loop.

The block-finding loop:
- While the remaining input is not empty, we get the next line. (Note that next_line will return everything left if there is no newline).
- If this line starts with the pattern, or if it is the first line in the file, then it begins a block (regardless of whether the line is empty), otherwise we simply do nothing and continue the loop.
- If we are counting blocks, we increment our counter, otherwise we assign .buf of a span for this block, and if we had a previous block, we assign .end of that block to the same offset.
- When we reach the end of the input we will assign .end of the last block to the end of the input.

(To determine if we are at the start of the input, we can compare .buf of the line with that of the input.)
*/

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

/* #find_blocks_language @langtable

In `spans find_blocks_language(span,span)`, we take a span (a file's contents) and a language, which is any of the supported languages.

We dispatch to another function that handles that language appropriately, and return the resulting spans.

We dispatch according to the "find blocks by language" implementation given by #langtable col. 3., above.

If the language is not known, we prt, flush, exit as per usual.
*/

spans find_blocks_language(span file_contents, span language) {
    if (span_eq(language, S("C"))) {
        return find_blocks_language_c(file_contents);
    } else if (span_eq(language, S("Python"))) {
        return find_blocks_language_python(file_contents);
    } else if (span_eq(language, S("JavaScript"))) {
        return find_blocks_language_c(file_contents);  // Note: JavaScript uses C rules.
    } else if (span_eq(language, S("Markdown"))) {
        return find_blocks_language_markdown(file_contents);
    } else {
        prt("Error: Unsupported language.");
        flush();
        exit(1);
    }
}

/*
Function getch to read a single character without echoing it to the terminal.

char getch();

@- TODO: we can replace with the more sophisticated getkey that we defined later
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

/* #main_loop

void main_loop();

In main_loop, we initialize:

@- moved to get_code: the current index (this is the j/k index) to 0,
- the marked index to -1 (indicating that we are not in visual selection mode).

In our loop, we call check_conf_vars(), which just handles the case where some essential configuration variables aren't set.

Next we clear the terminal, then print the current block or blocks.
Then we will wait for a single keystroke of keyboard input using our getch() above.
@- (This will change to something like we are using in the select-menu loop elsewhere.)
Just before we call this function, we also call flush(), which just prevents us having to call it a lot of other places all over the code.
@- probably both the clear_display and the flush should be in print_current_blocks

Once we have a keystroke, we will call another function, handle_keystroke, which takes the char that was entered and dispatches all our keybindings.
Before calling this function (but after getting the char) we will set state->now to the current time using clock_gettime and CLOCK_REALTIME.
(This gives us a consistent value of "now" which persists for each main loop entry, which is useful.)

@- An idea here is to allow injecting a member onto ui_state from here, with some syntax like "@ui_state: ..." on a line.
@- The idea is that we can say here that we have a struct timespec on state.
@- Otherwise, completing the feature requires us to go and update ui_state, run :bootstrap, and then regenerate this code again.
@- That's annoying, and it's also annoying that we have to describe the member in ui_state, where what we really would like to say is "this struct holds values that are needed by all the features that we have, and read those features for how those are used."
@- In other words, the feature (in filename_variables) is the reason why we are adding state->now, and really the changes to main_loop might be described there, just like the changes to ui_state could be better described here.
*/

void main_loop() {
    state->marked_index = -1;

    while (1) {
        check_conf_vars();
        clear_display();
        print_current_blocks();
        flush();

        char ch = getch();
        clock_gettime(CLOCK_REALTIME, &state->now);

        handle_keystroke(ch);
    }
}

/* #count_physical_lines

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

/* #render_empty_project_state #render_empty_file_state @print_single_block_with_skipping @files @prt_usage

void render_empty_project_state();
void render_empty_file_state();

In these two functions we output a "Block" line similar to that in #print_single_block_with_skipping, but with "-" instead of a number.

We then print one of the messages:

- "The project is empty, use :allfiles to add all files in the project directory to the project, or edit .cmpr/conf to add files manually."
- "The file %.*s is empty, hit 'e' to edit it." (with the .path of the current file, which will be set).
*/

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

/* #keybinds

We support the following single-char inputs:

j/k      Go up or down one block. If we are at the first or last block, these are no-ops.
g/G      Go to the first or last block resp.
e        Edit the current block in $EDITOR (or vi by default)
'        open the "prompt palette"
r        LLM rewrite of code part based on comment part
R        Used in clipboard mode to read code back into the current block.
U        List versions of current block and go back to a previous one (undo).
D        List recently deleted blocks (i.e. that don't match a block currently in the project).
space/b  Scroll one page down or back up.
B        Build by running the build command provided as buildcmd in .cmpr/conf, e.g. 
@- - v, sets the marked point to the current index, switching to "visual" selection mode, or leaves visual mode if in it
/        switches to search mode
:        switches to ex command line
n/N      repeat last "/" search in forward/backward direction
#        opens block id jump list
?        display brief help about the keyboard commands
q        exit (prt "goodbye\n", flush, exit)
*/
/* #handle_keystroke @keybinds

void handle_keystroke(char);

We call terpri() on the first line of this function (just to separate output from any handler function from the ruler line).

Implemented inline: q
All others call helper functions already declared, e.g.:
B -> compile()
R -> replace_code_clipboard()
? -> keyboard_help()
j/k/g/G -> handle_{j,k,g,G}

@- Any of j,k,g,G,n,N that change the curr_block_idx must also reset pagination (scrolled_lines -> 0).
@- These should all be moved into implementations, and this function just dispatches.
*/

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
        case '\'':
            prompt_palette();
            break;
        case 'r':
            nl2pl_rewrite();
            break;
        case 'R':
            replace_code_clipboard();
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

/* #keyboard_help

void keyboard_help();

To get the help text we basically copy the lines in #handle_keystrokes above, except formatted nicely for terminal output.
We split out ones like j/k and g/G onto their own lines though.
Include all relevant details about usage that might be non-obvious, e.g.:
- r "puts a prompt on the clipboard to rewrite the code part based on comment part"
Include mnemonic hints where given (e.g. "back" for b).

We call clear_display first, and flush, getch after, so the user has time to read the help (we prompt them about this).
*/

void keyboard_help() {
    clear_display();
    prt("Keyboard shortcuts:\n");
    prt("j    - Go down one block\n");
    prt("k    - Go up one block\n");
    prt("g    - Go to the first block\n");
    prt("G    - Go to the last block\n");
    prt("e    - Edit the current block in $EDITOR\n");
    prt("'    - Open the prompt palette\n");
    prt("r    - Rewrite code part based on comment part; clipboard updated\n");
    prt("R    - Replace code part with clipboard contents\n");
    //prt("u    - Undo\n");
    prt("space- Paginate down within a block\n");
    prt("b    - Paginate up (\"back\") within a block\n");
    prt("B    - Build project with provided command\n");
    //prt("v    - Toggle visual selection mode\n");
    prt("/    - Enter search mode\n");
    prt("#    - Open block id jump list\n");
    prt(":    - Enter ex command line\n");
    prt("n    - Repeat search forward\n");
    prt("N    - Repeat search backward\n");
    //prt("S    - Enter settings mode\n");
    prt("?    - Display this help\n");
    prt("q    - Quit\n");
    prt("\nPress any key to return...\n");
    flush();
    getch();
}
/* #handle_jkgG @jk_implementation

Here we write the four functions that handle keyboard navigation commands, as described above under #jk_implementation.

void handle_j();
void handle_k();
void handle_g();
void handle_G();
*/

void handle_j() {
    if (state->curr_file_idx == -1) return;
    
    if (state->curr_block_idx == -1) {
        if (state->curr_file_idx + 1 < state->files.n) {
            state->curr_file_idx += 1;
            if (empty(state->files.a[state->curr_file_idx].contents))
                return;
            state->curr_block_idx = first_block_in_file(state->curr_file_idx);
            state->scrolled_lines = 0;
        }
    } else {
        if (state->blocks.a[state->curr_block_idx].end == state->files.a[state->curr_file_idx].contents.end) {
            if (state->curr_file_idx + 1 < state->files.n) {
                state->curr_file_idx += 1;
                if (empty(state->files.a[state->curr_file_idx].contents)) {
                    state->curr_block_idx = -1;
                    return;
                }
                state->curr_block_idx = first_block_in_file(state->curr_file_idx);
              state->scrolled_lines = 0;
            }
        } else {
            if (state->curr_block_idx + 1 < state->blocks.n) {
                state->curr_block_idx += 1;
                state->scrolled_lines = 0;
            }
        }
    }
}

void handle_k() {
    if (state->curr_file_idx == -1) return;
    
    if (state->curr_block_idx == -1) {
        if (state->curr_file_idx - 1 >= 0) {
            state->curr_file_idx -= 1;
            if (empty(state->files.a[state->curr_file_idx].contents))
                return;
            state->curr_block_idx = last_block_in_file(state->curr_file_idx);
            state->scrolled_lines = 0;
        }
    } else {
        if (state->blocks.a[state->curr_block_idx].buf == state->files.a[state->curr_file_idx].contents.buf) {
            if (state->curr_file_idx - 1 >= 0) {
                state->curr_file_idx -= 1;
                if (empty(state->files.a[state->curr_file_idx].contents)) {
                    state->curr_block_idx = -1;
                    return;
                }
                state->curr_block_idx = last_block_in_file(state->curr_file_idx);
                state->scrolled_lines = 0;
            }
        } else {
            state->curr_block_idx -= 1;
            state->scrolled_lines = 0;
        }
    }
}

void handle_g() {
    if (state->files.n == 0) return;

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

    state->curr_file_idx = state->files.n - 1;
    if (empty(state->files.a[state->files.n - 1].contents)) {
        state->curr_block_idx = -1;
    } else {
        state->curr_block_idx = state->blocks.n - 1;
        state->scrolled_lines = 0;
    }
}

/* #first_block_in_file #last_block_in_file @files @blocks @file_for_block

int first_block_in_file(int);
int last_block_in_file(int);

Here we find the first or last block in a non-empty file.

We can simply iterate over the blocks, forwards or backwards, and return the first one we find that is a match according to file_for_block.

If nothing matches we return -1.
*/

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

/* #start_search

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

void start_search() {
    static char search_buffer[256] = {"/"}; // Static buffer for search, pre-initialized with "/"
    state->search = (span){.buf = (u8*)search_buffer, .end = (u8*)search_buffer + 1}; // Initialize search span to contain just "/"

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

/* #start_ex

To support ex commands, we have a static buffer (declared in start_ex() below) of length 256 which is used to hold the ex command while the user is typing it.
The first byte of this buffer is always ":".

In start_ex, to indicate that we are going into ex mode, we set ex_command on the state to a span that points at this static buffer, and includes only the ":", since this is what was just typed to get into ex command mode.
We can do this using S(), but we prefer to just directly construct the span.
Then we print the line at the bottom starting with ":" (which is currently the only thing on it) as described below, to indicate to the user that we're now in ex command entry mode.

However, if we've deleted the initial colon, that means the user doesn't want to be in ex command mode any more.
If the state element is empty it always means we are not in the mode.
If this happens we call print_current_blocks() to refresh the display, and then return.

As with search mode, we implement our own input handling with a getch() loop, handling both kinds of backspace, and if enter is hit, we will call handle_ex_command().
On any backspace character we will simply shorten the span (.end--) and on any other input at all we will extend it.
In particular, we want to transparently support UTF-8 input, so if the byte is not a backspace byte we simply append it without any further testing.

In our loop, unlike with search mode, we are not continuously changing what's displayed on the main area of the screen while the user is typing, so we can print an ANSI escape code to move to and clear the last row of the screen and then write our ex_command buffer on this last terminal row (including the ":").
(Obviously, we use prt() just like everywhere else in this codebase, not randomly printf for no reason.)
*/

void start_ex() {
    static char ex_buf[256] = ":";
    state->ex_command = (span){(u8*)ex_buf, (u8*)ex_buf + 1};

    prt("\033[%d;1H\033[K", state->terminal_rows);
    prt("%.*s", len(state->ex_command), state->ex_command.buf);
    flush();

    char ch;
    while ((ch = getch()) != '\n') {
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

/* #extable

The ex commands are defined as a table:

1. Supported ex commands:

:bootstrap, :config, :addfile, :addlib, :allfiles, :help, :model, :expand

2. Implementation functions:

bootstrap(), addfile(span), addlib(span), ex_help(), select_model(), ex_expand()

3. Arguments:

:addfile, :addlib:
  file path to add.

all others:
  no arguments.

4. Help text:

bootstrap:
  Run the user-provided bootstrap command, putting the result on the clipboard.

config:
  Edit and reload the config file.

addfile, addlib:
  Add a file or library to the project (adds file: or lib: line to conf).

allfiles:
  Adds all the files in the project directory to the conf file.

help:
  Print short help on available ex commands.

model:
  Select the LLM to use for "r" and other commands.

expand:
  Expands block references and displays the expanded result.
*/
/* #handle_ex_command @extable

Once the ex_command on the state is set up, this function actually handles it, and clears ex_command to leave ex command entry mode.
*/

// stubbed for now (manually)
void addfile(span s) {}
void addlib(span s) {}

void handle_ex_command() {
    if (starts_with(state->ex_command, S(":bootstrap"))) {
        bootstrap();
    } else if (starts_with(state->ex_command, S(":addfile"))) {
        span file_path = skip_n(state->ex_command, len(S(":addfile ")));
        addfile(file_path);
    } else if (starts_with(state->ex_command, S(":addlib"))) {
        span lib_path = skip_n(state->ex_command, len(S(":addlib ")));
        addlib(lib_path);
    } else if (starts_with(state->ex_command, S(":help"))) {
        ex_help();
    } else if (starts_with(state->ex_command, S(":model"))) {
        select_model();
    } else if (span_eq(state->ex_command, S(":expand"))) {
        ex_expand();
    }
    state->ex_command = nullspan();
}
/* #ex_help @extable

In ex_help we print the help messages given in #extable Col. 4 above.

Start with a newline, as the cursor will still be on the ex command line (from :help).

Each line should have the format ":command - $help_message$".

We flush and then getch() so the user can see it before returning to the main loop.
We prompt the user with "Press any key to continue...".
*/

void ex_help() {
    prt("\n");
    prt(":bootstrap - Run the user-provided bootstrap command, putting the result on the clipboard.\n");
    prt(":help - Print short help on available ex commands.\n");
    prt(":model - Select the LLM to use for \"r\" and other commands.\n");
    prt(":expand - Expands block references and displays the expanded result.\n");
    flush();
    prt("Press any key to continue...");
    flush();
    getch();
}

/* #set_highlight

The functions set_highlight and reset_highlight are helper functions for highlighting terminal output as black on white.
*/

void set_highlight() {
    prt("\033[7m");
}

void reset_highlight() {
    prt("\033[0m");
}

/* #print_menu

void print_menu(spans,int);

Here we show the user a list of options, and let them pick one.

Our arguments are a spans containing the options and a currently selected index into it.

First we clear the display.

We check the terminal height (from the state) and compare it to the number of options we've been given.
We also need to leave one row at the bottom for the instruction to the user.

We want to always have the selected option at the same row on the terminal.
First we find this row.
We take the total terminal rows, subtract one for the prompt line, and one for the selected row itself, and then take half of this.
That's the number of lines that we want to fill before we print the selected option, and also the max that we can fill after it.

To fill this space, we might have exactly enough options (prior to the selected one), or not enough, or too many.

If there are too many or exactly enough, we skip any initial options that we need to skip.
If there are not enough, we as many blank lines as necessary.

Then we print the options prior to the selected one.
Then we print the selected one.
Finally we print the rest of the options, up to the max we can fit.
Then we move the cursor to the last line with an escape code, and print the user prompt, without a newline:

"j/k or Up/Down to move, Enter to select, q to cancel"

@set_highlight
*/

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

/* #select_menu

int select_menu(spans,int);

In select_menu we allow the user to choose from a small list of options passed in as a spans.
The currently selected option is passed in as an int, or -1 if there is no current selection.
In this case, we should start with the 0th element selected.

We have a helper function print_menu which takes a spans of the options and a currently selected index and handles the screen updates.

We enter a loop, handle j/k and up/down arrow keys to highlight, enter for selecting, and q to exit without selecting anything, which we indicate by returning -1.
@- Someday we'll probably add Esc support here, see #escape_handling_issue.

Arrow keys are represented as multiple characters of input so you'll need to maintain some state to handle them correctly.
In particular, if you ever write something like '\033[B' it won't compile, so use nested calls to getch(), or maintain a small state machine as an int.

We return the index of the user's selection.
*/

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

/* #select_model

Here we allow the user to select the model.

We set up a spans and the currently selected index and then call select_menu.

The list of models:

- "gpt-3.5-turbo"
- "gpt-4-turbo"
- "gpt-4o"
- "llama.cpp"
- all ollama models listed in state->ollama_models
- "clipboard"

The initially selected option should be the one that matches state->model.

When select_menu returns we update state->model and call save_conf to store any change back to the conf file.

We can avoid leaking spans arena memory with the appropriate _push and _pop functions around the entire function body.
*/

void select_model() {
    spans models = spans_alloc(7 + state->ollama_models.n);
    spans_arena_push();
    models.a[0] = S("gpt-3.5-turbo");
    models.a[1] = S("gpt-4-turbo");
    models.a[2] = S("gpt-4o");
    models.a[3] = S("llama.cpp");
    models.a[4] = S("clipboard");
    for (int i = 0; i < state->ollama_models.n; i++) {
        models.a[5 + i] = state->ollama_models.a[i];
    }
    models.n = 5 + state->ollama_models.n;

    int selected_index = index_of(state->model, models);
    if (selected_index == -1) selected_index = 0;

    selected_index = select_menu(models, selected_index);

    if (selected_index >= 0 && selected_index < models.n) {
        state->model = models.a[selected_index];
        save_conf();
    }

    spans_arena_pop();
}

/* #bootstrap

Here we make sure that the conf variable bootstrap is set, and then run it.

The message is "The bootstrap command you provide should generate your initial prompt on stdout. It will be sent to the clipboard for you. See README for details.".

This is similar to #compile above.

However, instead of just running it and letting the output go to the terminal, we capture the output in a span using pipe_cmd_cmp(), which returns a span with the piped data.

We store this on state->bootstrapprompt and send it to the clipboard.
*/

void bootstrap() {
    ensure_conf_var(&state->bootstrap, S("The bootstrap command generates your initial prompt on stdout. See README for details."), nullspan());
    
    char buf[2048] = {0};
    s_buffer(buf, sizeof(buf), state->bootstrap);
    prt("Running bootstrap command: %s\n", buf);
    flush();

    state->bootstrapprompt = pipe_cmd_cmp(S(buf));
    send_to_clipboard(state->bootstrapprompt);
}
/* #perform_search

In perform_search(), we update the display after the search string has been updated.

The search string (span state.search) will always start with a slash.
We remove this and take the rest of it as the actual string to search for.
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
/* #print_ruler @prt_usage @jk_order @blocks @files

void print_ruler();

In print_ruler we use prt to show:

- the number of blocks
- the currently selected block
- the scrolled lines plus one (i.e. the one-based index of the top visible line)
- the filename of the current block
- the currently selected LLM (state->model)
- either a short note "? for help", or a string returned from get_debug_info()

all on a line without a newline.

Our output reads as "Block n/N, Line L, File <path>, Model <model>, ? for help".

We call get_debug_info(), and usually the result will be empty.
However, if it is not empty, we assume the user doesn't need to see the "? for help" part of the ruler line, so we replace this section with the debug info in this case.

If we are in the empty project state, we use "-/0" for the block and "-" for the file and line.
In the empty file state we show "-/N" for the block and "-" for the line.

Implementation:

First we set up some spans for convenience, and then we print everything.

@- TODO: the visible line index (instead of logical line index) is kind of useless
*/

void print_ruler() {
    span current_file_path = (state->curr_file_idx != -1) ? state->files.a[state->curr_file_idx].path : S("-");
    span model = state->model;
    span debug_info = get_debug_info();

    int block_count = state->blocks.n;
    int current_block_number = (state->curr_block_idx != -1) ? state->curr_block_idx + 1 : 0;

    if (state->curr_file_idx == -1 && state->curr_block_idx == -1) {
        prt("Block -/0, Line -, File -, Model %.*s", len(model), model.buf);
    } else if (state->curr_block_idx == -1) {
        prt("Block -/%d, Line -, File %.*s, Model %.*s", block_count, len(current_file_path), current_file_path.buf, len(model), model.buf);
    } else {
        int top_visible_line = state->scrolled_lines + 1;
        prt("Block %d/%d, Line %d, File %.*s, Model %.*s", current_block_number, block_count, top_visible_line, len(current_file_path), current_file_path.buf, len(model), model.buf);
    }

    if (empty(debug_info)) {
        prt(", ? for help");
    } else {
        prt(", %.*s", len(debug_info), debug_info.buf);
    }

    flush();
}

/* #get_debug_info @prt_usage

span get_debug_info();

We start with an empty span, and possibly add things to it, and finally return it.

If state->debug contains "sa", we add the high point of the spans arena and the total as "n/N".
We can read this from spans_global_arena.arena_size and .allocated, both size_t.

If it contains "inp", we add len(inp), as "inp:n", where n is len(inp).
When we add something, we check if our return value is already non-empty, and add a space if it is.
(We can do this with prs.)
*/

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

/* #print_single_block_with_skipping

In print_single_block_with_skipping we get a block index and a pagination index in the form of a number of lines already "scrolled off" above the top of the screen (skipped_lines).

First, we call count_physical_lines, which gives us a span of skipped lines and alters an int, subtracting the number of physical lines which this span represents.
We make a copy of the block and adjust this copy to the suffix which is meant to be aligned to the top of our content area, by setting the .buf of the copy to the .end of the scrolled-off span.

We set a variable remaining_rows which we initialize with state->terminal_rows and decrement as we print lines of output.
First we print a line "Block N" and decrement this variable by one.

We have a ruler line at the bottom that we need to leave room for, so we make another variable, remaining_content_lines, that is one less than remaining lines, and call count_physical_lines again with this variable, letting us determine how many lines are actually printed, and more importantly, giving us a span of the appropriate content to at-most fill the screen.

We then print this content by wrs().
We then ...
Finally, we call print_ruler to handle the last line of the terminal.
*/

void print_single_block_with_skipping(int block_index, int skipped_lines) {
    span block = state->blocks.a[block_index];
    int physical_lines = skipped_lines;
    span skipped_span = count_physical_lines(block, &physical_lines);
    span block_suffix = block;
    block_suffix.buf = skipped_span.end;

    int remaining_rows = state->terminal_rows;
    prt("Block %d\n", block_index + 1);
    --remaining_rows;

    int remaining_content_lines = remaining_rows - 1;
    span content_to_print = count_physical_lines(block_suffix, &remaining_content_lines);
    wrs(content_to_print);

    /* *** manual fixup *** totally failed to get GPT4 to write this */
    while (remaining_content_lines-- > 0) {
        terpri();
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
/* #finalize_search

In finalize_search(), we update the current block to point to the first result of the search given in the search string.
We ignore the first character of state.search which is always slash, and find the first block which contains the rest of the search string (using contains()).
Then call set_current_block().
We then reset state.search to an empty span to indicate that we are not in search mode any more, but we put the search on state.previous_search so that 'n' and 'N' can work.
*/

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

/* #search_forward #search_backward

In these two functions (used by n/N) we first get the sequence of blocks which match state.previous_search.
If this is empty, we do nothing.

As this includes the leading slash, we first strip that (using skip_n), then use contains() to find the blocks which match.

We then find either the lowest block greater than curr_block_idx which contains a match, for a forward search, or the highest matching block lower than curr_block_idx for a backward search.
In either case we update the current block using set_current_block().

If there is no other matching block, we do nothing.

If there are other matching blocks, but none which is higher/lower, then instead of wrapping around, we do nothing.

Later we can add match information (count and current) to the ruler, but we don't handle that yet.
*/

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

/* #parse_config

void parse_config();

In parse_config, we read the contents of our config file (at state->config_file_path) into the cmp space, parse it, and set on the ui_state all the appropriate values.

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

Finally we call a function split_comma_ws to populate state->ollama_models from state->ollamas.
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

    state->ollama_models = split_commas_ws(state->ollamas);
}
/* #read_line
@- This is used by the ensure_conf_var stuff.

In read_line, we get a span pointer to some space that we can use to store input from the user, and a default value.

Just to be sure, we always assert(len(buffer)) which helps catch some programming errors that have happened in the past.

We first print our prompt, which is "> ", followed by the default if any.

We use prt() as always.

We handle some basic line editing using our getch() in a loop, until enter is hit.
At that point we return a span containing the user's input.
The span which was passed in will have been shortened such that .end of our return value is now the .buf of the passed-in span.
(In this way, the caller can perform other necessary adjustments, or use the remaining buffer area in a loop etc.)

The line editing we support:

- all kinds of backspaces shorten the span by one and redraw the line (using ANSI escapes and \r).
- enter returns.
- everything else just gets appended to our span, which it extends.
*/

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

/* save_conf_files()

Here we write the language and file lines into the conf file.

The structure of this data in the conf file is that a language line should be included every time the language of the next file is different from the previous file.
For example, if there are three files, with languages C, Python, Python, then we would have a language line of C, then the first file, then language Python and both the other files.

Therefore, we maintain a local variable indicating the last-written language, initially empty of course.
For each file, if the language is already equal to this, then we just print the file line, otherwise we print a language line first.
When printing a language line, we put a blank line first, since these usually group related files together.

As mentioned elsewhere, each conf line includes the key, a colon, space and the value, followed by newline.
*/

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
/* #save_conf

void save_conf();

In save_conf(), we simply rewrite the conf file to reflect any settings that may have been changed.

First we will write into the cmp space the current configuration.
Next, we will write that into the file named by state.config_file_path.
Finally we can shorten the cmp space back to what it was.

First we store a span that has .buf pointing to the current cmp.end.
** Then we call prt_cmp() and use prt to print a line for each config var as described below.
Then we call prt_pop().
Next we set the .end of that span to be the current cmp.end.

Then we call write_to_file_span with the span and the configuration file name.
Finally we shorten cmp back to what it was, since the contents have been written out we no longer need them around.

To print a config var, we print the name, a colon and single space, and then the value itself followed by newline.
(We currently assume that none of our conf vars contain newlines (a safe assumption, as if they did we'd also have no way to read them in).)
Our X macro handles the "normal" config fields, but then we call another function, save_conf_files, that handles the file and language lines that are special.
*/

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

/* #add_projfile(span)

Here we add a file to the projfiles and ensure that the file exists on disk.
This is a helper function for check_conf_vars.

We add the file to the projfiles, but do not set a language on it (since that will be handled next by that function.)

If it is not creatable, writable by the user, etc, we will report the error, wait for a keystroke, and then return 0, otherwise we return 1.
*/

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

/* #check_dirs

void check_dirs();

Here we ensure that the required directories exist.
All of these are under state->cmprdir:

revs/
tmp/
api_calls/
cache/
cache/v8/
cache/v8/revs/
outputs/

*/

void check_dirs() {
    span dirs[] = {
        S("revs/"), 
        S("tmp/"), 
        S("api_calls/"), 
        S("cache/"), 
        S("cache/v8/"), 
        S("cache/v8/revs/"),
        S("outputs/")
    };
    
    char buffer[1024];

    for (int i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        snprintf(buffer, sizeof(buffer), "%.*s/%.*s", len(state->cmprdir), state->cmprdir.buf, len(dirs[i]), dirs[i].buf);
        mkdir(buffer, 0777);
    }
}

/* #check_conf_vars

void check_conf_vars();

TODO: instead we should add features and empty states that guide the user

Manually written.
Maintainance mode.
Will be deleted at some future point.
*/

void check_conf_vars() {
    int confChanged = 0;

    if (empty(state->cmprdir)) {
      state->cmprdir = S(".cmpr/");
      confChanged = 1;
    }
    if (empty(state->model)) {
      state->model = S("clipboard");
      confChanged = 1;
    }

    if (confChanged) {
        save_conf();
    }
}

/* #ensure_conf_var

void ensure_conf_var(span* var, span message, span default_value);

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
    *var = read_line(&buffer, default_value); // Read new value from user

    cmp.end = buffer.buf; // Update cmp.end to the end of the returned span from read_line

    save_conf(); // Rewrite the configuration file with the updated setting
}

/* #edit_current_block @jk_order @blocks

void edit_current_block();

If we're in the empty project state, we just return, as there's nothing to edit.
@- TODO: something more helpful

To edit the current block we first write it out to a file.

We get the filename from tmp_filename, and write the contents with write_to_file_span, without clobbbering since the file should not exist.

The block contents is a span at state->curr_block_idx on state.blocks.
However, if curr_block_idx is -1, there is no current block and we're in an empty file state; in this case we write out an empty file instead of using the block contents.

Once the file is written, we then launch the user's editor of choice on that file, which is handled by another helper function.

That function will wait for the editor process to exit, and will indicate to us by its return value whether the editor exited normally.

If so, then we call another function which will then read the edited file contents back in and handle storing the new version.

If not, we print a short message to let the user know their changes were ignored because of the editor exit code, and let them press any key before returning to the main loop.

The messages written by this function are:
"Temp file %.*s written for editing.\n"
"Editor exited successfully, creating rev.\n"
"Editor exited with error, changes not saved.\n"
*/

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

/* #tmp_filename @langtable

span tmp_filename();

To generate a tmp filename for launching the user's editor, we return a string starting with state->cmprdir followed by "/tmp/".
For the filename part, we construct a timestamp in a compressed ISO 8601-like format, as YYYYMMDD-hhmmss with just a single dash as separator.
We append a file extension: we use file_for_block and current_block to get the language for the current block and add the appropriate extension, switching on the language and adding the appropriate filename extension from #langtable above.
We are assure here that the language will be already set for every file, so if it is not recognized, it's acceptable to either crash (as this is a programming error) or simply to do nothing and carry on with no extension on added.
(The only thing we don't want to do is add something ridiculous like ".txt".)
Note that state->cmprdir may or may not include a trailing slash in the conf file, but by the time we get it here, any trailing slash will have already been removed.
We return a static buffer, so the caller does not need to free it.
*/

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
    snprintf(filename, sizeof(filename), "%s/tmp/%s%s", s(state->cmprdir), timestamp, extension);
    return S(filename);
}

/* #launch_editor

int launch_editor(char* filename);

In launch_editor, we are given a filename and must launch the user's editor of choice on that file, and then wait for it to exit and return its exit code.

We look in the env for an EDITOR environment variable and use that if it is present, otherwise we will use "vi".

As always, we never write const anywhere in C.
*/

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
/* #file_for_block

int file_for_block(span);

Here we're given the span of a block and we must find out the index of the file that contains that block.

We use contains_ptr in a loop on state->files.

If nothing matches, something has gone badly wrong and we complain as exit as usual (prt, flush_err, exit).

In short, file_for_block always returns an int, 0 <= n <= state->files.n, or doesn't return at all.
*/

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

/* #current_block_language @blocks @files @jk_order

span current_block_language();

Here we get the currently selected block and then use language_for_block to return the language for that block.

However, if we are in "empty file" mode, then we use guess_language_from_filename with the path of the current file.

If we are in the empty project state, it is an error to call this function.

@complain_and_exit
*/

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

/* #guess_language_from_filename @langtable

span guess_language_from_filename(span);

Here we find the last "." in the argument, and take everything from there to the end as a file extension.

We match these against the extensions listed in #langtable Col. 2, above, and return the matching language name from Col. 1.

If there is no match, we return "C" as a sensible default.
*/

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
/* #language_for_block

span language_for_block(span block);

We are given a span which is a block, and return the language for that block.

We get the file for the block (file_for_block()) and return the .language on the corresponding projfile on state->files.
*/

span language_for_block(span block) {
    int file_index = file_for_block(block);
    return state->files.a[file_index].language;
}

/* #handle_edited_file @jk_order @files @blocks

void handle_edited_file(char *filename);

Here we are given a tmp file containing new contents of a block or a file.

What we must do is replace the existing block contents with the new contents of that tmp file, and then write everything out to a new file on disk called a rev for the particular projfile that contains that block.

We already have on state all the current values of the blocks, which are spans that point into inp.
First we want to fix inp to reflect the new reality, and then we will write out the new disk file from inp (our global input span).
We will also fix the contents spans of all of the files starting with this one, and including all later ones.

We get the span corresponding to the current block, which is also the edited block, in a local variable for convenience.
We can do the same for the .contents of the current file.

However, there is a special case, which is the empty file state.
In this case there will be no current block, but there will still be a current file.
In this case, instead of the span of the current block, we can use the .contents of the current file, which will be an empty span at the correct location in inp; we will refer to this as "the original block" from here on, regardless of whether it was an actual block or an empty file.

(If we're in the empty project case, it is impossible for this function to be called, so we don't need to handle that.)

@complain_and_exit

The contents of inp are already correct, up to the start of this block (or empty file), which is what we want to replace.
Now we get the size of the tmp file and compare it to the len() of the existing block.
If it is larger, we need to move the contents after it (in inp) to the right in memory; if smaller, to the left.
We can do this using memmove.
We also must adjust inp.end so that it is correct after the adjustment.

Once we have done this, we have everything correct in inp except for the contents of the file itself, and we have a space that's sufficient for the file contents to be read into.
We construct a span that represents this space.
This span starts at the .buf of the original block, but has the length of the file.

We have a library function, span read_file_into_span(char*,span), which takes a span and reads a file into a prefix of that span, and then returns that prefix as another span.
We can pass our "gap" span representing the part of inp that we want to replace to this function, along with the filename.
We can confirm that the span that is returned is in fact the identical span as the buffer we passed in, since we are expecting that the file length has not changed.

As usual, if this expectation is violated we will complain and exit.

Now inp is representing the new state of the project.

However, the file[i].contents for the files including and subsequent to this one may be incorrect.
Specifically, the difference in length of this block must be added to the .end of the file contents span for this file, and to both the .buf and .end of every subsequent file.

@- As a sanity check, after this step, we could validate that the .end of the last file is equal to the .end of inp itself. -- this is now done by ingest

We need to update and re-index the blocks, since any blocks after and including this one may have moved, so we call ingest().

Then we call a helper function, new_rev, which takes the tmp filename we were given, and the file index for the projfile that was altered.
This function is responsible for storing a new rev, cleaning up the tmp file, and any reporting to the user that we might do.
*/

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
/* #new_rev

void new_rev(span tmp_filename, int file_index);

Here we store a new revision, given a tmp filename which contains a block that was edited and the index of the projfile that contains that block.
The file contents have already been read in and processed, we just get the name so that it can be cleaned up.

First we construct a path for the new rev, like <cmprdir>/revs/<timestamp>, where cmprdir is a conf var on state, and the timestamp is an ISO 8601-style compact timestamp like 20240501-210759.
We then write the contents of the projfile into this file (the "rev") using write_to_file().
The projfile.contents (which is on state->files) already contains the current contents that we want to write out.

Once this is all done, we call update_projfile, which handles the rest of the process, using the index, tmp path, and rev path.
This is the function that will actually replace the projfile, and unlink the tmp file if everything is successful.
*/

void new_rev(span tmp_filename, int file_index) {
    span dir = state->cmprdir;
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char timestamp[16];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", timeinfo);
    
    span rev_path = concat(concat(dir, S("/revs/")), S(timestamp));
    prt("writing new rev %.*s\n", len(rev_path), rev_path.buf);
    write_to_file_span(state->files.a[file_index].contents, rev_path, 1);
    
    update_projfile(file_index, tmp_filename, rev_path);
}

/* #update_projfile

void update_projfile(int file_index, span tmp_filename, span rev_path);

We get the index of an updated projfile, a tmp filename containing the new version of that file, which has already been processed, and is only passed in so that we can unlink it when all is successfully completed, and a rev path which contains the new version of that file.

From state->files we get the path of the projfile (state->files.a[file_index].path).
Our task is to replace that file with a copy of the new rev, after a few checks.

The file already there may have been edited and contain unsaved changes by some other process.
Therefore, if there is a file already there (at the projfile location), we rename it, adding ".bak".

We get the permissions of the projfile from the filesystem, as we will want to preserve them (for example, an exec bit may be set).

Then we copy the new rev into the projfile location.

We set the permissions to be equal to whatever we got from the original file.

Finally we unlink the tmp filename that was passed in, since we have now fully processed it.
The tmp filename is actually optional, since we sometimes also are processing clipboard input, so if it is empty we skip this step.

As usual, if any of these steps goes wrong, we prt, flush, and exit(1).
Any time we print an error involving a file, we always include both the filename and the OS error message (strerror).
*/

void update_projfile(int file_index, span tmp_filename, span rev_path) {
    span projfile_path = state->files.a[file_index].path;
    char projfile_path_str[2048];
    s_buffer(projfile_path_str, sizeof(projfile_path_str), projfile_path);

    char rev_path_str[2048];
    s_buffer(rev_path_str, sizeof(rev_path_str), rev_path);

    struct stat file_stat;
    if (stat(projfile_path_str, &file_stat) == 0) {
        char backup_path[2053];
        snprintf(backup_path, sizeof(backup_path), "%s.bak", projfile_path_str);
        if (rename(projfile_path_str, backup_path) != 0) {
            prt("Error backing up file %s: %s\n", projfile_path_str, strerror(errno));
            flush();
            exit(1);
        }
    } else {
        prt("Error accessing file %s: %s\n", projfile_path_str, strerror(errno));
        flush();
        exit(1);
    }

    if (copy_file(rev_path_str, projfile_path_str) != 0) {
        prt("Error copying file from %s to %s: %s\n", rev_path_str, projfile_path_str, strerror(errno));
        flush();
        exit(1);
    }

    if (chmod(projfile_path_str, file_stat.st_mode) != 0) {
        prt("Error setting permissions on file %s: %s\n", projfile_path_str, strerror(errno));
        flush();
        exit(1);
    }

    if (!empty(tmp_filename)) {
        char tmp_filename_str[2048];
        s_buffer(tmp_filename_str, sizeof(tmp_filename_str), tmp_filename);
        if (unlink(tmp_filename_str) != 0) {
            prt("Error removing temporary file %s: %s\n", tmp_filename_str, strerror(errno));
            flush();
            exit(1);
        }
    }
}

/* #gpt_message

We get a role and a message and we return a json_o that has "role" and "content" properties.
*/

json gpt_message(span role, span message) {
    json resp = json_o();
    json_o_extend(&resp, S("role"), json_s(role));
    json_o_extend(&resp, S("content"), json_s(message));
    return resp;
}

/* #send_to_llm

void send_to_llm(span, llm_message_handler cb);

Here we're given a prompt to send to the LLM.
If state->model is "clipboard" then we call send_to_clipboard, and we are done.

Otherwise we use an API.
In this case we build a json array and extend it with objects.
Each one has a role (either user or system) and a content.
As usual our json_* stuff is wrapped in prt_cmp and prt_pop.

If there is a block with ID "systemprompt", then we will send that as the first message.

If state->bootstrapprompt is non-empty, we will send that as the first user message, followed by an "OK" reply from the assistant.

To look up the blocks we use find_block(), which returns int, and state->blocks.

Then in any case we send our input prompt as the last user message, and send the whole messages array to the api.

We call call_llm() with state->model, the messages, and a function pointer to replace_block_code_part.

Manually edited.
*/

void send_to_llm(span prompt, llm_message_handler cb) {
    if (span_eq(state->model, S("clipboard"))) {
        send_to_clipboard(prompt);
        return;
    }

    //prt_cmp();
    json messages = json_a();
    //int system_index = find_block(S("#systemprompt"));
    int system_index = block_by_id(S("systemprompt"));
    if (system_index != -1) {
        json_a_extend(&messages, gpt_message(S("system"), state->blocks.a[system_index]));
    }

    if (!empty(state->bootstrapprompt)) {
        json_a_extend(&messages, gpt_message(S("user"), state->bootstrapprompt));
        json_a_extend(&messages, gpt_message(S("assistant"), S("OK")));
    }

    json_a_extend(&messages, gpt_message(S("user"), prompt));
    //prt_pop();

    call_llm(state->model, messages, cb);
}
/* #handle_openai_response @jsonlib @partials

void handle_openai_response(span response, llm_message_handler cb);

Here we get a span response from an LLM API such as OpenAI's.

We parse it as json.

Otherwise, we pull the content out and pass it on.
Specifically, we index into the JSON by:

choices, 0, message, content

If any of these steps fails, we print a message, with the entire response body, and exit.

@- Finally we get the string value from the JSON string, strip the markdown code block if any, and call the cb function with the result.
@- We want the cb function to do the markdown stripping as we're making this API more general now (not only used for 'r' like before).
@- Finally we get the string value from the JSON string and call the cb function with the result.
Finally we get the string value from the JSON string and apply the cb (which is an aliased Partial) using apply_partial.
*/

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

/* #handle_ollama_response @handle_openai_response

void handle_ollama_response(span response, llm_message_handler cb);

This is similar to handle_openai_response above, but for ollama API responses.

The only difference is the structure we descend into:

message, content

*/

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

/* #block_comment_part @langtable

int find_comment_end_c(span);
int find_comment_end_python(span);
span block_comment_part(span);

To split out the block_comment_part of a span, we first write two helper functions, one for C and one for Python.
Others will be added but these are the only ones we've needed so far.

Then we dispatch based on the language.
We lookup the using language_for_block on the block itself.
Previously, if the language was "Python" we called the Python version, and otherwise we called the C version (which was therefore our default).
Now, however, we look up the language in the #langtable above and call the find block comment end implementation.
For example, the third language we support is JS, which uses the same implementation as C.
For markdown, there is no comment part (or the whole block is considered a comment) so we simply return the entire block.

The helper function always takes a span and returns an index offset to the location of the "block comment part terminator".
For Python this is the second occurrence of the triple doublequote in the block, and for C it is star slash.
Simple for loops over block.buf up to len(block) are best here.

In the main function block_comment_part, we return the span up to and including the comment part terminator, and also including any newlines and whitespace after it.
This means each of the helper functions returns the offset at which the comment terminator ends, and the main function includes a while isspace loop to advance past any whitespace.
(Among other things, this ensures that "R" does not unduly change the amount of whitespace mid-block.)

All three functions are written below.
*/

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

/* #block_comment_part_excl @langtable

Similar to block_comment_part, we get a block and return the comment part, only in this case with the comment block delimiters, if any, removed.

First we call language_for_block to get the appropriate language to use.

Then we call block_comment_part, which gives us the comment part but with delimiters included.

We trim any whitespace (whitespace after the comment part will be included by block_comment_part).

We examine the first few characters of the trimmed result.
If it matches the starting comment delimiter for the language (if any) then we remove it.

Then we do the same for the last few characters, and finally return the span.
*/

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

/* #block_code_part

span block_code_part(span);

Meant to be dual to block_comment_part.

For that reason, we simply call block_comment_part to get the comment part, and then return the other part.

(We know that the comment is always the first part of the block, so we can set .buf on the input span to be the .end of the comment part and simply return that.)
*/

span block_code_part(span block) {
    span comment = block_comment_part(block);
    block.buf = comment.end;
    return block;
}

/* #prompt_palette_design

The prompt palette is a set of different short actions or operations that can be selected from a menu.

Each one of them uses an LLM in some way to do something related to the current block.
However, after that, what happens may vary.
For example, we might replace part of the block with the LLM output, or apply the output as a patch to the block, or even present the output separately in the block as a special comment.
So for now we are implementing these directly.
Eventually, however, we want the prompt palette to be extended easily by users, with descriptions written in accessible language.
*/
 /*
I previously had an idea of using the block system itself as a kind of extensible programming system.

For example, if a block can take other blocks as arguments, and produce further blocks as output, then the block system itself becomes agentic.
For example, we could have a "code formatting block" which would then be iterating over the other blocks and enforcing a code formatting invariant.
This could be allowed to consume a certain number of tokens (or really, cents) per day and would presumably be optimized to some standard.

This feels better.
*/
/* #prompt_palette

void prompt_palette();

Here we get the set of prompt names.

Then we use select_menu to present the palette to the user who can select one of the prompts (or cancel).

We will then run the action the user selected.
*/

void prompt_palette() {
    spans palette = get_palette();
    int sel = select_menu(palette, -1);
    if (sel >= 0 && sel < palette.n) {
        apply_prompt(palette.a[sel]);
    }
}

/*
Here is how we will let the user customize the prompts:

First of all, we are currently hard-coding the templates into the C code, which won't work if the user customizes them, unless we make the C compiler part of the customization process (actually, I'm somewhat OK with this, but we also have to make the LLM part of it, which I'm not).

Instead, we will get the template from a default in the code, or otherwise from a block having a particular ID.
This means we can install the customization by the block system itself, which seems reasonable given the "database idea".

This can be overloaded on the # block ids but this means stomping on the user's namespace for block ids.
So instead we will use another sigil, like ! to indicate that it's doing something, or somehow special to the system.

So we have to define the block special instructions or pragmas.

Let's call them pragmas, as they actually are "pragmatics" by their linguistic function.

This suggests we could also make them short and imperative, like !use_as_global_context, which perhaps is more informative than !global_context or !gcb.

Then we can have !use_as_rewrite_pl_template for the current 'r' template, and !use_as_rewrite_nl_template for the "second half" feature that we don't have yet.

Then all that's missing is the ability to automatically add these blocks to your project, with the default or current values, which we could do in a number of ways (later).
*/

/* #optable The operation palette table

The op palette (name may still change) is defined here as a table.

Each column is given as a numbered section for ease of editing (like in #langtable above).

The short name is also the name of the C function implementing the palette entry.

Cols 1, short name, and 2, human title.

- nl2pl_rewrite, NL -> PL rewrite
@- nl2pl_with_history, NL -> PL with history 
- pl2nl_rewrite, NL <- PL rewrite
- agreement, NL PL agreement
- agreement_to_pl_diff, NL PL agreement to PL patch
- agreement_to_nl_diff, NL PL agreement to NL patch
- summarize_block, block to one-line summary
- nl2algo, NL description to step-by-step algorithm

3. Description

nl2pl_rewrite: Rewrites the PL part of the block from just the NL part (bound to 'r').
@- nl2pl_with_history: ...
pl2nl_rewrite: Rewrites the NL part of the block from the existing PL part, along with context.
agreement: Determines whether the PL part correctly reflects the NL part, or reports why not.
agreement_to_nl_diff: Suggests changes to the NL part to bring it into agreement with the PL part.
summarize_block: Creates a summary for the block, often used to suggest which blocks might be relevant as context to some other.
nl2algo: Creates a block_id:algo output block (indented?) (or -algo?), containing a bullet-point style algorithm for the above NL part, as an intermediate step to generating code.

*/
 /* other palette ideas and snippets follow: */
 /*
There is a specific error that LLMs often make, where they assign the same name to two variables, or function arguments, or functions or globals that are in scope. Then they try to use the same identifier to refer to two completely separate things in the same scope, which obviously does not work.

Note: it would make a lot of sense to run this (and maybe some similar error checks) only in response to a certain kind of compiler error, or maybe any compiler error.

Examine the following code and reply only with "No" if it does not contain an error of this type, or "Yes: " followed by the identifier.

```c
spans read_output_headers(span bname) {
    span filename_template = concat(S("{cmprdir}/outputs/"), bname);
    span filename = filename_template(filename_template);
    span file_contents = read_file_into_cmp(filename);

    spans headers = spans_alloc(0);

    while (!empty(file_contents)) {
        span line = next_line(&amp;file_contents);
        if (empty(trim(line))) break;

        int colon_idx = find_char(line, ':');
        if (colon_idx == -1) continue;

        span key = trim(first_n(line, colon_idx));
        span value = trim(skip_n(line, colon_idx + 1));

        spans_push(&amp;headers, key);
        spans_push(&amp;headers, value);
    }

    cmp.end = file_contents.buf; // Reset cmp space to keep headers but give back memory for the body

    return headers;
}
```
*/
 /*
When we have added a new helper or utility function or library method that might be broadly applicable, we can apply an operation to that block in conjunction with each other block in the project (that has a code part) in turn.
The prompt would ask whether the function we added (e.g. the recent filename_template function) could be profitably applied to the target block to simplify and improve the code.
The result would be some number of suggested block that can potentially be simplified, which the user can then review.
*/
 /* Some examples from llm.c translation from C to English, by way of Claude.

Actually, these are going in a markdown file, since they contain embedded C block comments.

See #llmc_prompts
*/
/* #get_palette @prompt_palette_design @optable @generic_array_usage

spans get_palette();

Here we get the prompt palette entries.

Because this is for display, we return the human titles, Col. 1 from the #optable above.

We use a static buffer for the names and a static spans which we set up to point into them.

Because S() is a function, we can't use it in a static initializer.

Instead we manually construct a spans, with .n, .cap, and .a, and manually construct each span (recalling that spans are not null-terminated).
*/

spans get_palette() {
    static char *names[] = {
        "NL -> PL rewrite",
        "NL <- PL rewrite",
        "NL PL agreement",
        "NL PL agreement to PL patch",
        "NL PL agreement to NL patch",
        "block to one-line summary",
        "NL description to step-by-step algorithm"
    };

    static span entries[sizeof(names)/sizeof(names[0])];

    static spans result = { .n = sizeof(names)/sizeof(names[0]), .cap = sizeof(names)/sizeof(names[0]), .a = entries };

    for (size_t i = 0; i < result.n; ++i) {
        entries[i].buf = (u8*)names[i];
        entries[i].end = entries[i].buf + strlen(names[i]);
    }

    return result;
}

/* #apply_prompt @prompt_palette_design @optable @complain_and_prompt

void apply_prompt(span prompt_name);

We get a prompt name, and we call a function.
Specifically, we get a human title (col. 2 from #optable above) and we call the short name (col. 1), which is the name of the C function implementation.

@- TODO: rename this horribly misnamed function before we ship v9, since the names of everything has changed
*/

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
    } else if (span_eq(prompt_name, S("block to one-line summary"))) {
        summarize_block();
    } else if (span_eq(prompt_name, S("NL description to step-by-step algorithm"))) {
        nl2algo();
    } else {
        prt("Unknown prompt: %.*s\n", len(prompt_name), prompt_name.buf);
        flush();
        getch();
    }
}
/* #nl2plrewrite @prompt_palette_design @optable

Here we describe what the nl2plrewrite does.

(This is what is currently done for the "r" command.)

- we follow block references and get the context for the current block
- we follow inline references and expand the block itself
- we look up the system prompt
- we look up the global context prompt
- we use these to generate a prompt, and a conversation
- we pass that to the LLM
- we get back code in a markdown code block from the LLM (because of our prompt)
- we then do some string manipulation with all of the above and the result becomes the new block contents
- we then do our source code handling of the new block

we need to define !system_prompt, instead of the current #systemprompt, probably, or maybe !use_as_system_prompt
We need !use_as_global_context, which replaces the bootstrap mechanism.

If we want to support checking the NL and existing PL code for agreement, we can do a prompt like this:

the context: <context>
the comment: <the comment>
the code: <the code>
Does the code correctly implement the comment? [...more instructions...]

The flow here:

- as above, except for a slightly different LLM prompt
- the output will be either "Yes" (meaning PL correctly implements NL) or "No." followed by some description (in markdown, probably)
- then we would want to show the output to the user, but one way of doing this is by adding another kind of comment into the block
*/

/* #prompt_template_design

We have a single function that we can call with a name to get a prompt template.

span get_prompt_template(span);

This function simply dispatches to a pt_* function which returns the appropriate prompt template as a span.
These functions are written by another process, from template files that are maintained in the prompts/ directory.

We also use the list of files in this directory to create the list of prompt template names, which is how we know what to dispatch to.

@- The template will either be the default one, or it will come from a block in the current project, with a particular pragma (e.g. "!use_as_nl2pl_rewrite_template").
@- The function will look at the blocks and either return the matching one, or return the hard-coded in default.
@- Eventually we'll also want a feature that lets the user easily add a block with the default in it to their project so they can modify it.
@- For now, though, we only support returning the default template.

@- TODO: put the default templates in files, one per? But we still need to bake them into the binary, so it's just for making it easier for us
@- and then we need a way to refer to a file, or to have a file that "is" just a single block, or something

@- TODO: split this into a dispatch function and constant functions that contain the strings, for reduced diff churn.
*/
/*

Here we open an ifdef that will only include the following alternate main function if we compile with -D PROMPT_LIST.

*/

#ifdef PROMPT_LIST
/* #prompt_list_gen @gcb @blocks @spans_initialization

In this alternate main function, we output some generated C code.

Specifically, we generate one block comment, containing a list of prompt templates available in the directory prompts/, and then we generate one function for each of those templates, which is simply a constant function that returns the literal string value, pulled directly from the file.

The prompt template functions have the prefix pt_, so a file prompts/foo leads to a corresponding function pt_foo which returns the contents of foo.

Here is our own output in template form:

[[ #prompt_list
{list of prompt template names}
]]

span pt_{name}() {
  return S({content}
           {...});
}

...

In the above, [[ and ]] stand in for our block comment start and end delimiters, which are slash star and star slash.

The "#prompt_list" is the actual block id for that block comment, which will be included by some other blocks that generate code to dispatch to these functions.

The function definition is then repeated for each of the prompt templates.
Of course, {name} is replaced by the actual name (which is just the filename) and the {content} and {...} lines are replaced by C string literals, one per line in the original file contents.

To generate this output, we first get a directory listing for "prompts", which is already sorted.

We then output the block comment containing the list, one per line.

Then we output each function.

To turn the file content into the contents of our S() call, we first read the file into cmp space.
To do this we construct the filename prompts/{name} using concat(), and then read the file into cmp space.
Then we iterate over the lines of the file.
In each line, we iterate over the characters, and if it needs to be escaped inside a C string, we prt the appropriate C escape, otherwise we w_char the character itself.

The only things that have to be escaped inside C strings are newlines, double quotes, and existing backslashes, but since we are iterating over the lines, we only need to deal with double-quote and backslash.
At the end of each line before the terminating quote we add the "\n" (which next_line has removed for us when iterating over the lines).

For setup, we first call init_spans() and also spans_arena_alloc(1 << 20).

int main(int argc, char** argv);
*/

int main(int argc, char** argv) {
    init_spans();
    spans_arena_alloc(1 << 20);
    span prompt_dir = S("prompts/");
    spans prompt_files = dir_listing(prompt_dir);

    prt("/* #prompt_list\n");
    for (size_t i = 0; i < prompt_files.n; i++) {
        span file = prompt_files.a[i];
        prt("%.*s\n", len(file), file.buf);
    }
    prt("*/\n\n");
    
    for (size_t i = 0; i < prompt_files.n; i++) {
        span file = prompt_files.a[i];
        span prompt_file_path = concat(prompt_dir, file);
        span file_contents = read_file_into_cmp(prompt_file_path);
        
        prt("span pt_%.*s() {\n", len(file), file.buf);
        prt("  return S(\"\"");

        while (!empty(file_contents)) {
            span line = next_line(&file_contents);
            prt("\n           \"");
            while (!empty(line)) {
                char c = *line.buf;
                if (c == '\"') {
                    prt("\\\"");
                } else if (c == '\\') {
                    prt("\\\\");
                } else {
                    w_char(c);
                }
                advance1(&line);
            }
            prt("\\n\"");
        }

        prt(");\n}\n\n");
    }

    flush();
    return 0;
}

/*

...and here we close the ifdef that was opened above.

*/

#endif

/**/
#include "prompt_templates.c"

/* #get_prompt_template @gcb @prompt_template_design @prompt_list @complain_and_prompt

Here we implement the dispatch function.

span get_prompt_template(span name);

We match on the name and call the corresponding pt_{name} function, for each of the names in #prompt_list above.

@- TODO: we really should find a way to make this more automatic whenever a prompt is added.

#complain_and_prompt
*/

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

/* #agreement

void agreement();

The agreement palette entry is designed to ask the LLM whether the NL and PL code agree.
Specifically it asks whether the NL code correctly implements what the PL code asks for.

The answer is either "yes" or "no" along with an explanation of what is wrong.

We call a function to get the prompt template named agreement.

We call another function to get the template variables related to the current block.

We expand the template with the variables.

We then call send_to_llm, with agreement_SAV as the callback function.
*/

 /*output:
...text from the LLM...
*/

void agreement() {
    span template = get_prompt_template(S("agreement"));
    spans vars = current_block_template_vars();
    span expanded_template = expand_template(template, vars);
    wrs(expanded_template);
    flush();
    getch();
    //send_to_llm(expanded_template, &agreement_SAV);
    llm_message_handler cb = make_output_saver(S("agreement"));
    send_to_llm(expanded_template, cb);
}

/* #agreement_to_nl_diff @agreement @output_template_var

void agreement_to_nl_diff();

After using #agreement to see if the NL and PL code agree, if the LLM gives us a negative reply, we can use this operation to suggest a change to the NL part in the form of a diff that can be automatically applied to the NL code.

@- This is similar to #agreement above.

We call a function to get the prompt template named agreement_to_nl_diff.

We call another function to get the template variables related to the current block.
We extend the template context to include the output from "agreement".

We expand the template with the variables.

We then call send_to_llm, with proposed_diff_SAV as the callback function.
*/

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
/* #output_template_var @gcb @assoc_spans

Here we get an assoc spans representing a template context.

We extend it with a specific output for the current block, if there is one.

void output_template_var(spans* ctx, span human_name);

We use lookup_output to get the output.

*/

void output_template_var(spans* ctx, span human_name) {
    span output = lookup_output(human_name);

    if (!empty(output)) {
        spans_push(ctx, human_name);
        spans_push(ctx, output);
    }
}

/* #lookup_output @gcb @output_design @assoc_spans @checksums

span lookup_output(span output_of);

Here we find an output for the current block by checksum and which is the output of the given operation.

We will get the checksum for the current block and get this as a span using prs_checksum.

We will use get_outputs to get the list of output files.

Then we will call read_output_headers on each one, in reverse chronological order.
This returns an assoc spans.

The first output that matches the checksum of the current block and the output that was given is the one we return.

We'll use read_output_body to get our return value.
*/

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

/* #expand_template @gcb

span expand_template(span template, spans vars);

Here we parse a prompt template and print the expanded result into cmp space and return it as a span.

@out2cmp

Our return span starts from cmp.end before we do any printing, and ends with the value of cmp.end at the end.

We get the template, parse it, and expand it by iterating over the parsed template, printing the literal parts and syntax parts alternately, with print_template_literal and eval_template_variable.
*/

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

/* #print_template_literal

void print_template_literal(span);

We handle printing a template literal specially, since they require handling the two escape sequences "\\" and "\{".

Specifically, we iterate over the input span and use w_char on each one, except that if we find a "\\" or "\{" sequence, we only print a single "\" or "{" respectively.
*/

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

/* #gcb @spans_usage
*/
/* #current_block_template_vars @gcb @langtable @blocks

spans current_block_template_vars();

Here we evaluate variables that can be used in expanding prompts that relate to the current block.

In particular, we have:

- langtag
- context
- comment
- code

For each one of these, we push the name itself onto our return value, followed by the expanded value if any.

In some cases, a value does not exist but can be used and will evaluate to an empty string.
In other cases, a value does not exist and it is an error to try to expand it.
We will distinguish these cases by using nullspan() for the error cases and any other empty span for the former cases.

To get each of the variables:

langtag: get the language for the current block; switch on this and use the corresponding langtag; see Col. 6 from #langtable, above, for the langtag, and Col. 1 for the language names returned by current_block_language.
context: use expand_refs_2 on the comment part of the current block, with the "context" mode
comment: expand_refs_2 with the "body" mode
code: the code part of the current block
*/

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

/* #eval_template_variable @gcb

void eval_template_variable(span, spans);

We get a name of a variable and a list of variables and their values (in a particular format as a spans).

We trim the variable name.

We then iterate over the variables and values, which are in pairs, such that the (i * 2)'th element is a variable and the (i * 2 + 1)'th is the corresponding value.

We iterate and look for a match.
If there is a match, but the value is the null span, that is an error.
Otherwise, we simply wrs the value, without added whitespace or newlines.
In particular, empty spans are *not* an error, only the null span specifically (the null span is the span having .buf = .end = 0).

Also, if the value is not found, that is an error.

We alert the user to either kind of error, including relevant info such as the name of the variable.

@complain_and_prompt
@- TODO: use flush_err in above(?)
*/

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

/* #template_language_design

We want a simple templating language for expressing prompt templates.
The template language currently only needs to support variables that will be replaced with their values.
There will not be any control flow, conditionals, or other fancy features just yet.

We we will use single curly braces to mark template syntax.
An simple example template is therefore "a{b}c".
This expands to the literal strings "a" and "c" with the value of the variable "b" between them.

If a literal left curly brace "{" is meant to appear in the template, then it must be escaped with a backslash as "\{".
Also, double backslash can be used to create a literal backslash.
This is normally not necessary, but it is necessary before a "{" that is not intended to be escaped, as otherwise the backslash would escape it.
E.g. "a\\{b}c" gives "a\", "b", "c", but the backslash in "a\b{c}" does not have to be escaped and gives "a\b", "c".
No other backslash escapes are supported.
(Note that right curly braces don't need escaping, as only the left brace introduces template syntax.)

To represent the parsed template, we can simply use a spans.
We parse the template into a sequence of alternating spans which are respectively outside then inside the template syntax.
By convention, the first span is outside the template syntax, but if the template begins with template syntax, such as "{b}c", then this first span will simply be empty.
So:
"a{b}c" -> "a","b","c"
"{b}c" ->  "","b","c"

The interpreter can then simply process the first span as a literal string, the next one as template syntax, and so on to the end.

Note that we don't remove the escaping (as we return spans pointing into the input memory) so the caller will have to deal with the "\{" sequences themselves in the literal parts.
*/
/* #parse_template @template_language_design @generic_array_usage

spans parse_template(span);

We are given a template and we parse it into a simple linear AST as described above.

The language we are parsing is very simple, so we maintain a single loop over the bytes of the input span.

Implementation:

Loop over all the chars in the input.
Keep track of whether we are in literal mode or syntax mode with a variable `in_syntax`.
If we meet a "\" we check if the next character is a "{" or "\" and stay in literal mode.
When we meet an unescaped "{" we shift into syntax mode.
There is no escaping of right curly brace, so a "}" unconditionally ends syntax mode.
We begin in literal mode and always alternate literal and syntax spans that we push onto our return value.
*/

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

/* #print_block #print_comment #print_code #count_blocks
@- These are used by the similarly-named command line flags.

In these print_* implementations we print block contents completely or partially.

We use block_comment_part to get a span containing the comment part of a block.
We can use this pattern to get the code part: span code_part = block; code_part.buf = comment_part.end.

@- TODO: add this as a spanio method (complement of span in span) ?

count_blocks() is a trivial wrapper around state.blocks.n.
*/

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

/* #find_block

This is similar to the search implementation: we iterate over all the blocks, find the first block which contains the literal search text provided, and return the index of that block (or -1 if none matches).
*/

int find_block(span search_text) {
    for (int i = 0; i < state->blocks.n; i++) {
        if (contains(state->blocks.a[i], search_text)) {
            return i;
        }
    }
    return -1;
}

/* #block_by_id

int block_by_id(span id_no_hash);

We are given a block id, without the hash.

We compare with the block_idx to find the first id that is a match, disregarding the first char of the index element, as those do contain the '#'.

Then we tail-call block_for_span.
*/

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

/* #press_any_key

@- Pattern used whenever we've printed something the user might otherwise miss.
We print "Press any key to continue...", flush, and getch.
*/

/* #ex_expand @ex_expandrefs

void ex_expand();

Ex command handler, similar to the old ex_expandrefs, but now calling expand_refs_2 with the current block, with "both" as the mode.
*/

void ex_expand() {
    span current_block = state->blocks.a[state->curr_block_idx];
    span expanded = expand_refs_2(current_block, S("both"));
    clear_display();
    wrs(expanded);
    prt("Press any key to continue...");
    flush();
    getch();
}

/* Block reference design

A block may contain references and an id/metadata top line.

We will follow and expand references, remove the top line, and return the expanded result.

There may be reference loops.
For now we will just limit the expansions to 32, and if the limit is hit, we will alert the user.

We will support two kinds of references, both using the same "@<id>" syntax where <id> is any block id.

First we will have references listed in the header/metadata top line, and second will be inline references, which will appear on a line alone.

Example block, using [[ ]] as mock block comment syntax:

[[ #block_id @ref_1 @ref_2

Some text.

@ref_3

More text.
]]

When this block is expanded, we would include the content of blocks ref_1 and ref_2 first, then the block text with ref_3 expanded inline.

The top-line blocks should be separated by "\n\n" from each other and the start of the block.

References use @id and blocks are identified by #id, so to follow references we remove the "@", add the "#", and then do a find_block to search for the first block containing that text.

The above is the initial design, implemented by :expandrefs.

However, the new idea can be seen by :refsonly, which is to have a separate message for the references.
The actual 'r' message itself will then have the top line removed, including the block id.
*/
/* #expand_refs

Here we get a span with references to be expanded.

There is a resource-management part of the problem which we handle here, and a recursive part.

We set up span that we will return, pointing .buf to the current cmp.end.
We call prt_cmp and spans_arena_push.
Then we call expand_refs_rec for the recursive part (which will output the expanded block into cmp space using prt).
Finally we call prt_pop and spans_arena_pop.
We update our ret span's .end to the current cmp.end, and return it.

span expand_refs(span references) {
    span ret = {.buf = cmp.end, .end = cmp.end};
    void *o = out2cmp();
    spans_arena_push();
    expand_refs_rec(references,0);
    out_rst(o);
    spans_arena_pop();
    ret.end = cmp.end;
    return ret;
}

*/
/* #expand_refs_rec

Here we get the comment part of a block, which may contain two kinds of references, which we will expand and print.
Our second argument is the depth of the recursion so far, specifically it is the number of levels above this one (so it starts at zero).

First we have top-line references, which occur on the top line and will be expanded in order before the block contents.

Next, we have inline references, which occur on a line alone and will be expanded inline.

We take the top line off of the input (using next_line) and handle it separately.

We can split this top line on whitespace using split_whitespace, and iterate over the tokens.

For each token,
If the first character is "#", that is the "block id" for this block.
We stick it in a variable block_id for use later.

(The reason why we print the block id out of order is that the block id will usually be the first thing on the top line, followed by references to be expanded.
However in the expanded output, the id should come above the block comment text itself, not above other included content, as that would be hard to follow.)

If the first character is "@" then we call chase_ref with that token.
The return value from chase_ref will either be a block, or if the reference expansion failed, it will be an empty span.
If the span is empty, then we prt the token unmodified followed by a newline.
(This lets the user see that the expansion failed rather than failing silently.)
Otherwise, we recursively call expand_refs_rec on the returned block so that further references can be expanded.
@- To get the comment part we use block_comment_part_excl, since we want the block comment delimiters excluded.
We print a newline after the recursive call, to keep them nicely separated.

For tokens that don't start with "@" we simply print them as-is, followed by a newline.

Now we've handled the top line; next we will handle the block id, and then the rest of the block's contents.

If the recursion level is zero, i.e. this is the call corresponding to the top-level block being expanded, then we print four blank lines, "----" on a line by itself, "(end references)" with a blank line before and after it, and then finally the block id, also followed by a blank line.
Otherwise, we simply print the block id on a line by itself, with a blank line before and after it.

After handling the top line, we get to the block content itself, which we handle line by line.

If the first a line starts with "@- " we treat it as a comment and simply skip it.

If a line begins with "@" then we call chase_ref.
If chase_ref returns an empty span, then again we print the line unmodified, followed by a newline.
@- Otherwise, we again recurse on the comment part of the returned block.
@- Otherwise, we call strip_block_comment_delimiters, since we are already inside a comment, and we again recurse on the returned block.
Here, we do not print a newline after, as the recursive block will already end with one, and if further newlines are needed, they can be added around the references by the user.

For lines not starting with "@", we also simply print them as-is, followed by newline.

void expand_refs_rec(span block, int depth) {
    span line = next_line(&block);
    spans tokens = split_whitespace(line);
    span block_id = nullspan();
    for (int i = 0; i < tokens.n; i++) {
        span token = tokens.a[i];
        if (token.buf[0] == '#') {
            block_id = token;
        } else if (token.buf[0] == '@') {
            span ref_block = chase_ref(token);
            if (empty(ref_block)) {
                prt("%.*s\n", len(token), token.buf);
            } else {
                expand_refs_rec(ref_block, depth + 1);
                terpri();
            }
        } else {
            prt("%.*s\n", len(token), token.buf);
        }
    }
    if (depth == 0) {
        terpri(); terpri(); terpri(); terpri();
        prt("----\n");
        terpri();
        prt("(end references)\n");
        terpri(); terpri();
    }
    if (!empty(block_id)) {
        terpri();
        prt("%.*s\n", len(block_id), block_id.buf);
        terpri();
    }
    while (!empty(block)) {
        line = next_line(&block);
        if (len(line) >= 3 && line.buf[0] == '@' && line.buf[1] == '-' && line.buf[2] == ' ') {
            continue;
        }
        if (len(line) > 0 && line.buf[0] == '@') {
            span ref_block = chase_ref(line);
            if (empty(ref_block)) {
                prt("%.*s\n", len(line), line.buf);
            } else {
                expand_refs_rec(ref_block, depth + 1);
            }
        } else {
            prt("%.*s\n", len(line), line.buf);
        }
    }
}

*/
/* #out2cmp
@- Pattern for redirecting output.
@- Used to allow the convenient functions prt etc to be used when writing to cmp space.
We use out2cmp to redirect output functions to cmp space, and store the out_sav opaque type which it returns.
Later after our printing is all done and before returning, we must call out_rst and pass the out_sav value.
This will reset the output to whatever it may have been before.
E.g. `out_sav sav = out2cmp(); ... do all printing ... ; out_rst(sav);`.
*/

/* #out2file @out2cmp @span_ret
@- Redirecting output to a file.

The out2file pattern describes the best way of using spanio to write data into a file.
@- This may change over time, but it is intended that we can always use #out2file to get the current best practice.
@- Currently, this buffers everything in cmp space, but that's fine for the files we are writing, which are frequent but small.
@- (We reclaim the space after writing the file.)

This is a combination of #out2cmp, above, and write_to_file_span.

Specifically:

We use the span ret pattern to set up a span, which we aren't actually going to return here, but write into a file.

We set the .buf of this span to the current .end of cmp space.

Then we use out2cmp to redirect all print functions to cmp space, storing the value it returns.

Then we do all our printing, with prt, wrs, and all the other normal spanio functions.

Once all our printing is done, we set the .end of our ret buffer to the new .end of cmp.
We then call out_rst with the value from out2cmp earlier, resetting the output mode of the library to whatever it was before.

We then write the file to disk using write_to_file_span.

Once the file has been written to disk, we can give back the memory in the buffer, which we do by setting cmp.end to what it was at the start, namely the .buf of our "ret" span.
@- There's a good example of this pattern in get_revs_cache_put.
@- This one also resets cmp space and out space separately, and actually writes the file into out space.
@- I think this is because the internal calls extend cmp space, but this is something that we can fix with some cmp space usage invariants...
@- A common pattern here will be to allocate some strings in cmp space, then compose them into a full result, then copy that final result to the end of where cmp space was, and return it. In other words, a function that returns a string in cmp space should only extend cmp space by the length of the returned span.
@- Another comment here is that the pattern above seems like it can be easily abstracted by a library method, with one call to start counting, and another call to take everything added, write (or append) to a file, close the file, and give back the cmp space.
@- However, a desired invariant should be that intermediate calls to flush() would not alter where output goes (so probably should be no-ops in this mode when buffering a whole file in cmp space).
@- In any case, several attempts to abstract this into the library were abandoned due to inadequate usability.
*/
/* #blockref_id #blockref_fname

These two helper functions parse parts out of a block reference.

Block references look like:

"@<id>:<fn>"

Or:

"@<id>"

If the <fn> isn't present, the _fname function returns "comment", which is the default transform fname.
*/ 

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

/* #language_comment_starter #language_comment_ender @langtable

In these functions, we are given a language, and based on #langtable, above, Cols. 4 and 5, we return the comment start or end delimiter, respectively, not terminated by a newline.
*/

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

/* #expand_refs_2

span expand_refs_2(span,span);

Here we get a span with references to be expanded, and a mode.

There is a resource-management part of the problem which we handle here, and a recursive part.

We set up span that we will return, pointing .buf to the current cmp.end.
We call spans_arena_push.

@out2cmp

We call expand_refs_2_rec for the recursive part (which will output the expanded block into cmp space using prt).
It takes our two args, a 0 for suppressing block delims, and a 0 for recursion depth.

Finally we call spans_arena_pop.
We update our ret span's .end to the current cmp.end, and return it.
*/

span expand_refs_2(span refs, span mode) {
    span ret;
    ret.buf = cmp.end;

    spans_arena_push();
    out_sav saved_out_sav = out2cmp();
    
    expand_refs_2_rec(refs, mode, 0, 0);
    
    out_rst(saved_out_sav);
    spans_arena_pop();

    ret.end = cmp.end;
    return ret;
}

/* #expand_refs_2_rec
@- TODO: probably this should be two or three functions that are mutually recursive, instead of just one

@- TODO: depth check

void expand_refs_2_rec(span,span,int,int);

A block can have two kinds of references.

The "top refs", in the top line or metadata line, represent background information that the model (or programmer) may need to have.

It may be better to present this to the model as a separate message.

So here we take, in addition to the span, three arguments:

- whether to expand the top line only ("context"), the block body only ("body"), or "both".
- whether we are in a comment-delimiter-stripping context (0 or 1).
- a recursion depth (the number of recursive calls above us, i.e. 0 on the first call).

If we are in the "both" mode, we first recurse with "context", then with "body".
In both cases we pass the comment-delimiter-stripping context down if it is set.

We look up the block language with language_for_block.
We use this to get the block comment delimiters, and put these in variables for later use.

If we are in the context mode, then we handle only the top line.
First we get the top line with next_line, and tokenize with split_whitespace.

Then we iterate over the tokens.
If the token starts with '#' it is the block id and, we skip it.

If it starts with '@', then it is a block reference and we will process it as described below.

If it doesn't start with either of those things, we simply ignore it and continue.

To process a block reference in "top-line" or "context" mode:

- we get the id out of it, and the transform fname
- we call chase_ref_2 to get the block contents, if any, or the null span if not found

If the block content is not found then we print the failing block reference as-is on a line, with a blank line below it.

Now we handle the transform name.

If it is "comment", then we get the comment part of the block and recurse on that.
Specifically, we call ourselves with
- the comment part,
- the null span, indicating the "both" expansion mode, and
- the comment-delimiter-stripping context remaining the same.
After the recursive call, we print a blank line with terpri.

If it is "code", then we get the code part of the block and print it using wrs, also followed by a blank line.

If it is "all", then we first recurse on the comment part, exactly as described above, and then we print the code part.
(Including the blank line following each of them.)

This is the end of our handling of the top-line tokens, which is also the end of our handling of context mode.

Now we describe the "body" mode.

We remove the top line to process it separately.
We tokenize it as before and iterate over the tokens, finding the first one that starts with '#', if any, which is the block id.
@- TODO: we should allow multiple block ids since we're already using this pattern in a few places profitably
@- probabli: We [...] and build up a set of block ids using concat.
@- er, actually, we can just print the delimiter first and the parse the top line, printing all the block ids we find
@- tried this manually

If the comment-delimiter-stripping argument is 0, and there was a block id, we print the comment delimiter, a space, the block id (which has the '#' already included), and two newlines.
If there was no block id, we just print the comment delimiter and two newlines.

If the comment-delimiter-stripping argument is set, we do none of this.

Now we iterate over the remaining lines of the body, handling each:

If the line starts with "@- ", it is a comment line and we simply skip it.
If the line starts with '@', then we process it as described below.
Otherwise, we find out if the line is the last one, by checking if the trim() of the span that we are consuming next lines from is already empty, and if so we handle it specially as described below.
Otherwise, we simply print the line as-is, followed by a newline (as next_line will have stripped the original newline off).

To process a block reference in "body" mode:

- as before, we get the id and transform fname out of it
- we call chase_ref_2 to get the contents or null span

Again if the block reference is not found we print the failing reference followed by newline (but without a blank line following it in this context; in the comment body the user can include newlines directly where desired).

Now again we handle the transform name.
If it's "comment", we recurse exactly as above, but we don't add the blank line after.
If it's "code", we print the code part, and also don't add a blank line after.
If it's "all", we recurse on the comment part, then we do add the blank line, then we print the code, without a blank line.

To process the last body line:
If the comment-delimiter-stripping argument is 1, we use ends_with, shorten, and the block comment ender that we stored above to strip the block comment ender from the end of the last line, if one is found, if not just leave it as-is.
Then we print the last line just like it was any other body line, followed by a newline as usual.

@- After handling all the body lines, if the comment-delimiter-stripping argument is 0, we print the closing delimiter on a line by itself.
*/

void expand_refs_2_rec(span block, span mode, int comment_context, int depth) {
    if (depth > 512) { prt("block expansion depth limit (512) exceeded, possible reference cycle?\n"); flush(); exit(1); } // late manual addition

    span language = language_for_block(block);
    span comment_start = language_comment_starter(language);
    span comment_end = language_comment_ender(language);

    if (span_eq(mode, S("both"))) {
        expand_refs_2_rec(block, S("context"), comment_context, depth);
        expand_refs_2_rec(block, S("body"), comment_context, depth);
        return;
    }

    if (span_eq(mode, S("context"))) {
        span top_line = next_line(&block);
        spans tokens = split_whitespace(top_line);

        for (int i = 0; i < tokens.n; ++i) {
            span token = tokens.a[i];

            if (token.buf[0] == '#') continue;

            if (token.buf[0] == '@') {
                span ref_id = blockref_id(token);
                span transform = blockref_fname(token);
                span ref_content = chase_ref_2(ref_id);

                if (empty(ref_content)) {
                    prt("%.*s\n\n", len(token), token.buf);
                    continue;
                }

                if (span_eq(transform, S("comment"))) {
                    span comment = block_comment_part(ref_content);
                    //expand_refs_2_rec(comment, S("both"), 1, depth + 1);
                    expand_refs_2_rec(comment, S("both"), comment_context, depth + 1);
                    terpri();
                } else if (span_eq(transform, S("code"))) {
                    span code = block_code_part(ref_content);
                    wrs(code);
                    terpri();
                } else if (span_eq(transform, S("all"))) {
                    span comment = block_comment_part(ref_content);
                    //expand_refs_2_rec(comment, S("both"), 1, depth + 1);
                    expand_refs_2_rec(comment, S("both"), comment_context, depth + 1);
                    terpri();
                    span code = block_code_part(ref_content);
                    wrs(code);
                    terpri();
                }
            }
        }
        return;
    }

    if (span_eq(mode, S("body"))) {
        span top_line = next_line(&block);
        spans tokens = split_whitespace(top_line);

        // manual:
        if (!comment_context) {
          wrs(comment_start);
          sp();
        //}

          // I guess we actually only want to print the id when we're not in a comment already, i.e. not when inline-expanding
          for (int i = 0; i < tokens.n; ++i) {
              if (tokens.a[i].buf[0] == '#') {
                  wrs(tokens.a[i]);
                  sp();
              }
          }
          bksp(); // lose the extra space
          terpri();

        }

        /*
        span block_id = nullspan();
        for (int i = 0; i < tokens.n; ++i) {
            if (tokens.a[i].buf[0] == '#') {
                block_id = tokens.a[i];
                break;
            }
        }

        if (!comment_context && !empty(block_id)) {
            wrs(comment_start);
            sp();
            wrs(block_id);
            terpri();
            terpri();
        }
        */

        while (!empty(block)) {
            span line = next_line(&block);
            if (starts_with(line, S("@- "))) continue;

            if (line.buf[0] == '@') {
                span ref_id = blockref_id(line);
                span transform = blockref_fname(line);
                span ref_content = chase_ref_2(ref_id);

                if (empty(ref_content)) {
                    prt("%.*s\n", len(line), line.buf);
                    continue;
                }

                if (span_eq(transform, S("comment"))) {
                    span comment = block_comment_part(ref_content);
                    expand_refs_2_rec(comment, S("both"), 1, depth + 1);
                } else if (span_eq(transform, S("code"))) {
                    span code = block_code_part(ref_content);
                    wrs(code);
                } else if (span_eq(transform, S("all"))) {
                    span comment = block_comment_part(ref_content);
                    expand_refs_2_rec(comment, S("both"), 1, depth + 1);
                    terpri();
                    span code = block_code_part(ref_content);
                    wrs(code);
                }
            } else {
                if (empty(trim(block))) {
                    if (comment_context && ends_with(line, comment_end)) {
                        shorten(&line, len(comment_end));
                    }
                }
                wrs(line);
                terpri();
            }
        }
    }
}

/* #chase_ref @sio

Here we get a reference like "@id" where "id" is any block identifier.
The reference may also have a modifier, which is a ":" followed by a function name, like "@id:all".

We check that the "@" is present (and return the null span if not) but otherwise we won't need it so we move past it.
We check if the ":" is present, as we must handle both cases.

We use block_by_id to get the block, and if there is no match, we return the null span.

Finally we tail call block_transforms with the block (from state) and the modifier function name, if any.
The default modifier is "inner-comment", so if one was not provided, we use that.

span chase_ref(span ref) {
    span ret = nullspan();
    if (empty(ref) || ref.buf[0] != '@') return ret;

    advance1(&ref);

    int mod_index = find_char(ref, ':');
    span id = (mod_index != -1) ? first_n(ref, mod_index) : ref;
    span mod = (mod_index != -1) ? skip_n(ref, mod_index + 1) : S("inner-comment");

    int block_idx = block_by_id(id);
    if (block_idx == -1) return ret;

    span block = state->blocks.a[block_idx];
    return block_transforms(block, mod);
}

*/
/* #chase_ref_2 @sio

span chase_ref_2(span);

Here we get bare block id (i.e. without the "@" or anything else), and return the block comments.

We use block_by_id to get the block, and if there is no match, we return the null span.
Otherwise we return the block (from state).
*/

span chase_ref_2(span ref_id) {
    int idx = block_by_id(ref_id);
    if (idx == -1) {
        return nullspan();
    }
    return state->blocks.a[idx];
}

/* old chase_ref_2 @sio

Here we get a reference like "@id" where "id" is any block identifier.
The reference may also have a modifier, which is a ":" followed by a function name, like "@id:all".

We check that the "@" is present (and return the null span if not) but otherwise we won't need it so we move past it.
We check if the ":" is present, and if it is, we take the part before it as the id.
Otherwise the id is everything (after the "@" which we've already handled).

We use block_by_id to get the block, and if there is no match, we return the null span.
Otherwise we return the block (from state).

span chase_ref_2(span ref) {
    if (empty(ref) || *ref.buf != '@') return nullspan();
    advance1(&ref);

    span id = ref;
    int colon_pos = find_char(ref, ':');
    if (colon_pos != -1) {
        id = take_n(colon_pos, &ref);
    }

    int block_idx = block_by_id(id);
    if (block_idx == -1) return nullspan();

    span block = state->blocks.a[block_idx];
    
    return block;
}

*/
/* #strip_markdown_codeblock

span strip_markdown_codeblock(span);

We are given a span and we find the code inside a code block, if there is one.

First we declare a span that we will return.

We make a copy of the input and iterate over all the lines and find those that start with "```".
The first we call the top line and the second the end line.

If there are not exactly two such lines we return the input unchanged.

Otherwise, our return span starts after the newline of the top line and ends before the newline of the last line before the end line.

Therefore, in our loop, we can simply count the "```" lines we have seen.
If it is zero, we set ret.buf, if one, we set ret.end, and if it is two, we return the input unchanged.
After the loop, if the count is not exactly two, we again return the input unchanged, otherwise we return ret.
*/

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

/* #send_to_clipboard

void send_to_clipboard(span);

In send_to_clipboard, we are given a span and we must send it to the clipboard using a user-provided method, since this varies quite a bit between environments.

There is a global ui_state* variable "state" with a span cbcopy on it.

Before we do anything else we ensure this is set by calling ensure_conf_var with the message "The command to pipe data to the clipboard on your system. For Mac try \"pbcopy\", Linux \"xclip -i -selection clipboard\", Windows \"clip.exe\"".

We run this as a command and pass the span data to its stdin.
We use the `s_buffer()` library method and heap buffer pattern to get a null-terminated string for popen from our span conf var.

We complain and exit if anything goes wrong as per usual.
*/

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
/* #compile()

void compile();

In compile(), we take the state and execute buildcmd, which is a config parameter.

First we call ensure_conf_var(state->buildcmd), since we are about to use that setting.

Next we print the command that we are going to run and flush, so the user sees something before the compiler process, which may be slow to produce output.

Then we use system(3) on a 2048-char buf which we allocate and statically zero.
Our s_buffer() interface (s_buffer(char*,int,span)) lets us set buildcmd as a null-terminated string beginning at buf.

We wait for another keystroke before returning if the compiler process fails, so the user can read the compiler errors (later we'll handle them better).
(Remember to call flush() before getch() so the user sees the prompt (which is "Build failed, press any key to continue...").)

On the other hand, if the build succeeds, we don't need the extra keystroke and go back to the main loop after a 1s delay so the user has time to read the success message before the main loop refreshes the current block.
In this case we prt "Build succeeded" on a line.

(We aren't doing this yet, but later we'll put something on the state to provide more status info to the user.)
*/

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
        sleep(1); // Give time for the user to read the message
    }
}
/* #replace_code_clipboard

void replace_code_clipboard();

In replace_code_clipboard, we pipe in the result of running state->cbpaste.

First we call ensure_conf_var with the message "Command to get text from the clipboard on your platform (Mac: pbpaste, Linux: try xclip -o -selection clipboard, Windows: ?)"

This will contain a command like "xclip -o -selection clipboard" (our default) or "pbpaste" on Mac, and comes from our conf file.

Then we call pipe_cmd_cmp(span) and returns a span of piped in data from the clipboard, which we pass on to replace_block_code_part().
*/

void replace_code_clipboard() {
    ensure_conf_var(&state->cbpaste, S("Command to get text from the clipboard on your platform (Mac: \"pbpaste\", Linux: \"xclip -o -selection clipboard\", Windows: TODO: fill this in)"), S("xclip -o -selection clipboard"));
    span new_content = pipe_cmd_cmp(state->cbpaste);
    replace_block_code_part(new_content);
}

/* #pipe_cmd_cmp()

span pipe_cmd_cmp(span);

We get a command (as a span) and we run it, sending the output into the complement of cmp in cmp_space.

Use s_buffer() and a char[2048] to prep the popen() call.

We use the cmp buffer to store this data, starting from cmp.end (which is always somewhere before the end of the cmp space big buffer).

The space that we can use is the difference between cmp_space + BUF_SZ, which locates the end of the cmp_space, and cmp.end, which is always less than this limit.

We then create a span, which is pointing into cmp, capturing the new data we just captured, which is our return value.

Note: obviously we need the value of fread() to know the length of the incoming data

Note: we can assert that the bytes read fits in the cmp space
*/

span pipe_cmd_cmp(span cmd) {
    char cmd_str[2048];
    s_buffer(cmd_str, 2048, cmd);
    FILE *pipe = popen(cmd_str, "r");
    assert(pipe != NULL);

    size_t space_available = (cmp_space + BUF_SZ) - cmp.end;
    size_t bytes_read = fread(cmp.end, 1, space_available, pipe);
    assert(bytes_read <= space_available);

    span result = {cmp.end, cmp.end + bytes_read};
    cmp.end += bytes_read;

    pclose(pipe);
    return result;
}

/* #replace_block_code_part @handle_edited_file:all

void replace_block_code_part(span);

Here we get a span (into cmp space) which contains a message from an LLM which may contain a new code part for the current block.
First we strip the markdown codeblock (if any), to get just the raw code part.

Note: To get the length of a span, use len().
Note: Do this every time, not the just the first time.

Similar to handle_edited_file() above, we are given a span (instead of a file) and we must update the current block, moving data around in memory as necessary.

We put the original block's span in a local variable for convenience, as well as the file index for that block.

The end result of inp should contain:
- the contents of inp currently, up to the start (the .buf) of the original block.
- the comment part of the current block, which we can get from block_comment_part on the current block.
- up to two newlines unless the block comment part already ends with them
- the code part coming from the clipboard in our second argument
- one newline after the code part and before the next block
- current contents of inp from the .end of the original block to the inp.end of original input.

We check whether we are adding zero, one, or two newlines between the comment part and the new code part.
If the comment part is empty or is less than len 2, we don't do the check and don't add any newlines.
We get the index of the projfile from file_for_block.
Then we check the length of what the new block will be (comment part + opt. newlines + new part + newline).
We then compare this to the old block length and do a memmove if necessary on the "rest" of inp, so that we have a gap to accommodate the new block's len.
Then we simply copy any newlines and the new code into inp.
(We do not need to copy the comment part, as it is already there in the original block.)

We then must update the .end of the current file contents, and both the .buf and .end of all subsequent projfiles, since the block length may have changed and therefore the file contents lengths will have also changed.

As before we then find the current locations of the blocks and handle all downstream tasks with ingest().

Once all this is done, we call new_rev, passing a null span for the filename, since there's no filename here, and the file index.
*/

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
/* #output_design

Related to the palette feature.

When we have an output of an operation, we want to store it in a consistent way.

We store each output in a file.

For the filename, we use <cmprdir>/outputs/<timestamp>.

First we write the following headers:

block_id: <block_id, if any>
checksum: <checksum of the block contents>
output_of: <human name of the operation>

Then we write a blank line, followed by the output itself, which is just our input.
If the output does not end with a newline, we also add one with terpri() so that our file is a proper text file.

@- We could support the headers by also expanding a template.
@- We already have current_block_template_vars, we'd just add the block id and checksum.
@- But for now...
To get the checksum we will use selected_checksum on the current block.

To write the checksum into the header we will call the pr_checksum() function (which (unfortunately) includes a newline already).
@- These really should just go in the current_block_template_vars...

We store the output filenames on state.
To refresh this list we use get_outputs.

To lookup an output we have read_output_headers which takes a basename (which are the elements of state->outputs_filenames) and returns an assoc spans.

To get the actual output part, and not the headers, we have read_output_body, which uses the same key (the basename, which is also a timestamp of the output).

To parse the headers, we iterate over lines.
We split each line on ":".
Everything before the colon is the key.
The trim of everything after the colon is the value.

When we find a blank line, we have reached the end of the headers.
*/
/* #agreement_SAV @gcb @out2file @filename_template @id_for_block @blocks @output_design

void agreement_SAV(span);

We are given a message from an LLM (just the last message, not a full chat session).

We write the output file as described by #output_design, above.
The output_of header should be "agreement".

@- This probably shouldn't go here, but just to get things going...
@- After everything is done we call get_outputs() to let the rest of the system know about the new output (so it can appear in the UI).
*/

void agreement_SAV(span message) {
    span ret;
    ret.buf = cmp.end;
    out_sav sav = out2cmp();

    span block_id = id_for_block(state->blocks.a[state->curr_block_idx]);
    checksum cksum = selected_checksum(state->blocks.a[state->curr_block_idx]);

    prt("block_id: %.*s\n", len(block_id), block_id.buf);
    prt("checksum: "); pr_checksum(cksum);
    prt("output_of: agreement\n\n");
    wrs(message);
    if(empty(message) || message.end[-1] != '\n') terpri();

    ret.end = cmp.end;
    out_rst(sav);

    span cmprdir = S("{cmprdir}/outputs/");
    span timestamp = S("{timestamp}");
    span filepath_template = concat(cmprdir, timestamp);
    span filepath = filename_template(filepath_template);

    write_to_file_span(ret, filepath, 1);
    cmp.end = ret.buf;
    //get_outputs();
}

/* #output_save

void output_save(span operation, span message);

We dispatch on operation: agreement -> agreement_SAV, agreement_to_nl_diff -> proposed_diff_SAV, all others -> generic_output_save.
*/

void output_save(span operation, span message) {
    if (span_eq(operation, S("agreement"))) {
        agreement_SAV(message);
    } else if (span_eq(operation, S("agreement_to_nl_diff"))) {
        proposed_diff_SAV(message);
    } else {
        generic_output_save(operation, message);
    }
}

/* #make_output_saver @partials

Our llm_message_handler type (an alias for Partial) is the type used for callbacks to handle the output of an LLM.

Our make_output_saver returns this type and takes a single span as argument.

llm_message_handler make_output_saver(span operation);

It is a simple wrapper around partial_sp_sp.

Our llm machinery, instead of calling a function pointer that is passed in, will instead call apply_partial.
*/

llm_message_handler make_output_saver(span operation) {
    return partial_sp_sp(operation, output_save);
}

/* #generic_output_save @gcb @out2file @filename_template @id_for_block @blocks @output_design

void generic_output_save(span operation, span message);

void agreement_SAV(span);

We are given a message from an LLM (just the last message, not a full chat session).

We write the output file as described by #output_design, above.
The output_of header should be the operation.
*/

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

/* #proposed_diff_SAV @gcb @out2file @filename_template @id_for_block @blocks @output_design
@- this and agreement_SAV should probably be unified into some generic method that handles both

void proposed_diff_SAV(span);

We are given a message from an LLM (just the last message, not a full chat session).

We write the output file as described by #output_design, above.
For the output_of header we use "agreement_to_nl_diff".
We add a header "relationship" with the value "proposed diff".
*/

void proposed_diff_SAV(span message) {
    span block = state->blocks.a[state->curr_block_idx];
    checksum cksum = selected_checksum(block);
    char *output_of = "agreement_to_nl_diff";
    char *relationship = "proposed diff";

    span ret;
    ret.buf = cmp.end;

    out_sav sav = out2cmp();
    prt("block_id: "); wrs(id_for_block(block)); terpri();
    prt("checksum: "); pr_checksum(cksum);
    prt("output_of: %s\n", output_of);
    prt("relationship: %s\n", relationship);
    terpri();
    wrs(message);
    if (!empty(message) && message.end[-1] != '\n') terpri();
    ret.end = cmp.end;
    out_rst(sav);

    span filename = filename_template(S("{cmprdir}/outputs/{timestamp}"));
    write_to_file_span(ret, filename, 1);
    cmp.end = ret.buf;
    get_outputs();
}

/* #span_cmp_wrapper

Moved out of get_outputs because clang can't handle nested functions.

int span_cmp_wrapper(const void*, const void*);
*/

int span_cmp_wrapper(const void *a, const void *b) {
    return span_cmp(*(span *)a, *(span *)b);
}

/* #get_outputs @filename_template @get_revs:all

Here we get the filenames in the directory <cmprdir>/outputs/.

void get_outputs();

This is similar to get_revs, above.

However, we now support extending generic arrays, instead of needing the size in advance, so we can make the implementation simpler.

It's currently unoptimized and we don't care about making it very robust yet, so:

- We declare a static char buffer of size 2^{18}. Our filenames spans will point into it, and if we fill it, we will for now simply crash.
- We iterate just once over the directory, and put the filename into our static buffer, including the null terminator, and then create a span which also includes the null, which we push onto our return spans value.
- As in #get_revs, we verify that the filename matches the pattern of "YYYYMMDD-hhmmss", and otherwise skip it; this may let us extend the directory contents in the future with backward compatibility, and is needed anyway since we will parse the filenames as metadata downstream of this function.
- Also as in #get_revs, we sort the spans so they are in chronological order.
- We'll stick the outputs on state->outputs_files for now.

@- Note: gpt-4o decided to follow the example code, instead of the instruction, and leaves the null terminator out of the spans it constructs, just like get_revs did before.
@- I don't know if I care about this, but the point is that s() doesn't allocate when the buffer already includes the null byte.
@- I've considered having s() actually expand to take the byte following the span passed in... dangerous but s() is always used directly, never saved, so actually probably perfectly safe (the only problem being if the span is at the edge of some memory region, or if what follows it is later changed while the char * is for some reason stored...); it would be elegant to have s(S(.)) and S(s(.)) both be identity functions whenever possible (i.e. except when starting with a span that doesn't have a null byte immediately after it (or as part of it(?))).
*/

void get_outputs() {
    span outdir = concat(state->cmprdir, S("/outputs"));
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

/* #dir_listing @gcb @spans_usage

In this function we read a directory listing into cmp space and return a spans containing the filenames in that directory, in sorted order.

spans dir_listing(span dirname);

We use opendir and readdir to iterate over the directory contents.

For each entry->d_name, we use prs() to copy the filename into cmp space.

We use a spans ret to hold and return these results, and we sort them with qsort and a helper function span_cmp_wrapper, which is already written.

@- TODO: probably move this into spanio.
*/

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

/* #read_output_headers @output_design @filename_template @assoc_spans

spans read_output_headers(span bname);

We are given a basename for an output, and we construct the full filename (<cmprdir>/outputs/<timestamp> where <timestamp> is the basename).

We read the file contents into cmp space and then parse the headers, and reset cmp space to keep the headers but give back the memory for the body.

Then we return the headers.

We give back the memory to cmp by setting cmp.end to the end of the last value that we found (so that the spans that we return remains valid).
*/

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

/* #replace_block @replace_block_code_part @blocks

void replace_block(span);

This is the same as replace_block_code_part, above, except that instead of being given the code part of a block, we are given the entire contents of a block.
So we simply ignore everything having to do with code parts or comment parts, and replace the entire block contents, then otherwise proceed as before.
*/

void replace_block(span new_block) {
    span original_block = state->blocks.a[state->curr_block_idx];
    int file_index = file_for_block(original_block);

    size_t new_block_len = len(new_block);
    size_t old_block_len = len(original_block);
    size_t rest_len = state->files.a[file_index].contents.end - original_block.end;

    if (new_block_len != old_block_len) {
        memmove(original_block.buf + new_block_len, original_block.end, rest_len);
    }

    memcpy(original_block.buf, new_block.buf, new_block_len);

    state->files.a[file_index].contents.end = original_block.buf + new_block_len + rest_len;

    for (int i = file_index + 1; i < state->files.n; i++) {
        state->files.a[i].contents.buf = state->files.a[i - 1].contents.end;
        state->files.a[i].contents.end = state->files.a[i].contents.buf + len(state->files.a[i].contents);
    }

    ingest();
    new_rev(nullspan(), file_index);
}

/* #cmpr_init

We are called without args and set up some configuration and empty directories to prepare the CWD for use as a cmpr project.

In sh terms:

- mkdir -p .cmpr/{,revs,tmp,api_calls}
- touch .cmpr/conf

Note that if the CWD is already initialized as a cmpr project this is a no-op, i.e. the init is idempotent (up to file access times and similar).
*/

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

/* #simple_message_handler @partials

llm_message_handler is an alias for Partial.

To make it easy to use in op definitions, we have a thin wrapper around partial_0_sp for handlers that don't require any input besides the LLM message itself.

llm_message_handler simple_message_handler(void(*f)(span));

*/

llm_message_handler simple_message_handler(void(*f)(span)) {
    return partial_0_sp(f);
}

