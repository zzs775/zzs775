import os
import json
import time
from socket import *
from threading import Thread
import threading
import socket as socketName
import numpy as np
from .. import SOCKETIO
#from matplotlib.pyplot import *
from sqlalchemy import and_, or_
from . import acmi as Acmi
from flaskr import db,r_db
from sys import path
from . import parseData as UAV
from flaskr.modelList.models import platformList
from flaskr.SimMgr.funcationalities import UserMgr, UDPPortsShift, DomainId, check_duplicate_values, GlobalDomainIncrementor, find_new_domain, UDPServers
from flaskr.SimMgr.SocketInterface import EventInterface
from flask_socketio import SocketIO, emit, join_room, leave_room, close_room, rooms, disconnect
from flask import Flask, render_template, session, request, copy_current_request_context, current_app

path.append(os.path.abspath('.'))

acmi = Acmi.Acmi()
uav = UAV.Kinematics()
OperationThread = False
thread_lock = threading.Lock()
queue = []
ChanelDict = {}
AcmiDict = {}
RecorderFileHandles = {}
PlaneRecordCounts = {}
res = socketName.gethostbyname(socketName.gethostname())
OperationThread_RT = False

RecordDataForAiTraining = False



modellist = {}


def is_divisible(dividend, divisor):
    return dividend % divisor == 0
def is_file_empty(file_path):
    return os.stat(file_path).st_size == 0
RECORD_INTERVAL = 0.5

def convert_to_float(s):
    try:
        return float(s)
    except ValueError:
        return s

r_flag = 0

# def recv_msg_rt():
#     server_socket = socket(AF_INET, SOCK_DGRAM)
#     server_socket.bind((res, UAV.LOCALPORT+1))
#     print(f"UDP实时服务器已启动, 监听地址: {res}, 端口: {UAV.LOCALPORT+1}")
#     while True:
#         data, client_address = server_socket.recvfrom(1024)
#         try:
#             received_json = json.loads(data.decode('utf-8'))
#             list = UserMgr().GetInstance()
#             for value in list.OnlineList.values():
#                 SOCKETIO.emit(EventInterface.get("UdpServerRT", "Default"), received_json, to=value.SocketId)
#             # print(f"解析的JSON数据: {received_json}")
#         except json.JSONDecodeError:
#             continue


