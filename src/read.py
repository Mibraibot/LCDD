import serial
import json

PORT = '/dev/cu.usbserial-0001'   # linux
# PORT = "COM5"         # windows

BAUD = 115200

ser = serial.Serial(PORT, BAUD)

print("Serial reader started...")

while True:
    try:
        line = ser.readline().decode().strip()

        if line.startswith("{"):
            data = json.loads(line)

            with open("data.json","w") as f:
                json.dump(data,f)

            print("SCAN", data["scan"])

    except Exception as e:
        print("error:", e)