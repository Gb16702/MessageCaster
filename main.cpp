#include <iostream>
#include <thread>
#include <windows.h>
#include <atomic>
#include <iomanip>
#include <optional>
#include <condition_variable>
#include <mutex>
#include <format>
#include <ctime>
#include <chrono>
#include <vector>
#include <fstream>
#include <filesystem>

class MessageCaster {
public:
    MessageCaster() : running(false), messageInterval(180000), currentCommandIndex(0) {
        loadCommandsFromFile("commands.txt");
    }

    void start() {
        monitorThread = std::thread(&MessageCaster::monitorKeys, this);
        monitorThread.detach();
    }

    static std::string getCurrentTime() {
        std::time_t now_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        struct tm timeInfo = {};
        localtime_s(&timeInfo, &now_time);

        return (std::ostringstream{} << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S")).str();
    }

private:
    DWORD messageInterval;

    std::atomic<bool> running;
    std::thread monitorThread;
    std::optional<std::thread> messageThread;
    std::condition_variable cvar;
    std::mutex mtx;

    std::vector<std::string> commands;
    size_t currentCommandIndex;

    std::unique_ptr<std::string> currentMessage;


    void sendKeyPress(char c) {
        BYTE vk = static_cast<BYTE>(VkKeyScan(c));
        keybd_event(vk, 0, 0, 0);
        keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
    }

    void loadCommandsFromFile(const std::string& filename) {
        if (!std::filesystem::exists(filename)) {
            std::cerr << "Logger: " << getCurrentTime() << " - " << filename << " not found. " << "Please create the file in the same directory as the executable" << std::endl;
        }


        std::ifstream file(filename);
        if (file.is_open()) {
            std::string command;
            while (std::getline(file, command)) {
                if (!command.empty()) {
                    commands.push_back(command);
                }
            }

            file.close();
        } else {
            std::cerr << "Logger: " << getCurrentTime() << " - Unable to open " << filename << std::endl;
        }
    }

    bool isBrowserFocused() const {
        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            int length = GetWindowTextLengthA(hwnd);
            if (length > 0) {
                std::string windowTitle(length, '\0');
                GetWindowTextA(hwnd, &windowTitle[0], length + 1);

                static const std::vector<std::string> browsers{ "Arc", "Chrome", "Edge" };
                for (const auto& b : browsers) {
                    if (windowTitle.find(b) != std::string::npos) {
                        std::cout << "Logger: " << getCurrentTime() << " - " << b << " is active." << std::endl;
                        return true;
                    }
                }

                std::cout << "Logger: " << getCurrentTime() << " - No active browser detected." << std::endl;
            }
        }

        return false;
    }

    void sendMessage() {
        try {
            while (running && isBrowserFocused()) {
                if (!commands.empty()) {
                    currentCommandIndex = (currentCommandIndex + 1) % commands.size();
                    currentMessage = std::make_unique<std::string>(commands[currentCommandIndex]);

                    for (const char& c : *currentMessage) {
                        sendKeyPress(c);
                    }

                    sendKeyPress(VK_RETURN);

                    std::cout << "Logger: " << getCurrentTime() << " - Sent: " << *currentMessage << std::endl;

                    std::unique_lock<std::mutex> lock(mtx);
                    cvar.wait_for(lock, std::chrono::milliseconds(messageInterval), [this] { return !running; });
                }
            }

		} catch (const std::exception& e) {
            std::cerr << "Exception occured: " << e.what() << std::endl;

        } catch (...) {
		    std::cerr << "Unknown exception" << std::endl;

        }
    }

    void monitorKeys() {
        while(true) {
            if((GetAsyncKeyState('W') & 0X8000 && GetAsyncKeyState('Q') & 0x8000) && running) {
                running = false;
                std::cout << "Inactive" << std::endl;
                cvar.notify_all();
                if(messageThread.has_value() && messageThread->joinable()) {
                    messageThread->join();
                }
            }

            if((GetAsyncKeyState(VK_MBUTTON) & 0x8000) && !running) {
                running = true;
                std::cout << "Active" << std::endl;

                if(messageThread.has_value() && messageThread->joinable()) {
                    messageThread->join(); 
                }

                messageThread = std::thread(&MessageCaster::sendMessage, this);
                cvar.notify_all();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

int main() {
    std::cout << "MessageCaster started at " << MessageCaster::getCurrentTime() << std::endl;

    MessageCaster messageCaster;
    messageCaster.start();

    while(true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}
