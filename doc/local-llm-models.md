# Local models

This document is for running a model served on the same machine as an agent in this tree: what to install, the settings each model ran best with, and what each model did. Every statement in it comes from a session or a fixed request run between 2026-11-24 and 2026-11-27 on an Apple M5 Max with 128 GB of memory.

## The parts

| Part | Used here | Its job |
|---|---|---|
| Inference server | oMLX 0.8.4 | serves MLX models over the OpenAI chat API at `http://127.0.0.1:8000/v1` |
| Agent | `omp` 19.1.1 (oh-my-pi) | gives the model its tools, runs it in the tree, and loads `AGENTS.md` |
| Instructions | `AGENTS.md` | the rules, the machine, and how to write, run and debug Forth here |
| The tree's tool | `build/cmake/hexaforth --typed FILE.fs` and `--evaluate 'TEXT'` | types Forth at the built system, and prints what it printed, the stack and why the machine stopped |

LM Studio (Bionic 1.2.2) served the first sessions. oMLX replaced it because LM Studio ignores or overrides request fields that these models depend on (What each server does with a request, below).

## Recommended settings

| Model | Temperature | Top-p | Top-k | Repetition penalty | Reasoning | Thinking budget |
|---|---|---|---|---|---|---|
| gpt-oss-120b, MXFP4 | 1.0 | 1.0 | off | off | effort medium | none |
| Qwen 3.8 27B, 6-bit | 1.0 | 0.95 | 20 | off | effort medium | 8,192 tokens |
| Qwen 3.5 35B-A3B, 4-bit | 0.6 | 0.95 | 20 | 1.1 | on | none |
| Gemma 4 26B-A4B, 8-bit | 1.0 | 0.95 | 64 | 1.25 | on | 8,192 tokens |

The sampling in each row is the publisher's for that model. The reasoning, the budget and the penalty are those of the model's best session here, and for Gemma, whose sessions each ran to the time limit, those of its last (What each model did, below). For every model:

- Give the model its whole context window and an output limit of 32,768 tokens.
- Pass `--thinking medium` to `omp` for a model that takes an effort, which gpt-oss-120b and the Qwen 3.8 27B do. `omp`'s own default is high.
- Run `omp` with `--offline` and leave `web_search` out of its tools, which keeps the tree's text on this machine.
- Run the session under `caffeinate -is`. `--max-time` counts wall-clock time, so a laptop that sleeps uses the limit up.
- Treat the time limit as a guard against a session that runs on. Sessions here had 90 minutes.

## Setting it up

### oMLX

1. Install oMLX from its release's disk image, and copy the app to `/Applications`. Its command-line program is `/Applications/oMLX.app/Contents/MacOS/omlx-cli`.

2. Start the server on the folder that holds the models:

   ```
   /Applications/oMLX.app/Contents/MacOS/omlx-cli serve --model-dir ~/.lmstudio/models --paged-ssd-cache-dir ~/.omlx/cache --hot-cache-max-size 8GB
   ```

   It listens on `127.0.0.1:8000`, takes a request from the same machine without a key, and names each model by its folder (`Qwen3.8-27B-MLX-6bit`, `gpt-oss-120b-mxfp4-bf16`). `~/.lmstudio/models` is where LM Studio downloads models, so the two servers share one copy of each. The server's log is `~/.omlx/logs/server.log`.

3. Set a model's thinking budget in `~/.omlx/model_settings.json`, in that model's entry under `models`, while the server is stopped:

   ```json
   "thinking_budget_enabled": true,
   "thinking_budget_tokens": 8192
   ```

   The budget ends a turn's reasoning at that many tokens. Text that a model writes inside a tool call's arguments is outside the count.

### omp

`~/.omp/agent/models.yml` names the server and the models:

```yaml
providers:
  local-omlx:
    baseUrl: http://127.0.0.1:8000/v1
    api: openai-completions
    auth: none
    models:
      - id: gpt-oss-120b-mxfp4-bf16
        reasoning: true
        contextWindow: 131072
        maxTokens: 32768
      - id: Qwen3.8-27B-MLX-6bit
        reasoning: true
        contextWindow: 262144
        maxTokens: 32768
        compat:
          thinkingFormat: openai
      - id: Qwen3.5-35B-A3B-4bit
        reasoning: true
        contextWindow: 262144
        maxTokens: 32768
      - id: gemma-4-26B-A4B-it-MLX-8bit
        reasoning: true
        contextWindow: 262144
        maxTokens: 32768
        compat:
          thinkingFormat: qwen
```

`thinkingFormat: openai` makes `omp` send the reasoning effort as the request's top-level `reasoning_effort`. `thinkingFormat: qwen` makes it send `enable_thinking: true`, and Gemma on oMLX reasons only when a request carries that field.

The sampling for one run is a YAML file given with `--config`. This one is the Qwen 3.8 27B's:

