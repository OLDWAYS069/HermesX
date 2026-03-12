# Status and Changelog Notes

Sources:
- `../../../docs/REF_status.md`
- `../../../docs/CHANGELOG.md`
- `../../../docs/CHANGELOG_MINI.md`
- `../../../docs/REF_changelog.md`

## How to use status docs

- Treat `REF_status.md` as project state context, not as final protocol truth.
- Use it to identify open risks and unfinished integration areas before patching.

## How to use changelog docs

- Prefer dated entries in `CHANGELOG.md` for latest behavior direction.
- Use `CHANGELOG_MINI.md` for quick timeline scanning.
- Use `REF_changelog.md` structure when drafting comparison summaries.

## Minimal changelog-ready output for a patch

- Date and scope.
- Behavior changes.
- Risk or regression notes.
- Follow-up actions if partially complete.

## Conflict handling

- If status doc and latest changelog disagree, trust latest dated changelog entry.
- If docs conflict without clear date priority, state assumption explicitly in patch summary.
