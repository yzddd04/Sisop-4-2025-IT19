#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zlib.h>

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#define AES_KEYLEN 32
#define AES_IVLEN 16

const unsigned char aes_key[AES_KEYLEN] = "ini_kunci_rahasia_maimai_256bit!!"; // 32 bytes

int aes_encrypt(const unsigned char *plaintext, int plaintext_len, unsigned char *ciphertext, unsigned char *iv) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len, ciphertext_len;

    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, aes_key, iv);
    EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len);
    ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
    ciphertext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

int aes_decrypt(const unsigned char *ciphertext, int ciphertext_len, unsigned char *plaintext, unsigned char *iv) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len, plaintext_len;

    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, aes_key, iv);
    EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len);
    plaintext_len = len;
    EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    return plaintext_len;
}

void get_real_path(const char *fuse_path, char *real_path) {
    const char *base = "/home/yazid/chiho";
    if (strncmp(fuse_path, "/7sref/", 7) == 0) {
        // Format: /7sref/[area]_[namafile]
        const char *filename = fuse_path + 7;
        char area[NAME_MAX] = {0};
        char *underscore = strchr(filename, '_');
        if (underscore) {
            size_t area_len = underscore - filename;
            strncpy(area, filename, area_len);
            area[area_len] = '\0';
            const char *namafile = underscore + 1;
            char redirected_path[4096];
            snprintf(redirected_path, sizeof(redirected_path), "/%s/%s", area, namafile);
            get_real_path(redirected_path, real_path); // recursive call to get mapped backend path
            return;
        } else {
            // Jika tidak ada underscore, fallback ke base
            sprintf(real_path, "%s%s", base, fuse_path);
            return;
        }
    }
    if (strncmp(fuse_path, "/starter/", 9) == 0) {
        sprintf(real_path, "%s/starter/%s.mai", base, fuse_path + 9);
    } else if (strncmp(fuse_path, "/metro/", 7) == 0) {
        sprintf(real_path, "%s/metro/%s.ccc", base, fuse_path + 7);
    } else if (strncmp(fuse_path, "/dragon/", 8) == 0) {
        sprintf(real_path, "%s/dragon/%s.rot", base, fuse_path + 8);
    } else if (strncmp(fuse_path, "/blackrose/", 11) == 0) {
        sprintf(real_path, "%s/blackrose/%s.bin", base, fuse_path + 11);
    } else if (strncmp(fuse_path, "/heaven/", 8) == 0) {
        sprintf(real_path, "%s/heaven/%s.enc", base, fuse_path + 8);
    } else if (strncmp(fuse_path, "/skystreet/", 11) == 0) {
        sprintf(real_path, "%s/skystreet/%s.gz", base, fuse_path + 11);
    } else {
        sprintf(real_path, "%s%s", base, fuse_path);
    }
}

void rot13(const char *in, char *out, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        char c = in[i];
        if ('a' <= c && c <= 'z') {
            out[i] = ((c - 'a' + 13) % 26) + 'a';
        } else if ('A' <= c && c <= 'Z') {
            out[i] = ((c - 'A' + 13) % 26) + 'A';
        } else {
            out[i] = c;
        }
    }
}

static int maimai_getattr(const char *path, struct stat *stbuf) {
    char real_path[4096];
    get_real_path(path, real_path);
    int res = lstat(real_path, stbuf);
    if (res == -1)
        return -errno;
    // Untuk starter, tampilkan file tanpa .mai
    if (S_ISREG(stbuf->st_mode) && strncmp(path, "/starter/", 9) == 0) {
        stbuf->st_size -= 0; // Ukuran tetap, tidak perlu diubah
    }
    return 0;
}

