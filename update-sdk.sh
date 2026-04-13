#!/bin/bash

# update-sdk.sh - Update the brain-sdk submodule to the latest version from GitHub
# Usage: ./update-sdk.sh [--push]
#   --push: Automatically commit and push the update

set -e

PUSH=false
if [[ "$1" == "--push" ]]; then
  PUSH=true
fi

SDK_BRANCH="${SDK_BRANCH:-2.0}"

echo "Updating brain-sdk submodule..."
echo ""

cd brain-sdk
git fetch origin
git checkout "$SDK_BRANCH"
git pull origin "$SDK_BRANCH"
cd ..

echo ""
echo "✓ brain-sdk updated to latest version on branch ${SDK_BRANCH}"
echo ""

if [ "$PUSH" = true ]; then
  echo "Committing and pushing update..."
  git add brain-sdk
  git commit -m "Updated brain-sdk"
  git push
  echo ""
  echo "✓ Changes committed and pushed"
else
  echo "Don't forget to commit the update:"
  echo "  git add brain-sdk"
  echo "  git commit -m \"Updated brain-sdk\""
  echo "  git push"
  echo ""
  echo "Or run: ./update-sdk.sh --push"
fi