```yaml
temperature: 1.0
topP: 0.95
topK: 20
minP: 0
presencePenalty: 0
repetitionPenalty: 1.0
```

A session is one command, run from the top of the tree it works in:

```
caffeinate -is omp -p --offline --no-title --max-time 90m --thinking medium --config qwen27.yml --session-dir RECORDS --tools read,bash,edit,eval,glob,grep,task,wait,todo,write --model "local-omlx/Qwen3.8-27B-MLX-6bit" --cwd "$PWD" 'TASK'
```

| Argument | What it is for |
|---|---|
| `-p 'TASK'` | one task, run to its end and printed to the terminal |
| `--offline`, and `--tools` without `web_search` | the session reaches the local server only |
| `--max-time 90m` | the guard |
| `--thinking medium` | the reasoning effort |
| `--config FILE.yml` | the sampling |
| `--session-dir DIR` | where the session's record goes, a JSONL file that `omp render FILE.jsonl` draws |
| `--cwd DIR` | the tree, whose `AGENTS.md` `omp` loads |

### A tree for the session

A session runs in a copy of the tree, so that the model's edits stay out of the working tree and every session starts from the same files:

```
cp -Rc hexaforth session
cmake --fresh -S session -B session/build/cmake
cmake --build session/build/cmake
mv session session-base
```

The copied `build/cmake` still names the tree it was copied from, and `--fresh` configures it again for the copy. A configured build names its own path, so the copy is built at the path the sessions will run at and kept under another name, and each session starts with `cp -Rc session-base session`.

## What each server does with a request

Each row was measured with fixed requests at temperature 0: the same request with the field and without it, compared by the reply or by the size of the prompt the server rendered.

| In the request | LM Studio (Bionic 1.2.2) | oMLX 0.8.4 |
|---|---|---|
| nothing about a penalty | a repeat penalty of 1.1 on gpt-oss-120b, the Qwen 3.5 35B and Gemma 4 | none |
| `repetition_penalty` | ignored | applied |
| `presence_penalty` | ignored | applied |
| `reasoning_effort` for gpt-oss-120b | ignored: one reply, with 597 reasoning tokens, for `low`, for `high` and for a request without the field | applied: 91 reasoning tokens at `low`, 323 at `medium` and without the field, 1,539 at `high` |
| `reasoning_effort` for the Qwen 3.8 27B | applied at the top level of the request, ignored inside `chat_template_kwargs` | applied in both places |
| `enable_thinking` | ignored | applied |

oMLX also applies a request's `thinking_budget`. It sends a tool call only when the call is complete, and during the silence it sends a chunk with empty content every 10 seconds.

On LM Studio the default penalty is removed for one model by adding `{"key": "llm.prediction.repeatPenalty", "value": {"checked": true, "value": 1}}` to the model's file under `~/.lmstudio/apps/bionic/.internal/user-concrete-model-default-config/` and loading the model again. The hub definition of `qwen/qwen3.8-27b` already turns the penalty off.

## What omp does

| What it does | Its setting | What that meant here |
|---|---|---|
| reasons at high effort by default | `--thinking LEVEL` | gpt-oss-120b at high spent 90 minutes reading and searching, and at medium it finished in 3 |
| sends a turn again when the turn was cut at the output limit, with the turn's reasoning left out | `features.unexpectedStopDetection` | a Qwen 3.8 27B turn that reasoned to the 32,768-token limit took about half an hour and was then started over. The thinking budget ends the reasoning before that limit |
| drops a reply whose stream is silent for 300 seconds | `providers.streamIdleTimeoutSeconds`, where 0 turns it off | oMLX's empty chunks leave that timer running, so a tool call that takes longer than 300 seconds to generate is dropped with "stream stalled while waiting for the next event". Gemma's `write` calls of 12,000 to 32,000 tokens ended that way |
| stops a reply that repeats its reasoning, and sends the turn again | `model.loopGuard.enabled` | it fired on Gemma 4 and on the Qwen 3.5 35B |
| asks the system to stay awake | `power.sleepPrevention` | the laptop slept all the same, and `caffeinate -is` kept it awake |

## The task

Every session had the same task: write a word that multiplies two 64-bit cells into a 128-bit product, computed from 32-bit halves with the one-cell multiply, in a file under `build/`; run it on two given operands; and report what it printed. A correct word needs the four partial products and the carries between them. The system's own `um*` returns the same product, so a session can print the right digits with a word that leaves the halves out, and such a word is counted here as the task left undone.

## What each model did

Every session ended without a correct word built from 32-bit halves. The Qwen 3.8 27B is the one model that built its word from halves: the 6-bit in each of its four sessions, and the 8-bit in its session on oMLX.

