#include "flutter_window.h"
#include "hook_utils.h"
#include <optional>
#include <array>
#include <filesystem>
#include "flutter/generated_plugin_registrant.h"
#include <flutter/event_channel.h>
#include <flutter/event_sink.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/method_channel.h>
#include <flutter/standard_method_codec.h>
#include <vector>
#include <windows.h>
#include <memory>
#include <Windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <string>
#include <map>
#include <iostream>
#include <cwchar>
#include <Wtsapi32.h>
#include <codecvt>
#include <vector>
#include <ShlObj.h>
#include <stdexcept>
#include <Shlwapi.h>
#include <fstream>
#include <tlhelp32.h>
#include <Psapi.h>
#include <array>
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "shlwapi.lib")

#define SHARED_MEM_SIZE 256
#define SHARED_MEM_NAME "Local\\MySharedMemory"
#define EVENT_NAME "Local\\CheckPresence"

struct SharedData {
    char eventName[5];
    char data[20];
    char result[10];
    std::array<std::string, 10> moduleName;
    std::array<std::string, 10> parameters;
    bool monitor;
    bool error;
};
std::string GetModuleName(HMODULE hModule)
{
    char path[MAX_PATH];
    if (GetModuleFileNameA(hModule, path, sizeof(path)) == 0)
    {
        return ""; 
    }

    char *fileName = PathFindFileNameA(path);
    return std::string(fileName);
}

