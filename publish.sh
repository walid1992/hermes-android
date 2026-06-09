#!/bin/bash
#
# publish.sh — Build and publish hermes-android-wrapper to Maven
#
# Usage:
#   ./publish.sh local       # Publish to local repo (build/maven-repo/) for testing
#   ./publish.sh github      # Publish to GitHub Packages
#   ./publish.sh [version]   # Override version (e.g., ./publish.sh github 0.2.0)
#
# Prerequisites for GitHub Packages:
#   export GITHUB_ACTOR=walid1992
#   export GITHUB_TOKEN=ghp_xxxxx   (needs write:packages scope)
#
#   Or set in ~/.gradle/gradle.properties:
#     gpr.user=walid1992
#     gpr.key=ghp_xxxxx
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Parse arguments
TARGET="${1:-local}"
VERSION_OVERRIDE="${2:-}"

# Validate target
case "$TARGET" in
    local)   REPO_NAME="local" ;;
    github)  REPO_NAME="GitHubPackages" ;;
    *)
        # If first arg looks like a version, assume local + version override
        if [[ "$TARGET" =~ ^[0-9]+\.[0-9]+\.[0-9]+ ]]; then
            VERSION_OVERRIDE="$TARGET"
            TARGET="local"
            REPO_NAME="local"
        else
            error "Unknown target: $TARGET. Use 'local' or 'github'"
        fi
        ;;
esac

# Version override
if [ -n "$VERSION_OVERRIDE" ]; then
    info "Overriding version to: $VERSION_OVERRIDE"
    sed -i '' "s/versionName = '.*'/versionName = '$VERSION_OVERRIDE'/" build.gradle
fi

# Read current version
CURRENT_VERSION=$(grep "versionName = " build.gradle | head -1 | sed "s/.*versionName = '\\(.*\\)'/\\1/")
GROUP_ID=$(grep "groupId = " build.gradle | head -1 | sed "s/.*groupId = '\\(.*\\)'/\\1/")

info "Publishing hermes-android-wrapper v${CURRENT_VERSION}"
info "  Group: ${GROUP_ID}"
info "  Target: ${TARGET}"
echo ""

# Check GitHub credentials if needed
if [ "$TARGET" = "github" ]; then
    if [ -z "${GITHUB_TOKEN:-}" ] && ! grep -q "gpr.key" ~/.gradle/gradle.properties 2>/dev/null; then
        error "GITHUB_TOKEN not set. Export it or add gpr.key to ~/.gradle/gradle.properties"
    fi
    info "GitHub credentials: OK"
fi

# Clean build
info "Cleaning previous build..."
./gradlew clean

# Build release AARs
info "Building release AARs..."
./gradlew :hermes-engine:assembleRelease :wrapper-android:assembleRelease

# Publish hermes-engine first (wrapper-android depends on it)
info "Publishing hermes-engine..."
./gradlew :hermes-engine:publishReleasePublicationTo${REPO_NAME}Repository

# Publish wrapper-android
info "Publishing wrapper-android..."
./gradlew :wrapper-android:publishReleasePublicationTo${REPO_NAME}Repository

echo ""
info "✅ Published successfully!"
echo ""

if [ "$TARGET" = "local" ]; then
    LOCAL_REPO="$SCRIPT_DIR/build/maven-repo"
    info "Local repo: $LOCAL_REPO"
    echo ""
    echo "  To use in another project, add to build.gradle:"
    echo ""
    echo "    repositories {"
    echo "        maven { url '$LOCAL_REPO' }"
    echo "    }"
    echo ""
    echo "    dependencies {"
    echo "        implementation '${GROUP_ID}:wrapper-android:${CURRENT_VERSION}'"
    echo "        // hermes-engine is pulled transitively"
    echo "    }"
    echo ""
    # Show published files
    info "Published artifacts:"
    find "$LOCAL_REPO" -name "*.aar" -o -name "*.pom" | sort | while read f; do
        echo "    $(basename "$f")"
    done
else
    echo "  Maven coordinates:"
    echo ""
    echo "    repositories {"
    echo "        maven {"
    echo "            url 'https://maven.pkg.github.com/walid1992/hermes-android'"
    echo "            credentials {"
    echo "                username = project.findProperty('gpr.user') ?: System.getenv('GITHUB_ACTOR')"
    echo "                password = project.findProperty('gpr.key') ?: System.getenv('GITHUB_TOKEN')"
    echo "            }"
    echo "        }"
    echo "    }"
    echo ""
    echo "    dependencies {"
    echo "        implementation '${GROUP_ID}:wrapper-android:${CURRENT_VERSION}'"
    echo "    }"
fi
