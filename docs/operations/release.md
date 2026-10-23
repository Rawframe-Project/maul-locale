# Release

Releases are made by release-please from the conventional commit titles on `main`. Versions follow Semantic Versioning; before 1.0.0 a breaking change bumps the minor version, a feature the minor and a fix the patch. Tags are `vX.Y.Z`.

## Steps

1. release-please keeps a draft pull request titled `chore: release X.Y.Z` open. It moves the version between the markers in `include/maul-locale/base.h`, where CMake reads it, updates `.release-please-manifest.json`, and adds an entry to `CHANGELOG.md` from the `feat`, `fix` and `perf` titles.
2. A new version changes the data version every data file carries, so regenerate `src/generated/` on the release branch ([updating-data.md](updating-data.md#regenerating-srcgenerated)) and push it to the release pull request; the `generated` job fails until it is done.
3. Rewrite the changelog entry in user language before merging: what changed for someone using the library, grouped as Added, Changed, Fixed, Removed and Security. Leave out refactors, tests, CI and documentation. Say what a caller must change for a breaking change, and name any change of results (a new CLDR or tzdb release, a fixed rule), since callers may store formatted text and sort keys.
4. Check that the `required` CI job passed on the release pull request.
5. Mark the pull request ready and merge it. release-please tags the merge commit and publishes the GitHub release with the changelog entry as its notes. The Pages workflow publishes the documentation from `main`.
6. The `assets` job of `.github/workflows/release.yml` then attaches the source archive `maul-locale-X.Y.Z.tar.gz` (the tagged tree, generated sources included), every data file (`*.mloc`), their `SHA256SUMS`, an SPDX SBOM and a build provenance attestation. Check that they are on the release; a user verifies a download with `sha256sum -c SHA256SUMS --ignore-missing` and `gh attestation verify <file> -R Rawframe-Project/maul-locale`.

## Notes

- A new CLDR release is a minor release, never a patch: results change ([updating-data.md](updating-data.md#a-new-cldr-release)). A new tzdb release in a patch release is fine; programs can also take new rules without one through a zones file.
- The shared family files must match their canonical copies in the organization's `.github` repository before a release.
- Maul Unicode is pinned by tag in `CMakeLists.txt`; moving the pin is a pull request of its own.
