| No  | Nama                   | NRP        |
| --- | ---------------------- | ---------- |
| 1   | Ahmad Yazid Arifuddin  | 5027241040 |
| 2   | Muhammad Ziddan Habibi | 5027241122 |
| 3   | Andi Naufal Zaki       | 5027241059 |


## SOAL 2 
1. Install Dependensi
`
sudo apt update
sudo apt install fuse3 libfuse3-dev unzip wget
`

2. Compile Program
`
gcc fuse.c -o fusefs `pkg-config fuse3 --cflags --libs`
`

3. Siapkan Folder
`
mkdir -p mount_dir
mkdir -p relics
`

4. Jalankan Program FUSE
Jalankan program dengan:
`
./fusefs mount_dir
`
Jika ingin menjalankan di background (supaya terminal tidak terkunci):
`
./fusefs mount_dir &
`

5. Akses File Virtual
Masuk ke folder mount:
`
cd mount_dir
`
Di dalamnya akan muncul file Baymax.jpeg (dan file lain jika kamu buat).
Coba buka/copy file:
`
cp Baymax.jpeg /tmp/
xdg-open Baymax.jpeg
`

6. Unmount Jika Selesai
Setelah selesai, unmount dengan:
`
fusermount3 -u mount_dir
`
atau
`
fusermount -u mount_dir
`




###cara menual menjalankan nomor 2
cat relics/Baymax.jpeg.* > test.jpeg







## **Soal No 3**

Pujo, seorang komting angkatan 24, menciptakan sistem bernama **AntiNK (Anti Napis Kimcun)** untuk mendeteksi dan melindungi file-file penting dari dua mahasiswa anomali bernama **Nafis** dan **Kimcun**. Sistem ini:

* Berjalan dalam lingkungan **Docker** menggunakan **docker-compose**.
* Menggunakan **FUSE (Filesystem in Userspace)** di dalam container `antink-server` untuk melakukan virtualisasi sistem file.
* Memiliki container logger (`antink-logger`) yang mencatat **aktivitas secara real-time** ke dalam log.
* File yang memiliki nama mengandung `nafis` atau `kimcun` dianggap **berbahaya**:

  * Nama file dibalik saat ditampilkan.
  * Isinya **tidak** dienkripsi saat dibaca.
  * Aktivitas dicatat di log `/var/log/it24.log`.
* File **normal** (tidak berbahaya) akan dienkripsi menggunakan algoritma **ROT13** saat dibaca.

Struktur `docker-compose` terdiri atas:

* `antink-server` → menjalankan FUSE.
* `antink-logger` → monitor log `/var/log/it24.log`.
* `it24_host` → direktori bind mount penyimpanan file asli.
* `antink_mount` → mount point hasil dari virtualisasi FUSE.
* `antink-logs` → direktori log hasil pencatatan aktivitas.

Contoh output:

```bash
$ docker exec antink-server ls /antink_mount
test.txt  vsc.sifan  txt.nucmik

$ docker exec antink-server cat /antink_mount/test.txt
grfg grkg   # ROT13 dari "test text"

$ docker exec antink-server cat /antink_mount/nucmik.txt
isi asli   # Tanpa enkripsi
```

---

### **3. Cara Menjalankan Sistem**
### **Langkah 1: Persiapan Direktori**

Pastikan struktur direktori sebagai berikut:

```
soal_3/
├── Dockerfile
├── docker-compose.yml
├── antink.c          # FUSE Filesystem
├── it24_host/        # Direktori bind mount file asli
├── antink-logs/      # Direktori log
```

### **Langkah 2: Build dan Jalankan Container**

```bash
docker-compose up --build -d
```

![image](https://github.com/user-attachments/assets/05b2bb86-68d9-41f2-9f07-a28d4d91325d)


### **Langkah 3: Uji Fungsionalitas**

* Tambah file ke `it24_host/`.

* Periksa hasil virtualisasi di `antink_mount`:

  ```bash
  docker exec antink-server ls /antink_mount
  docker exec antink-server cat /antink_mount/<nama_file>
  ```

* Lihat log aktivitas:

  ```bash
  docker logs antink-logger
  ```

  ![image](https://github.com/user-attachments/assets/6fd85d22-203a-40fd-9777-aa4d5ddaa549)


### **Langkah 4: Bersihkan Sistem**

```bash
docker-compose down
```

---

### **4. Materi yang Digunakan**

###  **Docker & Docker Compose**

* Digunakan untuk membuat container terisolasi.
* `docker-compose.yml` mengatur jaringan dan volume antar container.

###  **FUSE (Filesystem in Userspace)**

* Library yang memungkinkan pembuatan filesystem custom oleh user-space program.
* Diimplementasikan pada `antink.c`.

###  **ROT13 Cipher**

* Algoritma enkripsi sederhana yang mengganti setiap huruf dengan huruf ke-13 berikutnya dalam alfabet.
* Diterapkan untuk file normal (tidak mengandung "nafis"/"kimcun").

###  **Real-Time Logging**

* Aktivitas dicatat ke `/var/log/it24.log`.
* Logger container membaca file tersebut secara terus menerus.

###  **Mount Point & Bind Mount**

* **`it24_host`**: tempat file asli berada.
* **`antink_mount`**: mount point hasil FUSE.
* **`antink-logs`**: direktori yang menyimpan log.

---

### **5. Kesimpulan**

Sistem **AntiNK** berhasil dibuat dengan fitur:

* Deteksi file berbahaya berdasarkan nama.
* Penyajian nama file berbahaya secara terbalik.
* Enkripsi isi file normal menggunakan ROT13.
* Logging aktivitas secara real-time.
* Isolasi sistem melalui container Docker dan pengelolaan menggunakan `docker-compose`.

Sistem ini memberikan **simulasi nyata** bagaimana sistem keamanan file dapat dibuat dan dijalankan secara modular dan aman.

---


