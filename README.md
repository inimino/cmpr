/*

# CMPr

## "Future programming" here now?

Thesis: the future of programming is driving an LLM to write code; this is a tool to support and play with this workflow.

Today we just support ChatGPT and interaction is by copy and paste.
This means you can use it with a free GPT account, or the paid account with GPT4, which writes better code.

(Later: API usage, local models, competing LLMs, etc.)

This is a framework and represent a particular and very opinionated approach to this workflow.
We will be updating continuously as we learn.
(This version was written in ONE DAY, using the workflow itself.)

All code by ChatGPT4, and all comments written by me, which is the idea of the workflow:

- You have a "block" which starts with a comment and ends with code.
- You write the comment; the LLM writes the code.
- You iterate on the comment until the code works and meets your standard.

The tool is a C program; compile it and run it locally, and it will go into a loop where it shows you your "blocks".
You can use j/k to move from block to block. (TODO: full text search with "/")
Once you find the block you want, use "e" to open it up in "$EDITOR" (or vim by default).
Then you do ":wq" or whatever makes your editor exit successfully, and a new revision of your code is automatically saved under "cmpr/revs/<timestamp>".

To get the LLM involved, when you're on a block you hit "r" and this puts a prompt into the clipboard for you.
Then you switch over to your ChatGPT window and hit "Ctrl-V" or whatever it is on your platform to paste.
ChatGPT writes the code, and you copy it directly into the block or you can fix it up if you want.
Mneumonic: "r" means "rewrite", where the LLM is rewriting the code, based on the comment, which is the human's work.

You can currently also hit "q" to quit, and more features are coming soon.

## Quick start:

- get the code into ~/cmpr
- assumption is that everything runs from ~, so `cd ~` with cmpr in your home dir
- `gcc -o cmpr/cmpr cmpr/cmpr.c` or use clang or whatever you like
- `export EDITOR=emacs` or whatever editor you use
- `cmpr/cmpr < cmpr/cmpr.c` now you're in the workflow (using cmpr to dev cmpr), you'll see blocks, you can try "j"/"k", "e" and "r"

Day one version only useful for developing itself, because we have "cmpr.c" and similar hard-coded everywhere.
Check back tomorrow for a way to adapt it to your own codebase, start a new project, etc.

Developed on Linux; volunteers to try on Windows/MacOS/... and submit bug reports / patches very much welcomed!
We are using "xclip" to send the prompts to the clipboard.
If on Linux, make sure you have xclip, or edit the `cbpaste` string in the source code to suit your environment (TODO: make this better).

