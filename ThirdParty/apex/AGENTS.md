# Agent Instructions

Apex is a C Markdown processor built on a vendored cmark-gfm. Build and
release tasks live in `buildnotes.md` (gitignored, run with `howzit -r
<task>`), but everything an agent needs day to day is below.

## Build and Test

```bash
mkdir -p build && cd build && cmake .. && cd ..   # first time only
cmake --build build                               # CLI, libraries, test runner
./build/apex_test_runner                          # full C suite
./build/apex_test_runner <suite>                  # one suite, e.g. callouts
./build/apex_test_runner --errors-only            # show failures only
```

CLI behavior has separate shell tests; run the relevant ones after
rebuilding `build/apex`:

```bash
./tests/metadata_cli_test.sh
./tests/multi_file_cli_test.sh
./tests/paginate_cli_test.sh
```

- Suites are registered in `tests/test_runner.c`; add new test functions
  there and to the matching `tests/test_*.c` file.
- Write a failing test before fixing a bug, and confirm it fails for the
  right reason.
- The CLI reads the user's `~/.config/apex` config and plugins, which can
  change output. Use `XDG_CONFIG_HOME=$(mktemp -d)` when checking CLI
  output, or test through `apex_markdown_to_html` in the C suite.
- `APEX_DEBUG_PIPELINE=1` prints the Markdown between preprocessing stages.

## Tracking Follow-up Work

- Note bugs or follow-ups found along the way in `apex.taskpaper` (under
  `Inbox:` or `Bugs:`) and mention them in the hand-off.
- GitHub issues live at `ApexMarkdown/apex`; use `gh` to read or comment
  on them when asked.

## Commits

The changelog is generated from commit bodies, so the format matters:

- Subject under 60 characters, then a blank line.
- Body lines for user-facing changes start with `@new`, `@fixed`,
  `@changed`, `@improved`, or `@breaking`, and bold the subject of the
  change: `@fixed **Callout titles** keep inline markup`.
- Keep each `@` line on one line; don't hard-wrap it.
- Technical details go in untagged lines. Don't tag documentation-only
  notes.
- Straight quotes and ASCII punctuation only; no emoji; no co-author
  trailers.
- The message can be drafted in `commit_message.txt` (gitignored) and
  committed with `git commit -F commit_message.txt`.

Only stage files related to the task. Leave unrelated local changes (for
example `Formula/apex.rb`, which the Homebrew release task manages)
uncommitted.

## Vendored cmark-gfm

`vendor/cmark-gfm` is a submodule tracking the `apex` branch of
`ApexMarkdown/cmark-gfm`. Changes there must be committed and pushed in
the submodule first, then the updated submodule pointer committed in
this repo; otherwise releases ship without them.

## Releases

Version bumps, tags, Homebrew, npm, gem, and wiki publishing are handled
by the `buildnotes.md` deploy tasks. Don't bump `VERSION`, edit
`CHANGELOG.md`, or tag a release unless asked.

## Finishing a Session

1. Run the C suite (and relevant CLI tests) if code changed.
2. Commit with the format above.
3. Push: `git push`, then confirm `git status -sb` shows `main...origin/main`
   with nothing ahead. `git pull --rebase` refuses while unrelated files are
   modified; fetch and rebase only when the push is rejected.
4. Hand off: what changed, what was verified, and anything left in
   `apex.taskpaper`.
