#!/bin/bash
#
# publish.sh — Build and publish hermes-android-wrapper
#
# Usage:
#   ./publish.sh local       # Publish to local repo (build/maven-repo/) for testing
#   ./publish.sh [version]   # Override version (e.g., ./publish.sh local 0.2.0)
#
# For public release via JitPack:
#   1. Tag a release: git tag v0.1.0 && git push origin v0.1.0
#   2. Visit: https://jitpack.io/#walid1992/hermes-android
#   3. Users add:
#        repositories { maven { url 'https://jitpack.io' } }
#        implementation 'com.github.walid1992.hermes-android:wrapper-android:v0.1.0'
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
    *)
        # If first arg looks like a version, assume local + version override
        if [[ "$TARGET" =~ ^[0-9]+\.[0-9]+\.[0-9]+ ]]; then
            VERSION_OVERRIDE="$TARGET"
            TARGET="local"
            REPO_NAME="local"
        else
            error "Unknown target: $TARGET. Use 'local'"
        fi
        ;;
esac

# Version override
if [ -n "$VERSION_OVERRIDE" ]; then
    info "Overriding version to: $VERSION_OVERRIDE"
    sed -i '' "s/versionName = '.*'/versionName = '$VERSION_OVERRIDE'/" build.gradle
fi

# Read current version
CURRENT_VERSION=$(grep "versionName = " build.gradle | head -1 | sed "s/.*versionName = '\(.*\)'/\1/")
GROUP_ID=$(grep "groupId = " build.gradle | head -1 | sed "s/.*groupId = '\(.*\)'/\1/")

info "Publishing hermes-android-wrapper v${CURRENT_VERSION}"
info "  Group: ${GROUP_ID}"
info "  Target: ${TARGET}"
echo ""

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

echo ""
info "For public release via JitPack:"
echo "  1. git tag v${CURRENT_VERSION}"
echo "  2. git push origin v${CURRENT_VERSION}"
echo "  3. Visit https://jitpack.io/#walid1992/hermes-android"
echo ""
echo "  Users add:"
echo "    repositories { maven { url 'https://jitpack.io' } }"
echo "    implementation 'com.github.walid1992.hermes-android:wrapper-android:v${CURRENT_VERSION}'"
