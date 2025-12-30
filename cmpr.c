#include "spanio.c"



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




typedef struct {
    u64 __u;
} checksum;

MAKE_ARENA(checksum, checksums, 256)


typedef struct {
    span path;
    span language;
    span contents;
    checksum cksum;
} projfile;

MAKE_ARENA(projfile, projfiles, 256)

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


typedef struct {
    span event_str;
    unsigned char strength;
} event_entry;

MAKE_ARENA(event_entry, event_entries, 256)

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
    #define X(name) span name;
    CONFIG_FIELDS
    #undef X
} ui_state;

ui_state* state;


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

void event_load_T() {
    span t_file = prs("%.*s/T", len(state->cmprdir), state->cmprdir.buf);
    span content = read_whole_file(t_file);
    if (empty(content)) return; // File doesn't exist or is empty

    event_parse_content(content);
}

void event_save_T() {
    span saved_cmp = cmp;
    span t_file = prs("%.*s/T", len(state->cmprdir), state->cmprdir.buf);

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

void event_add_internal(span event_str, unsigned char strength) {
    // Search for existing event
    for (size_t i = 0; i < state->events.n; i++) {
        if (span_eq(state->events.a[i].event_str, event_str)) {
            state->events.a[i].strength = strength;
            return;
        }
    }
    
    // Not found, add new event
    event_entry e;
    e.event_str = event_str;
    e.strength = strength;
    event_entries_push(&state->events, e);
}

void event_T0() {
    state->events.n = 0;
    event_save_T();
}

void event_add(span event_str, unsigned char strength) {
    event_add_internal(event_str, strength);
    event_save_T();
}

void event_memorize() {
    span saved_cmp = cmp;
    span events_dir = prs("%.*s/events", len(state->cmprdir), state->cmprdir.buf);
    mkdir(s(events_dir), 0777);

    // Get current timestamp
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    // Format timestamp as YYYYMMDD-HHMMSS
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

    for (size_t i = 0; i < state->events.n; i++) {
        prt("\"");
        wrs_esc(state->events.a[i].event_str);
        prt("\" %d.\n", state->events.a[i].strength);
    }

    content.end = cmp.end;
    out_rst(sav);

    write_to_file_span(content, filename, 0);
    cmp = saved_cmp;
}

void event_print_T() {
    for (size_t i = 0; i < state->events.n; i++) {
        prt("\"");
        wrs_esc(state->events.a[i].event_str);
        prt("\" %d.\n", state->events.a[i].strength);
    }
    flush();
}

void event_recall() {
    // 1. Check that T is non-empty (must have query events)
    if (state->events.n == 0) {
        prt("Error: Cannot recall with empty T. Add query events first.\n");
        flush_exit(1);
    }

    // Save current T events as query (before we potentially overwrite them)
    event_entries query_events = state->events;
    
    // 2. Get list of snapshot files
    span events_dir = prs("%.*s/events", len(state->cmprdir), state->cmprdir.buf);
    spans files = dir_listing(events_dir);

    if (files.n == 0) {
        prt("No memorized snapshots found.\n");
        flush_exit(1);
    }

    // 3. Search snapshots in reverse chronological order (newest first)
    for (int i = (int)files.n - 1; i >= 0; i--) {
        span snapshot_path = prs("%.*s/%.*s", len(events_dir), events_dir.buf, 
                                  len(files.a[i]), files.a[i].buf);
        span content = read_whole_file(snapshot_path);
        
        if (empty(content)) continue;

        // Parse snapshot into temporary storage
        event_entries snapshot_events = {0};
        span saved_cmp = cmp;
        event_entries saved_state_events = state->events;
        state->events = snapshot_events;
        
        event_parse_content(content);
        snapshot_events = state->events;
        
        // Check if any query event matches any snapshot event
        int found_match = 0;
        for (size_t qi = 0; qi < query_events.n; qi++) {
            for (size_t si = 0; si < snapshot_events.n; si++) {
                if (span_eq(query_events.a[qi].event_str, snapshot_events.a[si].event_str)) {
                    found_match = 1;
                    break;
                }
            }
            if (found_match) break;
        }

        if (found_match) {
            // Found a match! Load this snapshot into T
            state->events = snapshot_events;
            event_save_T();
            cmp = saved_cmp;
            return;
        }

        // No match, restore state and continue
        state->events = saved_state_events;
        cmp = saved_cmp;
    }

    // 4. No matching snapshot found
    prt("No memorized snapshot contains the query events.\n");
    flush_exit(1);
}

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

typedef struct {
  int success;
  span response;
  span error;
} network_ret;


typedef struct {
    int *revblock_indices;
    int max_index;
    int current_index;
    spans curr_block_ids;
    checksums sorted_line_cksums;
} sbv_state;


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

#include "fdecls.h"

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

void get_code(); // read and index current code
void get_revs(); // read and index revs
spans find_blocks(span); // find the blocks in a file
spans find_blocks_language(span file, span language); // find_blocks helper function dispatching on language
void find_all_lines(); // like find_all_blocks, but for lines; applies to the whole project
void index_block_ids();
void ingest(); // updates everything that needs to be updated after code has changed


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


int main(int argc, char** argv) {
    ui_state stack_state = (ui_state){0};
    state = &stack_state;

    init();
    read_(argc, argv);
    main_loop();
    return 0;
}


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


void read_(int argc, char** argv) {
    clock_gettime(CLOCK_REALTIME, &state->now);
    handle_args(argc, argv);
    check_conf_vars();
    check_dirs();
    event_load_T();
    get_code();
}

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


void read_openai_key() {
    char path[PATH_MAX];
    struct stat st;
    char *home = getenv("HOME");
  
    if (!home) return;

    snprintf(path, PATH_MAX, "%s/.cmpr/openai-key", home);

    if (stat(path, &st) != 0) return;

    state->openai_key = trim(read_file_into_cmp(S(path)));
}


void read_anthropic_key() {
    char path[PATH_MAX];
    struct stat st;
    char *home = getenv("HOME");
  
    if (!home) return;

    snprintf(path, PATH_MAX, "%s/.cmpr/anthropic-key", home);

    if (stat(path, &st) != 0) return;

    state->anthropic_key = trim(read_file_into_cmp(S(path)));
}


span filename_template(span template) {
    spans vars = filename_variables();
    return expand_template(template, vars);
}



span assoc_spans_lookup(spans assoc_list, span key) {
    for (size_t i = 0; i < assoc_list.n / 2; ++i) {
        if (span_eq(assoc_list.a[i*2], key)) {
            return assoc_list.a[i*2 + 1];
        }
    }
    return nullspan();
}


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


void print_config() {
    #define X(name) prt(#name ": %.*s\n", len(state->name), state->name.buf);
    CONFIG_FIELDS
    #undef X
    flush();
}


// Forward declaration for function generated in bootstrap_content.c
span get_bootstrap_content_span();

void print_bootstrap() {
    span content = get_bootstrap_content_span();
    prt("%.*s", len(content), content.buf);
    flush();
}


void handle_args(int argc, char **argv) {

int ind_conf = 0;
	int ind_print_conf = 0;
	int ind_print_bootstrap = 0;
	int ind_init = 0;
	int ind_help = 0;
	int ind_version = 0;
	int ind_print_block = 0;
	int ind_print_comment = 0;
	int ind_print_code = 0;
	int ind_expand_block = 0;
	int ind_content_index = 0;
	int ind_grep = 0;
	int ind_count_blocks = 0;
	int ind_files_blocks = 0;
	int ind_print_all = 0;
	int ind_rewritepl = 0;
	int ind_prompt = 0;
	int ind_after = 0;
	int ind_replace = 0;
	int ind_replace_comment = 0;
	int ind_replace_code = 0;
	int ind_run = 0;
	int ind_agents = 0;
	int ind_checksum = 0;
	int ind_T0 = 0;
	int ind_event = 0;
	int ind_strength = 0;
	int ind_memorize = 0;
	int ind_recall = 0;
	int ind_T = 0;
	int ind_map_error = 0;
	int ind_test_block_map = 0;
	int ind_wants = 0;
	int ind_wants_status = 0;
	int ind_agents_wants = 0;
	int ind_wants_dashboard = 0;
	int ind_event_report = 0;
	int ind_export_docs = 0;
	int ind_file_argument = 0;

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
	char *arg_replace = NULL;
	char *arg_replace_comment = NULL;
	char *arg_replace_code = NULL;
	char *event_string = NULL;
	char *event_strength_str = NULL;
	char *file_argument = NULL;
	
	int action_arg = 0;

for (int i = 1; i < argc; i++) {
		char *arg = argv[i];
		
		if (strcmp(arg, "--help") == 0) {
			ind_help = 1;
			// Check if next argument exists and doesn't start with "--"
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				help_topic = argv[++i];
			}
		} else if (strcmp(arg, "--version") == 0) {
			ind_version = 1;
		} else if (strcmp(arg, "--init") == 0) {
			ind_init = 1;
		} else if (strcmp(arg, "--conf") == 0) {
			ind_conf = 1;
			if (i + 1 >= argc) { prt("Missing <file> argument for --conf\n"); flush_exit(1); }
			conf_filepath = argv[++i];
		} else if (strcmp(arg, "--print-conf") == 0) {
			ind_print_conf = 1;
		} else if (strcmp(arg, "--print-bootstrap") == 0) {
			ind_print_bootstrap = 1;
		} else if (strcmp(arg, "--print-block") == 0) {
			ind_print_block = 1;
			if (i + 1 >= argc) { prt("Missing <id> argument for --print-block\n"); flush_exit(1); }
			arg_print_block = argv[++i];
		} else if (strcmp(arg, "--print-comment") == 0) {
			ind_print_comment = 1;
			if (i + 1 >= argc) { prt("Missing <id> argument for --print-comment\n"); flush_exit(1); }
			arg_print_comment = argv[++i];
		} else if (strcmp(arg, "--print-code") == 0) {
			ind_print_code = 1;
			if (i + 1 >= argc) { prt("Missing <id> argument for --print-code\n"); flush_exit(1); }
			arg_print_code = argv[++i];
		} else if (strcmp(arg, "--expand-block") == 0) {
			ind_expand_block = 1;
			if (i + 1 >= argc) { prt("Missing <id> argument for --expand-block\n"); flush_exit(1); }
			arg_expand_block = argv[++i];
		} else if (strcmp(arg, "--content-index") == 0) {
			ind_content_index = 1;
			if (i + 1 >= argc) { prt("Missing <search> argument for --content-index\n"); flush_exit(1); }
			content_index_search = argv[++i];
		} else if (strcmp(arg, "--grep") == 0) {
			ind_grep = 1;
			if (i + 1 >= argc) { prt("Missing <pattern> argument for --grep\n"); flush_exit(1); }
			grep_pattern = argv[++i];
		} else if (strcmp(arg, "--count-blocks") == 0) {
			ind_count_blocks = 1;
		} else if (strcmp(arg, "--files-blocks") == 0) {
			ind_files_blocks = 1;
		} else if (strcmp(arg, "--print-all") == 0) {
			ind_print_all = 1;
		} else if (strcmp(arg, "--rewritepl") == 0) {
			ind_rewritepl = 1;
			if (i + 1 >= argc) { prt("Missing <id> argument for --rewritepl\n"); flush_exit(1); }
			arg_rewritepl = argv[++i];
		} else if (strcmp(arg, "--prompt") == 0) {
			ind_prompt = 1;
			if (i + 1 >= argc) { prt("Missing <id> argument for --prompt\n"); flush_exit(1); }
			arg_prompt = argv[++i];
		} else if (strcmp(arg, "--after") == 0) {
			ind_after = 1;
			if (i + 1 >= argc) { prt("Missing <id> argument for --after\n"); flush_exit(1); }
			arg_after = argv[++i];
		} else if (strcmp(arg, "--replace") == 0) {
			ind_replace = 1;
			if (i + 1 >= argc) { prt("Missing <id> argument for --replace\n"); flush_exit(1); }
			arg_replace = argv[++i];
		} else if (strcmp(arg, "--replace-comment") == 0) {
			ind_replace_comment = 1;
			if (i + 1 >= argc) { prt("Missing <id> argument for --replace-comment\n"); flush_exit(1); }
			arg_replace_comment = argv[++i];
		} else if (strcmp(arg, "--replace-code") == 0) {
			ind_replace_code = 1;
			if (i + 1 >= argc) { prt("Missing <id> argument for --replace-code\n"); flush_exit(1); }
			arg_replace_code = argv[++i];
		} else if (strcmp(arg, "--run") == 0) {
			ind_run = 1;
			if (i + 1 >= argc) { prt("Missing <id> argument for --run\n"); flush_exit(1); }
			run_block_id = argv[++i];
		} else if (strcmp(arg, "--agents") == 0) {
			ind_agents = 1;
		} else if (strcmp(arg, "--checksum") == 0) {
			ind_checksum = 1;
		} else if (strcmp(arg, "--T0") == 0) {
			ind_T0 = 1;
		} else if (strcmp(arg, "--event") == 0) {
			ind_event = 1;
			if (i + 1 >= argc) { prt("Missing <string> argument for --event\n"); flush_exit(1); }
			event_string = argv[++i];
		} else if (strcmp(arg, "--strength") == 0) {
			ind_strength = 1;
			if (i + 1 >= argc) { prt("Missing <value> argument for --strength\n"); flush_exit(1); }
			event_strength_str = argv[++i];
		} else if (strcmp(arg, "--memorize") == 0) {
			ind_memorize = 1;
		} else if (strcmp(arg, "--recall") == 0) {
			ind_recall = 1;
		} else if (strcmp(arg, "--T") == 0) {
			ind_T = 1;
		} else if (strcmp(arg, "--map-error") == 0) {
			ind_map_error = 1;
		} else if (strcmp(arg, "--test-block-map") == 0) {
			ind_test_block_map = 1;
		} else if (strcmp(arg, "--wants") == 0) {
			ind_wants = 1;
		} else if (strcmp(arg, "--wants-status") == 0) {
			ind_wants_status = 1;
		} else if (strcmp(arg, "--agents-wants") == 0) {
			ind_agents_wants = 1;
		} else if (strcmp(arg, "--wants-dashboard") == 0) {
			ind_wants_dashboard = 1;
		} else if (strcmp(arg, "--event-report") == 0) {
			ind_event_report = 1;
		} else if (strcmp(arg, "--export-docs") == 0) {
			ind_export_docs = 1;
		} else if (arg[0] == '-' && arg[1] == '-') {
			prt("Unknown flag: "); prt(arg); prt("\n");
			flush_exit(1);
		} else {
                        // handle file arguments
                        ind_file_argument = 1;
                        file_argument = arg;
                }
	}

if (ind_file_argument) {
                state->manual_filename = S(file_argument);
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

	// Handle --print-bootstrap
	if (ind_print_bootstrap) {
		print_bootstrap();
		flush_exit(0);
	}

	// Count action flags (excluding event-related flags which are handled separately)
	action_arg = ind_print_block + ind_print_comment + ind_print_code + ind_expand_block +
	             ind_content_index + ind_grep + ind_count_blocks + ind_files_blocks + ind_print_all +
	             ind_rewritepl + ind_prompt + ind_after + ind_replace + ind_replace_comment + ind_replace_code +
	             ind_run + ind_agents + ind_checksum +
	             ind_map_error + ind_test_block_map +
	             ind_wants +
	             ind_wants_status +
	             ind_agents_wants +
	             ind_wants_dashboard +
	             ind_event_report +
	             ind_export_docs;
	
	if (action_arg > 1) {
		prt("Error: Only one action argument may be used at a time.\n");
		flush_exit(1);
	}
	
	// Get code database if needed (for most commands)
	if (action_arg > 0 && !ind_checksum && !ind_wants) {
		get_code();
	}
	
	// Dispatch to handlers
	if (ind_print_block) {
		int idx = block_from_arg(arg_print_block);
		if (idx < 0 || idx >= state->blocks.n) {
			prt("Block id or index not found: %s\n", arg_print_block);
			flush_exit(1);
		}
		print_block(idx);
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

	if (ind_export_docs) {
		handle_export_docs();
		flush_exit(0);
	}
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
			print_block(i);
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
	
	if (ind_run) {
		handle_run(run_block_id);
		flush_exit(0);
	}
	
	if (ind_agents) {
		handle_agents();
		flush_exit(0);
	}
	
	if (ind_checksum) {
		handle_checksum();
		flush_exit(0);
	}
	
	// Event system commands (have special validation)
	if (ind_T0 || ind_event || ind_strength || ind_memorize || ind_recall || ind_T) {
		if (ind_event && !ind_strength) {
			prt("Error: --event requires --strength\n");
			flush_exit(1);
		}
		if (ind_strength && !ind_event) {
			prt("Error: --strength must be used with --event\n");
			flush_exit(1);
		}
		
		int event_actions = ind_T0 + ind_event + ind_memorize + ind_recall + ind_T;
		if (event_actions > 1) {
			prt("Error: --T0, --event, --memorize, --recall, and --T cannot be combined\n");
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
		if (ind_memorize) {
			event_memorize();
			flush_exit(0);
		}
		if (ind_recall) {
			event_recall();
			flush_exit(0);
		}
		if (ind_T) {
			event_print_T();
			flush_exit(0);
		}
	}
	
	if (ind_map_error) {
		prt("Error: --map-error not yet implemented\n");
		flush_exit(1);
	}
	
	if (ind_test_block_map) {
		block_map_selftest();
		flush_exit(0);
	}
	
	if (ind_wants) {
		handle_wants();
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

	if (ind_event_report) {
		handle_event_report();
		flush_exit(0);
	}

	// No action arg - return to enter interactive mode
}


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

void clear_display() {
    prt("\033[2J\033[H"); // Escape codes to clear the screen and move the cursor to the top-left corner
    flush();
}


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


checksum selected_checksum(span input) {
    static const char key[16] = "ABCDEFGHIJKLMNOP";
    u64 result;
    siphash(input.buf, len(input), key, (uint8_t*)&result, sizeof(result));
    return (checksum){result};
}


void get_code() {
    for (int i = 0; i < state->files.n; i++) {
        state->files.a[i].contents = read_file_S_into_span(state->files.a[i].path, inp_compl());
        inp.end = state->files.a[i].contents.end; // Advance inp to not overwrite contents
    }

    if (state->files.n == 0) state->curr_file_idx = -1;
    else state->curr_file_idx = 0;

    ingest();
}


void ingest() {
    find_all_blocks();
    find_all_lines();
    index_block_ids();
    inp_sanity_checks();
}




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



int block_for_span(span s) {
    for (int i = 0; i < state->blocks.n; i++) {
        if (contains_ptr(state->blocks.a[i], s)) {
            return i;
        }
    }
    return -1;
}


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


checksum current_block_checksum() {
    return selected_checksum(state->blocks.a[state->curr_block_idx]);
}


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


span get_revdir() {
    static char buf[2048] = {0};
    span revs = S("/revs");
    span revdir = concat(state->cmprdir, revs);
    s_buffer(buf, 2048, revdir);
    return S(buf);
}



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



int get_revs_cache_get(span bname, span rev_contents) {
    u8* cmp_end_backup = cmp.end;
    span rev_cache_path = prs("%.*s/cache/v8/revs/%.*s", len(state->cmprdir), state->cmprdir.buf, len(bname), bname.buf);
    if (!readable_file(rev_cache_path)) return 0;
    span rev_cache_contents = read_file_into_cmp(rev_cache_path);
    int result = parse_revfile_cache(bname, rev_cache_contents, rev_contents);
    cmp.end = cmp_end_backup;
    return result;
}


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


int parse_int(span s) {
    if (empty(s) || !isdigit(*s.buf)) {
        prt("Error: initial characters are not digits\n");
        flush();
        exit(1);
    }
    return atoi((char *)s.buf);
}


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


span prs_checksum(checksum c) {
  out_sav sav = out2cmp();
  span ret = {cmp.end};
  pr_checksum(c);
  bksp();
  ret.end = cmp.end;
  out_rst(sav);
  return ret;
}


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


spans find_blocks_language_none(span file) {
    spans blocks = spans_alloc(1);
    spans_push(&blocks, file);
    return blocks;
}


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


void set_highlight() {
    prt("\033[7m");
}

void reset_highlight() {
    prt("\033[0m");
}


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


void bootstrap() {
    ensure_conf_var(&state->bootstrap, S("The bootstrap command generates your initial prompt on stdout. See README for details."), nullspan());
    
    char buf[2048] = {0};
    s_buffer(buf, sizeof(buf), state->bootstrap);
    prt("Running bootstrap command: %s\n", buf);
    flush();

    state->bootstrapprompt = pipe_cmd_cmp(S(buf));
    send_to_clipboard(state->bootstrapprompt);
}

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


void check_dirs() {
    span dirs[] = {
        S("revs/"),
        S("tmp/"),
        S("api_calls/"),
        S("cache/"),
        S("cache/v8/"),
        S("cache/v8/revs/"),
        S("outputs/"),
        S("events/")
    };
    
    char buffer[1024];

    for (int i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        snprintf(buffer, sizeof(buffer), "%.*s/%.*s", len(state->cmprdir), state->cmprdir.buf, len(dirs[i]), dirs[i].buf);
        mkdir(buffer, 0777);
    }
}


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

span language_for_block(span block) {
    int file_index = file_for_block(block);
    return state->files.a[file_index].language;
}


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


json gpt_message(span role, span message) {
    json resp = json_o();
    json_o_extend(&resp, S("role"), json_s(role));
    json_o_extend(&resp, S("content"), json_s(message));
    return resp;
}


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


span block_code_part(span block) {
    span comment = block_comment_part(block);
    block.buf = comment.end;
    return block;
}


/*
I previously had an idea of using the block system itself as a kind of extensible programming system.

For example, if a block can take other blocks as arguments, and produce further blocks as output, then the block system itself becomes agentic.
For example, we could have a "code formatting block" which would then be iterating over the other blocks and enforcing a code formatting invariant.
This could be allowed to consume a certain number of tokens (or really, cents) per day and would presumably be optimized to some standard.

This feels better.
*/

void prompt_palette() {
    spans palette = get_palette();
    int sel = select_menu(palette, -1);
    if (sel >= 0 && sel < palette.n) {
        apply_prompt(palette.a[sel]);
    }
}


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



// Hardcoded prompt templates
span pt_nl2pl_rewrite() { return S("```{langtag}\n{context}\n```\n\n(above: references)\n---\n(below: current task)\n\n```{langtag}\n{comment}\n```\n\nWrite the code only for the current task. Reply only with a code block beginning with \"```{langtag}\". Do not include comments.\n"); }
span pt_agreement() { return S("TODO"); }
span pt_agreement_to_nl_diff() { return S("TODO"); }
span pt_agreement_to_pl_diff() { return S("TODO"); }
span pt_pl2nl_rewrite() { return S("TODO"); }
span pt_nl2algo() { return S("TODO"); }
span pt_summarize_block() { return S("TODO"); }



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


/*output:
...text from the LLM...
*/

void agreement() {
}


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

void output_template_var(spans* ctx, span human_name) {
    span output = lookup_output(human_name);

    if (!empty(output)) {
        spans_push(ctx, human_name);
        spans_push(ctx, output);
    }
}


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

void handle_agents() {
    // Ensure .cmpr/agents/ directory exists
    system("mkdir -p .cmpr/agents");
    
    // Print table header
    prt("%-12s %-12s %-16s %-16s %-8s %s\n", 
        "AGENT", "INSTALLED", "RUNNING", "STATUS", "ISSUES", "LAST RUN");
    prt("%.12s %.12s %.16s %.16s %.8s %.20s\n",
        "------------", "------------", "----------------", 
        "----------------", "--------", "--------------------");
    
    int agent_count = 0;
    
    // Iterate through all blocks to find #agent_* patterns
    for (int i = 0; i < state->block_idx.n; i++) {
        span id = state->block_idx.a[i];
        
        // Check if ID starts with "#agent_"
        if (len(id) >= 8 && id.buf[0] == '#' && 
            id.buf[1] == 'a' && id.buf[2] == 'g' && 
            id.buf[3] == 'e' && id.buf[4] == 'n' && 
            id.buf[5] == 't' && id.buf[6] == '_') {
            
            // Extract agent name (everything after "#agent_")
            span name_span = {id.buf + 7, id.end};
            char agent_name[256];
            int name_len = len(name_span);
            if (name_len >= sizeof(agent_name)) name_len = sizeof(agent_name) - 1;
            memcpy(agent_name, name_span.buf, name_len);
            agent_name[name_len] = '\0';
            
            // Check installation status
            char agent_path[512];
            snprintf(agent_path, sizeof(agent_path), "agents/%s", agent_name);
            int installed = (access(agent_path, X_OK) == 0);
            
            // Check running status
            char pid_path[512];
            char pid_str[32] = "-";
            int running = 0;
            int pid = 0;
            
            snprintf(pid_path, sizeof(pid_path), ".cmpr/agents/%s/pid", agent_name);
            FILE *pid_file = fopen(pid_path, "r");
            if (pid_file) {
                if (fscanf(pid_file, "%d", &pid) == 1) {
                    // Verify process is actually running
                    char kill_cmd[256];
                    snprintf(kill_cmd, sizeof(kill_cmd), "kill -0 %d 2>/dev/null", pid);
                    if (system(kill_cmd) == 0) {
                        running = 1;
                        snprintf(pid_str, sizeof(pid_str), "yes (%d)", pid);
                    } else {
                        // Stale PID file, remove it
                        unlink(pid_path);
                        snprintf(pid_str, sizeof(pid_str), "no");
                    }
                } else {
                    snprintf(pid_str, sizeof(pid_str), "no");
                }
                fclose(pid_file);
            } else if (installed) {
                snprintf(pid_str, sizeof(pid_str), "no");
            } else {
                snprintf(pid_str, sizeof(pid_str), "-");
            }
            
            // Read metrics.json
            char metrics_path[512];
            char status_str[64] = "-";
            char issues_str[16] = "-";
            char timestamp_str[32] = "-";
            
            snprintf(metrics_path, sizeof(metrics_path), 
                    ".cmpr/agents/%s/metrics.json", agent_name);
            FILE *metrics_file = fopen(metrics_path, "r");
            if (metrics_file) {
                char line[1024];
                int count = -1;
                
                while (fgets(line, sizeof(line), metrics_file)) {
                    // Simple JSON parsing - look for key patterns
                    char *status_key = strstr(line, "\"status\"");
                    if (status_key) {
                        char *colon = strchr(status_key, ':');
                        if (colon) {
                            char *quote1 = strchr(colon, '"');
                            if (quote1) {
                                quote1++;
                                char *quote2 = strchr(quote1, '"');
                                if (quote2) {
                                    int slen = quote2 - quote1;
                                    if (slen >= sizeof(status_str)) slen = sizeof(status_str) - 1;
                                    memcpy(status_str, quote1, slen);
                                    status_str[slen] = '\0';
                                }
                            }
                        }
                    }
                    
                    char *count_key = strstr(line, "\"count\"");
                    if (count_key) {
                        char *colon = strchr(count_key, ':');
                        if (colon && sscanf(colon + 1, "%d", &count) == 1) {
                            snprintf(issues_str, sizeof(issues_str), "%d", count);
                        }
                    }
                    
                    char *ts_key = strstr(line, "\"timestamp\"");
                    if (ts_key) {
                        char *colon = strchr(ts_key, ':');
                        if (colon) {
                            char *quote1 = strchr(colon, '"');
                            if (quote1) {
                                quote1++;
                                char *quote2 = strchr(quote1, '"');
                                if (quote2) {
                                    // Extract just date and time, skip seconds
                                    // Format: "2025-12-29T02:10:48Z" -> "2025-12-29 02:10"
                                    if (quote2 - quote1 >= 16) {
                                        snprintf(timestamp_str, sizeof(timestamp_str), 
                                                "%.10s %.5s", quote1, quote1 + 11);
                                    }
                                }
                            }
                        }
                    }
                }
                fclose(metrics_file);
            }
            
            // Print agent row
            prt("%-12s %-12s %-16s %-16s %-8s %s\n",
                agent_name,
                installed ? "yes" : "no",
                pid_str,
                status_str,
                issues_str,
                timestamp_str);
            
            agent_count++;
        }
    }
    
    prt("\nTotal agents: %d\n", agent_count);
    flush_exit(0);
}

void handle_help_topic(char *topic) {
    span s = get_help_text(topic);
    
    if (s.buf == 0) {
        fprintf(stderr, "Unknown help topic: %s\n\n", topic ? topic : "");
        s = get_help_text("topics");
        if (s.buf) {
            fwrite(s.buf, 1, s.end - s.buf, stdout);
        }
        flush_exit(1);
    }
    
    fwrite(s.buf, 1, s.end - s.buf, stdout);
    flush_exit(0);
}

void handle_prompt(int block_idx) {
    state->curr_block_idx = block_idx;
    span op = S("nl2pl_rewrite");
    span template = get_prompt_template(op);
    spans vars = current_block_template_vars();
    span expanded_prompt = expand_template(template, vars);
    prt("%.*s", (int)(expanded_prompt.end - expanded_prompt.buf), expanded_prompt.buf);
}

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

void handle_wants() {
    // Helper function to parse SN line and check if it starts with "We want "
    void check_line(span line) {
        // Skip leading whitespace
        while (line.buf < line.end && (*line.buf == ' ' || *line.buf == '\t')) {
            line.buf++;
        }
        
        // Save the start position (after whitespace)
        u8 *line_start = line.buf;
        
        // Line must start with "
        if (line.buf >= line.end || *line.buf != '"') return;
        
        // Search backwards from end for pattern " <digits>.
        u8 *p = line.end - 1;
        
        // Must end with '.'
        if (p < line.buf || *p != '.') return;
        u8 *line_end = p + 1; // Save end position (inclusive of '.')
        p--;
        
        // Skip digits
        u8 *digit_end = p + 1;
        while (p >= line.buf && *p >= '0' && *p <= '9') p--;
        if (p < line.buf || p + 1 == digit_end) return; // No digits found
        
        // Must have space before digits
        if (*p != ' ') return;
        p--;
        
        // Must have " before space
        if (p < line.buf || *p != '"') return;
        
        // Extract event string: between opening " and this "
        span event_str = {line.buf + 1, p};
        
        // Check if starts with "We want "
        span want_prefix = S("We want ");
        if (event_str.end - event_str.buf >= want_prefix.end - want_prefix.buf &&
            memcmp(event_str.buf, want_prefix.buf, want_prefix.end - want_prefix.buf) == 0) {
            // Print the entire SN line
            span sn_line = {line_start, line_end};
            wrs(sn_line);
            terpri();
        }
    }
    
    // Helper function to scan a file
    void scan_file(const char *filepath) {
        FILE *f = fopen(filepath, "r");
        if (!f) return;
        
        // Read file into buffer
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        if (fsize < 0 || fsize > 100000000) { // Skip files > 100MB
            fclose(f);
            return;
        }
        fseek(f, 0, SEEK_SET);
        
        u8 *content = (u8 *)malloc(fsize);
        if (!content) {
            fclose(f);
            return;
        }
        
        size_t bytes_read = fread(content, 1, fsize, f);
        fclose(f);
        
        if (bytes_read != (size_t)fsize) {
            free(content);
            return;
        }
        
        span file_span = {content, content + fsize};
        
        // Process line by line
        while (file_span.buf < file_span.end) {
            span line = head_line(&file_span);
            check_line(line);
        }
        
        free(content);
    }
    
    // Scan source files
    scan_file("cmpr.c");
    scan_file("spanio.c");
    scan_file("INBOX.c");
    
    // Scan .cmpr/T
    scan_file(".cmpr/T");
    
    // Scan .cmpr/events/*
    DIR *events_dir = opendir(".cmpr/events");
    if (events_dir) {
        struct dirent *entry;
        while ((entry = readdir(events_dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            
            char path[512];
            snprintf(path, sizeof(path), ".cmpr/events/%s", entry->d_name);
            scan_file(path);
        }
        closedir(events_dir);
    }
    
    flush();
}


void handle_wants_status() {
    // Find the wants_status_report block
    int block_idx = block_by_id(S("#wants_status_report"));
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


void expand_refs_2_rec_both(span block, span transform, spans* already, int comment_context, int depth) {
    if (depth > 512) {
        prt("block expansion depth limit (512) exceeded, possible reference cycle?");
        flush_exit(1);
    }
    expand_refs_2_rec_context(block, transform, already, comment_context, depth);
    expand_refs_2_rec_body(block, transform, already, comment_context, depth);
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

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



span chase_ref_2(span ref_id) {
    int idx = block_by_id(ref_id);
    if (idx == -1) {
        return nullspan();
    }
    return state->blocks.a[idx];
}


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


span normalize_path_for_match(span path) {
    if (len(path) >= 2 && path.buf[0] == '.' && path.buf[1] == '/') {
        return skip_n(path, 2);
    }
    return path;
}


int paths_match_for_block_map(span a, span b) {
    a = normalize_path_for_match(trim(a));
    b = normalize_path_for_match(trim(b));

    if (span_eq(a, b)) return 1;

    if (len(a) >= len(b) && ends_with(a, b)) return 1;
    if (len(b) >= len(a) && ends_with(b, a)) return 1;

    return 0;
}


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


int block_map_selftest() {
    int failures = 0;

    #define CHECK(msg, cond) do { \
        if (!(cond)) { \
            prt("[selftest] %s\n", msg); \
            failures++; \
        } \
    } while (0)

    span block_map = S(
        "src/foo.c 1 5 #foo\n"
        "src/foo.c 10-12 #bar\n"
        "./src/baz.c:20-22 #baz\n"
        "/abs/path/qux.c:30 #qux\n"
    );

    spans ids = block_ids_for_file_line(block_map, S("src/foo.c"), 3);
    CHECK("expected src/foo.c:3 -> #foo", ids.n == 1 && span_eq(ids.a[0], S("#foo")));

    ids = block_ids_for_file_line(block_map, S("./src/foo.c"), 11);
    CHECK("expected ./src/foo.c:11 -> #bar", ids.n == 1 && span_eq(ids.a[0], S("#bar")));

    ids = block_ids_for_file_line(block_map, S("/home/user/src/baz.c"), 21);
    CHECK("expected suffix match for baz.c:21 -> #baz", ids.n == 1 && span_eq(ids.a[0], S("#baz")));

    ids = block_ids_for_file_line(block_map, S("/abs/path/qux.c"), 30);
    CHECK("expected absolute path + single line range -> #qux", ids.n == 1 && span_eq(ids.a[0], S("#qux")));

    ids = block_ids_for_file_line(block_map, S("src/foo.c"), 7);
    CHECK("expected no hit outside ranges", ids.n == 0);

    #undef CHECK

    if (failures == 0) {
        prt("block-map selftest: ok\n");
    }

    return failures;
}

void compile() {
    ensure_conf_var(&state->buildcmd, S("The build command will be run every time you hit 'B' and should build the code you are editing (typically in projfile)"), nullspan());

    char buf[2048] = {0};
    s_buffer(buf, sizeof(buf), state->buildcmd);

    prt("Running command: %s\n", buf);
    flush();

    char command[2100] = {0};
    snprintf(command, sizeof(command), "%s 2>&1", buf);

    span block_map = read_whole_file(S(".cmpr/block-map"));
    spans related_blocks = spans_alloc(0);

    FILE *pipe = popen(command, "r");
    if (!pipe) {
        perror("Failed to run build command");
        exit(EXIT_FAILURE);
    }

    char line_buf[4096];
    while (fgets(line_buf, sizeof(line_buf), pipe)) {
        prt("%s", line_buf);

        size_t line_len = strlen(line_buf);
        span line_span = {(u8*)line_buf, (u8*)line_buf + line_len};
        if (line_len && line_buf[line_len - 1] == '\n') {
            line_span.end--;
        }

        span diag_path = nullspan();
        int diag_line = 0;
        if (parse_compiler_error_line(line_span, &diag_path, &diag_line)) {
            spans ids = block_ids_for_file_line(block_map, diag_path, diag_line);
            if (ids.n > 0) {
                prt("    Related blocks: ");
                for (int i = 0; i < ids.n; i++) {
                    if (index_of(ids.a[i], related_blocks) == -1) {
                        spans_push(&related_blocks, ids.a[i]);
                    }
                    wrs(ids.a[i]);
                    if (i + 1 < ids.n) {
                        sp();
                    }
                }
                terpri();
            }
        }
    }

    int status = pclose(pipe);

    if (status != 0) {
        if (related_blocks.n > 0) {
            prt("Build failed; related blocks:\n");
            for (int i = 0; i < related_blocks.n; i++) {
                prt(" - ");
                wrs(related_blocks.a[i]);
                terpri();
            }
        } else if (empty(block_map)) {
            prt("Build failed; no .cmpr/block-map found to map errors to blocks.\n");
        } else {
            prt("Build failed; no matching block IDs found for the reported errors.\n");
        }
        prt("Press any key to continue...\n");
        flush();
        getch();
    } else {
        prt("Build succeeded\n");
        flush();
        sleep(1); // Give time for the user to read the message
    }
}

void replace_code_clipboard() {
    ensure_conf_var(&state->cbpaste, S("Command to get text from the clipboard on your platform (Mac: \"pbpaste\", Linux: \"xclip -o -selection clipboard\", Windows: TODO: fill this in)"), S("xclip -o -selection clipboard"));
    span new_content = pipe_cmd_cmp(state->cbpaste);
    replace_block_code_part(new_content);
}


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


void output_save(span operation, span message) {
    if (span_eq(operation, S("agreement"))) {
        agreement_SAV(message);
    } else if (span_eq(operation, S("agreement_to_nl_diff"))) {
        proposed_diff_SAV(message);
    } else {
        generic_output_save(operation, message);
    }
}


llm_message_handler make_output_saver(span operation) {
    return partial_sp_sp(operation, output_save);
}


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


int span_cmp_wrapper(const void *a, const void *b) {
    return span_cmp(*(span *)a, *(span *)b);
}


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


llm_message_handler simple_message_handler(void(*f)(span)) {
    return partial_0_sp(f);
}


void nl2pl_rewrite() {
    span op = S("nl2pl_rewrite");
    span template = get_prompt_template(op);
    spans vars = current_block_template_vars();
    span expanded_prompt = expand_template(template, vars);
    send_to_llm(expanded_prompt, simple_message_handler(replace_block_code_part));
}


void pl2nl_rewrite() {
    span template = get_prompt_template(S("pl2nl_rewrite"));
    spans vars = current_block_template_vars();
    span expanded = expand_template(template, vars);
    send_to_llm(expanded, simple_message_handler(pl2nl_rewrite_cb));
}


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


void nl2algo() {
    span op = S("nl2algo");
    span template = get_prompt_template(op);
    spans vars = current_block_template_vars();
    span expanded_template = expand_template(template, vars);
    llm_message_handler handler = make_output_saver(op);
    send_to_llm(expanded_template, handler);
}


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


void summarize_block() {
    span op = S("summarize_block");
    span template = get_prompt_template(op);
    spans vars = current_block_template_vars();
    span expanded_template = expand_template(template, vars);
    llm_message_handler handler = make_output_saver(op);
    send_to_llm(expanded_template, handler);
}


