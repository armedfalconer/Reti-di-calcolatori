from abc import ABC, abstractmethod
from dataclasses import dataclass

@dataclass
class PDU(ABC):
    src: str
    dst: str

@dataclass
class Segment(PDU):
    payload: str
    
@dataclass
class Packet(PDU):
    payload: Segment

@dataclass
class Frame(PDU):
    payload: Packet


class Protocol(ABC):   
    @abstractmethod
    def encapsulate(self, payload):
        pass

    @abstractmethod
    def decapsulate(self, payload):
        pass

class AppProtocol(Protocol):
    def __init__(self, msg: str):
        self.msg = msg

    def encapsulate(self, payload: str) -> str:
        return payload
    
    def decapsulate(self, payload: str) -> str:
        return payload

class TransportProtocol(Protocol):
    def __init__(self, port: str, size: int):
        self.__size = size
        self.__port = port
        self.__dstPort: str = None
        self._next_seq = 0

    def setSize(self, newSize: int):
        self.__size = newSize   

    def setDst(self, newDstPort: str):
        self.__dstPort = newDstPort

    def encapsulate(self, payload: str) -> list[Segment]:
        if self.__dstPort is None:
            raise ValueError("Destination port not set")
        segments: list[Segment] = []
        for i in range(0, len(payload), self.__size):
            chunk = payload[i : i + self.__size]
            seg = Segment(
                        src=self.__port, 
                        dst=self.__dstPort, 
                        payload=chunk
            )
            segments.append(seg)
            self._next_seq += len(chunk)
        return segments
    
    def decapsulate(self, payload: list[Segment]) -> str:
        data = ''.join(segment.payload for segment in payload)
        return data

class InternetProtocol(Protocol):
    def __init__(self, IP: str):
        self.__srcIP = IP
        self.__dstIP = None

    def setDst(self, IP: str):
        self.__dstIP = IP
    
    def encapsulate(self, payload: list[Segment]) -> list[Packet]:
        if self.__dstIP is None:
            raise ValueError("Destination IP not set")
        
        packets = [
            Packet(src=self.__srcIP, dst=self.__dstIP, payload=seg)
            for seg in payload
        ]
        
        return packets
    
    def decapsulate(self, payload: list[Packet]) -> list[Segment]:
        segments = [pkt.payload for pkt in payload]
        return segments

class DataLinkProtocol(Protocol):
    def __init__(self, mac: str):
        self.__srcMAC = mac
        self.__dstMAC = None

    def setDst(self, mac: str):
        self.__dstMAC = mac

    def encapsulate(self, payload: list[Packet]) -> list[Frame]:
        if self.__dstMAC is None:
            raise ValueError("Destination MAC not set")
        
        frames = [
            Frame(src=self.__srcMAC, dst=self.__dstMAC, payload=p)
            for p in payload
        ]
        return frames

    def decapsulate(self, payload: list[Frame]) -> list[Packet]:
        packets = [f.payload for f in payload]
        return packets

class Device:
    def __init__(self, 
                 name: str,
                 app_msg: str, 
                 port: str, 
                 mss: int,
                 ip: str, 
                 mac: str):
        self.name = name
        self.app = AppProtocol(app_msg)
        self.transport = TransportProtocol(port, mss)
        self.internet = InternetProtocol(ip)
        self.datalink = DataLinkProtocol(mac)

    def configure(self, dst_port: str, dst_ip: str, dst_mac: str):
        self.transport.setDst(dst_port)
        self.internet.setDst(dst_ip)
        self.datalink.setDst(dst_mac)

    def send(self) -> list[Frame]:
        app_payload = self.app.encapsulate(self.app.msg)
        segments = self.transport.encapsulate(app_payload)
        packets = self.internet.encapsulate(segments)
        frames = self.datalink.encapsulate(packets)

        return frames

    def receive(self, frames: list[Frame]) -> str:
        packets = self.datalink.decapsulate(frames)
        segments = self.internet.decapsulate(packets)
        data = self.transport.decapsulate(segments)
        msg =  self.app.decapsulate(data)

        return msg