int injectDLL(const char *arg, const char *szDLLPathToInject)
{
    int nDLLPathLen = lstrlenA(szDLLPathToInject);
    int nTotBytesToAllocate = nDLLPathLen + 1; 
    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD |
                                      PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                                  FALSE,
                                  atoi(arg));
    if (hProcess == NULL)
    {
        printf("Failed to open process. Error %d\n", GetLastError());
        if (GetLastError() == ERROR_ACCESS_DENIED)
        {
            return 5;
        }
    }
    LPVOID lpHeapBaseAddress1 = VirtualAllocEx(hProcess,
                                               NULL,
                                               nTotBytesToAllocate,
                                               MEM_COMMIT,
                                               PAGE_READWRITE);

    if (lpHeapBaseAddress1 == NULL)
    {
        printf("Failed to allocate memory in remote process. Error %d\n", GetLastError());
        CloseHandle(hProcess);
        return 1;
    }
    SIZE_T lNumberOfBytesWritten = 0;
    if (!WriteProcessMemory(hProcess, lpHeapBaseAddress1,
                            szDLLPathToInject, nTotBytesToAllocate, &lNumberOfBytesWritten))
    {
        printf("Failed to write DLL path to remote process. Error %d\n", GetLastError());
        VirtualFreeEx(hProcess, lpHeapBaseAddress1, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    LPTHREAD_START_ROUTINE lpLoadLibraryStartAddress =
        (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(L"Kernel32.dll"), "LoadLibraryA");

    if (lpLoadLibraryStartAddress == NULL)
    {
        printf("Failed to get address of LoadLibraryA. Error %d\n", GetLastError());
        VirtualFreeEx(hProcess, lpHeapBaseAddress1, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }
    HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, 0, lpLoadLibraryStartAddress,
                                              lpHeapBaseAddress1, 0, NULL);

    if (hRemoteThread == NULL)
    {
        printf("Failed to create remote thread. Error %d\n", GetLastError());
        VirtualFreeEx(hProcess, lpHeapBaseAddress1, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }
    WaitForSingleObject(hRemoteThread, INFINITE);
    CloseHandle(hRemoteThread);
    VirtualFreeEx(hProcess, lpHeapBaseAddress1, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return 0;
}

bool getIcon(const std::string &executablePath, const std::string &iconFilePath)
{
    
    std::ifstream iconFile(iconFilePath);
    if (iconFile.is_open())
    {
       
        iconFile.close();
        return true;
    }
    std::cout << executablePath << std::endl;
    std::cout << iconFilePath << std::endl;
    std::cout << "part" << std::endl;
   
    HICON hIconLarge = nullptr;
    HICON hIconSmall = nullptr;

    int iconsExtracted = ExtractIconExA(executablePath.c_str(), 0, &hIconLarge, &hIconSmall, 1);
    if (iconsExtracted <= 0)
    {
        return false;
    }

    HICON hIconToSave = hIconLarge != nullptr ? hIconLarge : hIconSmall;

    
    ICONINFO iconInfo;
    if (!GetIconInfo(hIconToSave, &iconInfo))
    {
       
        DestroyIcon(hIconToSave);
        return false;
    }

  
    BITMAP bitmapInfo;
    if (!GetObject(iconInfo.hbmColor, sizeof(bitmapInfo), &bitmapInfo))
    {
       
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        DestroyIcon(hIconToSave);
        return false;
    }

   
    std::ofstream outFile(iconFilePath, std::ios::out | std::ios::binary);
    if (!outFile.is_open())
    {
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        DestroyIcon(hIconToSave);
        return false;
    }

    BITMAPFILEHEADER bitmapFileHeader;
    bitmapFileHeader.bfType = 0x4D42; // BM
    bitmapFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bitmapInfo.bmWidthBytes * bitmapInfo.bmHeight;
    bitmapFileHeader.bfReserved1 = 0;
    bitmapFileHeader.bfReserved2 = 0;
    bitmapFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    BITMAPINFOHEADER bitmapInfoHeader;
    bitmapInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfoHeader.biWidth = bitmapInfo.bmWidth;
    bitmapInfoHeader.biHeight = bitmapInfo.bmHeight;
    bitmapInfoHeader.biPlanes = 1;
    bitmapInfoHeader.biBitCount = bitmapInfo.bmBitsPixel;
    bitmapInfoHeader.biCompression = BI_RGB;
    bitmapInfoHeader.biSizeImage = 0;
    bitmapInfoHeader.biXPelsPerMeter = 0;
    bitmapInfoHeader.biYPelsPerMeter = 0;
    bitmapInfoHeader.biClrUsed = 0;
    bitmapInfoHeader.biClrImportant = 0;

    outFile.write(reinterpret_cast<const char *>(&bitmapFileHeader), sizeof(BITMAPFILEHEADER));
    outFile.write(reinterpret_cast<const char *>(&bitmapInfoHeader), sizeof(BITMAPINFOHEADER));

    BYTE *buffer = new BYTE[bitmapInfo.bmWidthBytes * bitmapInfo.bmHeight];
    GetBitmapBits(iconInfo.hbmColor, bitmapInfo.bmWidthBytes * bitmapInfo.bmHeight, buffer);
    outFile.write(reinterpret_cast<const char *>(buffer), bitmapInfo.bmWidthBytes * bitmapInfo.bmHeight);

    outFile.close();
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
    DestroyIcon(hIconToSave);
    delete[] buffer;

    // std::cout << "Icon extracted and saved to: " << iconFilePath << std::endl;
    return true;
}

std::string lpwstrToString(LPWSTR wideString)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wideString, -1, NULL, 0, NULL, NULL);

    if (size_needed == 0)
    {
        return "";
    }
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wideString, -1, &result[0], size_needed, NULL, NULL);
    result.resize(size_needed - 1);
    return result;
}

std::filesystem::path getExecutableDir() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::filesystem::path exePath(buffer);
    return exePath.parent_path();
}

