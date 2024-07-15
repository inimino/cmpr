/* adding nl2algo

This time let's document everything here that we have to do to add this palette operation to the product.

We added the operation to #optable, so it shows up in the palette.

We added the file prompts/nl2algo.
This automatically adds "nl2algo" to the arguments that we can give to get_prompt_template().
Most of the thinking went into writing the prompt template that goes in this file.
The best way to start on this should be copying an existing template and modifying it.
The available template variables can be found by looking at the code.

We have to write this file, ops/nl2algo.c, containing the actual op implementation function, nl2algo().
The best way to write this file is also to copy an existing one, like ops/pl2nl_rewrite.c.
We might at some point in the future simplify this so that you can get a default implementation that just runs a template with the same name as the op and creates an output with the same name as the op, and have a default set of template variables that are available.
But for now, at least, you should copy an existing example.
You could write the code in English and let cmpr generate the code, or just write or modify the existing code and delete or edit the comment, or whatever is easier.
Here, in the next block, we'll actually just refer to the pl2nl_rewrite as an example, and tell the LLM to copy it.
We use the "@pl2nl_rewrite:code" context reference to pull in the existing code for the LLM to use.
*/

/* #nl2algo @pl2nl_rewrite:code

Here we copy the #pl2nl_rewrite, except that, of course, the name of the op here is "nl2algo".

@- Actually, we want this to create a block, after the current block, or something like that.
@- We'll come back to that part.

@- An interesting update here is that we later changed pl2nl_rewrite to have its own callback that does something different.
@- So, this comment is now quite inaccurate. It would be better to copy paste the NL code in a case like this rather than refer to it.
*/

void nl2algo() {
    span op = S("nl2algo");
    span template = get_prompt_template(op);
    spans vars = current_block_template_vars();
    span expanded_template = expand_template(template, vars);
    llm_message_handler handler = make_output_saver(op);
    send_to_llm(expanded_template, handler);
}

