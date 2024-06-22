# CMPr

## Program in English!

Cmpr is a tool for programming in natural language.

Code is written in NL, such as English, and translated or compiled by the LLM into a programming language, like Python or C.

The PL code is treated as generated code, while we work on the NL level.

Programming in English has a learning curve, like any new programming language.

You might also call this "DDD" for documentation-driven development: write the docs (or detailed spec) and get the code for free.

The difficulties are:

1. Writing English that is clear enough for GPT4 or another LLM to write correct code.
2. Determining whether the code that is written is correct.
3. Debugging it when it is not.

The programmer still has to work hard, perhaps harder than before.
However, it can also be quite enjoyable.

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

This has some implications for the future of programming as a career.
It's not economically valuable to pay a human to do something that an LLM can do at fractional pennies per token.
It is still valuable and necessary for the programmer to *know how* to do it, because otherwise you won't be able to guide the LLM to the solution.
Programmers remain relevant by rising up the levels of abstraction as those levels become available to us.
(How many of us still program assembly language by hand?)

Compare the English code with [the generated HTML, CSS, and JS](doc/examples/2048-gen.md).
Which one would you rather read?

A more complex example is [the cmpr code itself](https://github.com/inimino/cmpr/cmpr.c).
In contrast with 2048, this is not something the model was trained on, it's written in C, it's using an I/O library which is also not in the training set, so the LLM must be told about it, and it's a moderately complex program, not a toy.
This proves that modern LLMs is capable of writing real-world production-ready code in a challenging environment.

## What's this then?

This is a prototype of workflow tooling for programming in natural language.

If you have access to GPT4, it generally writes the best code.
The "clipboard model" just puts prompts onto the system clipboard, and you can copy and paste them into a chat window with ChatGPT or any other model with a web interface.
This is the recommended way of starting out, because it makes everything that is going on very visible!

As of v8 you can also use OpenAI's GPT models via API and we support local models via llama.cpp and ollama; see quick start section below for details.

## Who is this for?

Cmpr is best for programmers who are comfortable working in vim or another terminal-based editor, as this is how the workflow is organized.

It's a new way of programming, so it's easier to adopt on new projects.
Adapting an existing codebase is harder, just like integrating Rust with a C++ codebase is harder than using Rust for a new project.

You can use another IDE or editor alongside cmpr if you like, but it works better if you adopt it as your primary interface.
You use cmpr to navigate and search and manage your blocks, edit them in an editor, and get the LLM to rewrite the code parts of each block.
The tool also manages revisions, saving a copy every time either you or the LLM edits the codebase.

## Cmpr vs Others

In comparison with Copilot, you are giving up control of the programming-language (PL) code in order to move up a level of abstraction to the natural-language (NL) code.
The benefit of NL code is that it can be shorter, clearer, easier to understand a week or month later, and easier to maintain.
With Copilot, it's helping you edit the PL code, but it's not actually moving you up a level of abstraction to the NL level.

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
- You write the comment; the LLM writes the code.
- You iterate on the comment until the code works and meets your standard.

The cmpr tool itself runs on your machine and supports this workflow.

## Block references

As of v8 you can use this feature (the [Release Notes](release-notes.md) has more).
It's a powerful way to simplify your NL code, as it lets you define concepts in one place and then refer to them by inclusion in multiple places.
The references will then be expanded by cmpr before sending the NL code to the LLM (or clipboard).
The best way to try it out is look at some example code (like the cmpr source) or ask about it in the discord.

## Why blocks?

The block is the basic unit of interaction with the LLM.
We start with a "bootstrap block" that includes basic information about our codebase, libraries we're using, and so on (the stuff that a human programmer would learn from a readme file or onboarding resources).
See the 2048 demo codebase or the video walkthrough for more on this.

Current state-of-the-art LLMs like GPT4 can reliably generate a moderately-sized function from an English description that's sufficiently precise, along with this background information.
However, if you attempted to describe an entire large and complex program and get the all the code from a single prompt, today's LLMs will not be able to accomplish the task.
So the block size is determined by the amount of code that the LLM can write "in one go," and smaller blocks (like smaller functions) are easier to get right.

## Quick start:

0. Recommended: look at some of the sample code above or watch the 2048 demo video to see if the idea appeals to you; consider experimenting with bare ChatGPT program generation first if you haven't already.
1. Get the code and build; assuming git repo at ~/cmpr and you have gcc, `cd cmpr && make && sudo make install` should do.
2. Go to (or create) the directory for your project and run `cmpr --init`, this creates a `.cmpr/` directory (like git) with a conf file and some other stuff.
3. `export EDITOR=emacs` or whatever editor you want to use, otherwise just `vi` will be run by default.
4. Run `cmpr --init` in your project directory, then `cmpr` and it will ask you some configuration questions.
   If you want to change the answers later, you can edit the .cmpr/conf file.
5. Right now things are rough and changing all the time, so stop by discord and ask if you hit any roadblocks!

### Models

By default the model is set to "clipboard," which means you will have a ChatGPT (or any model) open in your browser and copy and paste.
However, we have early support for other models as of v8.
To use gpt-3.5-turbo or gpt-4-turbo, put your API key in `~/.cmpr/openai-key` and run `chmod 0400 ~/.cmpr/openai-key`.
Then in cmpr enter ":model" to get a menu where you can select the model.

To use llama.cpp, you need to run the llama.cpp server, then again use ":model" and choose llama.cpp.
It uses the same API as OpenAI's GPT models, but running locally.
When using llama.cpp, you use the llama.cpp server to configure which model it will use.

To use Ollama, you will need to run Ollama locally.
In contrast to llama.cpp, Ollama supports all models through the same server, and you specify the model you want in the API calls.
So you will edit `.cmpr/conf` in your project, and add a comma-separated list of Ollama models to the `ollamas` config parameter, e.g. `ollamas: llama3,llama3:70b` for the 8B and 70B Llama3 models.
When you run ":models" now you'll see your Ollama models added to the list and you can select them.
(We don't install the Ollama models for you, i.e. they must already be in `ollama list` and the Ollama server must be running.)

With any of these models used via API, we will record all API calls in `.cmpr/api_calls`.
You can use them to troubleshoot API issues, or as a dataset for statistics or finetuning your own model, etc.

With the "clipboard" model, you will want to run ":bootstrap", and then paste the output into the LLM chat window.
If you're using the LLM via API, this will be done for you, but you still need to run ":bootstrap" manually before using "r".

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

When you use an API (instead of clipboard mode) there is also a system prompt sent to the model on every request.
This comes from a block with the id "systemprompt".
For example, for cmpr itself, we have a file (also called systemprompt), this file is listed in our conf file, and there's a block in that file that has the system prompt we use.
If you're using ChatGPT or another model, putting similar instructions in the settings (ChatGPT calls them "custom instructions") will give good results.
Otherwise the models tend to try to be helpful, when what you want in this workflow is for it to follow instructions and write code.

The first time you use the 'r' or 'R' commands you will be prompted for the command to use to talk to the clipboard on your system.
For Mac you would use "pbcopy" and "pbpaste", on Linux we are using "xclip".

We support a small number of languages at the moment (this is mostly about how files get broken into blocks).
It's not hard to extend the support to other languages, just ask for what you want in the discord and it may happen soon!

## More

Development is sometimes [streamed on twitch](https://www.twitch.tv/inimino2).
Join [our discord](https://discord.gg/ekEq6jcEQ2).
