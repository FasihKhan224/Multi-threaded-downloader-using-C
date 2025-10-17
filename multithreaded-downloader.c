include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include <openssl/sha.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

typedef struct {
char *url;
char *filename;
long start;
long end;
int thread_id;
int result;
} DownloadTask;

typedef struct {
char *ptr;
size_t len;
} MemoryStruct;

// Global variables for thread synchronization
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
bool download_failed = false;

// Callback function for libcurl to write data to file
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *userdata) {
DownloadTask *task = (DownloadTask *)userdata;
size_t written;

pthread_mutex_lock(&file_mutex);

if (download_failed) {
pthread_mutex_unlock(&file_mutex);
return 0;
}

FILE *fp = fopen(task->filename, "r+b");
if (!fp) {
perror("Failed to open file");
download_failed = true;
pthread_mutex_unlock(&file_mutex);
return 0;
}

fseek(fp, task->start, SEEK_SET);
written = fwrite(ptr, size, nmemb, fp);
fclose(fp);

pthread_mutex_unlock(&file_mutex);

return written;
}

// Thread function to download a chunk
void *download_chunk(void *arg) {
DownloadTask *task = (DownloadTask *)arg;
CURL *curl;
CURLcode res;

printf("Thread %d: starting download of bytes %ld-%ld\n",
task->thread_id, task->start, task->end);

curl = curl_easy_init();
if (curl) {
char range_header[50];
snprintf(range_header, sizeof(range_header), "%ld-%ld", task->start, task->end);

curl_easy_setopt(curl, CURLOPT_URL, task->url);
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, task);
curl_easy_setopt(curl, CURLOPT_RANGE, range_header);
curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

res = curl_easy_perform(curl);
if (res != CURLE_OK) {
fprintf(stderr, "Thread %d: curl_easy_perform() failed: %s\n",
task->thread_id, curl_easy_strerror(res));
task->result = 1;
download_failed = true;
} else {
printf("Thread %d: finished download\n", task->thread_id);
task->result = 0;
}

curl_easy_cleanup(curl);
} else {
task->result = 1;
download_failed = true;
}

return NULL;
}

// Check if server supports range requests
bool supports_range_requests(const char *url) {
CURL *curl = curl_easy_init();
CURLcode res;
char *accept_ranges = NULL;
bool result = false;

if (curl) {
curl_easy_setopt(curl, CURLOPT_URL, url);
curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request

res = curl_easy_perform(curl);
if (res == CURLE_OK) {
res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, NULL);
if (res != CURLE_OK) {
fprintf(stderr, "Failed to get content length\n");
}

res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, NULL);
if (res == CURLE_OK) {
curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &accept_ranges);
if (accept_ranges && strstr(accept_ranges, "bytes")) {
result = true;
}
}
}

curl_easy_cleanup(curl);
}

return result;
}

// Get file size from URL
long get_file_size(const char *url) {
CURL *curl = curl_easy_init();
CURLcode res;
double file_size = 0;

if (curl) {
curl_easy_setopt(curl, CURLOPT_URL, url);
curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

res = curl_easy_perform(curl);
if (res == CURLE_OK) {
res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &file_size);
if (res != CURLE_OK) {
fprintf(stderr, "Failed to get content length\n");
file_size = 0;
}
}

curl_easy_cleanup(curl);
}

return (long)file_size;
}

// Calculate SHA256 hash of downloaded file
void calculate_sha256(const char *filename) {
FILE *file = fopen(filename, "rb");
if (!file) {
perror("Failed to open file for hash calculation");
return;
}

SHA256_CTX sha256;
SHA256_Init(&sha256);

unsigned char buffer[4096];
size_t bytes_read;

while ((bytes_read = fread(buffer, 1, sizeof(buffer), file))) {
SHA256_Update(&sha256, buffer, bytes_read);
}

fclose(file);

unsigned char hash[SHA256_DIGEST_LENGTH];
SHA256_Final(hash, &sha256);

printf("SHA256 Hash of downloaded file is: ");
for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
printf("%02x", hash[i]);
}
printf("\n");
}

