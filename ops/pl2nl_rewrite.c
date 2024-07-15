/* #pl2nl_rewrite @prompt_palette_design @optable @simple_message_handler

Function:
void pl2nl_rewrite();

Purpose:
The dual of nl2pl_rewrite, takes an existing PL part and context, and generates an NL part intended to effectively recreate it in its essential aspects.

This may be used when adapting an existing codebase.

@- First, we will typically take a chunk off the front of a file, making a new block.
@- TODO: add a palette op for this also (modify a block that's too big, e.g. is all or most of a C file that doesn't have block comments, by adding a block comment after some prefix of it.)
@- We will give the block an id derived from the code (such as a function name or struct name).

However, we will often have to manually add some context to the block so that we get good code.

Implementation:
We call a function to get the prompt template matching the shortname of this op.

We call another function to get the template variables related to the current block.

We expand the template with the variables.

Finally we call send_to_llm with pl2nl_rewrite_cb as the #simple_message_handler.
*/

void pl2nl_rewrite() {
    span template = get_prompt_template(S("pl2nl_rewrite"));
    spans vars = current_block_template_vars();
    span expanded = expand_template(template, vars);
    send_to_llm(expanded, simple_message_handler(pl2nl_rewrite_cb));
}

/* #pl2nl_rewrite_cb @prt_usage @blocks

void pl2nl_rewrite_cb(span message);

Here we handle the message returned from the LLM.

This is just the text of the message (the next message in the conversation), not the full JSON object that the LLM API returns.

Another name for this function could be replace_nl_part_maintaining_context.

We call a function to strip the markdown codeblock, getting the block comment that the LLM returned.

We put the current block contents in a variable for convenience.
We get the code part into a span for later use.

Then we get the first line of the current contents of the block (using next_line).
We want to keep this line the same, as it contains the block id and context reference which are expanded before the LLM sees them, so the LLM will not have preserved them.

We throw away the first line of the comment block returned by the LLM (also using next_line, this time for its side effect, not its return value).
We put the first line of the current block above the second and following lines of the comment block returned by the llm together using prs.
(They will need a newline between them, as next_line does not return the newline.)

We also get the code part of the current block.
This will not include any leading whitespace.
We test whether the assembled comment part so far ends with "\n\n" or with "\n" or with no newline.
We want two newlines before the code part, so we assemble the comment part, with one or two newlines if necessary, then the code part, again using prs.

Finally we call replace_block with this complete output, which will replace the contents of the current block, and save a rev, etc.
*/

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

/* notes on implementation effort

Apart from writing the function, we had to put pl2nl_rewrite.c (or let's just do ops/ *.c) into our fdecls stuff.

We had to manually include pl2nl_rewrite.c in cmpr.c and the Makefile.

We added the prompts directory, so we could put the prompt template into a file instead of the #templatetable we had before (which is gone now).
*/
