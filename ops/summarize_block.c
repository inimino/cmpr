/* #summarize_block @nl2algo:code

This implements the op summarize_block.

Apart from the op name, we follow nl2algo(), above.

@- This is becoming a kind of standard op shape; maybe we can abstract it.
*/

void summarize_block() {
    span op = S("summarize_block");
    span template = get_prompt_template(op);
    spans vars = current_block_template_vars();
    span expanded_template = expand_template(template, vars);
    llm_message_handler handler = make_output_saver(op);
    send_to_llm(expanded_template, handler);
}

