# Front-end and API plans

## Basic architecture

For input to the running system, we can have cmpr (the same binary) run in a --server mode.

All the data lives in flat files on the filesystem so this should be compatible with running the normal mode at the same time, and with using other editors, etc.

For the "read-only" mode prototype, we will set up a flat file structure under serve/ with each block getting a directory to itself.
Then we can have NL part, PL part, and later, output parts, instead of storing the whole block.

For the j/k order, I'll the f/e decide how to handle it, and just present, for now, the file information separately.
So we'll have, for each file, a block sequence.
We'll have a JSON array for these:

[{"file":"README.md","blocks":["0123456789ABCDEF",...]}
,...
]

To prototype, I can write all these out as static files on startup:

serve/blocks/[hash]/{nl,pl}
serve/files.json

We can use nginx or anything pointed at that .cmpr/serve directory to serve over HTTP on localhost.
So for testing we can run cmpr once, start a standalone http server, and then the f/e accesses the static files.
