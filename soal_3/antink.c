#define FUSE_USE_VERSION 313
#define _POSIX_C_SOURCE 200809L
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>

const char *base_path = "/it24_host";
const char *log_file = "/logs/it24.log"; 

// Membalik string dengan aman
char *strrev(char *str) {
    if (!str || !*str) return str;
    
    char *start = str;
    char *end = str + strlen(str) - 1;
    
    while (end > start) {
        char temp = *start;
        *start++ = *end;
        *end-- = temp;
    }
    return str;
}

// Cek anomaly dengan case-insensitive
int is_anomaly(const char *name) {
    if (!name) return 0;
    
    char lower[NAME_MAX + 1];
    size_t len = strlen(name);
    if (len > NAME_MAX) return 0;
    
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower(name[i]);
    }
    lower[len] = '\0';
    
    return strstr(lower, "nafis") || strstr(lower, "kimcun");
}

// Membalik string dengan aman
void reverse(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return;
    
    size_t len = strnlen(src, dst_size - 1);
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[len - i - 1];
    }
    dst[len] = '\0';
}

// Logging helper yang lebih robust
void write_log(const char *type, const char *msg) {
    if (!type || !msg) return;
    
    FILE *f = fopen(log_file, "a");
    if (!f) {
        syslog(LOG_ERR, "Failed to open log file %s: %s", log_file, strerror(errno));
        return;
    }
    
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            type, msg);
    
    fclose(f);
}

// Menerjemahkan path dengan pengecekan buffer overflow
int translate_path(char *dest, size_t dest_size, const char *path) {
    if (!dest || !path || dest_size == 0) return -EINVAL;
    
    char tmp[PATH_MAX];
    char filename[NAME_MAX + 1];
    char finalname[PATH_MAX];
    
    if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) {
        return -ENAMETOOLONG;
    }
    
    const char *fname = strrchr(tmp, '/');
    if (fname) {
        fname++;
        if (is_anomaly(fname)) {
            reverse(filename, fname, sizeof(filename));
            
            if (snprintf(finalname, sizeof(finalname), "%.*s%s",
                        (int)(fname - tmp), tmp, filename) >= (int)sizeof(finalname)) {
                return -ENAMETOOLONG;
            }
            
            char logmsg[2048];
            snprintf(logmsg, sizeof(logmsg), "File %s has been reversed to %s", fname, filename);
            write_log("REVERSE", logmsg);
        } else {
            if (snprintf(finalname, sizeof(finalname), "%s", tmp) >= (int)sizeof(finalname)) {
                return -ENAMETOOLONG;
            }
        }
    } else {
        if (snprintf(finalname, sizeof(finalname), "%s", tmp) >= (int)sizeof(finalname)) {
            return -ENAMETOOLONG;
        }
    }
    
    if (snprintf(dest, dest_size, "%s%s", base_path, finalname) >= (int)dest_size) {
        return -ENAMETOOLONG;
    }
    
    return 0;
}

// ROT13 enkripsi/dekripsi teks
void apply_rot13(char *str, size_t len) {
    if (!str || len == 0) return;
    
    for (size_t i = 0; i < len; i++) {
        if (('a' <= str[i] && str[i] <= 'z')) {
            str[i] = 'a' + (str[i] - 'a' + 13) % 26;
        } else if (('A' <= str[i] && str[i] <= 'Z')) {
            str[i] = 'A' + (str[i] - 'A' + 13) % 26;
        }
        // Karakter non-alfabet dibiarkan apa adanya
    }
}

static int antink_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    char fpath[PATH_MAX];
    int res = translate_path(fpath, sizeof(fpath), path);
    if (res != 0) return res;
    
    res = lstat(fpath, stbuf);
    return res == -1 ? -errno : 0;
}

static int antink_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
    char fpath[PATH_MAX];
    int res = translate_path(fpath, sizeof(fpath), path);
    if (res != 0) return res;
    
    DIR *dp = opendir(fpath);
    if (!dp) return -errno;
    
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        char name[NAME_MAX + 1];
        if (is_anomaly(de->d_name)) {
            reverse(name, de->d_name, sizeof(name));
        } else {
            strncpy(name, de->d_name, sizeof(name));
            name[sizeof(name) - 1] = '\0';
        }
        
        if (filler(buf, name, NULL, 0, 0)) break;
    }
    
    closedir(dp);
    return 0;
}

static int antink_open(const char *path, struct fuse_file_info *fi) {
    char fpath[PATH_MAX];
    int res = translate_path(fpath, sizeof(fpath), path);
    if (res != 0) return res;
    
    int fd = open(fpath, fi->flags);
    if (fd == -1) return -errno;
    
    fi->fh = fd;  // Simpan file descriptor untuk digunakan nanti
    return 0;
}

static int antink_read(const char *path, char *buf, size_t size, off_t offset, 
                      struct fuse_file_info *fi) {
    if (fi->fh == 0) {
        // Jika file belum terbuka, buka dulu
        int res = antink_open(path, fi);
        if (res < 0) return res;
    }
    
    int res = pread(fi->fh, buf, size, offset);
    if (res <= 0) return res;
    
    // Ambil nama file dari path
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    
    if (strstr(filename, ".txt") && !is_anomaly(filename)) {
        apply_rot13(buf, res);
        
        char logmsg[1024];
        snprintf(logmsg, sizeof(logmsg), "File %s has been encrypted with ROT13", filename);
        write_log("ENCRYPT", logmsg);
    }
    
    if (is_anomaly(filename)) {
        char logmsg[1024];
        snprintf(logmsg, sizeof(logmsg), "Anomaly detected in file: %s", filename);
        write_log("ALERT", logmsg);
    }
    
    return res;
}

static int antink_release(const char *path, struct fuse_file_info *fi) {
    if (fi->fh != 0) {
        close(fi->fh);
        fi->fh = 0;
    }
    return 0;
}

static const struct fuse_operations antink_oper = {
    .getattr = antink_getattr,
    .readdir = antink_readdir,
    .open    = antink_open,
    .read    = antink_read,
    .release = antink_release,
};

int main(int argc, char *argv[]) {
    umask(0);
    openlog("antink_fuse", LOG_PID, LOG_USER);
    write_log("SYSTEM", "FUSE filesystem initialized");
    
    int res = fuse_main(argc, argv, &antink_oper, NULL);
    
    closelog();
    return res;
}
