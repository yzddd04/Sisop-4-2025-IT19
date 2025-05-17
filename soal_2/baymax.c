#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <curl/curl.h>
#include <time.h>
#include <stdarg.h>

#define RELICS_DIR "relics"
#define LOG_FILE "activity.log"
#define BAYMAX_FILE "Baymax.jpeg"
#define PARTS 14
#define PART_SIZE 1024

const char *gdrive_url = "https://drive.google.com/uc?export=download&id=1MHVhFT57Wa9Zcx69Bv9j5ImHc9rXMH1c";

struct file_buffer {
    char *data;
    size_t size;
};
static struct file_buffer write_buf = {NULL, 0};
static char write_filename[256] = "";

void log_activity(const char *fmt, ...) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log, "[%04d-%02d-%02d %02d:%02d:%02d] ",
        t->tm_year+1900, t->tm_mon+1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec);
    va_list args;
    va_start(args, fmt);
    vfprintf(log, fmt, args);
    va_end(args);
    fprintf(log, "\n");
    fclose(log);
}

// Download file pecahan jika belum ada
void download_parts_if_needed() {
    struct stat st = {0};
    if (stat(RELICS_DIR, &st) == -1) {
        mkdir(RELICS_DIR, 0755);
    }
    char part_path[256];
    int need_download = 0;
    for (int i = 0; i < PARTS; i++) {
        snprintf(part_path, sizeof(part_path), "%s/%s.%03d", RELICS_DIR, BAYMAX_FILE, i);
        if (access(part_path, F_OK) == -1) {
            need_download = 1;
            break;
        }
    }
    if (!need_download) return;

    // Download zip dari Google Drive
    system("wget -O relics.zip 'https://drive.google.com/uc?export=download&id=1MHVhFT57Wa9Zcx69Bv9j5ImHc9rXMH1c'");
    system("unzip -o relics.zip -d relics");
    system("rm relics.zip");
}

static int baymax_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(path, "/" BAYMAX_FILE) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = PARTS * PART_SIZE;
    } else {
        return -ENOENT;
    }
    log_activity("getattr: %s\n", path);
    return 0;
}

static int baymax_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/") != 0)
        return -ENOENT;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, BAYMAX_FILE, NULL, 0);
    log_activity("readdir: %s\n", path);
    return 0;
}

static int baymax_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/" BAYMAX_FILE) != 0)
        return -ENOENT;
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EACCES;
    log_activity("open: %s\n", path);
    return 0;
}

static int baymax_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/" BAYMAX_FILE) != 0)
        return -ENOENT;
    if (offset >= PARTS * PART_SIZE)
        return 0;
    if (offset + size > PARTS * PART_SIZE)
        size = PARTS * PART_SIZE - offset;

    size_t bytes_read = 0;
    size_t start_part = offset / PART_SIZE;
    size_t start_offset = offset % PART_SIZE;
    size_t remain = size;

    for (size_t i = start_part; i < PARTS && remain > 0; i++) {
        char part_path[256];
        snprintf(part_path, sizeof(part_path), "%s/%s.%03zu", RELICS_DIR, BAYMAX_FILE, i);
        FILE *f = fopen(part_path, "rb");
        if (!f) return -EIO;
        fseek(f, (i == start_part) ? start_offset : 0, SEEK_SET);
        size_t to_read = PART_SIZE - ((i == start_part) ? start_offset : 0);
        if (to_read > remain) to_read = remain;
        fread(buf + bytes_read, 1, to_read, f);
        fclose(f);
        bytes_read += to_read;
        remain -= to_read;
    }
    if (offset == 0)
        log_activity("READ: %s", path+1);
    log_activity("read: %s offset=%ld size=%zu\n", path, offset, size);
    return bytes_read;
}

static int baymax_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    // Simpan nama file yang akan ditulis
    snprintf(write_filename, sizeof(write_filename), "%s", path+1); // skip '/'
    if (write_buf.data) free(write_buf.data);
    write_buf.data = NULL;
    write_buf.size = 0;
    log_activity("WRITE: %s (create)\n", write_filename);
    return 0;
}

static int baymax_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Simpan data ke buffer sementara
    if (strcmp(path+1, write_filename) != 0) return -EIO;
    if (offset + size > write_buf.size) {
        write_buf.data = realloc(write_buf.data, offset + size);
        write_buf.size = offset + size;
    }
    memcpy(write_buf.data + offset, buf, size);
    return size;
}

static int baymax_release(const char *path, struct fuse_file_info *fi) {
    if (write_buf.data && write_buf.size > 0) {
        char log_parts[1024] = "";
        size_t written = 0;
        for (int part = 0; written < write_buf.size; part++) {
            char part_path[256];
            snprintf(part_path, sizeof(part_path), "%s/%s.%03d", RELICS_DIR, write_filename, part);
            FILE *f = fopen(part_path, "wb");
            size_t to_write = (write_buf.size - written > PART_SIZE) ? PART_SIZE : (write_buf.size - written);
            fwrite(write_buf.data + written, 1, to_write, f);
            fclose(f);
            char part_name[64];
            snprintf(part_name, sizeof(part_name), "%s.%03d", write_filename, part);
            if (part > 0) strcat(log_parts, ", ");
            strcat(log_parts, part_name);
            written += to_write;
        }
        log_activity("WRITE: %s -> %s", write_filename, log_parts);
        free(write_buf.data);
        write_buf.data = NULL;
        write_buf.size = 0;
        write_filename[0] = 0;
    }
    return 0;
}

static int baymax_unlink(const char *path) {
    char fname[256];
    snprintf(fname, sizeof(fname), "%s", path+1);
    char log_parts[1024] = "";
    int first = 1;
    for (int i = 0; ; i++) {
        char part_path[256];
        snprintf(part_path, sizeof(part_path), "%s/%s.%03d", RELICS_DIR, fname, i);
        if (access(part_path, F_OK) == -1) break;
        remove(part_path);
        char part_name[64];
        snprintf(part_name, sizeof(part_name), "%s.%03d", fname, i);
        if (!first) strcat(log_parts, " - ");
        strcat(log_parts, part_name);
        first = 0;
    }
    log_activity("DELETE: %s", log_parts);
    return 0;
}

static struct fuse_operations baymax_oper = {
    .getattr = baymax_getattr,
    .readdir = baymax_readdir,
    .open    = baymax_open,
    .read    = baymax_read,
    .create  = baymax_create,
    .write   = baymax_write,
    .release = baymax_release,
    .unlink  = baymax_unlink,
};

int main(int argc, char *argv[]) {
    download_parts_if_needed();
    return fuse_main(argc, argv, &baymax_oper, NULL);
}
