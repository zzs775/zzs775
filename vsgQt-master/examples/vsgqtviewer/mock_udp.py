import socket
import struct
import time
import math

UDP_IP = "127.0.0.1"
UDP_PORT = 19999

# 创建 UDP Socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# 模拟一个飞机的初始状态 (16字节ID, 32字节Name)
flight_id = b'F-16A_01'[:15].ljust(16, b'\0')
flight_name = b'F-16C Fighting Falcon'[:31].ljust(32, b'\0')
lat, lon, alt = 39.9, 116.4, 7000.0 # 北京附近
pitch, yaw, roll = 0.0, 45.0, 0.0 # 朝向东北

print(f"开始向 {UDP_IP}:{UDP_PORT} 发送测试数据...")

try:
    while True:
        # 简单的圆周运动计算
        lon += 0.001 * math.cos(math.radians(yaw))
        lat += 0.001 * math.sin(math.radians(yaw))
        yaw = (yaw + 1.0) % 360 # 缓慢转弯
        roll = 15.0 # 保持一定倾角转弯

        # 使用 struct 打包成二进制字节流 (与C++的UdpDataPacket对应)
        # char id[16] -> 16s
        # char name[32] -> 32s
        # 6个double: lat, lon, alt, pitch, yaw, roll -> dddddd
        # bool destroyed -> ? (1字节 bool)
        packet = struct.pack('<16s32sdddddd?', flight_id, flight_name, lat, lon, alt, pitch, yaw, roll, False)

        sock.sendto(packet, (UDP_IP, UDP_PORT))
        print(f"发送数据: ID={flight_id.decode('utf-8').strip(chr(0))} Name={flight_name.decode('utf-8').strip(chr(0))} Lat={lat:.4f}, Lon={lon:.4f}, Alt={alt:.1f}, Yaw={yaw:.1f}")

        time.sleep(0.05) # 20Hz 更新率
except KeyboardInterrupt:
    print("停止发送")
    sock.close()
