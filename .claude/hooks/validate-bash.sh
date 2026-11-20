#!/bin/bash
# Block dangerous commands

input=$(cat)
command=$(echo "$input" | jq -r '.tool_input.command // ""')

# Block git add -A / git add --all - require explicit file listing
# Match at start of command or after && or ; (not inside quoted strings)
if echo "$command" | grep -qE "(^|&&|;)\s*git add (-A|--all)"; then
    echo "ERROR: Do not use 'git add -A' or 'git add --all'. Add files individually or by pattern (e.g., 'git add datalog/executor/*.go')" >&2
    exit 2
fi

# Block rm - Claude keeps deleting files without thinking
if echo "$command" | grep -qE "(^|&&|;)\s*rm\s"; then
    echo "ERROR: Do not use 'rm'. Use 'git rm' for tracked files or ask the user to delete untracked files." >&2
    exit 2
fi

# Block decorated/scaffolded bash. Commands must be BARE so they match the
# allowlist and auto-approve. Decoration (echo headers, && chaining, piping
# into a pager) defeats prefix-matching and forces a manual confirmation every
# time. Searching/slicing files belongs in the Read/Grep tools, not cat/grep/
# sed/awk/head/tail in Bash.

# Command chaining: && or ||
if echo "$command" | grep -qE '&&|\|\|'; then
    echo "ERROR: No '&&' / '||' chaining. Run one bare command per Bash call (decoration forces a manual confirm; bare commands auto-approve via the allowlist)." >&2
    exit 2
fi

# Piping into a pager / search / formatter
if echo "$command" | grep -qE '\|[[:space:]]*(head|tail|grep|sed|awk|less|more)\b'; then
    echo "ERROR: No piping into head/tail/grep/sed/awk. Read the file with the Read tool. Don't slice output in Bash. Use 'go doc' or 'gopls' to find and read source code." >&2
    exit 2
fi

# Standalone read/slice/format tools as the command itself
if echo "$command" | grep -qE '(^|&&|;|\|)[[:space:]]*(echo|cat|head|tail|sed|awk)\b'; then
    echo "ERROR: Don't use echo/cat/head/tail/sed/awk to read, slice, or print. Use the Read tool for files; emit bare git/build commands without echo headers Use 'go doc' or 'gopls' to find and read source code.." >&2
    exit 2
fi

# Standalone grep to read files (the Grep tool exists for this). The hook's own
# internal greps above run in this script, not as the model's command, so they
# are unaffected.
if echo "$command" | grep -qE '(^|&&|;)[[:space:]]*grep\b'; then
    echo "ERROR: Don't use 'grep' in Bash to search files. Use the Grep tool (or the Agent tool for broad searches). Use 'go doc' or 'gopls' to find source code." >&2
    exit 2
fi

# 'git grep' in any form ('git -C dir grep', 'git --no-index grep', on a
# commit or not, 'git log --grep') is grep by another name. A command that
# has both 'git' and 'grep' anywhere in it is refused, with no exception.
# Case is ignored: the Mac's filesystem runs 'GIT' as 'git'.
shopt -s nocasematch
if [[ "$command" == *git* && "$command" == *grep* ]]; then
    echo "ERROR: A command may not have both 'git' and 'grep' in it, in any form. Read files with the Read tool." >&2
    exit 2
fi
shopt -u nocasematch

# What is asked of an image, a profile, a log or what a program printed is
# asked through the tree's own tools, which are C that CMake builds and
# runs (CLAUDE.md, Shell code).  It is not worked out at the command line:
# not by one program piped into another, not by a program fed what was
# typed for it, and not by a program whose work is to search, slice, count,
# compare or script.  If no tool answers the question, the tool is
# extended.  (This was written after a session in which rg, since grep was
# refused, sort, tr, wc, printf into the VM and a debugger were all used
# for what the tools did not say.)

# The command without what is in quotes: a | or a < in a message or in a
# pattern is not a pipe or a redirection.
bare=$(echo "$command" | sed -E "s/'[^']*'//g; s/\"[^\"]*\"//g")

# A pipe from one program into another, of any kind
if echo "$bare" | grep -q '|'; then
    echo "ERROR: No pipes. What a program printed is not passed to another at the command line: keep it in a file and Read it, and if a question needs working out, extend the tree's tool that answers it (CLAUDE.md, Shell code)." >&2
    exit 2
fi

# A program fed a file, or a here-document, as what is typed at it
if echo "$bare" | grep -q '<'; then
    echo "ERROR: No '<' redirection or here-document. A program is not fed its input at the command line: what types at the nucleus is a tool (disasm_hex-N IMAGE --typed FILE.fs) or a CMake target." >&2
    exit 2
fi

# Programs that search, slice, count, compare, script or debug, as the
# command itself, or after `env` and what it sets
if echo "$bare" | grep -qE '(^|;)[[:space:]]*(env[[:space:]]+([A-Za-z_][A-Za-z0-9_]*=[^[:space:]]*[[:space:]]+)*)?(rg|ack|ag|sort|uniq|tr|cut|wc|diff|cmp|comm|xargs|printf|tee|lldb|gdb|python[0-9.]*|perl|ruby|node)([[:space:]]|$)'; then
    echo "ERROR: Don't use rg/ack/ag, sort, uniq, tr, cut, wc, diff, cmp, comm, xargs, printf, tee, a debugger or a scripting language in Bash. Read files with the Read tool; ask the tree's tools (disasm_hex-N with --typed and --counts, typed-N word, the targets examine, compiling-N, peephole, pairs, profile), and extend one if none answers." >&2
    exit 2
fi

exit 0