static int maimai_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    char real_path[4096];
    if (strcmp(path, "/7sref") == 0) {
        // List all files from all area directories, with [area]_[namafile] format
        const char *areas[] = {"starter", "metro", "dragon", "blackrose", "heaven", "youth", "skystreet"};
        char area_path[4096];
        for (int i = 0; i < 7; ++i) {
            snprintf(area_path, sizeof(area_path), "/home/yazid/chiho/%s", areas[i]);
            DIR *dp = opendir(area_path);
            if (!dp) continue;
            struct dirent *de;
            while ((de = readdir(dp)) != NULL) {
                if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
                // Strip ekstensi sesuai area
                char name[NAME_MAX];
                size_t len = strlen(de->d_name);
                if (strcmp(areas[i], "starter") == 0 && len > 4 && strcmp(de->d_name + len - 4, ".mai") == 0) {
                    strncpy(name, de->d_name, len - 4);
                    name[len - 4] = '\0';
                } else if (strcmp(areas[i], "metro") == 0 && len > 4 && strcmp(de->d_name + len - 4, ".ccc") == 0) {
                    strncpy(name, de->d_name, len - 4);
                    name[len - 4] = '\0';
                } else if (strcmp(areas[i], "dragon") == 0 && len > 4 && strcmp(de->d_name + len - 4, ".rot") == 0) {
                    strncpy(name, de->d_name, len - 4);
                    name[len - 4] = '\0';
                } else if (strcmp(areas[i], "blackrose") == 0 && len > 4 && strcmp(de->d_name + len - 4, ".bin") == 0) {
                    strncpy(name, de->d_name, len - 4);
                    name[len - 4] = '\0';
                } else if (strcmp(areas[i], "heaven") == 0 && len > 4 && strcmp(de->d_name + len - 4, ".enc") == 0) {
                    strncpy(name, de->d_name, len - 4);
                    name[len - 4] = '\0';
                } else if (strcmp(areas[i], "skystreet") == 0 && len > 3 && strcmp(de->d_name + len - 3, ".gz") == 0) {
                    strncpy(name, de->d_name, len - 3);
                    name[len - 3] = '\0';
                } else {
                    strncpy(name, de->d_name, len);
                    name[len] = '\0';
                }
                char sref_name[NAME_MAX * 2];
                snprintf(sref_name, sizeof(sref_name), "%s_%s", areas[i], name);
                filler(buf, sref_name, NULL, 0);
            }
            closedir(dp);
        }
        return 0;
    }
    get_real_path(path, real_path);
    DIR *dp = opendir(real_path);
    if (dp == NULL)
        return -errno;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(path, "/metro") == 0 || strncmp(path, "/metro/", 7) == 0) {
            size_t len = strlen(de->d_name);
            if (len > 4 && strcmp(de->d_name + len - 4, ".ccc") == 0) {
                char name[NAME_MAX];
                strncpy(name, de->d_name, len - 4);
                name[len - 4] = '\0';
                filler(buf, name, NULL, 0);
            }
        } else if (strcmp(path, "/blackrose") == 0 || strncmp(path, "/blackrose/", 11) == 0) {
            size_t len = strlen(de->d_name);
            if (len > 4 && strcmp(de->d_name + len - 4, ".bin") == 0) {
                char name[NAME_MAX];
                strncpy(name, de->d_name, len - 4);
                name[len - 4] = '\0';
                filler(buf, name, NULL, 0);
            }
        } else {
            filler(buf, de->d_name, NULL, 0);
        }
    }
    closedir(dp);
    return 0;
}

static int maimai_open(const char *path, struct fuse_file_info *fi) {
    char real_path[4096];
    get_real_path(path, real_path);
    int fd = open(real_path, fi->flags);
    if (fd == -1)
        return -errno;
    fi->fh = fd;
    return 0;
}

static int maimai_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    int fd;
    if(fi == NULL)
    {
        char real_path[4096];
        get_real_path(path, real_path);
        fd = open(real_path, O_RDONLY);
        if (fd == -1)
            return -errno;
    } else {
        fd = fi->fh;
    }
    int res = pread(fd, buf, size, offset);
    if (strncmp(path, "/dragon/", 8) == 0 && res > 0) {
        char *tmp = malloc(res);
        rot13(buf, tmp, res);
        memcpy(buf, tmp, res);
        free(tmp);
    }
    if (strncmp(path, "/heaven/", 8) == 0 && res > 0) {
        unsigned char iv[AES_IVLEN];
        pread(fd, iv, AES_IVLEN, 0);
        unsigned char *ciphertext = malloc(res - AES_IVLEN);
        pread(fd, ciphertext, res - AES_IVLEN, AES_IVLEN);
        unsigned char *plaintext = malloc(res - AES_IVLEN);
        int plain_len = aes_decrypt(ciphertext, res - AES_IVLEN, plaintext, iv);
        memcpy(buf, plaintext, plain_len);
        free(ciphertext);
        free(plaintext);
        return plain_len;
    }
    if (strncmp(path, "/skystreet/", 11) == 0 && res > 0) {
        char *decomp = NULL;
        size_t decomp_len = 0;
        if (gzip_decompress(buf, res, &decomp, &decomp_len) == 0) {
            memcpy(buf, decomp, decomp_len);
            free(decomp);
            return decomp_len;
        }
    }
    if(fi == NULL)
        close(fd);
    if (res == -1)
        res = -errno;
    return res;
}

