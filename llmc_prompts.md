# Comparison feature

Works great.
We manually stripped the comments from the first one (which we can do with some language-specific tooling), and added the "Identical" reply.

We can turn this into a palette op.

````
Compare the following two implementations of a function.
Original:
```c
void encoder_forward(float* out,
                   int* inp, float* wte, float* wpe,
                   int B, int T, int C) {
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float* out_bt = out + b * T * C + t * C;
            int ix = inp[b * T + t];
            float* wte_ix = wte + ix * C;
            float* wpe_t = wpe + t * C;
            for (int i = 0; i < C; i++) {
                out_bt[i] = wte_ix[i] + wpe_t[i];
            }
        }
    }
}
```
Alternate:
```c
void encoder_forward(float* out, int* inp, float* wte, float* wpe, int B, int T, int C) {
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float* out_bt = out + b * T * C + t * C;
            int ix = inp[b * T + t];
            float* wte_ix = wte + ix * C;
            float* wpe_t = wpe + t * C;
            
            for (int i = 0; i < C; i++) {
                out_bt[i] = wte_ix[i] + wpe_t[i];
            }
        }
    }
}
```
Identify the differences between the two.
First indicate whether the implementation is identical, i.e. whether the implementations will have the same effects when run.
Secondly, indicate the code style differences.
Do not mention the lack of comments, or changes in comments.
Do not comment on whitespace differences.
Reply only with a bulleted list of concise sentences, but be complete in reporting the differences that are found.
If no substantive differences are found, reply only with "Identical".
````
