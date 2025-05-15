
#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_PIECES 1000
#define PIECE_SIZE 1024

static const char *relics_dir = "./relics";
static const char *log_file = "./activity.log";

typedef struct {
    char filename[256];
    int pieces;
} virtual_file_t;

static virtual_file_t vfile = {
    .filename = "Baymax.jpeg",
    .pieces = 14
};

static void log_activity(const char *fmt, ...) {
    FILE *logf = fopen(log_file, "a");
    if (!logf) return;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "[%Y-%m-%d %H:%M:%S]", tm);

    fprintf(logf, "%s ", timestr);

    va_list args;
    va_start(args, fmt);
    vfprintf(logf, fmt, args);
    va_end(args);

    fprintf(logf, "\n");
    fclose(logf);
}

static int read_virtual_file(const char *path, char *buf, size_t size, off_t offset) {
    if (strcmp(path + 1, vfile.filename) != 0)
        return -ENOENT;

    size_t total_size = vfile.pieces * PIECE_SIZE;
    if (offset >= total_size)
        return 0;

    if (offset + size > total_size)
        size = total_size - offset;

    size_t bytes_read = 0;
    size_t piece_index = offset / PIECE_SIZE;
    size_t piece_offset = offset % PIECE_SIZE;

    while (bytes_read < size && piece_index < vfile.pieces) {
        char piece_path[512];
        snprintf(piece_path, sizeof(piece_path), "%s/%s.%03zu", relics_dir, vfile.filename, piece_index);

        FILE *f = fopen(piece_path, "rb");
        if (!f) return -EIO;

        fseek(f, piece_offset, SEEK_SET);

        size_t to_read = PIECE_SIZE - piece_offset;
        if (to_read > (size - bytes_read))
            to_read = size - bytes_read;

        size_t r = fread(buf + bytes_read, 1, to_read, f);
        fclose(f);

        if (r == 0) break;

        bytes_read += r;
        piece_index++;
        piece_offset = 0;
    }

    log_activity("READ: %s", vfile.filename);

    return bytes_read;
}

static int fuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (strcmp(path + 1, vfile.filename) == 0) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = vfile.pieces * PIECE_SIZE;
        return 0;
    }

    return -ENOENT;
}

static int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, vfile.filename, NULL, 0, 0);

    return 0;
}

static int split_and_save(const char *name, const char *fullpath) {
    FILE *src = fopen(fullpath, "rb");
    if (!src) return -EIO;

    unsigned char buffer[PIECE_SIZE];
    int piece_count = 0;

    while (1) {
        char old_piece[512];
        snprintf(old_piece, sizeof(old_piece), "%s/%s.%03d", relics_dir, name, piece_count);
        if (remove(old_piece) != 0) break;
        piece_count++;
    }

    piece_count = 0;

    while (1) {
        size_t r = fread(buffer, 1, PIECE_SIZE, src);
        if (r == 0) break;

        char piece_path[512];
        snprintf(piece_path, sizeof(piece_path), "%s/%s.%03d", relics_dir, name, piece_count);

        FILE *dst = fopen(piece_path, "wb");
        if (!dst) {
            fclose(src);
            return -EIO;
        }

        fwrite(buffer, 1, r, dst);
        fclose(dst);
        piece_count++;
    }

    fclose(src);

    if (strcmp(name, vfile.filename) == 0) {
        vfile.pieces = piece_count;
    }

    char logbuf[512];
    char pieces_str[256] = "";
    for (int i=0; i < piece_count; i++) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%s.%03d", name, i);
        strcat(pieces_str, tmp);
        if (i != piece_count-1)
            strcat(pieces_str, ", ");
    }
    snprintf(logbuf, sizeof(logbuf), "WRITE: %s -> %s", name, pieces_str);
    log_activity("%s", logbuf);

    return 0;
}

static int fuse_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path + 1, vfile.filename) != 0)
        return -ENOENT;

    return 0;
}

static int fuse_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    (void) fi;
    return read_virtual_file(path, buf, size, offset);
}

static int fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) mode;
    (void) fi;
    (void) path;
    return 0;
}

static int fuse_write(const char *path, const char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    char tmpfile[512];
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/fuse_temp_%s", path+1);

    FILE *f = fopen(tmpfile, "r+b");
    if (!f) {
        f = fopen(tmpfile, "wb");
        if (!f) return -EIO;
    }

    if (fseek(f, offset, SEEK_SET) != 0) {
        fclose(f);
        return -EIO;
    }

    size_t written = fwrite(buf, 1, size, f);
    fclose(f);

    return written;
}

static int fuse_flush(const char *path, struct fuse_file_info *fi) {
    (void) fi;

    char tmpfile[512];
    snprintf(tmpfile, sizeof(tmpfile), "/tmp/fuse_temp_%s", path+1);

    struct stat st;
    if (stat(tmpfile, &st) != 0) {
        return 0;
    }

    int res = split_and_save(path+1, tmpfile);

    remove(tmpfile);

    return res;
}

static int fuse_unlink(const char *path) {
    char name[256];
    strcpy(name, path + 1);

    int i = 0;
    char piece_path[512];
    int deleted_any = 0;
    while (1) {
        snprintf(piece_path, sizeof(piece_path), "%s/%s.%03d", relics_dir, name, i);
        if (remove(piece_path) == 0) {
            deleted_any = 1;
            i++;
        } else {
            break;
        }
    }

    if (!deleted_any)
        return -ENOENT;

    char logbuf[256];
    snprintf(logbuf, sizeof(logbuf), "DELETE: %s.000 - %s.%03d", name, name, i-1);
    log_activity("%s", logbuf);

    return 0;
}

static struct fuse_operations fuse_ops = {
    .getattr = fuse_getattr,
    .readdir = fuse_readdir,
    .open = fuse_open,
    .read = fuse_read,
    .create = fuse_create,
    .write = fuse_write,
    .flush = fuse_flush,
    .unlink = fuse_unlink,
};

int main(int argc, char *argv[]) {
    char test_piece[512];
    snprintf(test_piece, sizeof(test_piece), "%s/Baymax.jpeg.000", relics_dir);

    if (access(test_piece, F_OK) != 0) {
        printf("File pecahan belum ada, mengunduh dari Google Drive...\n");
        system("wget -O baymax.zip 'https://drive.google.com/uc?export=download&id=1MHVhFT57Wa9Zcx69Bv9j5ImHc9rXMH1c'");
        system("mkdir -p relics");
        system("unzip -o baymax.zip -d relics");
        system("rm baymax.zip");
    }

    DIR* dir = opendir(relics_dir);
    if (!dir) {
        fprintf(stderr, "Error: relics directory not found.\n");
        return 1;
    }
    closedir(dir);

    return fuse_main(argc, argv, &fuse_ops, NULL);
}
