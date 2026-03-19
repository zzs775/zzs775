#!/usr/bin/env python3
"""
ACMI → UDP JSON 回放发送器 (Raw Mode)
用途：读取 ACMI (Tacview) 格式飞行仿真回放文件，按时间戳节奏将原始文本数据通过 UDP 实时发送给 C++ Qt 程序。

使用方式：
    py acmi_replay_sender.py --file flight.acmi --speed 1.0
    py acmi_replay_sender.py --demo
"""

import socket
import time
import math
import argparse
import sys
import os

class AcmiRawSender:
    """ACMI UDP 原始文本发送器"""

    def __init__(self, ip: str = "127.0.0.1", port: int = 19999, speed: float = 1.0):
        self.ip = ip
        self.port = port
        self.speed = speed
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def send_line(self, line: str):
        """发送单行原始数据"""
        payload = line.encode('utf-8')
        self.sock.sendto(payload, (self.ip, self.port))

    def replay_file(self, filepath: str, loop: bool = False):
        if not os.path.exists(filepath):
            print(f"错误：文件不存在 → {filepath}")
            return

        print(f"=== ACMI 原生回放开始 ===")
        print(f"  文件路径: {filepath}")
        print(f"  回放速度: {'极速灌送' if self.speed <= 0 else f'{self.speed}x'}")
        print(f"  发送目标: {self.ip}:{self.port}")
        print(f"  循环回放: {'是' if loop else '否'}\n")

        iteration = 0
        try:
            while True:
                iteration += 1
                prev_sim_time = None
                frame_count = 0

                # 每次回放前发送场景重置信号，让 C++ 清空旧数据
                self.send_line("SimState=Start")
                time.sleep(0.1)  # 给 C++ 一点时间处理重置
                
                with open(filepath, 'r', encoding='utf-8-sig') as f:
                    for line_raw in f:
                        line = line_raw.strip()
                        if not line:
                            continue

                        # 跳过文件头和全局属性 (除了发送)
                        if line.startswith('FileType=') or line.startswith('FileVersion='):
                            continue
                        
                        if line.startswith('0,'):
                           # 全局属性直接忽略
                           continue

                        # 时间戳行
                        if line.startswith('#'):
                            ts_str = line[1:]
                            try:
                                timestamp = float(ts_str)
                            except ValueError:
                                continue
                            
                            # 睡眠等待（如果速度>0 则按倍速，速度<=0 表示极速灌送）
                            if prev_sim_time is not None and self.speed > 0:
                                sim_delta = timestamp - prev_sim_time
                                real_sleep = sim_delta / self.speed
                                if real_sleep > 0:
                                    time.sleep(real_sleep)
                            elif self.speed <= 0:
                                # 极速模式：每50帧休眠1ms防止UDP丢包
                                if frame_count % 50 == 0:
                                    time.sleep(0.001)
                            
                            prev_sim_time = timestamp
                            frame_count += 1
                            
                            if frame_count % 50 == 0:
                                print(f"  [回放 #{iteration}] 帧数={frame_count}, t={timestamp:.2f}s")
                            # 不直接发送时间戳，由实体数据行自己携带时间，或者C++端收到什么就认为是什么时间。
                            # （为了简单，我们可以发送时间戳行给C++，也可以不发。标准做法是发过去让C++同步时间）
                            self.send_line(line)
                            continue

                        # 实体数据行 (包含销毁行 -ID 和 数据行 ID,T=...)
                        # 发送原始行
                        self.send_line(line)

                if not loop:
                    break

                print(f"  --- 循环回放第 {iteration} 轮结束，等待 1 秒重新开始 ---")
                time.sleep(1.0)
                
        except KeyboardInterrupt:
            print("\n用户中断回放")

        print("=== ACMI 回放结束 ===")
        self.sock.close()


def create_demo_acmi(filepath: str):
    """生成一个演示用的 ACMI 文件（两架 F-16 对飞）"""
    lines = [
        "FileType=text/acmi/tacview",
        "FileVersion=2.1",
        "0,ReferenceTime=2025-07-30T08:35:22Z",
    ]

    # 生成 600 帧（约 30 秒 @ 20fps）
    dt = 0.05  # 20Hz
    for i in range(600):
        t = i * dt
        lines.append(f"#{t:.2f}")

        # F-16A (ID=800=0x320) 圆周运动
        angle1 = math.radians(i * 0.6)
        lon1 = 116.4 + 0.05 * math.cos(angle1)
        lat1 = 39.9 + 0.05 * math.sin(angle1)
        alt1 = 8000.0
        yaw1 = angle1 + math.pi / 2  # 切线方向
        roll1 = math.radians(15.0)    # 15° 倾斜转弯
        pitch1 = 0.0

        line1 = f"320,T={lon1:.6f}|{lat1:.6f}|{alt1:.1f}|{roll1:.5f}|{pitch1:.5f}|{yaw1:.5f}"
        if i == 0:
            line1 += ",Name=F-16A,Color=RedTeam"
        lines.append(line1)

        # Su-27 (ID=900=0x384) 相对方向圆周运动
        angle2 = math.radians(i * 0.6 + 180)
        lon2 = 116.4 + 0.08 * math.cos(angle2)
        lat2 = 39.9 + 0.08 * math.sin(angle2)
        alt2 = 7500.0
        yaw2 = angle2 + math.pi / 2
        roll2 = math.radians(-10.0)
        pitch2 = math.radians(2.0)

        line2 = f"384,T={lon2:.6f}|{lat2:.6f}|{alt2:.1f}|{roll2:.5f}|{pitch2:.5f}|{yaw2:.5f}"
        if i == 0:
            line2 += ",Name=Su-27,Color=BlueTeam"
        lines.append(line2)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')

    print(f"已生成演示 ACMI 文件: {filepath}\n")


def main():
    parser = argparse.ArgumentParser(description='ACMI 飞行回放 UDP 原生发送器')
    parser.add_argument('--file', '-f', type=str, help='ACMI 文件路径')
    parser.add_argument('--ip', type=str, default='127.0.0.1', help='目标 IP（默认 127.0.0.1）')
    parser.add_argument('--port', '-p', type=int, default=19999, help='目标端口（默认 19999）')
    parser.add_argument('--speed', '-s', type=float, default=10.0, help='回放速度（默认 10x，设为0=极速灌送）')
    parser.add_argument('--loop', '-l', action='store_true', help='循环回放')
    parser.add_argument('--demo', action='store_true', help='生成演示 ACMI 文件并回放')
    parser.add_argument('--fast', action='store_true', help='极速灌送模式（等价于 --speed 0）')

    args = parser.parse_args()

    # 如果没有指定文件且没有 --demo，显示帮助
    if not args.file and not args.demo:
        parser.print_help()
        print("\n提示：使用 --demo 生成并回放演示数据")
        sys.exit(1)

    # 演示模式
    if args.demo:
        demo_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "demo_flight.acmi")
        create_demo_acmi(demo_file)
        args.file = demo_file

    # 极速灌送模式
    if args.fast:
        args.speed = 0

    sender = AcmiRawSender(ip=args.ip, port=args.port, speed=args.speed)
    sender.replay_file(args.file, loop=args.loop)

if __name__ == '__main__':
    main()
