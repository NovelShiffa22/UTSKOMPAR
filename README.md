# Implementasi dan Evaluasi Kinerja Parallel Sample Sort (OpenMPI + Apptainer)

**Mata Kuliah:** Komputasi Paralel (UTS)  
**Kelompok:** Grup 8 (`kompargj27-g8`)  
**Lingkungan Eksekusi:** Klaster Multi-Node (`compute01` dan `compute02`, 16 Slot Total)

---

## 1. Deskripsi & Struktur File

Repositori/direktori ini berisi seluruh kode sumber, definisi kontainer, skrip konfigurasi klaster, serta data hasil pengukuran eksperimen *Parallel Sample Sort* berbasis C dan OpenMPI:

* **`serial_sort.c`**: Implementasi pengurutan sekuensial (*baseline*) menggunakan fungsi standar `qsort` dengan verifikasi urutan akhir (`Valid=YES`).
* **`parallel_sample_sort.c`**: Implementasi algoritma *Parallel Sample Sort* terdistribusi menggunakan operasi kolektif OpenMPI (`MPI_Gather`, `MPI_Bcast`, `MPI_Alltoall`, `MPI_Alltoallv`, dan `MPI_Reduce`).
* **`samplesort.def`**: File definisi (*recipe*) kontainer Apptainer berbasis `ubuntu:22.04` yang menginstal `build-essential`, `openmpi-bin`, dan `libopenmpi-dev`, serta otomatis mengompilasi binary ke direktori `/opt/bin/`.
* **`samplesort.sif`**: Image kontainer Apptainer hasil *build* yang siap dijalankan secara portabel di seluruh node komputasi.
* **`hostfile`**: Daftar alokasi node dan jumlah slot prosesor (`compute01 slots=8` dan `compute02 slots=8`).
* **`mpi_ssh.sh`**: Skrip jembatan SSH (`plm_rsh_agent`) yang memastikan daemon `orted` dan seluruh proses MPI dieksekusi di dalam kontainer `samplesort.sif` pada kedua node.
* **`run_benchmark.sh`**: Skrip otomasi untuk menjalankan 3 skenario pengujian (*Baseline Serial*, *Strong Scaling*, dan *Weak Scaling*) dengan masing-masing 3 kali pengulangan.
* **`hasil_eksperimen.txt`**: Log data mentah hasil pengukuran waktu eksekusi dan status validasi (`Valid=YES`).
* **`plot_results.py`**: Skrip Python untuk mem-parsing `hasil_eksperimen.txt` serta menghasilkan grafik analisis performa (`grafik_analisis_samplesort.png`).

---

## 2. Prasyarat Lingkungan

1. **Mesin Lokal (Build Container):** Linux / Ubuntu WSL dengan paket `apptainer` dan `uidmap` terinstal.
2. **Klaster HPC (`compute01` & `compute02`):**
   * `apptainer` terinstal di setiap node.
   * Koneksi SSH tanpa password (*passwordless SSH*) aktif dari `compute01` ke `compute02`.
   * Antarmuka jaringan internal TCP `10.236.0.0/24` aktif antar-node.

---

## 3. Langkah Reproduksi Eksperimen

### Tahap 1: Membangun Image Kontainer Apptainer (di Lokal / WSL)
Masuk ke direktori yang berisi `serial_sort.c`, `parallel_sample_sort.c`, dan `samplesort.def`, lalu jalankan perintah berikut untuk membangun `samplesort.sif`:

```bash
sudo apt update && sudo apt install -y uidmap
echo "mount binfmt_misc = no" | sudo tee -a /etc/apptainer/apptainer.conf
sudo apptainer build samplesort.sif samplesort.def
sudo chown $USER:$USER samplesort.sif
```

Verifikasi bahwa image telah terbentuk dengan menjalankan uji coba lokal:
```bash
apptainer exec samplesort.sif /opt/bin/serial_sort 1000000
```

---

### Tahap 2: Deployment & Sinkronisasi ke Node Klaster
*(Catatan Arsitektur: Karena direktori berbagi `pfs` pada `compute02` tidak tersedia, seluruh file proyek ditempatkan pada direktori lokal `$HOME/uts_samplesort_g8` di masing-masing node).*

