// Minimal bootstrap stub to allow building
// This should be regenerated with: cmpr --print-code '#generate_bootstrap' | bash > bootstrap_content.c

typedef unsigned char u8;

u8 bootstrap_content_data[] = "";
int bootstrap_content_len = 0;

span get_bootstrap_content_span() {
    span s;
    s.buf = (const char *)bootstrap_content_data;
    s.end = (const char *)bootstrap_content_data + bootstrap_content_len;
    return s;
}