static int maimai_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    int fd;
    if(fi == NULL)
    {
        char real_path[4096];
        get_real_path(path, real_path);
        fd = open(real_path, O_WRONLY);
        if (fd == -1)
            return -errno;
    } else {
        fd = fi->fh;
    }
    if (strncmp(path, "/heaven/", 8) == 0) {
        unsigned char iv[AES_IVLEN];
        RAND_bytes(iv, AES_IVLEN);
        unsigned char *ciphertext = malloc(size + AES_IVLEN + AES_KEYLEN);
        int cipher_len = aes_encrypt((unsigned char*)buf, size, ciphertext, iv);
        pwrite(fd, iv, AES_IVLEN, 0);
        int res = pwrite(fd, ciphertext, cipher_len, AES_IVLEN);
        free(ciphertext);
        if(fi == NULL) close(fd);
        return res;
    } else if (strncmp(path, "/skystreet/", 11) == 0) {
        char *comp = NULL;
        size_t comp_len = 0;
        if (gzip_compress(buf, size, &comp, &comp_len) == 0) {
            int res = pwrite(fd, comp, comp_len, offset);
            free(comp);
            if(fi == NULL) close(fd);
            return res;
        }
    } else {
        int res = pwrite(fd, buf, size, offset);
        if(fi == NULL)
            close(fd);
        return res;
    }
    return -EIO;
}

static int maimai_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char real_path[4096];
    get_real_path(path, real_path);
    int fd = creat(real_path, mode);
    if (fd == -1)
        return -errno;
    fi->fh = fd;
    return 0;
}

static int maimai_unlink(const char *path) {
    char real_path[4096];
    get_real_path(path, real_path);
    int res = unlink(real_path);
    if (res == -1)
        return -errno;
    return 0;
}

static int maimai_truncate(const char *path, off_t size) {
    char real_path[4096];
    get_real_path(path, real_path);
    // Untuk skystreet, jika truncate ke 0, hapus file backend
    if (strncmp(path, "/skystreet/", 11) == 0 && size == 0) {
        int res = unlink(real_path);
        if (res == -1 && errno != ENOENT)
            return -errno;
        return 0;
    }
    int res = truncate(real_path, size);
    if (res == -1)
        return -errno;
    return 0;
}

void ensure_chiho_structure() {
    const char *base = "/home/yazid/chiho"; // Ganti sesuai path chiho
    const char *dirs[] = {"starter", "metro", "dragon", "blackrose", "heaven", "youth", "skystreet"};
    char path[4096];
    mkdir(base, 0755); // Buat base jika belum ada
    for (int i = 0; i < 7; ++i) {
        snprintf(path, sizeof(path), "%s/%s", base, dirs[i]);
        mkdir(path, 0755);
    }
}

// Fungsi kompresi gzip
int gzip_compress(const char *src, size_t src_len, char **dest, size_t *dest_len) {
    z_stream strm = {0};
    int ret;
    size_t bound = compressBound(src_len);
    *dest = malloc(bound);
    if (!*dest) return -1;

    // 16 + MAX_WBITS = gzip with window bits
    ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        free(*dest);
        return -1;
    }

    strm.next_in = (Bytef *)src;
    strm.avail_in = src_len;
    strm.next_out = (Bytef *)*dest;
    strm.avail_out = bound;

    ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&strm);
        free(*dest);
        return -1;
    }
    *dest_len = strm.total_out;
    deflateEnd(&strm);
    return 0;
}

// Fungsi dekompresi gzip
int gzip_decompress(const char *src, size_t src_len, char **dest, size_t *dest_len) {
    z_stream strm = {0};
    int ret;
    size_t out_len = src_len * 10;
    *dest = malloc(out_len);
    if (!*dest) return -1;

    // 16 + MAX_WBITS = gzip with window bits
    ret = inflateInit2(&strm, 16 + MAX_WBITS);
    if (ret != Z_OK) {
        free(*dest);
        return -1;
    }

    strm.next_in = (Bytef *)src;
    strm.avail_in = src_len;
    strm.next_out = (Bytef *)*dest;
    strm.avail_out = out_len;

    ret = inflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        inflateEnd(&strm);
        free(*dest);
        return -1;
    }
    *dest_len = strm.total_out;
    inflateEnd(&strm);
    return 0;
}

static struct fuse_operations maimai_oper = {
    .getattr = maimai_getattr,
    .readdir = maimai_readdir,
    .open    = maimai_open,
    .read    = maimai_read,
    .write   = maimai_write,
    .create  = maimai_create,
    .unlink  = maimai_unlink,
    .truncate = maimai_truncate,
};

int main(int argc, char *argv[]) {
    ensure_chiho_structure();
    return fuse_main(argc, argv, &maimai_oper, NULL);
}
