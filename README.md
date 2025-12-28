# CMPr

## Program in English!

Cmpr is a tool for programming in natural language.

We often say "English" (and LLMs generally understand English well) but you can use other natural languages as well!
We usually use the term "NL code" to refer to code that can be written in any natural language.

Cmpr is really a platform for managing your code, using AI assistance.

You can use the AI to convert between NL code and PL code (e.g. between English and Python).
You can also use the AI to validate things that should be true of your code, whether in the NL or PL form.

You might also call this "DDD" for documentation-driven development: write the docs (or detailed spec) and get the code for free.

There is a learning curve, just as with any powerful new technology.
The programmer still has to work hard, perhaps harder than before!
However, it can also be quite enjoyable.

Programming becomes more productive, because you can now focus on a higher level of abstraction.
This also means it becomes possible to manage codebases of greater complexity than a single unaided human would previously have been able to manage.

Here are some examples of code written in this style:

A 2048 implementation:

- [Here](https://inimino.github.io/2048/) is the playable game.
- Here is [all the English code for this implementation](doc/examples/2048.md); this is everything the LLM sees before generating the playable game above (more info in [the repo](https://github.com/inimino/2048/)).

Notice that the code (that is, the English code) is mostly focused on what makes 2048 2048.
For example, the English code mentions that when a new tile appears, 90% of the time it will be a 2.
If you changed this ratio the game would be a different game.
The things that are left out of the English are implementation details.
For example, how to generate a 2 with probability .9 in JavaScript or Python is something that GPT4 already knows.
You could use the English code for 2048 to test your JavaScript, HTML, and CSS knowledge.
If you can write the JS and CSS from the English description as well or better than GPT4, then you know JS and CSS pretty well.
On the other hand, so does GPT4.
You can now level up and focus on higher-level applications of algorithms and data structures, program architecture, and UX considerations.

Compare the English code with [the generated HTML, CSS, and JS](doc/examples/2048-gen.md).
Which one would you rather read?

A more complex example is [the cmpr code itself](https://github.com/inimino/cmpr/cmpr.c).
In contrast with 2048, this is not something the model was trained on, it's written in C, it's using an I/O library which is also not in the training set, so the LLM must be told about it, and it's a moderately complex program, not a toy.
This proves that modern LLMs is capable of writing real-world production-ready code in a challenging environment.

## What's this then?

This is a prototype of workflow tooling for programming in natural language.

It supports a number of operations on code.
You remain in control of the code (and responsible for it) but the AI assistance lets you focus on the decisions that only a human programmer can make.
The decisions that are easy enough can be left to the AI.
As LLMs improve, you will simply become more effective.

We support a growing number of APIs, including OpenAI, Claude, and local models via Ollama or llama.cpp.
You can also get your feet wet with just a "clipboard model", where you use your clipboard to communicate with a web-based chat model like ChatGPT.
## Who is this for?

Cmpr is best for programmers who are comfortable working in vim or another terminal-based editor, as this is how the workflow is organized.

We do have other frontends planned, but we are currently focused very much on the terminal-based UI, and we will be adding features there for a while as we have a lot of powerful features coming soon.

It's a new way of programming, so it's easier to adopt on new projects.
Adapting an existing codebase is harder, just like integrating Rust with a C++ codebase is harder than using Rust for a new project.
However, cmpr can also help you understand existing code.

You can use another IDE or editor alongside cmpr if you like, but it works best when you adopt it as your primary interface.
You use cmpr to navigate and search and manage your blocks, edit them in an editor, and get the LLM to rewrite the code parts of each block.
The tool also manages revisions, saving a copy every time either you or the LLM edits the codebase.

## Cmpr vs Others

Copilot helps you write PL code, but you are still operating on the PL level.
With cmpr, you are giving up control of the programming-language (PL) code in order to move up a level of abstraction to the natural-language (NL) code.
The benefit of NL code is that it is much easier to understand and to communicate.
However, you're still going to have to be looking at the PL code to check the LLM's work.
It's a different focus, and Copilot and cmpr can be seen as complementary tools.
(You can, of course, use both.)

In comparison with tools like Devin, first of all, this workflow actually works.
All the code in cmpr is actually written by GPT4 using the tool; I do not believe Devin can make the same claim.
Programming requires AGI-level problem solving skills that LLMs do not have.

The Devin-style approach aims to replace programmers with AI, and the user of the AI is essentially a project manager.
We believe this is unrealistic given the current state of AI.
(If anything, we'll be able to replace the project manager role with AI before we can do the same for the programmer role.)
A better analogy is that we elevate the programmer to an architect with an AI assistant that can handle low-level details.
This might suggest that fewer programmers will be needed, but Jevon's paradox suggests demand may increase, as software development becomes more productive.

Are there other tools that should be mentioned here?
Let us know!

## Blocks

All the C code here is by GPT4 (or another LLM), and all the English is by the human authors, which captures a key idea:

- Code is organized into blocks; each has a comment and then (usually) some code.
- Generally, the human focuses on the NL part (e.g. the English comment) and the LLM focuses on the PL part.
- When writing new code, you generally iterate on the NL part until the PL part works and meets your standard.
- Decisions made, even when editing the PL part, are ultimately folded back into the NL part, which remains the source of truth.

The cmpr tool itself runs on your machine and supports this workflow.

The size of a block is normally one function or method, but may be several functions or a self-contained program.
It is the unit of interaction with the LLM.
If a block is too big, the interaction becomes awkward and you'll find that breaking it up gives better results.
## Block references

Block references are a powerful way to simplify your NL code.
References allow you to define concepts in one place and then refer to them by inclusion in multiple places.
The references will then be expanded by cmpr before sending the NL code to the LLM.

The references of a block provide the context that is necessary to understand the block.
They are useful for human programmers too.
Setting up block references across your codebase makes the key ideas and their relationships explicitly visible.
## Why blocks?

The block is the basic unit of interaction with the LLM.
We start with a "bootstrap block" that includes basic information about our codebase, libraries we're using, and so on (the stuff that a human programmer would learn from a readme file or onboarding resources).
See the 2048 demo codebase or the video walkthrough for more on this.

Current state-of-the-art LLMs like GPT4 can reliably generate a moderately-sized function from an English description that's sufficiently precise, along with this background information.
However, if you attempted to describe an entire large and complex program and get the all the code from a single prompt, today's LLMs will not be able to accomplish the task.
So the block size is determined by the amount of code that the LLM can write "in one go," and smaller blocks (like smaller functions) are easier to get right.

## Quick start:

0. Recommended: look at some of the sample code above or watch the 2048 demo video; consider experimenting with bare ChatGPT program generation first.
1. Get the code and build; assuming git repo at ~/cmpr and you have gcc, `cd cmpr && make && sudo make install` should do.
2. Go to (or create) the directory for your project and run `cmpr --init`, this creates a `.cmpr/` directory and makes this a cmpr project.
3. Run `export EDITOR=emacs` or `nano` or whatever editor you want to use, otherwise `vi` will be run by default.
4. Run `cmpr` in your project directory, and it will ask you some configuration questions.
   If you want to change the answers later, you can edit the .cmpr/conf file.
5. Right now things are rough and changing all the time, so stop by discord and ask if you hit any roadblocks!

### Models

By default the model is set to "clipboard," which means you will have a ChatGPT (or any model) open in your browser and copy and paste.
To use OpenAI's GPT models, put your API key in `~/.cmpr/openai-key` and run `chmod 0400 ~/.cmpr/openai-key`.
Then in cmpr, enter ":model" to get a menu where you can select the model.

To use llama.cpp, you need to run the llama.cpp server, then again use ":model" and choose llama.cpp.
You will use the llama.cpp server to configure which model it uses.

To use Ollama, you will need to run Ollama locally.
In contrast to llama.cpp, Ollama supports all models through the same server, and you specify the model you want in the API calls.
So, you will edit `.cmpr/conf` in your project, and add a comma-separated list of Ollama models to the `ollamas` config parameter, e.g. `ollamas: llama3,llama3:70b` for the 8B and 70B Llama3 models.
Then when you run ":models" in cmpr you'll see your Ollama models added to the list, and you can select the one you want.

With any of these models, we record all API calls in `.cmpr/api_calls`.
You can use them to troubleshoot API issues, or as a dataset for statistics or finetuning, etc.

With the clipboard "model," you will want to run ":bootstrap", and then paste the output into the LLM chat window, e.g. ChatGPT.
If you're using the API, this will be done for you, but you still need to run ":bootstrap" manually before using "r".
If you use the clipboard model with the palette, it will similarly just put the prompt on the clipboard for you.

## Running agents

cmpr ships agent implementations as blocks, so you can run them directly from the built binary.

1. Build cmpr (`make`) to produce `dist/cmpr`. Use `./dist/cmpr` in the repo root, or `sudo make install` to add `cmpr` to your PATH.
2. Discover agents: `./dist/cmpr --agents` lists names such as `root_agent` and `migration_agent`.
3. Inspect what an agent does before executing it:
   - `./dist/cmpr --print-comment '#root_agent'` shows the want it enforces.
   - `./dist/cmpr --print-comment '#root_agent_check_impl'` shows the CHECK script it will run (and similarly `_fix_impl`).
4. Run the agent in CHECK or FIX mode: `./dist/cmpr --agent-run <agent_name> CHECK` or `./dist/cmpr --agent-run <agent_name> FIX`.
   - Example: `./dist/cmpr --agent-run root_agent CHECK` verifies the hub coverage want from `#root`.
   - If a FIX implementation exists, `./dist/cmpr --agent-run root_agent FIX` attempts the repair.
5. `--agent-run` extracts the shell script from the `<agent_name>_check_impl` or `<agent_name>_fix_impl` block, writes it to a temp file, makes it executable, and runs it, so updating your repo keeps agents current.
### Bonus: cmpr in cmpr

1. We ship our own cmpr conf file, so run cmpr in the cmpr repo to see the code the way we do while building it.
   Use j/k to move around, ? to get help on keyboard commands, B to do a build, and q to quit.

## Caveats:

Developed on Linux; works on Mac or Windows with WSL2.

It's early days and there <s>may be</s> <ins>are</ins> bugs!

There's support for a "bootstrap" script with the ":bootstrap" ex command.
You make a script that produces your bootstrap block on stdout, you put it in the conf file as "bootstrap: ./your-script.sh", and you use :bootstrap to run it.
(It can actually be any command that produces output on stdout, it doesn't have to be a shell script.)
For more details on this feature, stop by the discord and ask!
We also ship our own bootstrap.sh which you can look at as a template.
There is also a version that's adapted for Python code.
We have some "global context" features coming soon that provide an alternative to writing a bootstrap script.

We support mainly C and Python at the moment.
This is mostly about how files get broken into blocks.
Languages that support C-style block comments (Java, JavaScript, Rust, C++, ...) should work without much trouble.
It's not hard to extend the support to other languages, just ask for what you want in the discord and it may happen soon!

For a visual overview of the cmpr1 block graph and navigation goals, see the `#blog_post_blockset_visualization` block (run `dist/cmpr --print-comment '#blog_post_blockset_visualization'`) for proposed diagrams and a blog-style walkthrough of the workflow, including hub-and-spoke and force-directed views.
## More

Development is sometimes [streamed on twitch](https://www.twitch.tv/inimino2).
Join [our discord](https://discord.gg/ekEq6jcEQ2).
