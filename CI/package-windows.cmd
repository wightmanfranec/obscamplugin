mkdir package
cd package

git rev-parse --short HEAD > package-version.txt
set /p PackageVersion=<package-version.txt
del package-version.txt

REM Package ZIP archive
7z a "obs-iDevice-cam-source-%PackageVersion%-Windows.zip" "..\release\*"

REM Build installer
iscc ..\installer\installer.iss /O. /F"obs-iDevice-cam-source-%PackageVersion%-Windows-Installer"
