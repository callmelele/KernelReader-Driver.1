

class Serial {
private:
    HANDLE hSerial;
    bool connected;
public:
    Serial(const char* portName) {
        connected = false;
        hSerial = CreateFileA(portName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hSerial != INVALID_HANDLE_VALUE) {
            DCB dcbSerialParams = { 0 };
            dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
            if (GetCommState(hSerial, &dcbSerialParams)) {
                dcbSerialParams.BaudRate = CBR_115200;
                dcbSerialParams.ByteSize = 8;
                dcbSerialParams.StopBits = ONESTOPBIT;
                dcbSerialParams.Parity = NOPARITY;
                dcbSerialParams.fDtrControl = DTR_CONTROL_DISABLE;
                dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;
                if (SetCommState(hSerial, &dcbSerialParams)) {
                    connected = true;
                    PurgeComm(hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
                    std::cout << "[+] Stable Bridge Connected on COM3" << std::endl;
                }
            }
        }
    }
    ~Serial() { if (connected) CloseHandle(hSerial); }
    bool IsConnected() { return connected; }

    bool SendData(int8_t x, int8_t y, uint8_t click) {
        if (!connected) return false;
        uint8_t packet[3] = { (uint8_t)x, (uint8_t)y, click };
        DWORD bytesWritten;
        return WriteFile(hSerial, packet, 3, &bytesWritten, NULL);
    }
};

inline Serial esp32("\\\\.\\COM3");