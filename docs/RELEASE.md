# Release Process

This document describes how to create releases for phu-splitter.

The release workflow (`.github/workflows/release.yml`) builds every platform through the reusable per-platform workflows (`build-windows-vst3.yml`, `build-linux.yml`, `build-macos.yml`, the same ones used by `build.yml`), generates `README.pdf` via `generate-pdf.yml`, injects it into each zip and publishes a GitHub release.

## Automatic Release (Tag Push)

When you push a tag matching `v[0-9]*` (e.g., `v1.0.0`, `v1.0.0-rc1`), the release workflow automatically:

1. Validates the tag (must look like `v1.2.3` or `v1.2.3-rc1`)
2. Builds Windows x64 (with pluginval), Windows ARM64, Linux x64 and macOS arm64/x64 (with pluginval)
3. Packages each platform as a ZIP file and adds `README.pdf`
4. Creates a GitHub release and uploads all ZIPs

**Example:**
```bash
git tag v1.0.0
git push origin v1.0.0
```

## Manual Release (Workflow Dispatch)

1. Go to **Actions** → **Release** workflow
2. Click **Run workflow** and select the branch (or tag) to release from
3. Enter the required **Tag** input (e.g., `v1.0.0`)
4. Click **Run workflow**

If the tag does not exist yet, the workflow creates and pushes it at the selected commit. If it already exists, it must point to the commit the run was started on; otherwise the run fails — select the tag itself under **Use workflow from** to re-run a release for an existing tag.

## Pre-release Detection

A release is marked as pre-release if the tag contains a hyphen (e.g., `v1.0.0-alpha.1`, `v1.0.0-beta.2`, `v1.0.0-rc.1`).

## Release Artifacts

Each release includes one ZIP per platform, each containing the plugin bundle(s) and `README.pdf`:

| ZIP | Contents |
|-----|----------|
| `PHU-SPLITTER-windows-x64-<version>.zip` | `PHU SPLITTER.vst3` |
| `PHU-SPLITTER-windows-arm64-<version>.zip` | `PHU SPLITTER.vst3` |
| `PHU-SPLITTER-linux-x64-<version>.zip` | `PHU SPLITTER.vst3` |
| `PHU-SPLITTER-macos-arm64-<version>.zip` | `PHU SPLITTER.vst3`, `PHU SPLITTER.component` |
| `PHU-SPLITTER-macos-x64-<version>.zip` | `PHU SPLITTER.vst3`, `PHU SPLITTER.component` |

## Testing the Workflow

1. Run the **Release** workflow manually with a test tag such as `v0.0.1-test` (it is marked as pre-release)
2. Verify all jobs complete successfully
3. Check that the release artifacts are correct (plugin ZIPs, not .lib/.exp files)
4. Delete the test release and tag if satisfied

## Troubleshooting

### Release workflow failed
1. Check the workflow logs in the Actions tab
2. Fix the problem and re-run: for a tag push, re-run the failed jobs; or run the workflow manually with the tag itself selected under **Use workflow from**
3. You don't need to delete and recreate the tag

### Tag already exists at a different commit
Run the workflow with that tag selected under **Use workflow from**, or choose a new tag.

### Workflow doesn't appear in Actions UI
- Ensure the workflow file is on the default branch (main)
- Check that the YAML syntax is valid
- The workflow_dispatch trigger allows manual runs from any branch
