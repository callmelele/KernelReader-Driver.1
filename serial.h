#pragma once

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")

class Communication {
private:
    HANDLE hSerial = INVALID_HANDLE_VALUE;
    SOCKET udpSocket = INVALID_SOCKET;
    sockaddr_in destAddr;
    bool serialConnected = false;
    bool udpConnected = false;

public:
    Communication(const char* portName, const char* ipAddress, int port) {
        //Setup Serial (COM3)
        hSerial = CreateFileA(portName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hSerial != INVALID_HANDLE_VALUE) {
            DCB dcb = { 0 };
            dcb.DCBlength = sizeof(dcb);
            if (GetCommState(hSerial, &dcb)) {
                dcb.BaudRate = CBR_115200;
                dcb.fDtrControl = DTR_CONTROL_ENABLE;
                dcb.fRtsControl = RTS_CONTROL_ENABLE;
                if (SetCommState(hSerial, &dcb)) {
                    serialConnected = true;
                    PurgeComm(hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
                    std::cout << "[+] Serial Bridge Ready on " << portName << std::endl;
                }
            }
        }

        //Setup UDP (Wi-Fi)
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (udpSocket != INVALID_SOCKET) {
                destAddr.sin_family = AF_INET;
                destAddr.sin_port = htons(port);
                inet_pton(AF_INET, ipAddress, &destAddr.sin_addr);
                udpConnected = true;
                std::cout << "[+] UDP Socket Ready for " << ipAddress << ":" << port << std::endl;
            }
        }
    }

    ~Communication() {
        if (serialConnected) CloseHandle(hSerial);
        if (udpConnected) {
            closesocket(udpSocket);
            WSACleanup();
        }
    }

    bool SendData(int8_t x, int8_t y, uint8_t click, bool useUDP) {
        uint8_t packet[3] = { (uint8_t)x, (uint8_t)y, click };

        if (useUDP && udpConnected) {
            sendto(udpSocket, (char*)packet, 3, 0, (sockaddr*)&destAddr, sizeof(destAddr));
            
            return true;
        }

        if (!useUDP && serialConnected) {
            DWORD written;
            return WriteFile(hSerial, packet, 3, &written, NULL);
        }

        return false;
    }

    bool IsAnyConnected() { return serialConnected || udpConnected; }
};

inline Communication esp32("\\\\.\\COM3", "192.168.1.81", 4444);
