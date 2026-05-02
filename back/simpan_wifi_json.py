import serial
import json
import time
import re
import os
from datetime import datetime

# Konfigurasi Serial - SESUAIKAN DENGAN PORT ANDA!
SERIAL_PORT = '/dev/cu.usbserial-0001'
BAUD_RATE = 115200

# Nama file JSON utama
MAIN_JSON_FILE = "wifi_scans_history.json"

def init_json_file():
    """Inisialisasi file JSON jika belum ada"""
    if not os.path.exists(MAIN_JSON_FILE):
        # Buat struktur awal
        initial_data = {
            "device": "ESP32 WiFi Scanner",
            "total_scans": 0,
            "scans": []
        }
        with open(MAIN_JSON_FILE, 'w') as f:
            json.dump(initial_data, f, indent=2)
        print(f"📁 File {MAIN_JSON_FILE} dibuat!")

def tambah_ke_json(data_baru):
    """Tambahkan data scan baru ke file JSON utama"""
    try:
        # Baca data yang sudah ada
        with open(MAIN_JSON_FILE, 'r') as f:
            existing_data = json.load(f)
        
        # Tambah timestamp ke data baru
        scan_entry = {
            "scan_time": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "timestamp_esp": data_baru.get("timestamp"),
            "total_networks": data_baru.get("total_networks"),
            "networks": data_baru.get("networks", [])
        }
        
        # Tambahkan ke array scans
        existing_data["scans"].append(scan_entry)
        existing_data["total_scans"] += 1
        
        # Simpan kembali ke file
        with open(MAIN_JSON_FILE, 'w') as f:
            json.dump(existing_data, f, indent=2)
        
        print(f"✅ Data ditambahkan ke {MAIN_JSON_FILE}")
        print(f"📊 Total scans sekarang: {existing_data['total_scans']}")
        
    except Exception as e:
        print(f"❌ Error menambah data: {e}")

def extract_json(text):
    """Ekstrak JSON dari teks yang mungkin ada tambahan"""
    # Cari pola JSON object { ... }
    json_pattern = r'\{.*\}'
    match = re.search(json_pattern, text)
    if match:
        return match.group()
    return None

def baca_dari_esp32():
    """Baca data JSON dari ESP32 via Serial"""
    try:
        # Inisialisasi file JSON
        init_json_file()
        
        # Buka koneksi serial
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"🔌 Terhubung ke {SERIAL_PORT}")
        print(f"📁 Menyimpan semua data ke: {MAIN_JSON_FILE}")
        print("📡 Menunggu data dari ESP32...")
        print("Tekan Ctrl+C untuk berhenti\n")
        
        buffer = ""
        scan_counter = 0
        
        while True:
            if ser.in_waiting:
                # Baca satu baris
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                # Abaikan baris yang bukan JSON
                if line and not line.startswith(('Mengirim', 'Memulai', 'Scan', '---', 'Menunggu')):
                    buffer += line
                    
                    # Coba cari JSON di dalam buffer
                    json_str = extract_json(buffer)
                    
                    if json_str:
                        try:
                            # Parse JSON
                            data = json.loads(json_str)
                            
                            scan_counter += 1
                            
                            # Tampilkan preview
                            print(f"\n📥 Scan #{scan_counter}: {datetime.now().strftime('%H:%M:%S')}")
                            print(f"   Timestamp ESP: {data.get('timestamp')}")
                            print(f"   Total networks: {data.get('total_networks')}")
                            
                            # Preview beberapa SSID
                            networks = data.get('networks', [])
                            if networks:
                                print("   SSIDs:", ", ".join([n.get('ssid', 'Unknown')[:15] for n in networks[:3]]))
                                if len(networks) > 3:
                                    print(f"   ... dan {len(networks)-3} lainnya")
                            
                            # TAMBAHKAN KE FILE JSON UTAMA
                            tambah_ke_json(data)
                            
                            # Reset buffer setelah berhasil parse
                            buffer = ""
                            
                        except json.JSONDecodeError as e:
                            print(f"❌ Error parsing JSON: {e}")
                            # Reset buffer kalau error
                            buffer = ""
                else:
                    # Tampilkan status dari ESP32 (optional)
                    if line:
                        print(f"📟 {line}")
            
            time.sleep(0.1)
            
    except serial.SerialException as e:
        print(f"❌ Error serial: {e}")
        print("💡 Cek:")
        print("   - Apakah ESP32 terhubung?")
        print("   - Apakah port sudah benar?")
        print("   - Apakah Serial Monitor sudah ditutup?")
    except KeyboardInterrupt:
        print("\n👋 Program dihentikan user")
        print(f"📁 Data tersimpan di {MAIN_JSON_FILE}")
    finally:
        if 'ser' in locals():
            ser.close()

if __name__ == "__main__":
    print("=" * 50)
    print("  WiFi Scanner Data Logger")
    print("  (Semua scan dalam 1 file JSON)")
    print("=" * 50)
    baca_dari_esp32()