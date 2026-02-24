# Poor Man's Catalog (creator)

I have been looking for the CD/Path catalog creators. But they are all too old to be used. Almost all of them were unmaintained for a long time and compiling them would be a big hassle. So, I decided to use this opportunity to do something with the QT Creator.

I have bunch of external harddrives, old archive DVDs and SDCards or FlashDrives and I often need to go through them to find what I am looking for.

Instead, now I use this tool now to create an index/catalog of those drives.

There are probably some parts that I totally messed up and the code might need some facelifting. In my defense, I created this for myself.

## Download

I have created a 64bit Deb package and a Win64 version here: https://github.com/sinanislekdemir/poorman/releases/tag/v1.0.6 

![poorman](https://www.16x4.com/content/images/poorman1.jpg)

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
- **AppImage**: `PoorMansCatalog-1.0.6-x86_64.AppImage`
- **DEB package**: `poormanscatalog_1.0.6_amd64.deb`

### Requirements

Install build dependencies:

```bash
sudo apt install qt5-qmake build-essential equivs wget
```

### Manual Build

#### AppImage

```bash
qmake && make
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage
./linuxdeploy-x86_64.AppImage --appdir AppDir --executable PoorMansCatalog --plugin qt --output appimage
```

#### DEB Package

```bash
qmake && make
equivs-build package.conf
```

### Installation

**AppImage:**
```bash
chmod +x PoorMansCatalog-1.0.6-x86_64.AppImage
./PoorMansCatalog-1.0.6-x86_64.AppImage
```

**DEB:**
```bash
sudo dpkg -i poormanscatalog_1.0.6_amd64.deb
sudo apt install -f  # Fix dependencies if needed
```
