# Sistem Monitoring dan Kontrol Listrik Cerdas Berbasis IoT

![Dashboard Monitoring Listrik](https://via.placeholder.com/800x400.png?text=Ganti+dengan+Screenshot+Dashboard+Anda)
*(Ganti gambar di atas dengan screenshot dashboard utama proyek Anda)*

Sebuah proyek IoT full-stack yang dirancang untuk memantau penggunaan energi listrik secara real-time, mengontrol perangkat dari jarak jauh, dan memberikan notifikasi cerdas. Proyek ini dibangun sebagai bagian dari pekerjaan di **Allbright Project**.

## ✨ Fitur Utama

-   **Monitoring Real-Time:** Memantau Tegangan (V), Arus (A), Daya (W), Energi Kumulatif (kWh), dan Estimasi Biaya (Rp).
-   **Dashboard Interaktif:** Antarmuka web modern dengan data yang diperbarui secara otomatis dan efek visual untuk data baru.
-   **Visualisasi Data:** Grafik historis untuk melacak tren pemakaian listrik dari waktu ke waktu menggunakan Chart.js.
-   **Kontrol Perangkat Jarak Jauh:** Menyalakan dan mematikan perangkat (lampu) yang terhubung ke modul relay melalui web.
-   **Notifikasi Cerdas Bertingkat:** Sistem secara otomatis mengirimkan notifikasi email ketika pemakaian:
    -   Kembali Normal
    -   Mendekati Batas yang ditetapkan
    -   Melebihi Batas yang ditetapkan
-   **Kontrol Otomatis:** Lampu dapat diatur untuk mati secara otomatis jika batas pemakaian terlampaui.
-   **Laporan & Histori:** Halaman khusus untuk melihat riwayat data sensor dengan pagination dan laporan akumulasi pemakaian bulanan.
-   **Reset Sensor:** Fitur untuk mereset data energi kumulatif pada sensor PZEM langsung dari antarmuka web.

## 🛠️ Tumpukan Teknologi (Tech Stack)

Arsitektur sistem ini terdiri dari tiga komponen utama:

#### 1. Perangkat Keras (IoT Device)
-   **Mikrokontroler:** ESP32
-   **Sensor:** PZEM-004T v3.0
-   **Aktor:** Modul Relay 1-Channel
-   **Display Lokal:** LCD I2C 16x2

#### 2. Backend Server
-   **Framework:** Laravel 12 (PHP 8.2+)
-   **Database:** MySQL
-   **Komunikasi:** RESTful API melalui HTTPS
-   **Mail Driver:** SMTP (dikonfigurasi untuk Gmail)

#### 3. Frontend
-   **Templating:** Laravel Blade
-   **Styling:** Custom CSS
-   **Interaktivitas:** Vanilla JavaScript (AJAX/Fetch API untuk polling data)
-   **Grafik:** Chart.js
-   **UI Framework (Pagination):** Bootstrap CSS

## ⚙️ Instalasi & Konfigurasi

Berikut adalah langkah-langkah untuk menjalankan proyek ini di lingkungan lokal atau server.

### Prasyarat
-   Server Web (Nginx/Apache) dengan PHP >= 8.2
-   Composer 2
-   Database MySQL
-   Arduino IDE atau PlatformIO
-   Akun email (Gmail direkomendasikan) dengan "App Password" untuk notifikasi.

### 1. Backend (Laravel)

1.  **Clone repository ini:**
    ```bash
    git clone https://github.com/username-anda/nama-repository.git
    cd nama-repository
    ```

2.  **Install dependensi Composer:**
    ```bash
    composer install
    ```

3.  **Buat file `.env`:**
    Salin file `.env.example` menjadi `.env`.
    ```bash
    cp .env.example .env
    ```

4.  **Generate Application Key:**
    ```bash
    php artisan key:generate
    ```

5.  **Konfigurasi `.env`:**
    Buka file `.env` dan sesuaikan variabel berikut:
    ```dotenv
    # Detail Aplikasi & Database
    APP_NAME="Monitoring Listrik"
    APP_URL=http://localhost:8000

    DB_CONNECTION=mysql
    DB_HOST=127.0.0.1
    DB_PORT=3306
    DB_DATABASE=nama_database_anda
    DB_USERNAME=root
    DB_PASSWORD=password_database_anda

    # Tarif Listrik
    COST_PER_KWH=1444.70

    # Konfigurasi Email untuk Notifikasi
    MAIL_MAILER=smtp
    MAIL_HOST=smtp.gmail.com
    MAIL_PORT=587
    MAIL_USERNAME=email_pengirim_anda@gmail.com
    MAIL_PASSWORD=app_password_gmail_anda
    MAIL_ENCRYPTION=tls
    MAIL_FROM_ADDRESS="${MAIL_USERNAME}"
    MAIL_FROM_NAME="${APP_NAME}"

    # Email Penerima Notifikasi
    NOTIFICATION_EMAIL_RECIPIENT=email_penerima_anda@example.com
    ```

6.  **Jalankan Migrasi Database:**
    Perintah ini akan membuat tabel `pzem_data` dan tabel lain yang diperlukan.
    ```bash
    php artisan migrate
    ```

7.  **Jalankan Server Development:**
    ```bash
    php artisan serve
    ```
    Aplikasi Laravel sekarang berjalan di `http://localhost:8000`.

### 2. Perangkat Keras (ESP32)

1.  **Buka File Sketch:**
    Buka file `.ino` dari direktori `esp32_code` (atau nama direktori yang sesuai) dengan Arduino IDE.

2.  **Install Library yang Dibutuhkan:**
    Melalui Library Manager di Arduino IDE, install library berikut:
    -   `PZEM-004T-v30` oleh mandulaj
    -   `ArduinoJson` oleh Benoit Blanchon
    -   `LiquidCrystal_I2C` oleh Frank de Brabander

3.  **Konfigurasi Sketch Arduino:**
    Buka file sketch dan sesuaikan variabel di bagian konfigurasi:
    ```cpp
    // --- Konfigurasi WiFi & Server ---
    const char* ssid = "NAMA_WIFI_ANDA";
    const char* password = "PASSWORD_WIFI_ANDA";

    // Arahkan ke server lokal atau server hosting
    const char* serverName = "ip_address_server_laravel_anda"; // contoh: "192.168.1.10" atau "domainanda.com"
    const int serverPort = 8000; // 80 untuk HTTP, 443 untuk HTTPS
    ```

4.  **Upload ke ESP32:**
    Pilih board ESP32 yang sesuai (misal: "ESP32 Dev Module") dan port COM yang benar, lalu klik "Upload".

## 🚀 Penggunaan

1.  Pastikan perangkat keras ESP32 sudah terhubung dengan sensor PZEM dan relay, lalu nyalakan.
2.  Pastikan server Laravel sudah berjalan.
3.  Buka Serial Monitor di Arduino IDE untuk melihat log dari ESP32.
4.  Akses dashboard web melalui browser (`http://localhost:8000/monitor` atau domain hostingmu).
5.  Data akan mulai muncul di dashboard secara real-time.

## 📄 Lisensi

Proyek ini dilisensikan di bawah [MIT License](LICENSE.md).

---
