from abc import ABC, abstractmethod

SEGMENT_SIZE = 10

class Protocol(ABC):   
    @abstractmethod
    def encapsulate(self, payload):
        pass

    @abstractmethod
    def decapsulate(self, data):
        pass


# APPLICATION
class Application(Protocol):
    def encapsulate(self, payload: str) -> str:
        return payload

    def decapsulate(self, data: list[str]) -> str:
        return ''.join(data)


# TRANSPORT
class Segment:
    def __init__(self, srcPort: int, dstPort: int, data: str):
        self.srcPort = srcPort
        self.dstPort = dstPort
        self.data = data

    def __str__(self):
        return (f"Segment(srcPort={self.srcPort}, "
                f"dstPort={self.dstPort}, "
                f"data='{self.data}')")

class Transport(Protocol):
    def __init__(self, segmentSize: int, srcPort: int):
        self.segmentSize = segmentSize
        self.srcPort = srcPort
        self.dstPort = None

    def setDstPort(self, dstPort: int):
        self.dstPort = dstPort

    def encapsulate(self, payload: str) -> list[Segment]:
        if self.dstPort is None:
            raise ValueError("Destination port not set.")
        
        segments = [
            Segment(self.srcPort, self.dstPort, payload[i:i + self.segmentSize])
            for i in range(0, len(payload), self.segmentSize)
        ]
        return segments

    def decapsulate(self, data: list[Segment]) -> list[str]:
        return [segment.data for segment in data]


# INTERNETWORK
class Packet:
    def __init__(self, srcAddress: str, dstAddress: str, segment: Segment):
        self.srcAddress = srcAddress
        self.dstAddress = dstAddress
        self.segment = segment

    def __str__(self):
        return (f"Packet(srcAddress={self.srcAddress}, "
                f"dstAddress={self.dstAddress}, "
                f"segment={self.segment})")

class Internetwork(Protocol):
    def __init__(self, srcAddress: str):
        self.srcAddress = srcAddress
        self.dstAddress = None

    def setDstAddress(self, dstAddress: str):
        self.dstAddress = dstAddress

    def encapsulate(self, payload: list[Segment]) -> list[Packet]:
        if self.dstAddress is None:
            raise ValueError("Destination IP address not set.")
        
        packets = [
            Packet(self.srcAddress, self.dstAddress, segment)
            for segment in payload
        ]
        return packets

    def decapsulate(self, data: list[Packet]) -> list[Segment]:
        return [packet.segment for packet in data]


# DATA LINK
class Frame:
    def __init__(self, srcMac: str, dstMac: str, datagram: Packet):
        self.srcMac = srcMac
        self.dstMac = dstMac
        self.datagram = datagram

    def __str__(self):
        return (f"Frame(srcMac={self.srcMac}, "
                f"dstMac={self.dstMac}, "
                f"datagram={self.datagram})")

class Network(Protocol):
    def __init__(self, srcMac: str):
        self.srcMac = srcMac
        self.dstMac = None

    def setDstMac(self, dstMac: str):
        self.dstMac = dstMac

    def encapsulate(self, payload: list[Packet]) -> list[Frame]:
        if self.dstMac is None:
            raise ValueError("Destination MAC address not set.")
        
        frames = [
            Frame(self.srcMac, self.dstMac, datagram)
            for datagram in payload
        ]
        return frames

    def decapsulate(self, data: list[Frame]) -> list[Packet]:
        return [frame.datagram for frame in data]
       
class Device:
    def __init__(self, name: str, srcPort: int, ip: str, mac: str):
        self.name = name
        self.app = Application()
        self.transport = Transport(segmentSize=SEGMENT_SIZE, srcPort=srcPort)
        self.network = Internetwork(srcAddress=ip)
        self.link = Network(srcMac=mac)

    def configureDestination(self, dstPort: int, dstIP: str, dstMac: str):
        self.transport.setDstPort(dstPort)
        self.network.setDstAddress(dstIP)
        self.link.setDstMac(dstMac)

    def send(self, message: str) -> list[Frame]:
        print(f"[{self.name}] Sending: {message}")

        data = self.app.encapsulate(message)
        segments = self.transport.encapsulate(data)
        packets = self.network.encapsulate(segments)
        frames = self.link.encapsulate(packets)

        return frames

    def receive(self, frames: list[Frame]) -> str:
        print(f"[{self.name}] Receiving...")

        packets = self.link.decapsulate(frames)
        segments = self.network.decapsulate(packets)
        data = self.transport.decapsulate(segments)
        message = self.app.decapsulate(data)
        print(f"[{self.name}] Message received: {message}")

        return message

def main():
    sender = Device("HostA", ip="192.168.1.2", mac="AA:BB:CC:DD:EE", srcPort=1234)
    receiver = Device("HostB", ip="192.168.1.10", mac="11:22:33:44:55", srcPort=80)

    # Configura la destinazione **dinamicamente**
    sender.configureDestination(dstPort=80, dstIP="192.168.1.10", dstMac="11:22:33:44:55")
    frames = sender.send("Messaggio dinamico!")

    receiver.receive(frames)


if __name__ == "__main__":
    main()