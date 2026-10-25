# Release

Releases are made by release-please from the conventional commit titles on `main`. Versions follow Semantic Versioning; before 1.0.0 a breaking change bumps the minor version, a feature the minor and a fix the patch. Tags are `vX.Y.Z`. While the major version is 0, the shared library's SOVERSION is the whole version and the CMake package accepts only its exact version, since any release may change the ABI.

## Steps

1. release-please keeps a draft pull request titled `chore: release X.Y.Z` open. It moves the version between the markers in `include/maul-locale/base.h`, where CMake reads it, updates `.release-please-manifest.json`, and adds an entry to `CHANGELOG.md` from the `feat`, `fix` and `perf` titles and the `BREAKING CHANGE:` footers.
2. Rewrite that entry in user language, in `CHANGELOG.md` on the release pull request's branch (`release-please--branches--main--components--maul-locale`): what changed for someone using the library, grouped as Added, Changed, Fixed, Removed and Security.
   - A breaking change comes first under Changed, prefixed **Breaking:**, and says what a program must change: regenerate its data files, rebuild its catalogs, compile stored messages again, ask for a buffer's capacity again.
   - Name every change of results (a new CLDR or tzdb release, a fixed rule), since callers may store formatted text and sort keys.
   - User documentation (the README and `docs/user/`) gets one short line under Changed when it changes what a user can find or do.
   - Leave out refactors, tests, CI, and the internals and operations documentation.
3. Put the same entry in the pull request's body, in place of the notes between release-please's `---` lines: release-please publishes the GitHub release notes from the body, not from `CHANGELOG.md`. release-please rewrites both whenever anything else lands on `main`, so the curation is the last change before the merge, and nothing else merges to `main` until the release is published. Curate only after the `release` run for `main`'s latest push has finished, and do not dispatch `release.yml` afterwards: a run in flight force-updates the release branch and writes the body again over the curation.
4. Check that the `required` CI job passed on the release pull request, that the last nightly run on `main` passed, and that the tag `vX.Y.Z` does not exist yet (`git ls-remote --tags origin vX.Y.Z` prints nothing).
5. Mark the pull request ready and merge it. release-please tags the merge commit and publishes the GitHub release with the body's notes. The Pages workflow publishes the documentation from `main`.
6. The `assets` job of `.github/workflows/release.yml` then attaches the source archive `maul-locale-X.Y.Z.tar.gz` (the tagged tree, generated sources included), every data file (`*.mloc`), their `SHA256SUMS`, an SPDX SBOM and a build provenance attestation. Check that they are on the release; a user verifies a download with `sha256sum -c SHA256SUMS --ignore-missing` and `gh attestation verify <file> -R Rawframe-Project/maul-locale`.

## Notes

- A change of results is never a patch release. A pull request that gives existing inputs a different result, refuses stored bytes, or breaks the API or ABI carries `!` in its title and a `BREAKING CHANGE:` footer saying what a program must do, whatever its type (`AGENTS.md`, Pull requests).
- A new CLDR release changes formatted text, and a new tzdb release the local time of existing instants: each is breaking, a minor release before 1.0 ([updating-data.md](updating-data.md)). A program that needs current zone rules sooner takes them without a release through a zones file ([data.md](../user/data.md#a-newer-time-zone-release)).
- A release changes no file in `src/generated/`: the data carries the data format version, not the library version ([data.md](../internals/data.md#versions)), and a pull request that changes the format regenerates the data itself, which CI's `generated` job checks. A file of an earlier release opens with every later library of the same data version.
- The shared family files must match their canonical copies in the organization's `.github` repository before a release.
- Maul Unicode is pinned by tag in `CMakeLists.txt`; moving the pin is a pull request of its own.
