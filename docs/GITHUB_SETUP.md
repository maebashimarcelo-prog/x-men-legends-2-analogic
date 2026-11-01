# GitHub publication setup (not performed here)

- Repository name: **XML2-Modern-Controller**
- Description: Modern controller support for X-Men Legends II on PC: true analog
  movement, hot-plug, reconnect and dynamic Xbox/PlayStation button prompts.
- Default branch: **main**
- Release tag: **v0.5.0-beta.1**
- Topics: `x-men-legends-ii`, `xml2`, `modding`, `controller`, `directinput`,
  `true-analog`, `xbox-controller`, `playstation-controller`.

Use the source ZIP's root as the standalone repository. It contains only the
public project; do not publish a parent modding workspace or game installation.
Initialize its own Git history/main branch when creating the repository, and
record the source freeze/preparation commits in release provenance. The local
workspace tag is preparation provenance, not a claim that GitHub already exists.

Before publishing: review the issue template, MIT license and limitations; add
real screenshots from `screenshots/README.md`; replace placeholders in POSTING_COPY.
Attach the binary ZIP, source ZIP and SHA256SUMS.txt to the release. Use
RELEASE_NOTES_v0.5.0-beta.1.md as its description.

For a rebuilt release, run both Package commands in README, then generate hashes
only after the final archives exist:

```powershell
$releaseFiles = @('build/modern-controller/DINPUT.dll',
  'dist/XML2-Modern-Controller-v0.5.0-beta.1.zip',
  'dist/XML2-Modern-Controller-v0.5.0-beta.1-source.zip')
$lines = foreach ($file in $releaseFiles) {
  '{0}  {1}' -f (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant(),
    [IO.Path]::GetFileName($file)
}
$lines | Set-Content -LiteralPath dist/SHA256SUMS.txt -Encoding ascii
```

Checksums are separate release artifacts, excluded from the source archive to
avoid self-referential archive hashes. Do not reuse hashes from older candidates.
No remote, push, online release or community post is part of local preparation.