1. **Unggah seluruh file proyek dari lokal ke `compute01`:**
   ```bash
   scp -P 3009 samplesort.sif serial_sort.c parallel_sample_sort.c samplesort.def hostfile mpi_ssh.sh run_benchmark.sh kompargj27-g8@172.232.232.34:~/uts_samplesort_g8/
   ```

2. **Login ke `compute01` dan beri hak akses eksekusi pada skrip:**
   ```bash
   ssh -p 3009 kompargj27-g8@172.232.232.34
   cd ~/uts_samplesort_g8
   chmod +x mpi_ssh.sh run_benchmark.sh
   ```

3. **Sinkronisasi direktori proyek ke `compute02`:**
   ```bash
   ssh compute02 "mkdir -p ~/uts_samplesort_g8"
   scp -r ~/uts_samplesort_g8/* compute02:~/uts_samplesort_g8/
   ```

---

### Tahap 3: Verifikasi Distribusi Proses Lintas Node (`--display-map`)
Sebelum menjalankan benchmark penuh, pastikan eksekusi 16 proses (`-np 16`) terdistribusi merata di kedua node (Rank 0–7 pada `compute01` dan Rank 8–15 pada `compute02`):

```bash
apptainer exec ~/uts_samplesort_g8/samplesort.sif mpirun \
  --display-map \
  --hostfile ~/uts_samplesort_g8/hostfile \
  --mca plm_rsh_agent "$HOME/uts_samplesort_g8/mpi_ssh.sh" \
  --mca btl_tcp_if_include 10.236.0.0/24 \
  --mca btl_vader_single_copy_mechanism none \
  -np 16 /opt/bin/parallel_sample_sort 1000000
```

Output yang diharapkan menampilkan `JOB MAP` dengan `Num procs: 8` pada `compute01` dan `Num procs: 8` pada `compute02`, diakhiri dengan status `Valid=YES`.

---

### Tahap 4: Eksekusi Benchmark Lengkap
Jalankan skrip `run_benchmark.sh` untuk mereproduksi seluruh pengukuran dan menyimpannya ke dalam `hasil_eksperimen.txt`:

```bash
stdbuf -oL ~/uts_samplesort_g8/run_benchmark.sh | tee ~/uts_samplesort_g8/hasil_eksperimen.txt
```

Skenario yang dijalankan otomatis oleh skrip ini meliputi:
1. **Baseline Serial:** $N = 40.000.000$ elemen (3 pengulangan).
2. **Strong Scaling:** Ukuran total tetap $N = 40.000.000$ elemen dengan variasi proses $P \in \{1, 2, 4, 8, 16\}$ (masing-masing 3 pengulangan).
3. **Weak Scaling:** Beban tetap $N/P = 5.000.000$ elemen per proses untuk $P \in \{1, 2, 4, 8, 16\}$ (total $N$ dari $5.000.000$ hingga $80.000.000$ elemen, masing-masing 3 pengulangan).

---

### Tahap 5: Reproduksi Grafik Analisis (Opsional / Lokal)
Unduh file `hasil_eksperimen.txt` ke mesin lokal dan jalankan skrip plotting Python untuk menghasilkan grafik performa:

```bash
scp -P 3009 kompargj27-g8@172.232.232.34:~/uts_samplesort_g8/hasil_eksperimen.txt .
python3 plot_results.py
```

---

## 4. Ringkasan Hasil Pengukuran Aktual

| Skenario / Jumlah Proses ($P$) | Total Elemen ($N$) | Rata-Rata Waktu ($T_p$) | Relative Speedup ($T_1/T_p$) | Absolute Speedup ($T_{\text{serial}}/T_p$) | Status Validasi |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Serial Baseline** | 40.000.000 | 5,5638 s | — | 1,00× | `Valid=YES` |
| **Strong Scaling ($P = 1$)** | 40.000.000 | 14,4438 s | 1,00× | 0,39× | `Valid=YES` |
| **Strong Scaling ($P = 2$)** | 40.000.000 | 7,3392 s | 1,97× | 0,76× | `Valid=YES` |
| **Strong Scaling ($P = 4$)** | 40.000.000 | 1,8410 s | 7,85× | 3,02× | `Valid=YES` |
| **Strong Scaling ($P = 8$)** | 40.000.000 | 1,4167 s | 10,20× | 3,93× | `Valid=YES` |
| **Strong Scaling ($P = 16$)** | 40.000.000 | **1,2280 s** | **11,76×** | **4,53×** | `Valid=YES` |