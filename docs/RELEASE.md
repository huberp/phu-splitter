# Release Process

This document describes how to create releases for phu-splitter.

## Automatic Release (Tag Push)

When you push a tag starting with `v` (e.g., `v1.0.0`), the release workflow automatically:

1. Builds the VST3 plugin
2. Packages it as a ZIP file
3. Creates a GitHub release with auto-generated release notes
4. Uploads the plugin ZIP to the release

**Example:**
```bash
git tag v1.0.0
git push origin v1.0.0
```

## Manual Release (Workflow Dispatch)

If a release workflow fails or you need to re-run it for an existing tag:

1. Go to **Actions** → **Release** workflow
2. Click **Run workflow**
3. Fill in the inputs:
   - **Tag**: The tag to create/update release for (e.g., `v1.0.0`)
   - **Draft**: Check this to create a draft release that you can edit before publishing
4. Click **Run workflow**

This allows you to:
- Re-run a failed release without creating a new tag
- Create releases for existing tags
- Create draft releases for review before publishing

## Pre-release Detection

The workflow automatically marks releases as pre-release if the tag contains:
- `alpha` (e.g., `v1.0.0-alpha.1`)
- `beta` (e.g., `v1.0.0-beta.2`)
- `rc` (e.g., `v1.0.0-rc.1`)

## Release Artifacts

Each release includes a ZIP file containing the complete VST3 plugin folder, ready to install.

The ZIP filename is based on the plugin name (e.g., `PHU SPLITTER.zip`).

## Troubleshooting

### Release workflow failed
1. Check the workflow logs in the Actions tab
2. If the build succeeded but release creation failed, use **workflow dispatch** to re-run
3. You don't need to delete and recreate the tag

### Wrong artifacts uploaded
The workflow now packages only the VST3 plugin folder, not intermediate build files (.lib, .exp).
Re-run the workflow using workflow dispatch to upload correct artifacts.

### Cannot trigger workflow again
Use the **workflow dispatch** feature described above - you don't need a new tag.