int main(int argc, char *argv[]) {
if (argc < 2 || argc > 4) {
fprintf(stderr, "Usage: %s [--threads N] [--name FILENAME] URL\n", argv[0]);
return 1;
}

// incase the user doesnt specify number of threads and the name for the file default is 2
int num_threads = 2;
char *filename = NULL;
char *url = NULL;

for (int i = 1; i < argc; i++) {
if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
num_threads = atoi(argv[++i]);
// edge case handling
if (num_threads < 1) {
fprintf(stderr, "Thread count must be a positive integer\n");
return 1;
}
} else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
filename = argv[++i];
} else { // if no name is provided then it will simply name it the url
url = argv[i];
}
}

if (!url) {
fprintf(stderr, "URL is required\n");
return 1;
}

// Initialize libcurl
curl_global_init(CURL_GLOBAL_ALL);

// Check if server supports range requests
bool multi_thread_possible = supports_range_requests(url);
long file_size = get_file_size(url);

if (file_size <= 0) {
fprintf(stderr, "Failed to get file size or invalid URL\n");
curl_global_cleanup();
return 1;
}

// Determine filename if not provided
if (!filename) {
const char *last_slash = strrchr(url, '/');
if (last_slash) {
filename = strdup(last_slash + 1);
} else {
filename = strdup("downloaded_file");
}
printf("Filename not provided. File will be named: %s\n", filename);
}

// Single-threaded download if range not supported or only 1 thread requested
if (!multi_thread_possible || num_threads == 1) {
printf("Starting download in single-thread mode\n");

DownloadTask task = {
.url = url,
.filename = filename,
.start = 0,
.end = file_size,
.thread_id = 0,
.result = 0
};

// Create empty file of the correct size
FILE *fp = fopen(filename, "wb");
if (fp) {
fseek(fp, file_size - 1, SEEK_SET);
fputc(0, fp);
fclose(fp);
} else {
perror("Failed to create file");
curl_global_cleanup();
return 1;
}

download_chunk(&task);

if (task.result != 0) {
fprintf(stderr, "Download failed\n");
curl_global_cleanup();
return 1;
}
} else {
// Multi-threaded download
printf("Starting download in multi-threaded mode with %d threads\n", num_threads);

// Create empty file of the correct size
FILE *fp = fopen(filename, "wb");
if (fp) {
fseek(fp, file_size - 1, SEEK_SET);
fputc(0, fp);
fclose(fp);
} else {
perror("Failed to create file");
curl_global_cleanup();
return 1;
}

pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
DownloadTask *tasks = malloc(num_threads * sizeof(DownloadTask));
long chunk_size = file_size / num_threads;

// Create download tasks
for (int i = 0; i < num_threads; i++) {
tasks[i].url = url;
tasks[i].filename = filename;
tasks[i].start = i * chunk_size;
tasks[i].end = (i == num_threads - 1) ? file_size : (i + 1) * chunk_size - 1;
tasks[i].thread_id = i;
tasks[i].result = 0;
}

// Start threads
for (int i = 0; i < num_threads; i++) {
if (pthread_create(&threads[i], NULL, download_chunk, &tasks[i])) {
perror("Failed to create thread");
free(threads);
free(tasks);
curl_global_cleanup();
return 1;
}
}

// Wait for threads to finish
for (int i = 0; i < num_threads; i++) {
pthread_join(threads[i], NULL);
}

// Check for errors
for (int i = 0; i < num_threads; i++) {
if (tasks[i].result != 0) {
fprintf(stderr, "Download failed\n");
free(threads);
free(tasks);
curl_global_cleanup();
return 1;
}
}

free(threads);
free(tasks);
}

printf("%s downloaded successfully\n", filename);
calculate_sha256(filename);

curl_global_cleanup();
return 0;
}
