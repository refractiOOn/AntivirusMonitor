#include <iostream>
#include <sstream>
#include <vector>
#include <mutex>
#include <windows.h>
#include <psapi.h>

class HandleWrapper
{
public:
    HandleWrapper(HANDLE handle) :
        m_handle(handle)
    {

    }
    ~HandleWrapper()
    {
        if (m_handle && m_handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_handle);
        }
    }
    HANDLE GetHandle() const
    {
        return m_handle;
    }
private:
    HANDLE m_handle = INVALID_HANDLE_VALUE;
};

class BannedProcess
{
public:
    BannedProcess(std::wstring name) :
        m_name(name)
    {

    }
    void IncrementCount()
    {
        ++m_killedProcessesCount;
    }
    std::wstring GetName() const
    {
        return m_name;
    }
    size_t GetCount() const
    {
        return m_killedProcessesCount;
    }
private:
    static size_t m_killedProcessesCount;
    std::wstring m_name;
};
size_t BannedProcess::m_killedProcessesCount = 0;

bool KillProcessByName(DWORD processID, const std::wstring& name)
{
    HandleWrapper process(OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID));
    if (!process.GetHandle())
    {
        return 0;
    }

    std::wstring processName(MAX_PATH, L'\0');
    DWORD nameSize = GetModuleBaseNameW(process.GetHandle(), 0, &processName[0], processName.length());
    if (!nameSize)
    {
        return 0;
    }
    processName.resize(nameSize);

    if (processName == name)
    {
        return TerminateProcess(process.GetHandle(), 1);
    }

    return 0;
}

DWORD __stdcall ThreadProc(const LPVOID lpParameter)
{
    BannedProcess bannedProcess = *static_cast<BannedProcess*>(lpParameter);
    std::mutex mutex;
    while (bannedProcess.GetCount() < 5)
    {
        Sleep(1000);
        DWORD processes[1024], bytesReturned;
        if (!EnumProcesses(processes, sizeof(processes), &bytesReturned))
        {
            std::cout << "Could not get processes ids" << std::endl;
        }
        DWORD processesAmount = bytesReturned / sizeof(DWORD);
        bool isKilled = false;
        for (size_t i = 0; i < processesAmount; ++i)
        {
            if (processes[i])
            {
                if (KillProcessByName(processes[i], bannedProcess.GetName()))
                {
                    isKilled = true;
                }
            }
        }
        mutex.lock();
        if (isKilled)
        {
            bannedProcess.IncrementCount();
            std::cout << "Processes killed: " << bannedProcess.GetCount() << std::endl;
        }
        mutex.unlock();
    }
	return 0;
}

int main()
{
	std::wstring str;
	std::getline(std::wcin, str);
	std::wstringstream stream;
	stream << str;
    std::vector<BannedProcess> bannedProcesses;
    while (stream)
    {
        std::wstring temp;
        stream >> temp;
        bannedProcesses.push_back(temp);
    }

	std::vector<HANDLE> threads;
	for (size_t i = 0; i < bannedProcesses.size(); ++i)
	{
		threads.push_back(CreateThread(0, 0, ThreadProc, &bannedProcesses[i], 0, 0));
	}

	if (!threads.empty())
	{
		WaitForMultipleObjects(threads.size(), &threads[0], TRUE, INFINITE);
	}

    for (size_t i = 0; i < threads.size(); ++i)
    {
        if (threads[i] != INVALID_HANDLE_VALUE)
        {
            CloseHandle(threads[i]);
        }
    }
}