#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

int compare_filenames(const void *a, const void *b);

unsigned char* hex_to_bin(const char *hex_str, size_t *bin_len) {
    size_t len = strlen(hex_str);
    if (len % 2 != 0) return NULL;
    
    *bin_len = len / 2;
    unsigned char bin_data = (unsigned char)malloc(*bin_len);
    if (!bin_data) return NULL;
    
    for (size_t i = 0; i < len; i += 2) {
        sscanf(hex_str + i, "%2hhx", &bin_data[i/2]);
    }
    return bin_data;
}

void process_file(const char *filename, FILE *log_file) {
    FILE *txt_file = fopen(filename, "r");
    if (!txt_file) {
        perror("Error opening text file");
        return;
    }

    fseek(txt_file, 0, SEEK_END);
    long file_size = ftell(txt_file);
    rewind(txt_file);

    char hex_str = (char)malloc(file_size + 1);
    if (!hex_str) {
        fclose(txt_file);
        return;
    }

    size_t read_size = fread(hex_str, 1, file_size, txt_file);
    hex_str[read_size] = '\0';
    fclose(txt_file);

    // Clean non-hex characters
    char clean_hex = (char)malloc(read_size + 1);
    if (!clean_hex) {
        free(hex_str);
        return;
    }
    size_t clean_len = 0;
    for (size_t i = 0; i < read_size; i++) {
        if (isxdigit(hex_str[i])) {
            clean_hex[clean_len++] = hex_str[i];
        }
    }
    clean_hex[clean_len] = '\0';
    free(hex_str);

    if (clean_len % 2 != 0) {
        free(clean_hex);
        return;
    }

    size_t bin_len;
    unsigned char bin_data = (unsigned char)(clean_hex, &bin_len);
    free(clean_hex);
    if (!bin_data) return;

    // Generate timestamp
    time_t now;
    struct tm *tm_info;
    char date_part[11], time_part[9], combined_ts[20];
    time(&now);
    tm_info = localtime(&now);
    strftime(date_part, 11, "%Y-%m-%d", tm_info);
    strftime(time_part, 9, "%H:%M:%S", tm_info);
    snprintf(combined_ts, 20, "%s_%s", date_part, time_part);

    // Create image filename
    char base[256];
    strncpy(base, filename, 255);
    base[255] = '\0';
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    char image_basename[256];
    snprintf(image_basename, 256, "%s_image_%s.png", base, combined_ts);
    char image_path[512];
    snprintf(image_path, 512, "image/%s", image_basename);

    // Write image
    FILE *image_file = fopen(image_path, "wb");
    if (image_file) {
        fwrite(bin_data, 1, bin_len, image_file);
        fclose(image_file);
    }
    free(bin_data);

    // Log
    fprintf(log_file, "[%s][%s]: Successfully converted hexadecimal text %s to %s.\n",
            date_part, time_part, filename, image_basename);
    fflush(log_file);
}

int compare_filenames(const void *a, const void *b) {
    int num_a = atoi(*(const char **)a);
    int num_b = atoi(*(const char **)b);
    return num_a - num_b;
}

int main() {
    system("wget -q -O samples.zip 'https://drive.google.com/uc?export=download&id=1hi_GDdP51Kn2JJMw02WmCOxuc3qrXzh5'");
    system("unzip -q samples.zip");
    system("rm samples.zip");
    system("mkdir -p image");

    FILE *log_file = fopen("conversion.log", "a");
    if (!log_file) return 1;

    DIR *dir = opendir(".");
    struct dirent *entry;
    char *filenames[100];
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            const char *name = entry->d_name;
            size_t len = strlen(name);
            if (len > 4 && strcmp(name + len - 4, ".txt") == 0 &&
                strcmp(name, "conversion.log") != 0) {
                filenames[count] = strdup(name);
                count++;
            }
        }
    }
    closedir(dir);

    qsort(filenames, count, sizeof(char *), compare_filenames);

    for (int i = 0; i < count; i++) {
        process_file(filenames[i], log_file);
        free(filenames[i]);
    }

    fclose(log_file);
    return 0;
}