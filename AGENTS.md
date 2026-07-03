# ff4-gnw — agent context

> Native C port of Final Fantasy IV for the Nintendo Game & Watch. See
> [README.md](README.md) for the architecture and build details.
>
> This repo is one of three that make up the FF4 → Game & Watch porting
> effort, alongside [`ff4-port`](https://github.com/hcross/ff4-port)
> (translation workshop / oracle) and the umbrella
> [`ff4`](https://github.com/hcross/ff4) repo, which carries the full stable
> context, the dispatch registry, and the workflows (`WF-DECOMP`, `WF-VALID`,
> `WF-RELEASE`). Read `ff4/AGENTS.md` when working from the umbrella clone.

---

## Language policy — English only, everywhere, no exceptions

**Every artifact produced for this project MUST be in English**, regardless
of any global or session-level language preference the assisting agent might
otherwise follow (e.g. a `~/.claude/rules/*.md` "respond in French"
directive). That preference governs conversation with the user — it never
governs what gets written into these repositories. This project instruction
**overrides** it, per Claude Code's own precedence rules for project-level
`AGENTS.md`/`CLAUDE.md` files.

Covers, without exception, across `ff4`, `ff4-gnw`, and `ff4-port`:

- **File content** — code, comments, docstrings, commit-tracked
  Markdown/docs, generated artifacts kept under version control (spikes, logs).
- **Git commit messages** — subject and body.
- **GitHub assets** — issue titles/bodies/comments, PR titles/descriptions/
  reviews, release notes, GitHub Actions workflow names, repository
  description and topics.

> ⚠ **This has already broken twice.** `ff4-port` commit `e370941` (2026-06)
> documents an LLM translation run that picked up the user's global French
> CLI preference and emitted French inline comments in generated C — caught
> only after the fact. The entire `translate/en` history rewrite (2026-07,
> all three repos, `archive/untranslated-main` holds the pre-rewrite state)
> exists because French had crept back into commits and docs since. Do not
> repeat either failure mode: **check the language of what you are about to
> write to this repo before writing it, independent of what language you are
> conversing in.** If in doubt, write in English.
