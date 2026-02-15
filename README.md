# brain-cv-utils

A Brain module application built with the brain-sdk.

## Build

```bash
mkdir build
cd build
cmake ..
make
```

## Flash

Flash the generated `brain-cv-utils.uf2` file to your Brain module by holding the BOOTSEL button while connecting it to your computer, then copy the .uf2 file to the mounted drive.

## Development

This project includes brain-sdk as a git submodule. To update the SDK:

```bash
cd brain-sdk
git pull origin main
cd ..
git add brain-sdk
git commit -m "Update brain-sdk"
```
