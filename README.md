# CMPr

## "Future programming" here now?

Thesis: the future of programming is driving an LLM to write code.
Why?
Mostly, because it's way more fun.
There are some practical benefits too.
You can work with any technology, so it's a great technique for a generalist.
When code is written by an AI, we treat it as disposable.
What's important is the English that described that code.
It doesn't matter as much what language the code is in.

## What's this then?

This is a tool to play with and support the LLM programming workflow.

Today we just support ChatGPT and interaction is by copy and paste.
This means you can use it with a free account, or if you have a paid account you can use GPT4, which writes better code.

(Coming soon: API usage, local models, competing LLMs, etc.)

This is a framework and represent a particular and very opinionated approach to this workflow.
We will be updating continuously as we learn.
(This version was written in ONE DAY, using the workflow itself.)

All code here is by ChatGPT4, and all comments by me, which is the idea of the workflow:

- You have a "block" which starts with a comment and ends with code.
- You write the comment; the LLM writes the code.
- You iterate on the comment until the code works and meets your standard.

The tool is a C program; compile it and run it locally, and it will go into a loop where it shows you your "blocks".
You can use j/k to move from block to block. (TODO: full text search with "/")
Once you find the block you want, use "e" to open it up in "$EDITOR" (or vim by default).
Then you do ":wq" or whatever makes your editor exit successfully, and a new revision of your code is automatically saved under "cmpr/revs/<timestamp>".

To get the LLM involved, when you're on a block you hit "r" and this puts a prompt into the clipboard for you.
(You won't see anything happen when you hit "r", but the clipboard has been updated.)
Then you switch over to your ChatGPT window and hit "Ctrl-V".
You could edit the prompt, but usually you'll just hit Enter.
ChatGPT writes the code, and then you copy it directly into back into the block or you can fix it up if you want.
You can click "copy code" in the ChatGPT window and then hit "R" (uppercase) in the tool to replace everything after the comment with the clipboard contents.
Mnemonic: "r" gets the LLM to "rewrite" (or "replace") the code to match the comment (and "R" is the opposite of "r").

You can currently also hit "q" to quit, "?" for short help, "b" to build, and more features are coming soon.

## Quick start:

- get the code into ~/cmpr
- assumption is that everything runs from ~, so `cd ~` with cmpr in your home dir
- `gcc -o cmpr/cmpr cmpr/cmpr.c` or use clang or whatever you like
- `export EDITOR=emacs` or whatever editor you use
- `cmpr/cmpr < cmpr/cmpr.c` now you're in the workflow (using cmpr to dev cmpr), you'll see blocks, you can try "j"/"k", "e" and "r"

Day one version only useful for developing itself, because we have "cmpr.c" and similar hard-coded everywhere.
Check back tomorrow for a way to adapt it to your own codebase, start a new project, etc.
If you want to use "b" to build your code then run it like this: `CMPR_BUILD="my build command" cmpr/cmpr < cmpr/cmpr.c`, or by default it will just run `make`.

Developed on Linux; volunteers to try on Windows/MacOS/... and submit bug reports / patches very much welcomed!
We are using "xclip" to send the prompts to the clipboard.
Either install xclip, or edit the `cbsend` and `cbpaste` strings in the source code to suit your environment (TODO: make this better).
I.e. for Mac you would use "pbcopy" and "pbpaste".

## More

Development is being [streamed on twitch](https://www.twitch.tv/inimino2).
Join [our discord](https://discord.gg/ekEq6jcEQ2).