std::map<std::string, std::vector<std::string>> getMap()
{
    std::map<std::string, std::vector<std::string>> processes;

    DWORD sessionCount = 0;
    PWTS_SESSION_INFO sessionInfo = nullptr;

    if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessionInfo, &sessionCount))
    {
        for (DWORD i = 0; i < sessionCount; ++i)
        {
           
            DWORD processCount = 0;
            PWTS_PROCESS_INFO processInfo = nullptr;
            int count = 1;
            if (WTSEnumerateProcesses(WTS_CURRENT_SERVER_HANDLE, sessionInfo[i].SessionId, 1, &processInfo, &processCount))
            {
                for (DWORD j = 0; j < processCount; ++j)
                {
                    // Convert wide string to string using helper function
                    std::string processName = lpwstrToString(processInfo[j].pProcessName);
                    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processInfo[j].ProcessId);
                    if (hProcess != nullptr)
                    {
                        char buffer[MAX_PATH];
                        DWORD bufferSize = MAX_PATH;
                        std::filesystem::path iconFilePath;
                        std::string executablePath;
                        if (QueryFullProcessImageNameA(hProcess, 0, buffer, &bufferSize))
                        {
                            executablePath = std::string(buffer, bufferSize);
                            iconFilePath = getExecutableDir()/"Icons-For-Exe"/(processName +".png");
                            if (!getIcon(executablePath, iconFilePath.string()))
                            {
                                iconFilePath = getExecutableDir() / "Icons-For-Exe" / "system7434.png";
                            }
                        }
                        if (processes.count(processName) == 1)
                        {
                            processName += std::to_string(count);
                            count++;
                        }
                       
                        processes[processName] = {std::to_string(static_cast<int>(processInfo[j].ProcessId)), iconFilePath.string(), executablePath};
                    }
                }
                WTSFreeMemory(processInfo);
            }
        }
        WTSFreeMemory(sessionInfo);
    }

    return processes;
}

std::tuple<std::unique_ptr<flutter::MethodResult<>>, SharedData *, HANDLE> injectUtilDll(std::string pid, std::unique_ptr<flutter::MethodResult<>> &&result)
{
    std::filesystem::path full_path = std::filesystem::absolute(getExecutableDir() / "dllinjector-utils.dll");

    std::string full_path_string = full_path.string();
    std::cout << full_path_string;

    int injectionCode = injectDLL(pid.c_str(), full_path_string.c_str());
    flutter::EncodableList toPush;
    if (injectionCode == 5)
    {

        toPush.push_back(flutter::EncodableValue("5"));
        // std::cout << std::get<std::string>(toPush.at(0));
        result->Success(flutter::EncodableValue(toPush));
    }
    if (injectionCode == 1)
    {
        toPush.push_back(flutter::EncodableValue("1"));
        result->Success(flutter::EncodableValue(toPush));
    }
    if (injectionCode == 0)
    {

        std::cout << "injected success utils" << std::endl;
        HANDLE hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM_NAME);
        if (hMapFile == NULL)
        {
            std::cerr << "Could not open file mapping object. error:-" << GetLastError() << std::endl;
        }

        SharedData *pSharedData = (SharedData *)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));

        if (pSharedData == NULL)
        {
            std::cerr << "Could not map view of file." << std::endl;
            CloseHandle(hMapFile);
        }
        return {std::move(result), pSharedData, hMapFile};
    }
    return {nullptr, nullptr, nullptr};
}

FlutterWindow::FlutterWindow(const flutter::DartProject &project)
    : project_(project) {}

FlutterWindow::~FlutterWindow() {}

