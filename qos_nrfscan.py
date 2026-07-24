# qos_nrfscan.py (v2)
# - Menampilkan SEMUA baris serial (tidak pernah diam)
# - DTR/RTS dimatikan: perbaikan utama — pySerial di Windows menahan ESP32
#   di mode bootloader sehingga board tidak pernah boot (gejala: hening total)
# Pakai : python qos_nrfscan.py [--port COM5] [--expect 26-48]
# Stop  : Ctrl+C -> ringkasan + CSV

import csv
import sys
import time
import argparse
import statistics
import serial
import serial.tools.list_ports

BAUD = 115200
TEORETIS_MS = 125 * (0.130 + 2.2)  # ~291 ms per sweep

def pilih_port(arg_port):
    ports = list(serial.tools.list_ports.comports())
    print("Port terdeteksi:")
    for p in ports:
        print(f"  {p.device} - {p.description}")
    if arg_port:
        return arg_port
    for p in ports:
        d = (p.device + p.description).lower()
        if any(k in d for k in ("usbserial", "usbmodem", "ch340", "cp210", "usb")):
            return p.device
    return ports[0].device if ports else None

def parse_expect(teks):
    bins = set()
    if teks:
        for bagian in teks.split(","):
            a, b = bagian.split("-")
            bins.update(range(int(a), int(b) + 1))
    return bins

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default=None)
    ap.add_argument("--expect", default="", help="rentang bin kanal, mis. 26-48")
    args = ap.parse_args()
    expect = parse_expect(args.expect)

    port = pilih_port(args.port)
    if not port:
        print("Tidak ada port serial sama sekali. Colok node dulu.")
        sys.exit(1)

    # PENTING: buka port TANPA menyalakan DTR/RTS, lalu reset board manual
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = BAUD
    ser.timeout = 1
    ser.dtr = False
    ser.rts = False
    ser.open()
    # Kedipkan DTR/RTS sesaat untuk me-reset ESP32 secara normal (bukan bootloader)
    ser.rts = True; time.sleep(0.1); ser.rts = False

    print(f"\nMembuka {port} @ {BAUD} — menunggu data... (Ctrl+C untuk berhenti)")
    print("Kalau 5 detik hening: tekan tombol RESET/EN di board node.\n")

    durasi_ms, aktif_per_sweep = [], []
    n_sweep = n_invalid = 0
    okupansi = [0] * 125
    hit_expect = 0
    fp_bins = 0
    t_data_terakhir = time.perf_counter()

    fname = f"qos_nrf_{time.strftime('%Y%m%d_%H%M%S')}.csv"
    fcsv = open(fname, "w", newline="", encoding="utf-8")
    w = csv.writer(fcsv)
    w.writerow(["seq", "durasi_ms", "panjang", "valid", "kanal_aktif", "data"])

    t_start = time.perf_counter()
    try:
        while True:
            line = ser.readline().decode("utf-8", errors="ignore").strip()

            if not line:
                if time.perf_counter() - t_data_terakhir > 10:
                    print(">> 10 dtk hening. Tekan RESET di node, atau cek: "
                          "firmware uji sudah di-upload? port benar? "
                          "monitor serial lain sudah ditutup?")
                    t_data_terakhir = time.perf_counter()
                continue

            t_data_terakhir = time.perf_counter()

            if not line.startswith("NRFQOS,"):
                print(f"[SERIAL] {line}")   # semua baris lain ditampilkan apa adanya
                continue

            try:
                _, s_seq, s_dur, s_len, s_valid, s_act, data = line.split(",", 6)
            except ValueError:
                continue

            n_sweep += 1
            d_ms = int(s_dur) / 1000.0
            valid = (len(data) == 125 and all(c in "01" for c in data))
            if not valid:
                n_invalid += 1
            durasi_ms.append(d_ms)
            aktif = data.count("1")
            aktif_per_sweep.append(aktif)
            for i, c in enumerate(data[:125]):
                if c == "1":
                    okupansi[i] += 1
                    if expect and i not in expect:
                        fp_bins += 1
            if expect and any(data[i] == "1" for i in expect if i < len(data)):
                hit_expect += 1

            w.writerow([s_seq, f"{d_ms:.2f}", len(data), int(valid), aktif, data])
            print(f"sweep {n_sweep:5d} | {d_ms:6.1f} ms | kanal aktif {aktif:3d}")
    except KeyboardInterrupt:
        pass
    finally:
        fcsv.close()
        ser.close()

    if not n_sweep:
        print("\nTidak ada data NRFQOS diterima sama sekali.")
        print("Lihat baris [SERIAL] di atas untuk tahu apa yang dicetak board.")
        return

    dur_total = time.perf_counter() - t_start
    jitter = (sum(abs(durasi_ms[i] - durasi_ms[i-1])
                  for i in range(1, len(durasi_ms))) / (len(durasi_ms) - 1)
              if len(durasi_ms) > 1 else 0.0)

    print("\n" + "=" * 68)
    print("RINGKASAN KINERJA MODUL AKUISISI nRF24")
    print("=" * 68)
    print(f"Jumlah sweep        : {n_sweep} dalam {dur_total:.0f} s "
          f"({n_sweep/dur_total:.2f} sweep/detik)")
    print(f"Waktu akuisisi (ms) : rata={statistics.mean(durasi_ms):.1f} "
          f"min={min(durasi_ms):.1f} max={max(durasi_ms):.1f} "
          f"std={statistics.pstdev(durasi_ms):.2f} (teoretis ~{TEORETIS_MS:.0f} ms)")
    print(f"Jitter akuisisi     : {jitter:.2f} ms")
    print(f"Integritas data     : {100.0*(n_sweep-n_invalid)/n_sweep:.2f} % "
          f"({n_invalid} sweep tidak valid)")
    print(f"Kanal aktif/sweep   : rata={statistics.mean(aktif_per_sweep):.2f} "
          f"min={min(aktif_per_sweep)} max={max(aktif_per_sweep)}")

    if expect:
        total_act = sum(okupansi)
        print(f"\nValidasi sumber uji (bin {min(expect)}-{max(expect)}):")
        print(f"  Hit rate deteksi  : {100.0*hit_expect/n_sweep:.2f} % sweep")
        print(f"  Aktif di luar bin : {fp_bins}/{total_act} "
              f"({100.0*fp_bins/total_act if total_act else 0:.2f} %)")
    else:
        fp_rate = 100.0 * sum(okupansi) / (125 * n_sweep)
        print(f"\nOkupansi total (idle = false positive rate): {fp_rate:.3f} %")

    top = sorted(range(125), key=lambda i: okupansi[i], reverse=True)[:10]
    print("\n10 kanal paling aktif:")
    for i in top:
        if okupansi[i] == 0:
            break
        bar = "#" * int(40 * okupansi[i] / max(okupansi))
        print(f"  ch {i:3d} ({2400+i} MHz) : {okupansi[i]:5d} {bar}")
    print(f"\nCSV tersimpan: {fname}")

if __name__ == "__main__":
    main()