**C-Downloader 🚀**
A lightweight, multi-threaded file downloader built in C for Linux/Unix systems. It's designed to speed up downloads by splitting files into chunks and fetching them in parallel.

TL;DR: It's like a basic wget but on steroids, using multiple threads to download files faster. 🏎️


**✨ Features**
Multi-threaded Downloading: Uses pthreads to download different parts of a file simultaneously.

**Smart Fallback:** Automatically switches to a single-threaded mode if the server doesn't support partial downloads (range requests).

**Customizable:** You can specify the number of threads and the output filename.

**File Integrity Check:** Automatically calculates and displays the SHA256 hash of the downloaded file so you can verify it's not corrupted.

Minimal Dependencies: Built with standard C libraries, libcurl, and OpenSSL.

**🛠️ Getting Started**
**Prerequisites**
Before you can compile the project, you'll need to have a few things installed on your system:

gcc (or any other C compiler)

libcurl development library

OpenSSL development library (libcrypto)
**
You can install these on a Debian/Ubuntu system with:**

**Bash**
sudo apt-get update
sudo apt-get install build-essential libcurl4-openssl-dev libssl-dev
Compilation
Compiling is super simple. Just run this command in your terminal:

**Bash**

gcc cproject.c -o cdownloader -lcurl -lpthread -lssl -lcrypto
This command does the following:

gcc cproject.c: Compiles your C code.

-o cdownloader: Names the output executable cdownloader.

-lcurl: Links the libcurl library for handling HTTP requests.

-lpthread: Links the POSIX threads library for multi-threading.

-lssl -lcrypto: Links the OpenSSL libraries needed for the SHA256 hash calculation.

**💻 How to Use**
The basic syntax is to provide the URL of the file you want to download.

**Bash**
./cdownloader [OPTIONS] <URL>

**Options**
--threads N: Specifies the number of threads to use for the download. Defaults to 2.

--name FILENAME: Sets the name for the downloaded file. If you don't use this, it will try to guess the name from the URL.
**
Examples**

1. Basic Download This will download the file using the default 2 threads and name it ubuntu-22.04.3-desktop-amd64.iso.

**Bash**
./cdownloader https://releases.ubuntu.com/22.04/ubuntu-22.04.3-desktop-amd64.iso

2. Download with 8 Threads Boost your download speed by using more threads. ⚡

**Bash**
./cdownloader --threads 8 https://releases.ubuntu.com/22.04/ubuntu-22.04.3-desktop-amd64.iso

3. Download with a Custom Name Save the file with a specific name.

**Bash**
./cdownloader --name my-ubuntu.iso https://releases.ubuntu.com/22.04/ubuntu-22.04.3-desktop-amd64.iso

4. All Options Combined Go full custom mode.

**Bash**
./cdownloader --threads 16 --name my-ubuntu.iso https://releases.ubuntu.com/22.04/ubuntu-22.04.3-desktop-amd64.iso