| Model | Server and settings | What it did |
|---|---|---|
| gpt-oss-120b | oMLX, effort medium | ended by itself in 3 minutes after 7 tool calls, with a run made after its last edit and that run's output quoted exactly. Its word was `um*` under the new name, and every gpt-oss-120b session, on either server, left the 32-bit halves out |
| gpt-oss-120b | oMLX, effort high | ran to the 90-minute limit. 214,667 of its 217,299 output tokens were reasoning, and its 116 tool calls were reads and searches of the source and one write, with the shell left unused |
| gpt-oss-120b | LM Studio, which runs it at medium effort whatever the request says | ended by itself in 7 to 12 minutes with `um*` under the new name. In one session 8 of its 13 shell commands were here-documents into `hexaforth --typed -` |
| Qwen 3.8 27B, 6-bit | oMLX, effort medium, budget 8,192 | made its first tool call at 7 minutes and settled `hi32`, `lo32`, `u>`, `3rd`, `2swap` and `lshift` on the machine in six probe runs. It wrote the word at 47 minutes, rewrote it through four turns of about 9 minutes each before running it once at 80 minutes, and reached the limit with wrong output and a stack underflow |
| Qwen 3.8 27B, 6-bit | LM Studio, effort medium, no budget | made its first tool call at 44 minutes, then used the underflow report, probe files and `trace-on`, and reached the limit with a product shifted up by 32 bits |
| Qwen 3.8 27B, 4-bit | LM Studio, effort medium, no budget | made its first tool call at 38 minutes and two probe runs, and reached the limit before it wrote a file |
| Qwen 3.8 27B, 8-bit | oMLX, effort medium, budget 12,288, a 120-minute limit | made its first tool call at 12 minutes, checked its formula against arbitrary-precision arithmetic in Python through `omp`'s `eval` tool, and wrote and ran its word at 46 minutes. It then wrote that it could not count the F's in the output by eye, compared its word with `um*` by printing the differences in decimal, found them non-zero, and was printing each intermediate value when the limit came. Its word is built from halves and prints a wrong product, because a `2drop` removes the sum that its stack comment says is kept. Four of its turns reasoned to the budget, 13 to 14 minutes each at 15 tokens a second, and 3 of its 8 shell commands broke the shell rule |
| Qwen 3.8 27B, 8-bit | LM Studio, effort medium, no budget | reasoned for the whole 90 minutes at about 17 tokens a second, through two turns that each reached the output limit, and the session ended before its first tool call |
| Qwen 3.5 35B-A3B | oMLX, repetition penalty 1.1 | ended by itself in 18 minutes and reported that its word was unfinished, which was true |
| Qwen 3.5 35B-A3B | oMLX, no penalty | ended by itself in 13.5 minutes with a report that blamed `um*` and gave a wrong value as the right answer |
| Qwen 3.5 35B-A3B | oMLX, presence penalty 1.5 | ran to the limit with 226 tool calls and a word that wrapped `um*`. 159 of its 166 shell commands broke the shell rule of `AGENTS.md`, many of them `hexaforth build/64/nuc.hex words` piped into `grep`, which waited on stdin until it timed out |
| Qwen 3.5 35B-A3B | LM Studio, penalty off, and again at 1.1 | wrote and ran the same unfinished definition every 10 seconds until it was stopped, for 70 of the 90 minutes with the penalty off |
| Gemma 4 26B-A4B | LM Studio, penalty off | wrote its first file at 5 minutes and ran it at 8. It used the names in its stack comments as words (`l1 l2 +`), so each of its 8 runs ended in `error: -13`, and it kept those names through every rewrite. `omp` stopped 11 loops in its reasoning |
| Gemma 4 26B-A4B | oMLX, budget 8,192, repetition penalty 1.25 | made 29 writes and one run in 90 minutes. It reasoned inside the text of its `write` calls, and 9 of those calls outlasted `omp`'s 300-second timer and were dropped |

What the sessions show about the settings:

- The reasoning effort decides whether a model acts. At high effort gpt-oss-120b read and searched for the whole 90 minutes, and at medium it was done in 3.
- A thinking budget bounds how long a turn reasons before it acts. On oMLX, with a budget of 8,192 tokens and the rule in `AGENTS.md` against working a whole solution out first, the Qwen 3.8 27B made its first tool call at 7 minutes. On LM Studio, with neither, it made it at 44. At the model's 15 to 20 tokens a second a turn at that budget still takes about 9 minutes.
- A repetition penalty changed how the Qwen 3.5 35B ended: an honest report at 1.1, and a false one without it.
- The Qwen 3.5 35B's endless repetition went with LM Studio. On oMLX, with the same sampling, it ended by itself.
- The shell rule of `AGENTS.md` rests on the model under `omp`, because only Claude Code's hook enforces it. The Qwen 3.5 35B broke it in most of its commands in every session, 159 of 166 in one. The Qwen 3.8 27B broke it in none of 11 commands in one session and in 3 of 8 in the next, and gpt-oss-120b in none of 6 in one session and in 8 of 13 in another.
- A model reaches for whatever tool it has. The Qwen 3.8 27B at 8-bit checked its arithmetic in Python through `omp`'s `eval` tool, which is in the `--tools` list above.

