# CMPr

## Program in English!

People started out programming machines by directly writing the machine code, and then we got assemblers and compilers, and we could write high-level languages instead.
Now we have LLMs, and the LLM is the new compiler.
We can write English and let the LLM turn that into code.
However, the programmer still has to be closely involved in the work, specifying the approach and checking the code.

## What's this then?

This is a tool to support the LLM programming workflow.

Today we just support ChatGPT, and interaction is by copy and paste.
This means <em>you can use it with a free account</em>, or if you have a paid account you can use GPT4, which writes better code.

(Coming soon: API usage, local models, competing LLMs, etc.)

This is a framework and represents a particular and opinionated approach to this workflow.
We will be updating continuously as we learn.
The first working version was written in a day, and the first generally useful version in a week, using the workflow itself.

All code here is by ChatGPT4, and all comments by me, which is the idea of the workflow:

- You have a "block" which starts with a comment and ends with code.
- You write the comment; the LLM writes the code.
- You iterate on the comment until the code works and meets your standard.

The tool runs locally and supports this workflow.
You use the tool to navigate and search and manage your blocks, edit them in an editor, and get the LLM to rewrite the code parts of each block as you develop.
You can use vi-like keybindings (j/k to navigate blocks, "/" for search, etc).
The tool also manages revisions, saving a copy every time you edit.

Your code still lives in files in a directory just like any other development workflow, and you can use another IDE or editor.
Cmpr is similar to an IDE in that it's the primary tool that you use to manage your code, but you can continue using any other tools alongside.
Blocks are defined by some pattern that starts a block (for example, in C code, a block starts with a C-style block comment) and so your blocks are just contiguous segments of an ordinary text file.

To get the LLM involved, when you're on a block you hit "r" and this puts a prompt into the clipboard for you.
(You won't see anything happen when you hit "r", but the clipboard has been updated.)
Then you switch over to your ChatGPT window and hit "Ctrl-V".
You could edit the prompt, but usually you'll just hit Enter.
ChatGPT writes the code, you can click "copy code" in the ChatGPT window, and then hit "R" (uppercase) back in cmpr to replace everything after the comment (i.e. the code half of the block) with the clipboard contents.
Mnemonic: "r" gets the LLM to "rewrite" (or "replace") the code to match the comment (and "R" is the opposite of "r").

Hit "q" to quit, "?" for short help, and "b" to build by running some build command that you specify.

The features we have today are mostly around editing a single block; we are currently adding support for more multi-block operations.

## Why blocks?

The block is the basic unit of interaction with the LLM.
Current state-of-the-art LLMs like GPT4 can reliably generate a moderately-sized function from an English description that's sufficiently precise.
However, if you attempted to describe an entire large and complex program and get the all the code from a single prompt, today's LLMs will not be able to accomplish the task.
So the block size is determined by the amount of code that the LLM can write in one go, and this determines how much iteration you do on the comment.
(We have some demo videos coming soon.)

## Quick start:

1. Get the code and build; assuming git repo at ~/cmpr and you have gcc, `(cd cmpr && make)` should do. Put the executable in your path with e.g. `sudo install cmpr/dist/cmpr /usr/local/bin`.
2. Go to the directory you want to work in and run `cmpr --init`, this creates a `.cmpr/` directory (similar to how git works).
3. `export EDITOR=emacs` or whatever editor you use, or vi will be run by default.
4. Run `cmpr` in this directory, and it will ask you some configuration questions.
   If you want to change the answers later, you can edit the .cmpr/conf file.
   At the moment you'll also probably need to edit the conf file to add a line that says "file: ..." with at least one file that you want to have in your project.

### Bonus: cmpr in cmpr

1. We ship our own cmpr conf file, so run cmpr in `~/cmpr` to see the code the way we do while building it.

## Caveats:

Developed on Linux, please report any bugs on other platforms!

The first time you use the 'r' or 'R' commands you will be prompted for the command to use to talk to the clipboard on your system.
For Mac you would use "pbcopy" and "pbpaste", on Linux we are using "xclip".

The tool is still very new and light on features.
We only support C and Python; mostly this is around syntax of where blocks start (in C we use block comments, and triple-quoted strings in Python).
It's not hard to extend the support to other languages, just ask for what you want in the discord and it may happen soon!
It's not hard to contribute, you don't need to know C well, but you do need to be able to read it (you can't trust the code from GPT without close examination).

To track progress look at the TODO file in the repo and you can see what's changed between releases and what's coming up.

## More

Development is being [streamed on twitch](https://www.twitch.tv/inimino2).
Join [our discord](https://discord.gg/ekEq6jcEQ2).
