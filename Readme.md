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