# def recv_msg(sid):
#     global r_flag
#     global uav
#     global PlaneRecordCounts
#     global RecorderFileHandles
#     global RecordFilePath
#     if RecordDataForAiTraining:
#         RecordFilePath = os.getcwd() + "\\Record_" + str(sid) + ".csv"
#         RecorderFileHandle = open(RecordFilePath, 'w', encoding="utf-8")
#         RecorderFileHandles[sid] = RecorderFileHandle
#     taskid = "CDMS01"
#     print("****************************recv uav data start***********************")
#     with socket(AF_INET,SOCK_DGRAM) as so:
#         #初始化删除垃圾数据
#         # r_db.delete(taskid)
#         so.bind((res,UAV.LOCALPORT))
#         print("udp数据地址:",{(res,UAV.LOCALPORT)})
#         while True:
#             data,c_addr = so.recvfrom(2048)
#             if c_addr[0] not in AcmiDict:
#                 AcmiDict[c_addr[0]] = Acmi.Acmi()
#             try:
#                 Flag = AcmiDict[c_addr[0]].line_do_parse(data.decode())
#                 # queue.append(data.decode())
#                 list = UserMgr().GetInstance()
#                 global CurrentChanel
#                 # flag[0]==1 :发送数据是位置
#                 # flag[0]==-1 :没有模型 发送状态如Start
#                 # flag[0]== 2 : 发送事件
#                 if Flag[0] == 1:
#                     web_dic = {
#                     "id":str(Flag[1]),
#                     "lon":float(AcmiDict[c_addr[0]].objects[Flag[1]].get_value("Longitude",AcmiDict[c_addr[0]].cur_reftime)),
#                     "lat":float(AcmiDict[c_addr[0]].objects[Flag[1]].get_value("Latitude",AcmiDict[c_addr[0]].cur_reftime)),
#                     "height":float(AcmiDict[c_addr[0]].objects[Flag[1]].get_value("Altitude",AcmiDict[c_addr[0]].cur_reftime) or 0.0 ),
#                     "heading":float(AcmiDict[c_addr[0]].objects[Flag[1]].get_value("Yaw",AcmiDict[c_addr[0]].cur_reftime) or 0.0 ),
#                     "pitch":float(AcmiDict[c_addr[0]].objects[Flag[1]].get_value("Pitch",AcmiDict[c_addr[0]].cur_reftime) or 0.0 ),
#                     "roll": float(AcmiDict[c_addr[0]].objects[Flag[1]].get_value("Roll",AcmiDict[c_addr[0]].cur_reftime) or 0.0 ),
#                     "color":str(AcmiDict[c_addr[0]].objects[Flag[1]].get_value("Color",AcmiDict[c_addr[0]].cur_reftime)).lower() or "Null",
#                     "name":str(AcmiDict[c_addr[0]].objects[Flag[1]].get_value("Name")) or "Null",
#                     "time":float(AcmiDict[c_addr[0]].cur_reftime),
#                     'type':'data',
#                     "url":"",
#                 }   
#                     # print("当前模型经度:",AcmiDict[c_addr[0]].objects[Flag[1]].get_value("Altitude",AcmiDict[c_addr[0]].cur_reftime),";时间:",AcmiDict[c_addr[0]].cur_reftime,"客户端ip:",c_addr[0])
#                     # if c_addr[0] in ChanelDict:
#                     #     CurrentChanel = ChanelDict[c_addr[0]]
#                     # else:
#                     #     #当前客户端没有通道
#                     #     CurrentChanel = ''
#                     if is_divisible(web_dic["time"], RECORD_INTERVAL) and RecordDataForAiTraining:
#                         if PlaneRecordCounts.get(web_dic["id"]) == None:
#                             PlaneRecordCounts[web_dic["id"]] = 0
#                         localtext = data.decode()
#                         parts = localtext.split(',')
#                         LocalID = parts[0]
#                         info_dict = {}
#                         for part in parts[1:]:
#                             key, value = part.split('=')
#                             key = key.strip()
#                             value = value.strip()
#                             info_dict[key] = value
#                         if 'T' not in info_dict or 'Name' not in info_dict or 'Color' not in info_dict:
#                             raise ValueError("Missing required keys in input string")
#                         t_values = info_dict['T'].split('|')
#                         if len(t_values) < 3:
#                             raise ValueError("Incomplete T values in input string")
#                         lon = t_values[0]
#                         lat = t_values[1]
#                         alt = t_values[2]
#                         name = info_dict['Name']
#                         camp = info_dict['Color']
#                         PlaneRecordCounts[web_dic["id"]] += RECORD_INTERVAL
#                         stmp = PlaneRecordCounts[web_dic["id"]]
#                         new_str = f"{stmp},{LocalID},{name},{camp},{lon},{lat},{alt}\n"
#                         RecorderFileHandle.write(new_str)

#                     CurrentChanel = AcmiDict[c_addr[0]].Chanel
#                     for value in list.OnlineList.values():  
#                         # print(value)
#                         #没有频道 发给ip相同的
#                         if CurrentChanel == '' or value.Ip == "127.0.0.1":
#                             SOCKETIO.emit(EventInterface.get("UdpServer", "Default"),
#                         [web_dic],to=value.SocketId)
#                             break
#                         elif value.Chanel == CurrentChanel:
#                             SOCKETIO.emit(EventInterface.get("UdpServer", "Default"),
#                         [web_dic],to=value.SocketId)
#                             break
#                 elif Flag[0] == -1:
#                     #当前为频道属性
#                     if not Flag[1]== "Start" and not Flag[1]== "Done":
#                         ChanelDict[c_addr[0]] = Flag[1]
#                         continue
#                     # if c_addr[0] in ChanelDict:
#                     #     CurrentChanel = ChanelDict[c_addr[0]]
#                     # else:
#                     #     CurrentChanel = ''
                    
#                     CurrentChanel = AcmiDict[c_addr[0]].Chanel
#                     for value in list.OnlineList.values():
#                         if CurrentChanel == '' or value.Ip == "127.0.0.1":
#                             SOCKETIO.emit(EventInterface.get("UdpServer", "Default"),
#                     [Flag[1],Flag[2]],to=value.SocketId)
#                             break
#                         if value.Chanel == CurrentChanel:
#                             SOCKETIO.emit(EventInterface.get("UdpServer", "Default"),
#                     [Flag[1],Flag[2]],to=value.SocketId)
#                             break
#                 elif Flag[0] == 2:
#                     # print(Flag)
#                     for value in list.OnlineList.values():  
#                         # print(value)
#                         #没有频道 发给ip相同的
#                         if CurrentChanel == '' or value.Ip == "127.0.0.1":
#                             SOCKETIO.emit(EventInterface.get("UdpServer", "Default"),
#                         [Flag[1]],to=value.SocketId)
#                             break
#                         elif value.Chanel == CurrentChanel:
#                             SOCKETIO.emit(EventInterface.get("UdpServer", "Default"),
#                         [Flag[1]],to=value.SocketId)
#                             break    
#             except Exception as e :
#                 print("数据格式异常请检查",e)
#     if RecordDataForAiTraining:
#         RecorderFileHandle.close()

