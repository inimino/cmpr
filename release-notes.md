# v9

### Anthropic Claude support

Anthropic's Claude models are now supported, just put your key in ~/.cmpr/anthropic-key, and use the :models ex command in cmpr to switch to any of the Claude models.

### j/k order

The j/k order is the order that the j and k keybindings move you through in the UI.

Previously this was just moving through the blocks, but in v9 we added an empty file state, which you can use to edit an empty file.
So the j/k order is now all of your blocks, plus any empty files listed in your conf file.
(This replaces the really awkward handling of empty files we had before, where they created empty blocks. Now there's no such thing as an empty block.)
We still don't handle files that don't exist, but now you can add a file to your project by touching the file and adding it to .cmpr/conf.
### The Palette

The operations palette or palette is probably the biggest new feature in v9.

To reach the palette, hit ' (single-quote, apostrophe) on a block.

You'll currently see the following set of operations:

```
NL -> PL rewrite
NL <- PL rewrite
NL PL agreement
NL PL agreement to PL patch
NL PL agreement to NL patch
block to one-line summary
NL description to step-by-step algorithm
```

NL -> PL rewrite is a rewrite of the PL (programming language) part from the natural language part.
In other words, we get the LLM to rewrite the code based on the comment and the context.
This is already familiar as the "r" keybinding in previous versions.
It does the same thing whether you choose it from the palette or with the keybinding; we just added it to the palette for completeness.

The NL <- PL rewrite operation is the reverse operation, rewriting the comment part from the existing code.
This can be useful when bringing existing codebases into cmpr.
First, you chop up the code into blocks as you see fit, then you can ask the LLM to write some block comments as a starting point.
You can then create block references, factor the blocks in a way that makes sense, and end up with a nice NL codebase.
You can use the NL -> PL direction to test whether the comment is good enough, i.e. whether the code ends up at least as good as the original.
Once you reach that point, you can comfortably start maintaining your code in English (or any other NL), and let the LLM deal with the PL part.

The NL PL agreement operations are a related set that can be used together.
These are based on the "output" feature, which isn't fully supported in the UI yet, so they are a little hard to understand right now.
Stay tuned for v10, where we'll probably have some kind of UI around the outputs, and the related "fixed point operator" features.
However, if you want to try it, you can look in .cmpr/outputs/ for the most recent timestamped output and you can see what it's doing.
The first op asks the model if the NL and PL parts agree, the other two, if it doesn't, asks the model to suggest a patch to either the NL or PL part that would bring it in line with the other.

The summary operation also just produces an output summarizing the current block.
We have some features planned that will use these outputs to help find the most relevant context blocks.

Finally the NL to algorithm operation just writes a step-by-step bullet-point algorithm from a prose description of a function or program.
This can be helpful for the LLM to divide the work into two steps, so the NL description you write can stay at a higher level, and you can separate the LLM's understanding of the algorithm from its ability to translate that algorithm into code.
This also just goes into an output under .cmpr/outputs currently.
We're still thinking about the best way to integrate it.
Should it create a separate block with the algorithm part?
Should it extend the comment part of the current block?

One exciting benefit of the palette is that it makes cmpr more extensible.
To this end, we implement palette operations in their own files (under ops/) and they make use of prompt templates (under prompts/) which are in a simple syntax.
If you want to see how any of our prompts work, just look at the files under prompts.
All you have to do to add your own ops to the palette is to copy one of the existing ones and modify it to your needs.
As we expose more of the functionality of cmpr to the palette ops, this extension point will become more powerful.
### Undo

There is now a basic undo feature.

The first time you use the feature it will create a rev cache.
(Subsequent times will still have to read the cache, but it will be faster than the first time.)
The cache is only generated, or read, the first time you use the "U" command, so startup remains snappy.

To use the "U" command, hit "U" while on a block.
All previous revs will be searched for blocks that are "similar enough" to this one to count as a version of it.
"Similar enough" means that either 8 lines in the block are the same as the current contents of the block, or that one of the ids of the block is a match.
At some future point we might give you a way to tune the sensitivity of the similarity search if it seems useful.
This is one reason to use multiple block ids, if you want to "connect" current version of code to some specific prior implementation.
(This is another reason to give your blocks ids.)
Note that the id match is only against the *current* block id, so if you undo back to an earlier version with a different id, or change the id manually, and then "U" again, you may find other matches.
(That's also a clever way to get back a block you deleted, if you know the id: just create a new, empty block with that id, hit "U", and you'll get all previous blocks that had that id.)
(We'll have a "D" feature coming soon to directly search for blocks that are no longer in the codebase.)

Once you hit "U" you are in the undo mode.
Use j/k to navigate through the history.
Up (k) goes back in time and j goes forwards.
For each older version of the block you can see the timestamp when it was saved, and a number of versions away from the current one that it is.
If you want to revert to an old version of a block, hit Enter on it, and it will replace the current contents of the block.
If you then want to go back again, you can use "U" again.
The previous version will still be in there, and since the similarity metric is symmetric, it will still be matched, even if the block doesn't have an id.
(However, the other blocks that are matched when you "U" the second time may be different!)

The similarity measure is very basic (and fast) but will probably be tuned over the coming months.
If you find any cases that don't do the most useful thing, please let us know about them!

Note that you can also use the similarity to find code that is similar but different; for example copy-pasted functions will probably be picked up.
# v8

### Block references

Block references are now supported.
There are two kinds, "inline" and "topline".
The inline refs are created by writing "@blockid" on a line alone in the NL code, while the topline refs are created by putting the "@blockid" ref on the top line of the block.
Block ids themselves are always created by writing "#blockid" on the top line of the block.
Blocks can have more than one id.
The best way to see these features for now is to check the cmpr source itself for examples.
You can use the ":expand" ex command in cmpr to see how the block you're looking at looks when expanded.
This can be a useful way to check some references quickly yourself.
You can use "@blockid:code" to include the PL section of the block instead of the natural language section, and "@blockid:all" to include both.

### Block ids

Block ids with the "hashtag syntax", like "#block\_id" are now a supported feature.
In addition to the block references feature using them, they are also used by the "#" keybinding, which lets you see and jump quickly to any block that has an id.

### Markdown support

Markdown is now supported.
Each heading creates a block: if you have "# h1\n## h2" you'll get two blocks.
Markdown blocks don't have a "code part", they are all NL and no PL.

### Windows and build fixes

Thanks to @dannovikov for the Windows improvements.

### Ollama support

Run the ollama server locally and load the models, and add the models you want to use to the conf file (e.g. "ollamas: llama3,llama3:70b").
The models will then appear in ":models".

### Llama.cpp support

Just run llama.cpp with the model you want to use, and use ":models" to pick llama.cpp.

### Curl binary instead of curllib

We no longer link against libcurl, but just call out to the curl binary.
You can set the curlbin in the conf file if you don't want the default (which is just "curl" in your PATH).

### bootstrap-py.sh

Thanks to @petterik for the Python bootstrap script.
To use it, copy bootstrap-py.sh into your project, put "bootstrap: ./bootstrap-py.sh" in your .cmpr/conf and modify it to suit your needs.
The script assumes you have a top-level comment in block 1 that is relevant for the LLM and that you have ctags installed.

# v7

### API support

Support for LLM rewriting directly via API is now supported in addition to the clipboard style interaction.
Use ":models" to switch between the modes, and put your OpenAI API key in .cmpr/openai-key.

# v6 ... v1

### bootstrap, --init, and conf file handling

Many minor improvements and fixes.
