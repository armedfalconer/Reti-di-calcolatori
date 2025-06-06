from abc import ABC, abstractmethod
from protocols import *


class Device(ABC):
    def __init__(self, mac: str):
        self.mac = mac
        self.datalink: DatalinkProtocol = DatalinkProtocol(self.mac)

    @abstractmethod
    def updateArpTable(self):
        pass

class Switch(Device):
    def __init__(self, mac, portsNum: int):
        super().__init__(mac)
        self.arpTable: dict[int, list[str]] = {} # physical port -> list[mac]
        self.maxPorts: int = portsNum
        self.usedPorts: int = 0
        
    def updateArpTable(self, physPort: int, newMacAddr: str):
        if newMacAddr == self.mac:
            raise RuntimeError(f"MAC {newMacAddr} is the same as the switch")

        if physPort not in self.arpTable:
            if self.usedPorts >= self.maxPorts:
                raise RuntimeError(f"Switch has reached maximum capacity of {self.maxPorts} ports")
            self.arpTable[physPort] = []
            self.usedPorts += 1

        if newMacAddr in self.arpTable[physPort]:
            raise RuntimeError(f"MAC {newMacAddr} already in port {physPort}")

        self.arp_table[physPort].append(newMacAddr)
        print(f"Port {physPort}: added mac {newMacAddr}")

# Third Layer
class TLDevice(Device):
    def __init__(self, mac: str, ip: str):
        super().__init__(mac)
        self.IPs = [ip]
        self.currentOpenPort: list[int] = []
        self.arpTable: dict[str, list[str]] = {} # mac -> list[IPs]

    def addIP(self, ip: str):
        if ip not in self.IPs:
            self.IPs.append(ip)
        else:
            raise RuntimeError(f"IP {ip} already in device")

    def updateArpTable(self, srcMac: str, newIP: str):
        if srcMac == self.mac:
            raise RuntimeError(f"Mac address {srcMac} is the same as device mac")

        if srcMac not in self.arpTable:
            self.arpTable[srcMac] = []

        if newIP in self.IPs:
            raise RuntimeError(f"IP {newIP} is one of this device's IPs")
        if newIP in self.arpTable[srcMac]:
            raise RuntimeError(f"IP {newIP} already in arp table at mac {srcMac}")

        self.arpTable[srcMac].append(newIP)
        print(f"MAC {srcMac}: added IP {newIP}")
    
class DNSserver(TLDevice):
    def __init__(self, mac: str, ip: str, webname: str):
        super().__init__(mac, ip)
        self.database: dict[str, str] = {} # from webname (e.g. www.google.com) to ip (8.8.8.8)

    def addToDatabase(self, IP: str, webname: str):
        pass
    
    



