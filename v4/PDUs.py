from abc import ABC
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