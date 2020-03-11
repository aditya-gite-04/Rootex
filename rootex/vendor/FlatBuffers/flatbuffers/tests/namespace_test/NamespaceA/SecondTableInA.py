# automatically generated by the FlatBuffers compiler, do not modify

# namespace: NamespaceA

import flatbuffers
from flatbuffers.compat import import_numpy
np = import_numpy()

class SecondTableInA(object):
    __slots__ = ['_tab']

    @classmethod
    def GetRootAsSecondTableInA(cls, buf, offset):
        n = flatbuffers.encode.Get(flatbuffers.packer.uoffset, buf, offset)
        x = SecondTableInA()
        x.Init(buf, n + offset)
        return x

    # SecondTableInA
    def Init(self, buf, pos):
        self._tab = flatbuffers.table.Table(buf, pos)

    # SecondTableInA
    def ReferToC(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(4))
        if o != 0:
            x = self._tab.Indirect(o + self._tab.Pos)
            obj = TableInC()
            obj.Init(self._tab.Bytes, x)
            return obj
        return None

def SecondTableInAStart(builder): builder.StartObject(1)
def SecondTableInAAddReferToC(builder, referToC): builder.PrependUOffsetTRelativeSlot(0, flatbuffers.number_types.UOffsetTFlags.py_type(referToC), 0)
def SecondTableInAEnd(builder): return builder.EndObject()

try:
    from typing import Optional
except:
    pass

class SecondTableInAT(object):

    # SecondTableInAT
    def __init__(self):
        self.referToC = None  # type: Optional[TableInCT]

    @classmethod
    def InitFromBuf(cls, buf, pos):
        secondTableInA = SecondTableInA()
        secondTableInA.Init(buf, pos)
        return cls.InitFromObj(secondTableInA)

    @classmethod
    def InitFromObj(cls, secondTableInA):
        x = SecondTableInAT()
        x._UnPack(secondTableInA)
        return x

    # SecondTableInAT
    def _UnPack(self, secondTableInA):
        if secondTableInA is None:
            return
        if secondTableInA.ReferToC() is not None:
            self.referToC = TableInCT.InitFromObj(secondTableInA.ReferToC())

    # SecondTableInAT
    def Pack(self, builder):
        if self.referToC is not None:
            referToC = self.referToC.Pack(builder)
        SecondTableInAStart(builder)
        if self.referToC is not None:
            SecondTableInAAddReferToC(builder, referToC)
        secondTableInA = SecondTableInAEnd(builder)
        return secondTableInA
