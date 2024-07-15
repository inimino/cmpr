/* #agreement_to_pl_diff @agreement_to_nl_diff:code

void agreement_to_pl_diff();

The implementation is identical to agreement_to_nl_diff(), above, but with the obvious substitution s/agreement_to_nl_diff/agreement_to_pl_diff/ everywhere.
*/

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