bool FlutterWindow::OnCreate()
{
    if (!Win32Window::OnCreate())
    {
        return false;
    }

    RECT frame = GetClientArea();
    // The size here must match the window dimensions to avoid unnecessary surface
    // creation / destruction in the startup path.
    flutter_controller_ = std::make_unique<flutter::FlutterViewController>(
        frame.right - frame.left, frame.bottom - frame.top, project_);
    // Ensure that basic setup of the controller was successful.
    if (!flutter_controller_->engine() || !flutter_controller_->view())
    {
        return false;
    }
    RegisterPlugins(flutter_controller_->engine());
    flutter::MethodChannel<> channel(
        flutter_controller_->engine()->messenger(), "com.example.flutter_cpp_example/method_channel",
        &flutter::StandardMethodCodec::GetInstance());
    channel.SetMethodCallHandler(
        [](const flutter::MethodCall<>& call,
            std::unique_ptr<flutter::MethodResult<>> result)
        {
            if (call.method_name().compare("getMap") == 0)
            {

                sayHello();
                auto map = getMap();
                std::cout << "getMap called";
                flutter::EncodableMap result_map;
                for (const auto& pair : map)
                {
                    flutter::EncodableList encodable_list;
                    for (const auto& value : pair.second)
                    {
                        encodable_list.push_back(flutter::EncodableValue(value));
                    }
                    result_map[flutter::EncodableValue(pair.first)] = flutter::EncodableValue(encodable_list);
                }
                std::cout << "done";
                result->Success(flutter::EncodableValue(result_map));
            }

            if (call.method_name().compare("injectDll") == 0)
            {
                std::cout << "inject called";
                auto* values = std::get_if<std::vector<flutter::EncodableValue>>(call.arguments());
                auto filePath = std::get<std::string>(values->at(1));
                auto pid = std::get<std::string>(values->at(0));
                std::cout << filePath;
                std::cout << pid;
                int injectionCode = injectDLL(pid.c_str(), filePath.c_str());
                if (injectionCode == 0)
                {

                    result->Success(flutter::EncodableValue(0));
                }
                if (injectionCode == 5)
                {
                    result->Success(flutter::EncodableValue(5));
                }
                if (injectionCode == 1)
                {

                    result->Success(flutter::EncodableValue(1));
                }
            }
            if (call.method_name().compare("unHook") == 0) {
                HANDLE hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM_NAME);

                SharedData* pSharedData = (SharedData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
                strcpy_s(pSharedData->eventName, "5");
                HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
                if (hEvent != NULL) {
                    SetEvent(hEvent);
                    Sleep(1000);
                    std::cout << "Successfully set the event." << std::endl;
                    CloseHandle(hEvent);
                }
                else {
                    std::cerr << "Failed to open event." << std::endl;
                }

                UnmapViewOfFile(pSharedData);
                CloseHandle(hMapFile);

                result->Success(flutter::EncodableValue(true));
            }
         
        
            if (call.method_name().compare("hookFunction") == 0) {
                auto* values = std::get_if<std::map<flutter::EncodableValue, flutter::EncodableValue>>(call.arguments());
                if (!values) {
                    std::cerr << "Invalid data format received." << std::endl;
                    return;
                }

                std::string functionName;
                std::map<std::string, std::string> labels;
                bool monitor = false;
                std::string pid;

                for (const auto& [key, value] : *values) {
                    std::string outerKey = std::get<std::string>(key);

                    if (outerKey == "monitor") {
                        monitor = (std::get<std::string>(value) == "true");
                    }
                    else if (outerKey == "pid") {
                        pid = std::get<std::string>(value);
                    }
                    else {
                        // Found the function key like "CreateFileW"
                        functionName = outerKey;

                        // Extract inner "override" map
                        auto* innerMap = std::get_if<std::map<flutter::EncodableValue, flutter::EncodableValue>>(&value);
                        if (innerMap) {
                            auto it = innerMap->find(flutter::EncodableValue("override"));
                            if (it != innerMap->end()) {
                                auto* overrideMap = std::get_if<std::map<flutter::EncodableValue, flutter::EncodableValue>>(&it->second);
                                if (overrideMap) {
                                    for (const auto& [labelKey, labelValue] : *overrideMap) {
                                        std::string label = std::get<std::string>(labelKey);
                                        std::string val;
                                        if (std::holds_alternative<std::string>(labelValue))
                                            val = std::get<std::string>(labelValue);
                                        else if (std::holds_alternative<bool>(labelValue))
                                            val = std::get<bool>(labelValue) ? "true" : "false";
                                        else if (std::holds_alternative<int32_t>(labelValue))
                                            val = std::to_string(std::get<int32_t>(labelValue));
                                        labels[label] = val;
                                    }
                                }
                            }
                        }
                    }
                }

                std::cout << "Function: " << functionName << "\n";
                std::cout << "Monitor: " << monitor << "\n";
                std::cout << "PID: " << pid << "\n";
                std::cout << "Overrides:\n";
                for (auto& [k, v] : labels) std::cout << k << ": " << v << "\n";

                // Proceed with shared memory injection
                auto [resVar, pSharedData, hMapFile] = injectUtilDll(pid, std::move(result));

                if (resVar && pSharedData && hMapFile) {
                    result = std::move(resVar);

                    strcpy_s(pSharedData->data, functionName.c_str());
                    pSharedData->monitor = monitor;
                    strcpy_s(pSharedData->eventName, "6");

                    int index = 0;
                    for (const auto& [k, v] : labels) {
                        if (index >= 10) break;
                        pSharedData->parameters[index++] = v;
                    }

                    HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
                    if (hEvent != NULL) {
                        SetEvent(hEvent);
                        Sleep(1000);
                        std::cout << "Successfully set the event." << std::endl;
                        CloseHandle(hEvent);
                    }
                    else {
                        std::cerr << "Failed to open event." << std::endl;
                    }

                    UnmapViewOfFile(pSharedData);
                    CloseHandle(hMapFile);

                    result->Success(flutter::EncodableValue("success"));
                }
                else {
                    result->Success(flutter::EncodableValue("failed"));
                }
            }

            if (call.method_name().compare("getDlls") == 0)
            {
                auto* values = std::get_if<std::vector<flutter::EncodableValue>>(call.arguments());
                auto pid = std::get<std::string>(values->at(0));
                flutter::EncodableList list1;
                HMODULE hmods[1024];
                DWORD nbNeeded;
                std::vector<std::string> res;
                HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, std::stoul(pid));
                if (EnumProcessModules(handle, hmods, sizeof(hmods), &nbNeeded))
                {
                    for (int i = 0; i < (nbNeeded / sizeof(HMODULE)); i++)
                    {
                        HMODULE hModule = hmods[i];
                        res.push_back(GetModuleName(hModule));
                    }
                }
                for (const std::string& item : res)
                {
                    if (item.compare("") == 0)
                        continue;
                    list1.push_back(flutter::EncodableValue(item));
                }
                CloseHandle(handle);
                result->Success(flutter::EncodableValue(list1));
            }
            if (call.method_name().compare("unloadDll") == 0)
            {
                auto* values = std::get_if<std::vector<flutter::EncodableValue>>(call.arguments());
                auto dllName = std::get<std::string>(values->at(0));
                auto pid = std::get<std::string>(values->at(1));
                auto [resVar, pSharedData, hMapFile] = injectUtilDll(pid, std::move(result));
                if (resVar && pSharedData && hMapFile)
                {
                    // Copy the function name to shared memory
                    result = std::move(resVar);
                    strcpy_s(pSharedData->data, dllName.c_str());
                    strcpy_s(pSharedData->eventName, "4");
                    HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
                    if (hEvent == NULL)
                    {
                        std::cerr << "Could not open event." << std::endl;
                        UnmapViewOfFile(pSharedData);
                        CloseHandle(hMapFile);
                    }

                    SetEvent(hEvent);

                    while (true)
                    {
                        // Wait for the DLL to process
                        Sleep(500); // Adjust sleep time as needed
                        std::cout << "iterating in while loop..." << pSharedData->result << std::endl;
                        // Check the result
                        if (strcmp(pSharedData->result, "wait") == 0)
                        {
                            std::cout << "its wait";
                            continue;
                        }
                        else
                        {
                            std::cout << "its a diff value" << pSharedData->result;
                            break;
                        }
                    }
                    std::cout << "done iterating";
                    flutter::EncodableList list1;
                    if (pSharedData->error == true)
                    {

                        list1.push_back(flutter::EncodableValue("error"));
                        result->Success(list1);
                    }
                    list1.push_back(flutter::EncodableValue("no"));
                    if (strcmp(pSharedData->result, "yes") == 0)
                    {
                        list1.pop_back();
                        list1.push_back(flutter::EncodableValue("yes"));
                    }

                    strcpy_s(pSharedData->eventName, "2");
                    SetEvent(hEvent);
                    Sleep(500);
                    UnmapViewOfFile(pSharedData);
                    CloseHandle(hMapFile);
                    CloseHandle(hEvent);
                    result->Success(flutter::EncodableValue(list1));
                }
            }
            if (call.method_name().compare("isPresent") == 0)
            {

                auto* values = std::get_if<std::vector<flutter::EncodableValue>>(call.arguments());
                auto functionName = std::get<std::string>(values->at(1));
                auto pid = std::get<std::string>(values->at(0));
                auto eventName = std::get<std::string>(values->at(2));
                printf(functionName.c_str());
                std::cout << "to inject pid:-" << pid << std::endl;
                std::string relative_path = "dllinjector-utils.dll";
                auto [resVar, pSharedData, hMapFile] = injectUtilDll(pid, std::move(result));
                if (resVar && pSharedData && hMapFile)
                {
                    // Copy the function name to shared memory
                    result = std::move(resVar);
                    strcpy_s(pSharedData->data, functionName.c_str());
                    printf(pSharedData->data);
                    if (eventName == "1")
                    {
                        strcpy_s(pSharedData->eventName, "1");
                    }
                    else if (eventName == "3")
                    {
                        strcpy_s(pSharedData->eventName, "3");
                    }
                    // Signal the event
                    HANDLE hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
                    if (hEvent == NULL)
                    {
                        std::cerr << "Could not open event." << std::endl;
                        UnmapViewOfFile(pSharedData);
                        CloseHandle(hMapFile);
                    }

                    SetEvent(hEvent);

                    while (true)
                    {
                        Sleep(500); 
                        std::cout << "iterating in while loop..." << pSharedData->result << std::endl;
                        // Check the result
                        if (strcmp(pSharedData->result, "wait") == 0)
                        {
                            std::cout << "its wait";
                            continue;
                        }
                        else
                        {
                            std::cout << "its a diff value" << pSharedData->result;
                            break;
                        }
                    }
                    std::cout << "done iterating";
                    if (pSharedData->error == true)
                    {
                        flutter::EncodableList list1;
                        list1.push_back(flutter::EncodableValue("error"));
                        result->Success(list1);
                    }

                    if (strcmp(pSharedData->result, "yes") == 0)
                    {
                        std::cout << "yess got";
                        flutter::EncodableList list1;
                        list1.push_back(flutter::EncodableValue("yes"));
                        if (eventName == "1")
                        {
                            for (const std::string& value : pSharedData->moduleName)
                            {
                                std::cout << value;
                                list1.push_back(flutter::EncodableValue(value));
                            }
                        }

                        strcpy_s(pSharedData->eventName, "2");
                        SetEvent(hEvent);
                        Sleep(500);
                        // uninjectDLL(pid.c_str(), full_path_string.c_str());
                        UnmapViewOfFile(pSharedData);
                        CloseHandle(hMapFile);
                        CloseHandle(hEvent);

                        result->Success(flutter::EncodableValue(list1));
                    }
                    else
                    {

                        flutter::EncodableList list2;
                        list2.push_back(flutter::EncodableValue("no"));
                        strcpy_s(pSharedData->eventName, "2");
                        SetEvent(hEvent);
                        Sleep(500);
                        printf("uninjected successfully");
                        UnmapViewOfFile(pSharedData);
                        CloseHandle(hMapFile);
                        CloseHandle(hEvent);

                        result->Success(flutter::EncodableValue(list2));
                    }

                    printf("result returned");
                
            }}
        });

    SetChildContent(flutter_controller_->view()->GetNativeWindow());

    flutter_controller_->engine()->SetNextFrameCallback([&]()
                                                        { this->Show(); });

    // Flutter can complete the first frame before the "show window" callback is
    // registered. The following call ensures a frame is pending to ensure the
    // window is shown. It is a no-op if the first frame hasn't completed yet.
    flutter_controller_->ForceRedraw();

    return true;
}

void FlutterWindow::OnDestroy()
{
    if (flutter_controller_)
    {
        flutter_controller_ = nullptr;
    }

    Win32Window::OnDestroy();
}

LRESULT
FlutterWindow::MessageHandler(HWND hwnd, UINT const message,
                              WPARAM const wparam,
                              LPARAM const lparam) noexcept
{
    // Give Flutter, including plugins, an opportunity to handle window messages.
    if (flutter_controller_)
    {
        std::optional<LRESULT> result =
            flutter_controller_->HandleTopLevelWindowProc(hwnd, message, wparam,
                                                          lparam);
        if (result)
        {
            return *result;
        }
    }

    switch (message)
    {
    case WM_FONTCHANGE:
        flutter_controller_->engine()->ReloadSystemFonts();
        break;
    }

    return Win32Window::MessageHandler(hwnd, message, wparam, lparam);
}
