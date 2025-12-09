import serial
from pythonosc.udp_client import SimpleUDPClient

ser = serial.Serial("COM7", 9600)   # 修改成你的 Arduino Port
client = SimpleUDPClient("127.0.0.1", 4560)

while True:
    line = ser.readline().decode(errors="ignore").strip()
    if line:
        print("SEND:", line)
        client.send_message("/event", line)
