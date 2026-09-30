# Poor Man's Catalog (creator)

I have been looking for the CD/Path catalog creators. But they are all too old to be used. Almost all of them were unmaintained for a long time and compiling them would be a big hassle. So, I decided to use this opportunity to do something with the QT Creator.

I have bunch of external harddrives, old archive DVDs and SDCards or FlashDrives and I often need to go through them to find what I am looking for.

Instead, now I use this tool now to create an index/catalog of those drives.

There are probably some parts that I totally messed up and the code might need some facelifting. In my defense, I created this for myself.

## Download

I have created a 64bit Deb package and a Win64 version here: https://github.com/sinanislekdemir/poorman/releases/tag/v1.2.1 

<img width="1144" height="671" alt="image" src="https://github.com/user-attachments/assets/54b91a25-cbf6-4ffc-9efc-b8517fab60fb" />

## Notes

1. You need QT Creator (and obviously QT 5.x >) to compile and run this.
2. I am planning to create AppImage but I am too lazy to learn it. (Don't blame me, I'm a backend developer and a cli junkie)
3. I take no responsibility.
4. Feel free to create PRs. I always welcome them.

By default, it uses `~/poorman.sqlite` file but you can create multiple SQLite files.

## Building Packages

### Quick Package Build

Run the automated packaging script:

```bash
./package.sh
```

This will create both:
- **AppImage**: `PoorMansCatalog-1.2.1-x86_64.AppImage`
- **DEB package**: `poormanscatalog_1.2.1_amd64.deb`

> **Important:** An AppImage inherits the glibc of the machine it is built on.
> Build it on the *oldest still-supported Ubuntu LTS* (currently 20.04) so it
> runs on all supported distributions. Releases are built automatically by
> `.github/workflows/release.yml` inside an Ubuntu 20.04 container.

### AppImage-only build (glibc compatible)

`scripts/build-appimage.sh` builds just the AppImage. Run it in an Ubuntu 20.04
container so the result only depends on an old glibc:

```bash
podman run --rm -v "$PWD:/work:Z" -w /work \
  -e INSTALL_DEPS=1 -e VERSION=1.2.1 \
  ubuntu:20.04 bash scripts/build-appimage.sh
```

### Requirements

Install build dependencies:

```bash
sudo apt install qt5-qmake build-essential equivs wget
```

### Manual Build

#### AppImage

```bash
./scripts/build-appimage.sh
```

Run it on Ubuntu 20.04 (or in the container shown above) to keep the AppImage
compatible with older systems. The script downloads linuxdeploy and its Qt
plugin, assembles the `AppDir` and produces `PoorMansCatalog-<version>-x86_64.AppImage`.

#### DEB Package

```bash
qmake && make
equivs-build package.conf
```

### Installation

**AppImage:**
```bash
chmod +x PoorMansCatalog-1.2.1-x86_64.AppImage
./PoorMansCatalog-1.2.1-x86_64.AppImage
```

**DEB:**
```bash
sudo dpkg -i poormanscatalog_1.2.1_amd64.deb
sudo apt install -f  # Fix dependencies if needed
```
