#!/bin/sh

set -e

OSTYPE=$(uname)

if [ "${OSTYPE}" != "Darwin" ]; then
    echo "[obscamplugin - Error] macOS package script can be run on Darwin-type OS only."
    exit 1
fi


echo "[obscamplugin] Preparing package build"

GIT_HASH=$(git rev-parse --short HEAD)
GIT_BRANCH_OR_TAG=$(git name-rev --name-only HEAD | awk -F/ '{print $NF}')

VERSION="$GIT_HASH-$GIT_BRANCH_OR_TAG"
LATEST_VERSION="$GIT_BRANCH_OR_TAG"

FILENAME_UNSIGNED="obs-iDevice-cam-source-$VERSION-Unsigned.pkg"
FILENAME="obs-iDevice-cam-source-$VERSION.pkg"

echo "-- Modifying obs-iDevice-cam-source.so"
install_name_tool \
	-change /usr/local/opt/ffmpeg/lib/libavcodec.58.dylib @rpath/libavcodec.58.dylib \
	-change /usr/local/opt/ffmpeg/lib/libavutil.56.dylib @rpath/libavutil.56.dylib \
	-change /tmp/obsdeps/bin/libavcodec.58.dylib @rpath/libavcodec.58.dylib \
	-change /tmp/obsdeps/bin/libavutil.56.dylib @rpath/libavutil.56.dylib \
	./build/obs-iDevice-cam-source.so

echo "-- Dependencies for obs-iDevice-cam-source"
otool -L ./build/obs-iDevice-cam-source.so

if [[ "$RELEASE_MODE" == "True" ]]; then
	echo "-- Signing plugin binary: obs-iDevice-cam-source.so"
	codesign --sign "$CODE_SIGNING_IDENTITY" ./build/obs-iDevice-cam-source.so
else
	echo "-- Skipped plugin codesigning"
fi

echo "-- Actual package build"
packagesbuild ./CI/macos/obs-iDevice-cam-source.pkgproj

echo "-- Renaming obs-iDevice-cam-source.pkg to $FILENAME_UNSIGNED"
# mkdir release
mv ./release/obs-iDevice-cam-source.pkg ./release/$FILENAME_UNSIGNED


if [[ "$RELEASE_MODE" == "True" ]]; then
	echo "[obs-iDevice-cam-source] Signing installer: $FILENAME"
	productsign \
		--sign "$INSTALLER_SIGNING_IDENTITY" \
		./release/$FILENAME_UNSIGNED \
		./release/$FILENAME

	echo "[obs-iDevice-cam-source] Submitting installer $FILENAME for notarization"
	zip -r ./release/$FILENAME.zip ./release/$FILENAME
	UPLOAD_RESULT=$(xcrun altool \
		--notarize-app \
		--primary-bundle-id "com.vu.obs-iDevice-cam-source.pkg" \
		--username "$AC_USERNAME" \
		--password "$AC_PASSWORD" \
		--asc-provider "$AC_PROVIDER_SHORTNAME" \
		--file "./release/$FILENAME.zip")
	rm ./release/$FILENAME.zip

	REQUEST_UUID=$(echo $UPLOAD_RESULT | awk -F ' = ' '/RequestUUID/ {print $2}')
	echo "Request UUID: $REQUEST_UUID"

	echo "[obs-iDevice-cam-source] Wait for notarization result"
	# Pieces of code borrowed from rednoah/notarized-app
	while sleep 30 && date; do
		CHECK_RESULT=$(xcrun altool \
			--notarization-info "$REQUEST_UUID" \
			--username "$AC_USERNAME" \
			--password "$AC_PASSWORD" \
			--asc-provider "$AC_PROVIDER_SHORTNAME")
		echo $CHECK_RESULT

		if ! grep -q "Status: in progress" <<< "$CHECK_RESULT"; then
			echo "[obs-iDevice-cam-source] Staple ticket to installer: $FILENAME"
			xcrun stapler staple ./release/$FILENAME
			break
		fi
	done
else
	echo "[obs-iDevice-cam-source] Skipped installer codesigning and notarization"
fi