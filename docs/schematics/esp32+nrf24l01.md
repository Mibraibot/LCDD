# ESP32 dan NRF24L01 Wiring Documentation

## 1. Deskripsi Sistem
Rangkaian ini menghubungkan **ESP32** dengan modul **NRF24L01 Wireless Transceiver** untuk komunikasi data nirkabel pada frekuensi **2.4 GHz**.

NRF24L01 menggunakan protokol **SPI (Serial Peripheral Interface)** untuk berkomunikasi dengan mikrokontroler.  
Pada sistem ini:

- **ESP32** bertindak sebagai **SPI Master**
- **NRF24L01** bertindak sebagai **SPI Slave**

Komunikasi ini memungkinkan pengiriman dan penerimaan data secara wireless antara dua atau lebih perangkat yang menggunakan NRF24L01.

---

# 2. Diagram Wiring

![ESP32 NRF24L01 Wiring](wiring.png)

---

# 3. Tabel Koneksi Pin

| Pin NRF24L01 | Pin ESP32 | Keterangan |
|---|---|---|
| VCC | 3.3V | Sumber tegangan modul |
| GND | GND | Ground |
| CE | GPIO4 (D4) | Enable mode transmit/receive |
| CSN | GPIO5 (D5) | Chip Select SPI |
| SCK | GPIO18 (D18) | Clock SPI |
| MOSI | GPIO23 (D23) | Data dari ESP32 ke NRF24L01 |
| MISO | GPIO19 (D19) | Data dari NRF24L01 ke ESP32 |

---

# 4. Penjelasan Fungsi Pin

## VCC
Memberikan suplai tegangan **3.3V** ke modul NRF24L01.

⚠️ **Penting:** Modul NRF24L01 hanya mendukung **3.3V** dan tidak boleh menggunakan 5V.

---

## GND
Ground dari ESP32 dan NRF24L01 harus terhubung agar sistem memiliki referensi tegangan yang sama.

---

## CE (Chip Enable)
Digunakan untuk mengaktifkan mode komunikasi pada NRF24L01.

Mode operasi:
- **HIGH** → Modul aktif (Transmit / Receive)
- **LOW** → Standby

Terhubung ke **GPIO4** pada ESP32.

---

## CSN (Chip Select Not)
Digunakan untuk memilih perangkat SPI yang aktif.

- **LOW** → NRF24L01 aktif menerima komunikasi
- **HIGH** → SPI tidak aktif

Terhubung ke **GPIO5** pada ESP32.

---

## SCK (Serial Clock)
Clock untuk sinkronisasi komunikasi SPI.

Terhubung ke **GPIO18** pada ESP32.

---

## MOSI (Master Out Slave In)
Jalur data dari **ESP32 ke NRF24L01**.

Digunakan untuk mengirim:
- Perintah
- Konfigurasi
- Data payload

Terhubung ke **GPIO23**.

---

## MISO (Master In Slave Out)
Jalur data dari **NRF24L01 ke ESP32**.

Digunakan untuk mengirim:
- Status modul
- Data yang diterima

Terhubung ke **GPIO19**.

---

# 5. Alur Komunikasi Sistem

Proses komunikasi antara ESP32 dan NRF24L01:

1. ESP32 mengaktifkan modul menggunakan **CE**
2. ESP32 memilih NRF24L01 melalui **CSN**
3. Data dikirim melalui **MOSI**
4. Komunikasi disinkronkan menggunakan **SCK**
5. NRF24L01 mengirim respon melalui **MISO**
6. Data dipancarkan melalui jaringan **2.4 GHz wireless**

---

# 6. Catatan Penting

Beberapa hal penting saat menggunakan NRF24L01:

### Gunakan Tegangan Stabil
NRF24L01 sangat sensitif terhadap tegangan.

Gunakan **3.3V yang stabil** dari ESP32 atau regulator eksternal.

---

### Tambahkan Kapasitor
Disarankan menambahkan kapasitor: 10µF – 100µF


di antara **VCC dan GND** pada modul NRF24L01 untuk mencegah noise dan drop tegangan.

---

### Gunakan Kabel Pendek
Untuk menjaga stabilitas komunikasi SPI, gunakan kabel jumper yang **pendek dan rapi**.

---

# 7. Kesimpulan
Dengan konfigurasi wiring ini, ESP32 dapat berkomunikasi dengan modul NRF24L01 menggunakan protokol SPI untuk mengirim dan menerima data secara wireless pada frekuensi **2.4 GHz**.

Konfigurasi ini umum digunakan pada aplikasi seperti:

- Wireless sensor network
- Remote control system
- IoT communication
- Robot communication

