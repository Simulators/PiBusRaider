#!/usr/bin/python
# coding: utf8

__version__ = '0.2'

import logging
import struct
import time
from threading import Thread
from PyCRC.CRCCCITT import CRCCCITT
import sys

logger = logging.getLogger(__name__)


def calcCRC(data):
    crc = CRCCCITT("FFFF").calculate(bytes(data))
    b = bytearray(struct.pack(">H", crc))
    return b

class Frame(object):
    STATE_READ = 0x01
    STATE_ESCAPE = 0x02

    def __init__(self):
        self.finished = False
        self.error = False
        self.state = self.STATE_READ
        self.data = bytearray()
        self.crc = bytearray()
        self.reader = None

    def __len__(self):
        return len(self.data)

    def addByte(self, b):
        if b == 0x7D:
            self.state = self.STATE_ESCAPE
        elif self.state == self.STATE_ESCAPE:
            self.state = self.STATE_READ
            b = b ^ 0x20
            self.data.append(b)
        else:
            self.data.append(b)

    def finish(self):
        self.crc = self.data[-2:]
        self.data = self.data[:-2]
        self.finished = True

    def checkCRC(self):
        res = bool(self.crc == calcCRC(self.data))
        if not res:
            logger.warning(f"invalid crc {self.crc} != {calcCRC(self.data)}")
            self.error = True
        return res

    def toString(self):
        return self.data.decode('utf-8').rstrip('\0')

class HDLC(object):
    def __init__(self, serial, dumpFile=None):
        self.serial = serial
        self.current_frame = None
        self.last_frame = None
        self.frame_callback = None
        self.error_callback = None
        self.running = False
        self.dumpFile = dumpFile

    @classmethod
    def toBytes(cls, data):
        return bytearray(data)

    def sendFrame(self, data):
        bs = self._encode(self.toBytes(data))
        # logger.info("Sending Frame: %d", len(data))
        res = self.serial.write(bs)
        # logger.info("Send %s bytes", res)

    def _onFrame(self, frame):
        self.last_frame = frame
        s = self.last_frame.toString()
        # logger.info("Received Frame: %d %s", len(s), s[:20])
        if self.frame_callback is not None:
            self.frame_callback(s)

    def _onError(self, frame):
        self.last_frame = frame
        s = self.last_frame.toString()
        logger.warning("Frame Error: %d %s", len(s), s[:20])
        if self.error_callback is not None:
            self.error_callback(s)

    def _readBytesAndProc(self, size):
        b = bytearray(self.serial.read(size))
        if self.dumpFile is not None:
            self.dumpFile.write(b)
        for i in range(len(b)):
            self._readByteAndProc(b[i])

    def _readByteAndProc(self, b):
        assert 0 <= b <= 255
        if b == 0x7E:
            # Start or End
            if not self.current_frame or len(self.current_frame) < 1:
                # Start
                self.current_frame = Frame()
            else:
                # End
                self.current_frame.finish()
                self.current_frame.checkCRC()
        elif self.current_frame is None:
            # Ignore before Start
            return False
        elif not self.current_frame.finished:
            self.current_frame.addByte(b)
        else:
            # Ignore Bytes
            pass

        # Validate and return
        if self.current_frame.finished and not self.current_frame.error:
            # Success
            self._onFrame(self.current_frame)
            self.current_frame = None
            return True
        elif self.current_frame.finished:
            # Error
            self._onError(self.current_frame)
            self.current_frame = None
            return True
        return False

    # def readFrame(self, timeout=5):
    #     timer = time.time() + timeout
    #     while time.time() < timer:
    #         i = self.serial.in_waiting
    #         if i < 1:
    #             time.sleep(0.0001)
    #             continue

    #         res = self._readBytes(i)

    #         if res:
    #             # Validate and return
    #             if not self.last_frame.error:
    #                 # Success
    #                 s = self.last_frame.toString()
    #                 return s
    #             elif self.last_frame.finished:
    #                 # Error
    #                 raise ValueError("Invalid Frame (CRC FAIL)")
    #     raise RuntimeError("readFrame timeout")

    @classmethod
    def _encode(cls, bs):
        data = bytearray()
        data.append(0x7E)
        crc = calcCRC(bs)
        bs = bs + crc
        for byte in bs:
            if byte == 0x7E or byte == 0x7D:
                data.append(0x7D)
                data.append(byte ^ 0x20)
            else:
                data.append(byte)
        data.append(0x7E)
        return bytes(data)

    def _receiveLoop(self):
        while self.running:
            i = self.serial.in_waiting
            if i < 1:
                time.sleep(0.001)
                # print(".", end="")
                # sys.stdout.flush()
                continue
            self._readBytesAndProc(i)

    def startReader(self, onFrame, onError=None):
        if self.running:
            raise RuntimeError("reader already running")
        self.reader = Thread(target=self._receiveLoop)
        self.reader.setDaemon(True)
        self.frame_callback = onFrame
        self.error_callback = onError
        self.running = True
        self.reader.start()

    def stopReader(self):
        self.running = False
        try:
            self.reader.join()
        except:
            pass
        self.reader = None

