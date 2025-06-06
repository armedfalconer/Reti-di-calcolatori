from abc import ABC, abstractmethod
from collections import deque

class SharedMemory:
    def __init__(self):
        self.memory = deque()

    def put(self, frame):
        self.memory.append(frame)

    def get(self):
        if self.memory:
            return self.memory.popleft()
        return None

class ApplicationClass(ABC):
    @abstractmethod
    def getData(self):
        pass

    @abstractmethod
    def receiveData(self, data):
        pass

class MyApp(ApplicationClass):
    def getData(self):
        return "Application Layer message"

    def receiveData(self, data):
        print(f"Application Layer received: {data}")

class SegmentsClass:
    def __init__(self, data, max_size=10):
        self.segments = [data[i:i+max_size] for i in range(0, len(data), max_size)]

class TransportClass:
    def createSegments(self, data):
        return SegmentsClass(data)

class PacketsClass:
    def __init__(self, segments):
        self.packets = segments

class InternetworkClass:
    def __init__(self, IPAddress):
        self.IP = IPAddress

    def createPackets(self, segments):
        return PacketsClass(segments)

class FramesClass:
    def __init__(self, packets):
        self.frames = packets

class NetworkAccessClass:
    def __init__(self):
        self.sharedMemory = SharedMemory()

    def createFrames(self, packets):
        return FramesClass(packets)

    def putFrame(self, frame):
        self.sharedMemory.put(frame)

    def getFrame(self):
        return self.sharedMemory.get()

class DeviceClass:
    def __init__(self, app, transport, internetwork, netAccess):
        self.app = app
        self.transport = transport
        self.internetwork = internetwork
        self.netAccess = netAccess

    def encapsulate(self):
        data = self.app.getData()
        segments = self.transport.createSegments(data).segments
        packets = self.internetwork.createPackets(segments).packets
        return self.netAccess.createFrames(packets)

    def send(self, destDevice):
        frame = self.encapsulate()
        print(f"[{self.internetwork.IP}] sending frame to [{destDevice.internetwork.IP}]")
        destDevice.netAccess.putFrame(frame)

    def receive(self):
        return self.netAccess.getFrame()

    def decapsulate(self):
        frame = self.receive()
        if not frame:
            print(f"[{self.internetwork.IP}] no frame to decapsulate.")
            return
        segments = frame.frames
        data = "".join(segments)
        self.app.receiveData(data)

def main():
    firstDevice = DeviceClass(
        MyApp(),
        TransportClass(),
        InternetworkClass('10.4.3.2'),
        NetworkAccessClass()
    )

    secondDevice = DeviceClass(
        MyApp(),
        TransportClass(),
        InternetworkClass('10.4.3.3'),
        NetworkAccessClass()
    )

    firstDevice.send(secondDevice)
    secondDevice.decapsulate()

if __name__ == "__main__":
    main()
