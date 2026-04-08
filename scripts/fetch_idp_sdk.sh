#!/usr/bin/env sh

set -eu

SDK_ROOT="${1:-.deps/idp-sdk}"
RELEASE_API="https://api.github.com/repos/iskra-delta/idp-sdk/releases/latest"

tmpdir="$(mktemp -d)"
cleanup() {
	rm -rf "$tmpdir"
}
trap cleanup EXIT INT TERM

release_json="$tmpdir/release.json"
curl -fsSL "$RELEASE_API" -o "$release_json"

version="$(sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"v\([^"]*\)".*/\1/p' "$release_json" | head -n 1)"
asset_url="$(sed -n 's/.*"browser_download_url"[[:space:]]*:[[:space:]]*"\([^"]*idp-sdk-[^"]*\.tar\.gz\)".*/\1/p' "$release_json" | head -n 1)"

if [ -z "$version" ] || [ -z "$asset_url" ]; then
	echo "failed to resolve latest idp-sdk release metadata" >&2
	exit 1
fi

target_dir="$SDK_ROOT/$version"
current_link="$SDK_ROOT/current"

mkdir -p "$SDK_ROOT"

if [ -f "$target_dir/lib/crt0cpm3-z80.rel" ] && \
   [ -f "$target_dir/lib/libsdk.lib" ] && \
   [ -f "$target_dir/lib/libcpm3-z80.lib" ] && \
   [ -f "$target_dir/lib/libsdcc-z80.lib" ] && \
   [ -d "$target_dir/include" ]; then
	ln -sfn "$version" "$current_link"
	echo "idp-sdk $version already present"
	exit 0
fi

archive_path="$tmpdir/idp-sdk-$version.tar.gz"
download_dir="$tmpdir/unpack"

echo "downloading idp-sdk $version"
curl -fsSL "$asset_url" -o "$archive_path"
mkdir -p "$download_dir"
tar -xzf "$archive_path" -C "$download_dir"

archive_root="$(find "$download_dir" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
if [ -z "$archive_root" ] || \
   [ ! -f "$archive_root/lib/crt0cpm3-z80.rel" ] || \
   [ ! -f "$archive_root/lib/libsdk.lib" ] || \
   [ ! -f "$archive_root/lib/libcpm3-z80.lib" ] || \
   [ ! -f "$archive_root/lib/libsdcc-z80.lib" ] || \
   [ ! -d "$archive_root/include" ]; then
	echo "downloaded archive does not contain expected idp-sdk layout" >&2
	exit 1
fi

rm -rf "$target_dir"
mv "$archive_root" "$target_dir"
ln -sfn "$version" "$current_link"
echo "$asset_url" > "$target_dir/.download-url"
echo "idp-sdk $version ready at $target_dir"