def serverthread(ss):
    global uav
    so,addr = ss
    print('give a handshake: ')
    #print(UAV.HandShakeData + "\n")
    #d = "XtraLib.Stream.0\r\nTacview.RealTimeTelemetry.0\rHost username\r"
    so.send(UAV.HandShakeData1.encode('utf-8'))
    so.send(UAV.HandShakeData2.encode('utf-8'))
    so.send(UAV.HandShakeData3.encode('utf-8'))
    so.send(b'\x00')

    data = so.recv(1024)
    t = time.strftime(UAV.TelReferenceTimeFormat).encode('utf-8')
    
    tt = time.time()
    so.send(UAV.TelFileHeader.encode('utf-8'))
    so.send(t)
    global queue 
    while len(queue) != 0:
        time.sleep(0.1)
        # delta_t = time.time() - tt
        dic = queue.pop(0)
        # data = format(UAV.TelDataFormat%(dic["time"],dic["lon"],dic["lat"],dic["height"],0,0,0))
        # print(data)
        so.send(dic.encode('utf-8'))

def startSendThread(th):
    with socket(AF_INET,SOCK_STREAM,IPPROTO_TCP) as so:
        # conf_file = open(path[0]+"\\flaskr\\udpServer\\config.json","r")
        # conf = json.load(conf_file)
        print("trackView地址:",{(UAV.acmiServerIp,UAV.acmiServerPort)})
        # so.bind((conf["serverip"],conf["serverport"]))
        so.bind((UAV.acmiServerIp,UAV.acmiServerPort))
        so.listen()
        print("listen")
        while True:
            print("wait for connect")
            ss = so.accept()
            print("a client connected")
            th.append(Thread(target=serverthread,args=(ss,)))
            print(th)
            th[-1].start()



@SOCKETIO.event
def connect(message):
    client_sid = request.sid
    global OperationThread
    global OperationThread_RT
    global modellist
    print("gogo",message)
    global GlobalDomainIncrementor    
    modellist = platformList.query.all()
    if DomainId.get(client_sid) == None:
        DomainId[client_sid] = -1
        LocalDict = DomainId.copy()
        LocalDict[client_sid] = GlobalDomainIncrementor
        if not check_duplicate_values(LocalDict):
            DomainId[client_sid] = GlobalDomainIncrementor
            UDPPortsShift[client_sid] = GlobalDomainIncrementor
            UDPServers[client_sid] = UDPServer(client_sid)
            del LocalDict
            GlobalDomainIncrementor += 1
        else:
            try:
                while True:
                    new_domain = find_new_domain(DomainId.values())
                    GlobalDomainIncrementor = new_domain
                    DomainId[client_sid] = -1
                    LocalDict = DomainId.copy()
                    LocalDict[client_sid] = GlobalDomainIncrementor
                    if not check_duplicate_values(LocalDict):
                        DomainId[client_sid] = GlobalDomainIncrementor
                        UDPPortsShift[client_sid] = GlobalDomainIncrementor
                        UDPServers[client_sid] = UDPServer(client_sid)
                        del LocalDict
                        GlobalDomainIncrementor += 1
                        break
            except ValueError as ve:
                # 找不到可用的domain id了
                emit(EventInterface.get("ServiceMsg", "Default"), {'Chunk': "Connect Failed", 'Reason': "任务已满，请切换至其他节点。"})
                return
            except Exception as e:
                # 程序本身错误
                raise("登入错误:", e)



    # with thread_lock:
    #     if not OperationThread:
            #OperationThread = SOCKETIO.start_background_task(udp_server, HOST, PORT)
            # th = []
            # OperationThread = SOCKETIO.start_background_task(recv_msg, request.sid)
            # SOCKETIO.start_background_task(startSendThread,th)
            # OperationThread_RT = SOCKETIO.start_background_task(recv_msg_rt)


