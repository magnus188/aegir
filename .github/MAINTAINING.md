# Maintaining Ægir

## Issues and the v2 backlog

Use an issue to record a problem or investigation; use a pull request to propose
the actual file changes. An issue does not need to have a solution yet.

- `idea`: an option to investigate, with no implementation commitment.
- `bug`: something that behaves incorrectly.
- `enhancement`: a proposed improvement with a clearer scope.
- `help wanted`: a specific task where outside help is useful.
- `good first issue`: a small, explained task a newcomer can complete.

The [v2 milestone](https://github.com/magnus188/aegir/milestone/1) collects future
investigations. Keep current prototype defects and qualification work separate.
After investigating an idea, record the options, evidence, remaining unknowns
and decision in the issue. It is fine to close an idea with a reason or split
accepted work into smaller implementation issues.

## Everyday contribution workflow

1. Make a branch from up-to-date `main`, or use a fork for an outside contribution.
2. Open a focused pull request and link the issue. Review the final diff,
   including AI-generated work and dependency updates.
3. Wait for the required checks and resolve review conversations. If
   `main` changed, update the PR branch and let checks run again.
4. For measurement, calibration, alarms, power and gas handling, obtain relevant
   expert review and record physical validation or its pending status. A passing
   automated check alone is not evidence of measurement accuracy.
5. Squash and merge the PR with a meaningful title. The merged branch is deleted
   automatically. Close an investigation only when its questions are answered.

Inspect an outside contributor's proposed workflow and code before approving a
fork workflow run. Approval to run CI is separate from code review. All outside
contributors' fork workflows require maintainer approval under the repository's
Actions policy.

## Main-branch rules

The repository ruleset targets only the default branch and requires:

- A pull request for every change, including the maintainer's changes.
- Successful `Build ESP32-P4 Firmware` and `Validate GitHub workflows` checks
  from GitHub Actions on an up-to-date branch, and resolved review conversations.
- No force pushes or branch deletion, with no routine bypass actors.

There is currently one maintainer. Required approvals are set to zero so the
owner can merge their own checked PRs; GitHub does not allow self-approval.
`CODEOWNERS` requests the owner's review on other contributors' PRs. This is
review routing, not enforced independent review. When another maintainer agrees
to review changes regularly, add them and require at least one approval with
stale approvals dismissed. Keep expert review expectations for physical and
measurement changes in the contribution guide regardless of this setting.

Administrators can edit the ruleset in Settings → Rules → Rulesets. Do not add
a release-bot bypass to get around a failed check; diagnose the failure instead.
Check names in the ruleset must be updated if CI jobs are renamed.

## Releases and automation

Merging to `main` continues to run the automatic firmware release and web-demo
workflows. Review what will be published before merging. PR builds only upload
test artifacts; they do not publish releases.

The release workflow chooses a version from the latest release tag and the
commit titles, injects it into the build, and tags the exact source commit. It
does not push a version-update commit to protected `main`. The checked-in
`main/version.h` is a development baseline and may lag published releases.
To reproduce a release version locally, check out its tag and run
`software/firmware/scripts/set_version.sh X.Y.Z` before following the firmware
build guide. A deliberately raised baseline above all release tags still sets
the next release version. Manual release runs are restricted to `main`.

Release publication still requires both P4 builds, tests, size checks and
uploaded-image digest validation. These are software checks, not physical
qualification. If publication fails, inspect the run and any remaining draft
before retrying; do not overwrite a published release tag.

Workflow actions are pinned to commit SHAs. Dependabot proposes weekly action
updates; review their release notes and passing checks before merging. Default
workflow tokens are read-only, with write permissions explicitly granted only
where needed. GitHub Pages deployment permissions belong to its deploy job.

Dependency alerts, security updates, secret scanning, push protection and
private vulnerability reporting are enabled in repository settings. Dependency
coverage depends on the manifests GitHub understands; the ESP-IDF dependency
lock and hardware components still need manual review. Handle vulnerability
reports according to [SECURITY.md](../SECURITY.md).
