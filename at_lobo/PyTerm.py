#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
import tty
import termios
import time
import argparse
import binascii
import re
from threading import Thread
try:
    import serial
except ImportError:
    print("\033[1;31mPySerial must be installed, run \033[1;34m`pip3 install pyserial`\033[0m\r\n")
    sys.exit(1)

#============
class PyTerm:

    #-----------------------------------------------------------------
    def __init__(self, baudrate=115200, device='/dev/ttyUSB0', rst=0):
        self.DEVICE     = device
        self.BAUDRATE   = baudrate
        self.ESCAPECHAR = "\033"
        self.VERSION = "1.2.0"
        self.ShutdownReceiver = False
        self.ReceiverToStdout = True
        self.DefaultTimeout = 0.1

        print("\n\033[1;31m--[ \033[1;34mESP8266 terminal \033[1;31m     ver. \033[1;34m" + self.VERSION + "\033[1;31m ]-- \033[0m")
        print("\033[1;31m--[ \033[1;34mPress ESC twice for command mode\033[1;31m ]-- \033[0m\n")
        # Open remote terminal device
        try:
            self.uart = serial.Serial(
                port    = self.DEVICE,
                baudrate= self.BAUDRATE,
                bytesize= serial.EIGHTBITS,
                parity  = serial.PARITY_NONE,
                stopbits= serial.STOPBITS_ONE,
                timeout = self.DefaultTimeout,
                xonxoff = 0,
                rtscts  = 0,
                interCharTimeout=None
            )
            if rst:
                self.uart.dtr = False
                time.sleep(0.1)
                self.uart.dtr = True
        except Exception as e:
            raise Exception("\033[1;31mAccessing \033[1;37m" + self.DEVICE + " \033[1;31mfailed\r\n\033[1;37mPyTerm exit\033[0m\r\n")

        # Setup local terminal
        self.stdinfd          = sys.stdin.fileno()
        self.oldstdinsettings = termios.tcgetattr(self.stdinfd)
        tty.setraw(self.stdinfd) # from now on, end-line must be "\r\n"
        
        # Start receiver thread
        self.ReceiverThread = Thread(target=self.ReceiveData, args=(self.uart, False))
        self.ReceiverThread.start()

        # this is the main loop of this software
        try:
            self.HandleUnbufferedUserInput();
        except Exception as e:
            print("\r\n\033[1;31mError: failed with the following exception:\033[0m\r\n")
            print(e, "\r\n")

        # Shutdown receiver thread
        self.ShutdownReceiver = True
        if self.ReceiverThread.isAlive():
            self.ReceiverThread.join()

        # Clean up everything
        termios.tcsetattr(self.stdinfd, termios.TCSADRAIN, self.oldstdinsettings)
        self.uart.close()

    #-----------------------------------------
    def ReceiveData(self, uart, binary=False):
        data = ""
        while not self.ShutdownReceiver:

            if not self.ReceiverToStdout:
                time.sleep(0.001);
                continue

            try:
                data = self.uart.read(self.uart.inWaiting())
            except:
                return

            if data:
                try:
                    string = data.decode("utf-8")
                except UnicodeDecodeError:
                    string = "[???]\r\n"

                f = False
                for c in data:
                    if c == 7:
                        f = True
                        break
                if f is True:
                    self.uart.baudrate = 115200
                    sys.stdout.write("[115200]\r\n")
                    sys.stdout.flush()
                else:
                    sys.stdout.write(string)
                    sys.stdout.flush()
        
            time.sleep(0.01);

    #---------------------
    def ReadCommand(self):
        char    = ""
        command = ""

        while True:
            char = sys.stdin.read(1)
            if char == "\r":
                break
            elif char == self.ESCAPECHAR:
                if len(command) == 0:
                    command = self.ESCAPECHAR
                break
            else:
                sys.stdout.write(char)
                sys.stdout.flush()
                command += char

        return command

    #----------------------
    def Get2ndEscape(self):
            char = sys.stdin.read(1)
            if char == self.ESCAPECHAR:
                return True
            elif char == "[":
                self.uart.write("\033[".encode("utf-8"))
                return False
            data = char.encode("utf-8")
            self.uart.write(data)
            return False

    #-----------------------------------
    def HandleUnbufferedUserInput(self):
        char = ""

        while True:
            char = sys.stdin.read(1)

            if char == self.ESCAPECHAR:
                if self.Get2ndEscape():
                    print("\r\n\033[1;31m--[\033[1;34mPyTerm command: \033[0m", end="")
                    command = self.ReadCommand()

                    if command == self.ESCAPECHAR:
                        sys.stdout.write("\r\n")
                        sys.stdout.flush()
                        self.uart.write(self.ESCAPECHAR.encode("utf-8"))

                    if command == "exit":
                        print("\r\n\033[1;34m Exit PyTerm \033[1;31m]--\033[0m\r\n", end="")
                        break

                    elif command == "version":
                        sys.stdout.write("\r\n")
                        sys.stdout.flush()
                        print("Version: " + self.VERSION, end="\r\n")

                    elif command[0:9] == "baudrate ":
                        try:
                            cmd = re.sub(' +', ' ', command.strip()).split(' ')
                            if len(cmd) == 2:
                                baudrate = int(cmd[1])
                                print("\r\nbautrate set to {}\r\n".format(baudrate), end="\r\n")
                                self.uart.baudrate = baudrate
                            else:
                                print("\r\nWrong command arguments", end="\r\n")
                        except:
                            print("\r\nError", end="\r\n")

                    else:
                        print(""" \033[1;37munknown command\033[0m, use one of the following commands:\r
\033[1;34m                exit\033[0m - exit the terminal\r
\033[1;34m             version\033[0m - print version info\r
\033[1;34m      baudrate <bdr>\033[0m - set terminal baudrate\r
""")

                    print("\033[1;34mback to device \033[1;31m]--\033[0m\r\n", end="")
            else:
                data = char.encode("utf-8")
                self.uart.write(data)
                if data == b'\r':
                    self.uart.write(b'\n')

#=========================
if __name__ == '__main__':
    cli = argparse.ArgumentParser(
    description="Serial ternimal optimized for ESP8266.",
    #formatter_class=argparse.ArgumentDefaultsHelpFormatter
    formatter_class=argparse.RawTextHelpFormatter
    )

    cli.add_argument("--baudrate", default=115200, type=int, action="store",
        help="The baudrate used for the communication (default: 115200).")
    cli.add_argument("--device",   default='/dev/ttyUSB0',type=str, action="store",
        help="Path to the serial communication device (default: '/dev/ttyUSB0'.")
    cli.add_argument("--reset",    action="store_true",
        help="Reset the device on start")

    args = cli.parse_args()
    print(args)

    trm = PyTerm(baudrate=args.baudrate, device=args.device, rst=args.reset)
