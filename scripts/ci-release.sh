#!/bin/sh
# On a tag: check that the tag, src/version.h and metadata.json say the same
# version, then make the Gitea release (RELEASE_NOTES.md as its text) with
# warp64u-<version>.prg, and publish the same file to the WarpEngine of
# teletypegames.org (platform c64), which puts it on the C64 shelf.
#
#   GITEA_TOKEN=... UPDATE_SECRET=... CI_COMMIT_TAG=v0.1.0 CI_REPO=stores/warp64u scripts/ci-release.sh
set -eu

TAG="${CI_COMMIT_TAG:?a tag}"
VERSION="${TAG#v}"
API="${GITEA_API:-https://git.teletypegames.org/api/v1}"
UPDATE_SERVER="${UPDATE_SERVER:-https://teletypegames.org}"

say() { echo "[release] $*"; }
die() { echo "[release] error: $*" >&2; exit 1; }

HEADER_VERSION="$(sed -n 's/.*WARP64U_VERSION "\(.*\)".*/\1/p' src/version.h)"
META_VERSION="$(jq -r .version metadata.json)"
[ "$HEADER_VERSION" = "$VERSION" ] || die "src/version.h says $HEADER_VERSION, the tag $TAG"
[ "$META_VERSION" = "$VERSION" ] || die "metadata.json says $META_VERSION, the tag $TAG"
[ -f build/warp64u.prg ] || die "no build/warp64u.prg"

PRG="warp64u-$VERSION.prg"
cp build/warp64u.prg "$PRG"
cp metadata.json "warp64u-$VERSION.metadata.json"

if [ -n "${GITEA_TOKEN:-}" ]; then
  AUTH="Authorization: token $GITEA_TOKEN"
  ID="$(curl -fsS -H "$AUTH" "$API/repos/$CI_REPO/releases/tags/$TAG" 2>/dev/null | jq -r '.id // empty' || true)"
  if [ -z "$ID" ]; then
    say "creating the release WARP64U $TAG"
    ID="$(jq -n --arg tag "$TAG" --arg name "WARP64U $TAG" --rawfile body RELEASE_NOTES.md \
            '{tag_name: $tag, name: $name, body: $body, draft: false, prerelease: false}' |
          curl -fsS -X POST -H "$AUTH" -H "Content-Type: application/json" -d @- "$API/repos/$CI_REPO/releases" |
          jq -r .id)"
  fi
  OLD="$(curl -fsS -H "$AUTH" "$API/repos/$CI_REPO/releases/$ID/assets" | jq -r --arg n "$PRG" '.[] | select(.name == $n) | .id')"
  [ -z "$OLD" ] || curl -fsS -X DELETE -H "$AUTH" "$API/repos/$CI_REPO/releases/$ID/assets/$OLD" >/dev/null
  curl -fsS -X POST -H "$AUTH" -F "attachment=@$PRG" "$API/repos/$CI_REPO/releases/$ID/assets?name=$PRG" >/dev/null
  say "uploaded $PRG to the Gitea release"
else
  say "no GITEA_TOKEN: no Gitea release"
fi

: "${UPDATE_SECRET:?the WarpEngine application token}"
for f in "$PRG" "warp64u-$VERSION.metadata.json"; do
  curl -fsS -H "X-Update-Secret: $UPDATE_SECRET" -F "file=@$f" "$UPDATE_SERVER/build/upload?name=warp64u&version=$VERSION" >/dev/null
done
curl -fsS -X POST -H "X-Update-Secret: $UPDATE_SECRET" "$UPDATE_SERVER/build/publish?name=warp64u&platform=c64&version=$VERSION" >/dev/null
say "published warp64u $VERSION to $UPDATE_SERVER"
