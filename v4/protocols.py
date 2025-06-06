from abc import ABC, abstractmethod
from PDUs import *

class Protocol(ABC):   
    @abstractmethod
    def encapsulate(self, payload):
        pass

    @abstractmethod
    def decapsulate(self, payload):
        pass

class DatalinkProtocol(Protocol):
    def __init__(self, mac: str):
        self.mac = mac

    def encapsulate(self, payload: list[Packet]) -> list[Frame]:
        pass

    def decapsulate(self, payload: list[Frame]) -> list[Packet]:
        pass

