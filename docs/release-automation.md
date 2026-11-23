# Automated CMake source releases

The existing `KrnlC C11 portable kernel` workflow must pass on the *exact latest main SHA*
before GitHub Actions may generate a semantic version release pull request,
or publish a tagged GitHub source release. CMake reads its project version
from `version.txt`; Release Please updates this file and the changelog.

Native developers can consume immutable `vX.Y.Z` GitHub source tags via
CMake FetchContent; no local credentials or registry secret are required.
This does **not** register a vcpkg port or Conan package. Separate registry
release automation and consumer verification are necessary for those systems.

Enable Actions PR creation. `GITHUB_TOKEN`-created release PRs do not
trigger CI; for fully automated safe PR merges use an authorized GitHub App
token, required checks, and auto-merge. Avoid bypassing branch protection.