class UDPServer:
    def __init__(self, client_sid):
        self.client_sid = client_sid
        self.running = True
        self.IsFormalData = False

        self.RecentMissionName = ""
        self.LocalUDPPort = UAV.LOCALPORT + UDPPortsShift.get(self.client_sid)
        self.AcmiInstance = Acmi.Acmi()
        self.thread = threading.Thread(target=self.UdpMsgRecvr)
        self.RTthread = threading.Thread(target=self.UdpMsgRecvrRT)
        self.PlaneRecordCount = 0
        self.LocalUDPIP = res

        self.MissionStart = False
        self.RecordFilePath = None
        self.RecordFilePathAcmi = None
        self.RecorderFileHandle = None
        self.RecorderACMIHandle = None
        if RecordDataForAiTraining:
            self.RecordFilePath = os.getcwd() + "\\Record_" + str(self.client_sid) + ".csv"
            self.RecorderFileHandle = open(self.RecordFilePath, 'w', encoding="utf-8")
        self.thread.start()
        self.RTthread.start()

    def UdpMsgRecvr(self):
        with socket(AF_INET,SOCK_DGRAM) as so:
            #初始化删除垃圾数据 每个套接字用一次
            # r_db.delete(taskid)
            so.settimeout(3)
            so.bind((res, self.LocalUDPPort))
            print("udp数据地址:",{(res,self.LocalUDPPort)})
            
            while True:
                try:
                    data,c_addr = so.recvfrom(2048)
                    # AcmiDict[c_addr[0]] = Acmi.Acmi()
                    DecodedData = data.decode()
                    if 'FileType' in DecodedData:
                        self.RecordFilePathAcmi = os.getcwd() + "\\simulationRecords\\" + str(self.RecentMissionName) + ".acmi" if self.RecentMissionName != '' else os.getcwd() + "\\simulationRecords\\" + time.strftime('%Y_%m_%d_%H_%M_%S', time.localtime()) + ".acmi"
                        self.RecorderACMIHandle = open(self.RecordFilePathAcmi, 'w', encoding="utf-8")
                        # self.RecorderACMIHandle.write(DecodedData + "\n")
                        self.MissionStart = True
                    if self.MissionStart:
                        self.RecorderACMIHandle.write(DecodedData)
                        # if self.IsFormalData:
                        #     self.RecorderACMIHandle.write(DecodedData)
                        # else:
                        #     if '#' in DecodedData: self.IsFormalData = True
                        #     self.RecorderACMIHandle.write(DecodedData + "\n")
                    Flag = self.AcmiInstance.line_do_parse(DecodedData)
                    # queue.append(data.decode())
                    #list = UserMgr().GetInstance()
                    global CurrentChanel
                    # flag[0]==1 :发送数据是位置
                    # flag[0]==-1 :没有模型 发送状态如Start
                    # flag[0]== 2 : 发送事件
                    if Flag[0] == 1:
                        web_dic = {
                        "id":str(Flag[1]),
                        "lon":float(self.AcmiInstance.objects[Flag[1]].get_value("Longitude",self.AcmiInstance.cur_reftime)),
                        "lat":float(self.AcmiInstance.objects[Flag[1]].get_value("Latitude",self.AcmiInstance.cur_reftime)),
                        "height":float(self.AcmiInstance.objects[Flag[1]].get_value("Altitude",self.AcmiInstance.cur_reftime) or 0.0 ),
                        "heading":float(self.AcmiInstance.objects[Flag[1]].get_value("Yaw",self.AcmiInstance.cur_reftime) or 0.0 ),
                        "pitch":float(self.AcmiInstance.objects[Flag[1]].get_value("Pitch",self.AcmiInstance.cur_reftime) or 0.0 ),
                        "roll": float(self.AcmiInstance.objects[Flag[1]].get_value("Roll",self.AcmiInstance.cur_reftime) or 0.0 ),
                        "color":str(self.AcmiInstance.objects[Flag[1]].get_value("Color",self.AcmiInstance.cur_reftime)).lower() or "Null",
                        "name":str(self.AcmiInstance.objects[Flag[1]].get_value("Name")) or "Null",
                        "time":float(self.AcmiInstance.cur_reftime),
                        'type':'data',
                        'url':'',
                    }
                        if web_dic['name'] in ['truckM923', 'transferCart', 'PLZ-05', 'WZ551', 'Radar_S400', 'Benson-class-destroyer', 'Chongqing-destroyer', 'Leopard2A4', 'M1A2Tank', 'M4A2ShermanMeduimTank', 'ArmyTruck', 'liaoNing', 'CVE-9']:
                            continue
                        # print("当前模型经度:",AcmiDict[c_addr[0]].objects[Flag[1]].get_value("Altitude",AcmiDict[c_addr[0]].cur_reftime),";时间:",AcmiDict[c_addr[0]].cur_reftime,"客户端ip:",c_addr[0])
                        # if c_addr[0] in ChanelDict:
                        #     CurrentChanel = ChanelDict[c_addr[0]]
                        # else:
                        #     #当前客户端没有通道
                        #     CurrentChanel = ''
                        # if int(web_dic['time']) == 10 :
                        #     SOCKETIO.emit('actiondetect', [{ "id":web_dic['id'], "timeStamp" : 100, "state" : 2 }],to=self.client_sid)

                        if is_divisible(web_dic["time"], RECORD_INTERVAL) and RecordDataForAiTraining:
                            if PlaneRecordCounts.get(web_dic["id"]) == None:
                                PlaneRecordCounts[web_dic["id"]] = 0
                            localtext = data.decode()
                            parts = localtext.split(',')
                            LocalID = parts[0]
                            info_dict = {}
                            for part in parts[1:]:
                                key, value = part.split('=')
                                key = key.strip()
                                value = value.strip()
                                info_dict[key] = value
                            if 'T' not in info_dict or 'Name' not in info_dict or 'Color' not in info_dict:
                                raise ValueError("Missing required keys in input string")
                            t_values = info_dict['T'].split('|')
                            if len(t_values) < 3:
                                raise ValueError("Incomplete T values in input string")
                            lon = t_values[0]
                            lat = t_values[1]
                            alt = t_values[2]
                            name = info_dict['Name']
                            camp = info_dict['Color']
                            PlaneRecordCounts[web_dic["id"]] += RECORD_INTERVAL
                            stmp = PlaneRecordCounts[web_dic["id"]]
                            new_str = f"{stmp},{LocalID},{name},{camp},{lon},{lat},{alt}\n"
                            self.RecorderFileHandle.write(new_str)
                        CurrentChanel = self.AcmiInstance.Chanel
                        # for value in list.OnlineList.values():
                        #     if CurrentChanel == '' or value.Ip == "127.0.0.1":
                        #         SOCKETIO.emit(EventInterface.get("UdpServer", "Default"),
                        #     [web_dic],to=value.SocketId)
                        #         break
                        #     elif value.Chanel == CurrentChanel:
                        #         SOCKETIO.emit(EventInterface.get("UdpServer", "Default"),
                        #     [web_dic],to=value.SocketId)
                        #         break
                        SOCKETIO.emit(EventInterface.get("UdpServer", "Default"), [web_dic],to=self.client_sid)
                    elif Flag[0] == -1:
                        #当前为频道属性
                        if not Flag[1]== "Start" and not Flag[1]== "Done":
                            ChanelDict[c_addr[0]] = Flag[1]
                            continue
                        CurrentChanel = self.AcmiInstance.Chanel
                        SOCKETIO.emit(EventInterface.get("UdpServer", "Default"), [Flag[1],Flag[2]],to=self.client_sid)
                        
                    elif Flag[0] == 2:
                        # print(Flag)
                        # for value in list.OnlineList.values():  
                        #     # print(value)
                        #     #没有频道 发给ip相同的
                        #     if CurrentChanel == '' or value.Ip == "127.0.0.1":
                        #         SOCKETIO.emit(EventInterface.get("UdpServer", "Default"),
                        #     [Flag[1]],to=value.SocketId)
                        #         break
                        #     elif value.Chanel == CurrentChanel:
                        #         SOCKETIO.emit(EventInterface.get("UdpServer", "Default"),
                        #     [Flag[1]],to=value.SocketId)
                        #         break    
                        SOCKETIO.emit(EventInterface.get("UdpServer", "Default"), [Flag[1]],to=self.client_sid)
                except OSError as oe:
                    if not self.running: break
                except Exception as e :
                    print("数据格式异常请检查",e)
        if RecordDataForAiTraining:
            self.RecorderFileHandle.close()
            self.RecorderACMIHandle.close()
            self.MissionStart = False

    def UdpMsgRecvrRT(self):
        import struct
        
        # 用于存储来自不同客户端的分片数据
        # 结构: {client_address: {total_chunks: int, received_chunks: {index: data_chunk}, ...}}
        chunk_buffers = {}
        
        with socket(AF_INET, SOCK_DGRAM) as so:
            # 初始化删除垃圾数据 每个套接字用一次
            # so.settimeout(5)
            so.bind((res, self.LocalUDPPort - 10000))
            print(f"UDP实时服务器已启动, 监听地址: {res}, 端口: {self.LocalUDPPort - 10000}")
            
            # 获取UserMgr实例一次，避免重复创建
            user_mgr = UserMgr().GetInstance()
            
            while True:
                try:
                    # 设置合理的缓冲区大小，UDP单包最大约65507字节
                    data, c_addr = so.recvfrom(65536)
                    
                    # 检查数据长度是否至少包含头部
                    if len(data) >= 8 and data[0] != ord('{'):
                        # 解析头部: 4字节总片数 + 4字节当前片索引
                        total_chunks, chunk_index = struct.unpack('!II', data[:8])
                        print(f"收到来自 {c_addr} 的分片 {chunk_index}/{total_chunks}")
                        
                        # 获取实际数据部分
                        chunk_data = data[8:]
                        
                        # 初始化或更新客户端的分片缓冲区
                        if c_addr not in chunk_buffers:
                            chunk_buffers[c_addr] = {
                                'total_chunks': total_chunks,
                                'received_chunks': {},
                                'last_activity': time.time()
                            }
                        
                        # 更新客户端缓冲区信息
                        client_buffer = chunk_buffers[c_addr]
                        client_buffer['total_chunks'] = total_chunks  # 更新以防不一致
                        client_buffer['received_chunks'][chunk_index] = chunk_data
                        client_buffer['last_activity'] = time.time()
                        
                        # 检查是否所有分片都已收到
                        if len(client_buffer['received_chunks']) == total_chunks:
                            print(f"收到客户端 {c_addr} 的所有分片，正在重组数据...")
                            
                            # 按顺序重组数据
                            complete_data = bytearray()
                            for i in range(total_chunks):
                                if i in client_buffer['received_chunks']:
                                    complete_data.extend(client_buffer['received_chunks'][i])
                            
                            # 清理已完成的缓冲区
                            del chunk_buffers[c_addr]
                            
                            # 尝试解析为JSON
                            try:
                                received_json = json.loads(complete_data.decode('utf-8'))
                                
                                # 处理3D数据
                                if received_json.get("Type") == "3D" and "Data" in received_json:
                                    print(f"成功解析3D数据，共 {len(received_json['Data'])} 个点")
                                    for value in user_mgr.OnlineList.values():
                                        SOCKETIO.emit("3Ddata", received_json["Data"], to=value.SocketId)
                                # 处理2D数据
                                elif received_json.get("Type") == "2D":
                                    # 可以根据需要添加2D数据的处理逻辑
                                    
                                    for value in user_mgr.OnlineList.values():
                                        SOCKETIO.emit("2Ddata", received_json, to=value.SocketId)
                                    pass
                            except json.JSONDecodeError:
                                print(f"JSON解析失败: 无法解析重组后的数据")
                            except Exception as e:
                                print(f"处理重组数据时出错: {e}")
                    else:
                        # 尝试直接解析为普通JSON（非分片数据）
                        try:
                            received_json = json.loads(data.decode('utf-8'))
                            
                            if received_json.get("Type") == "3D" and "Data" in received_json:
                                for value in user_mgr.OnlineList.values():
                                    SOCKETIO.emit("3Ddata", received_json["Data"], to=value.SocketId)
                            elif received_json.get("Type") == "2D":
                                # 2D数据处理逻辑
                                for value in user_mgr.OnlineList.values():
                                    SOCKETIO.emit("2Ddata", received_json, to=value.SocketId)
                                pass
                        except json.JSONDecodeError:
                            # 解析失败，打印收到的非JSON字符串（可选）
                            # print(f"收到非JSON字符串: {data.decode('utf-8')}")
                            pass
                            
                    # 清理超时的缓冲区（防止内存泄漏）
                    current_time = time.time()
                    timeout_clients = [addr for addr, buffer in chunk_buffers.items() 
                                      if current_time - buffer['last_activity'] > 30]  # 30秒超时
                    for addr in timeout_clients:
                        del chunk_buffers[addr]
                        print(f"清理超时客户端 {addr} 的分片缓冲区")
                except Exception as e:
                    print(f"接收UDP数据时出错: {e}")
                    continue