## How a model reads a printed cell

A fixed set of 48 questions was put to a model with the same 64-bit cells printed three ways, all in upper case: without leading zeros (`1FFFFFFFE`), as 16 digits (`00000001FFFFFFFE`), and as two groups of 8 digits (`00000001 FFFFFFFE`). Sixteen questions ask for the upper and the lower 32 bits of a cell, sixteen whether two cells are equal, and sixteen for the sum of two cells. Each question is one request, sent with the model's own sampling.

Each cell of this table has three figures, for the cells printed without leading zeros, as 16 digits, and as 8 + 8 digits, in that order. The first three columns are right answers of 16, and the fourth is their total of 48.

| Model | Reasoning | The two halves | Whether equal | The sum | All | Output tokens a reply |
|---|---|---|---|---|---|---|
| Qwen 3.8 27B, 8-bit | off | 2, 13, 16 | 12, 16, 14 | 1, 3, 3 | 15, 32, 33 | 11, 8, 16 |
| Qwen 3.5 35B-A3B | off | 5, 13, 15 | 10, 16, 13 | 0, 1, 0 | 15, 30, 28 | 7, 9, 7 |
| Gemma 4 26B-A4B | off | 7, 13, 16 | 10, 15, 15 | 2, 5, 5 | 19, 33, 36 | 21, 14, 15 |
| Qwen 3.8 27B, 8-bit | on | 14, 16, 16 | 16, 16, 16 | 11, 13, 12 | 41, 45, 44 | 1,189, 608, 821 |
| Qwen 3.5 35B-A3B | on | 10, 14, 16 | 16, 16, 16 | 6, 8, 7 | 32, 38, 39 | 1,845, 1,596, 1,443 |
| Gemma 4 26B-A4B | on | 13, 16, 16 | 16, 16, 16 | 8, 9, 8 | 37, 41, 40 | 1,983, 1,355, 1,348 |
| gpt-oss-120b | effort medium | 16, 16, 16 | 16, 16, 16 | 15, 16, 14 | 47, 48, 46 | 739, 645, 511 |

- Without reasoning, each of the three models gets about twice as many answers right when the zeros are printed. It takes the first eight digits of an unpadded cell as the upper half (`83DC7C569A` gave `83DC7C56 9A`), and calls two cells equal when they differ by one F in a run (`FF000FF8` and `FFF000FF8`).
- With reasoning, the three get 3 to 7 more answers of 48 right with the zeros, and pay for an unpadded cell in tokens. Taking the halves of one cost the Qwen 3.8 27B 1,271 tokens a reply against 461 for 16 digits, and Gemma 4 2,217 against 492.
- gpt-oss-120b reads the three forms alike.
- Two groups of eight digits give the halves away. On equality without reasoning, 16 unbroken digits were right at least as often as the two groups, for every model.
- In its session the Qwen 3.8 27B printed a cell as two groups of eight digits, wrote that it could not count the F's by eye, and built a verifier in place of comparing the digits.

`.x` therefore prints every digit of a cell, and `hexaforth`'s own reports, which a person reads beside a model, leave the leading zeros out.

## What the sessions changed in the tree

| What a model did | What the tree has for it |
|---|---|
| built a program by itself with `cross.fs`, where the nucleus's words are missing | `hexaforth --typed FILE.fs`, which types a file at the built system, where every word the models expected exists |
| misread the first error of a run | an `error:` line that ends with the word the error came from, and a report of the line a stopped machine was running |
| began a comment with `#` | a `\` comment as the first line of the first example in `AGENTS.md`, and `#`, `//` and `/*` named there as words |
| used the names in a stack comment as words | the sentence in `AGENTS.md` that such a name is a label for the reader |
| reasoned to the output limit before its first run | the rule against working a whole solution out before running anything, and the thinking budget |
| reasoned inside the text of a `write` call | the rule against reasoning inside a tool call's arguments |
| checked its probes by hand in place of running them | the rule against working out by hand what a line leaves on the stack, and the steps in `AGENTS.md` that try words on numbers and trace a definition that gives a wrong answer |
| wrote that it could not count the F's of a cell printed as two groups of eight digits, and spent its next two turns, 20 minutes, on a verifier in its place | `.x` prints a whole cell at the machine's width after a `$`, and every hex number in `hexaforth`'s reports is printed the same way, without the leading zeros |
| ran `hexaforth` on an image and waited on stdin, or fed it a here-document | `hexaforth --evaluate 'TEXT'`, and a Tools table in `AGENTS.md` without the row for stdin |
