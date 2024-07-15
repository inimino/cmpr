/* #nl2pl_rewrite @prompt_palette_design @optable @simple_message_handler

void nl2pl_rewrite();

Implementation:
We make a span called op with the value "nl2pl_rewrite" (the shortname of this op).

We call a function to get the prompt template matching op.

We call another function to get the template variables related to the current block.

We expand the template with the variables.

We send the expanded prompt to the LLM with replace_block_code_part as the handler.
*/

void nl2pl_rewrite() {
    span op = S("nl2pl_rewrite");
    span template = get_prompt_template(op);
    spans vars = current_block_template_vars();
    span expanded_prompt = expand_template(template, vars);
    send_to_llm(expanded_prompt, simple_message_handler(replace_block_code_part));
}

