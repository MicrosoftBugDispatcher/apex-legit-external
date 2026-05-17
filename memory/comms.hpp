#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>
#include <winternl.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <type_traits>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS static_cast<NTSTATUS>(0x00000000)
#endif

#ifndef STATUS_PROCEDURE_NOT_FOUND
#define STATUS_PROCEDURE_NOT_FOUND static_cast<NTSTATUS>(0xC000007A)
#endif

namespace Helpers {
    inline std::uint32_t GetProcessId(const std::string& process_name) {
        PROCESSENTRY32W entry{ };
        entry.dwSize = sizeof(entry);

        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return 0;

        const std::wstring wide_name(process_name.begin(), process_name.end());
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, wide_name.c_str()) == 0) {
                    CloseHandle(snapshot);
                    return entry.th32ProcessID;
                }
            } while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return 0;
    }
}

namespace Kernel {
    enum control_type {
        none = 0,
        verify,
        get_eprocess,
        get_process_id,
        get_process_peb,
        get_base_address,
        get_directory_table_base,
        swap_directory_table_base,
        hyperspace_entries,
        hide_process_pages,
        map_process_page,
        read_physical,
        write_physical,
        read_virtual,
        write_virtual,
        lookup_thread,
        suspend_thread,
        resume_thread,
        get_thread_context,
        set_thread_context,
        clone_in_hyperspace,
        hyperspace_get_context,
        hyperspace_allocate_virtual,
        hyperspace_free_virtual,
        hyperspace_remap_pages,
        hyperspace_expose_pages,
        hyperspace_clone_pages,
        hyperspace_rexpose_pages,
        unload_driver
    };

    struct pt_entries_t {
        std::uint64_t pml4e;
        std::uint64_t pdpte;
        std::uint64_t pde;
        std::uint64_t pte;
    };

    struct control_initialize_t {
        std::uint64_t process_id;
        std::uint64_t base_address;
        void* response_semaphore;
        void* request_semaphore;
    };

    struct control_data_t {
        control_type request_type;
        pt_entries_t pt_entries;
        std::uint64_t pml4e;
        std::uint64_t pdpte;
        std::uint64_t pde;
        std::uint64_t pte;
        std::uint32_t thread_id;
        std::uint32_t process_id;
        std::uint32_t protection;
        std::uint32_t count;
        std::uint32_t mode;
        CONTEXT* context;
        void* process;
        void* process2;
        void* thread;
        void* process_peb;
        std::uint64_t address;
        std::uint64_t address1;
        void* address2;
        std::size_t size;
        bool remove;
        bool status;
    };

    struct shared_data_t {
        control_data_t control;
        std::uint8_t payload[0x1000];
    };

    struct wnf_state_name_t {
        std::uint32_t data[2];
    };

    class Driver {
    private:
        using nt_update_wnf_state_data_t = NTSTATUS(NTAPI*)(
            const wnf_state_name_t* state_name,
            const void* buffer,
            ULONG length,
            const void* type_id,
            const void* explicit_scope,
            ULONG matching_change_stamp,
            BOOLEAN check_stamp
            );

        static constexpr std::uint64_t wnf_state = 0x0d83063ea3be0875ull;
        static constexpr DWORD wait_timeout = 5000;

        HANDLE response_semaphore = nullptr;
        HANDLE request_semaphore = nullptr;
        HANDLE section_mapping = nullptr;
        shared_data_t* shared = nullptr;
        nt_update_wnf_state_data_t nt_update_wnf_state_data = nullptr;

        bool send_request(control_type type) {
            if (!shared || !request_semaphore || !response_semaphore)
                return false;

            shared->control.request_type = type;
            shared->control.status = false;

            if (!ReleaseSemaphore(request_semaphore, 1, nullptr))
                return false;

            if (WaitForSingleObject(response_semaphore, wait_timeout) != WAIT_OBJECT_0)
                return false;

            return shared->control.status;
        }

        bool initialize_driver() {
            auto ntdll = GetModuleHandleW(L"ntdll.dll");
            if (!ntdll)
                return false;

            nt_update_wnf_state_data = reinterpret_cast<nt_update_wnf_state_data_t>(
                GetProcAddress(ntdll, "NtUpdateWnfStateData"));
            if (!nt_update_wnf_state_data)
                return false;

            response_semaphore = CreateSemaphoreW(nullptr, 0, 1, nullptr);
            request_semaphore = CreateSemaphoreW(nullptr, 0, 1, nullptr);
            if (!response_semaphore || !request_semaphore)
                return false;

            section_mapping = CreateFileMappingW(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE,
                0,
                static_cast<DWORD>(sizeof(shared_data_t)),
                nullptr
                );
            if (!section_mapping)
                return false;

            shared = static_cast<shared_data_t*>(
                MapViewOfFile(section_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(shared_data_t)));
            if (!shared)
                return false;

            ZeroMemory(shared, sizeof(shared_data_t));

            control_initialize_t init{ };
            init.process_id = process_id;
            init.base_address = reinterpret_cast<std::uint64_t>(shared);
            init.response_semaphore = response_semaphore;
            init.request_semaphore = request_semaphore;

            wnf_state_name_t state_name{ };
            state_name.data[0] = static_cast<std::uint32_t>(wnf_state & 0xffffffff);
            state_name.data[1] = static_cast<std::uint32_t>((wnf_state >> 32) & 0xffffffff);

            const auto status = nt_update_wnf_state_data(
                &state_name,
                &init,
                sizeof(init),
                nullptr,
                nullptr,
                0,
                FALSE
                );
            if (status != STATUS_SUCCESS)
                return false;

            if (!Verify())
                return false;

            process = GetProcess();
            Base = GetImage();
            CR3 = GetCR3();
            return true;
        }

    public:
        std::uint32_t process_id = 0;
        std::uintptr_t process = 0;
        std::uintptr_t base = 0;
        std::uintptr_t cr3 = 0;
        std::uintptr_t PID = 0;
        std::uintptr_t Base = 0;
        std::uintptr_t CR3 = 0;

        Driver(const std::string& process_name) {
            process_id = Helpers::GetProcessId(process_name);
            PID = process_id;
            if (process_id)
                initialize_driver();
        }

        Driver(std::uint32_t pid) {
            process_id = pid;
            PID = process_id;
            if (process_id)
                initialize_driver();
        }

        ~Driver() {
            if (shared)
                UnmapViewOfFile(shared);
            if (section_mapping)
                CloseHandle(section_mapping);
            if (request_semaphore)
                CloseHandle(request_semaphore);
            if (response_semaphore)
                CloseHandle(response_semaphore);
        }

        bool IsValid() const {
            return shared != nullptr && request_semaphore != nullptr && response_semaphore != nullptr;
        }

        bool Verify() {
            return send_request(verify);
        }

        std::uintptr_t GetProcess() {
            if (!shared)
                return 0;

            shared->control.process_id = process_id;
            if (!send_request(get_eprocess))
                return 0;

            process = reinterpret_cast<std::uintptr_t>(shared->control.process);
            return process;
        }

        std::uintptr_t GetImage() {
            if (!shared)
                return 0;

            if (!process)
                GetProcess();

            shared->control.process = reinterpret_cast<void*>(process);
            if (!send_request(get_base_address))
                return 0;

            base = reinterpret_cast<std::uintptr_t>(shared->control.address2);
            Base = base;
            return base;
        }

        std::uintptr_t GetCR3() {
            if (!shared)
                return 0;

            if (!process)
                GetProcess();

            shared->control.process = reinterpret_cast<void*>(process);
            if (!send_request(get_directory_table_base))
                return 0;

            cr3 = shared->control.address;
            CR3 = cr3;
            return cr3;
        }

        bool ReadPhysical(std::uintptr_t address, void* buffer, std::size_t size) {
            if (!shared || !buffer || !size || size > sizeof(shared->payload))
                return false;

            shared->control.address = address;
            shared->control.size = size;

            if (!send_request(read_physical))
                return false;

            CopyMemory(buffer, shared->payload, size);
            return true;
        }

        void ReadPhysical(PVOID address, PVOID buffer, DWORD size) {
            ReadPhysical(reinterpret_cast<std::uintptr_t>(address), buffer, static_cast<std::size_t>(size));
        }

        bool WritePhysical(std::uintptr_t address, const void* buffer, std::size_t size) {
            if (!shared || !buffer || !size || size > sizeof(shared->payload))
                return false;

            CopyMemory(shared->payload, buffer, size);
            shared->control.address = address;
            shared->control.address2 = const_cast<void*>(buffer);
            shared->control.size = size;

            return send_request(write_physical);
        }

        void WritePhysical(PVOID address, PVOID buffer, DWORD size) {
            WritePhysical(reinterpret_cast<std::uintptr_t>(address), buffer, static_cast<std::size_t>(size));
        }

        bool ReadVirtual(std::uintptr_t address, void* buffer, std::size_t size) {
            if (!shared || !buffer || !size || size > sizeof(shared->payload))
                return false;

            shared->control.address = address;
            shared->control.size = size;

            if (!send_request(read_virtual))
                return false;

            CopyMemory(buffer, shared->payload, size);
            return true;
        }

        bool WriteVirtual(std::uintptr_t address, const void* buffer, std::size_t size) {
            if (!shared || !buffer || !size || size > sizeof(shared->payload))
                return false;

            CopyMemory(shared->payload, buffer, size);
            shared->control.address = address;
            shared->control.size = size;

            return send_request(write_virtual);
        }

        template<typename T>
        T Read(std::uintptr_t address) {
            T value{ };
            ReadVirtual(address, &value, sizeof(T));
            return value;
        }

        template<typename T>
        bool Write(std::uintptr_t address, const T& value) {
            return WriteVirtual(address, &value, sizeof(T));
        }

        bool ReadBytes(std::uintptr_t address, void* buffer, std::size_t size) {
            return ReadVirtual(address, buffer, size);
        }

        bool ReadBytes(std::uintptr_t address, std::size_t length, BYTE* buffer) {
            return ReadBytes(address, buffer, length);
        }

        bool ReadBytes(std::uintptr_t address, std::size_t length, char* buffer) {
            return ReadBytes(address, buffer, length);
        }

        std::string ReadStringRaw(std::uintptr_t address, std::size_t max_length = 200) {
            std::string output;
            output.reserve(max_length);

            for (std::size_t i = 0; i < max_length; i++) {
                const auto ch = Read<char>(address + i);
                if (!ch)
                    break;

                output.push_back(ch);
            }

            return output;
        }

        std::string ReadString(std::uintptr_t address) {
            const auto length = Read<std::uintptr_t>(address + 0x18);
            if (length >= 16)
                return ReadStringRaw(Read<std::uintptr_t>(address));

            return ReadStringRaw(address);
        }

        template<typename T>
        T ReadChain(std::uintptr_t address, const std::vector<std::uintptr_t>& offsets) {
            auto current = address;

            for (std::size_t i = 0; i + 1 < offsets.size(); i++)
                current = Read<std::uintptr_t>(current + offsets[i]);

            return Read<T>(current + offsets.back());
        }

        std::uintptr_t LookupThread(std::uint32_t thread_id) {
            if (!shared)
                return 0;

            shared->control.thread_id = thread_id;
            if (!send_request(lookup_thread))
                return 0;

            return reinterpret_cast<std::uintptr_t>(shared->control.thread);
        }

        bool SuspendThread(std::uintptr_t thread, std::uint32_t* previous_count = nullptr) {
            if (!shared || !thread)
                return false;

            shared->control.thread = reinterpret_cast<void*>(thread);
            const auto result = send_request(suspend_thread);
            if (previous_count)
                *previous_count = shared->control.count;

            return result;
        }

        bool ResumeThread(std::uintptr_t thread, std::uint32_t* previous_count = nullptr) {
            if (!shared || !thread)
                return false;

            shared->control.thread = reinterpret_cast<void*>(thread);
            const auto result = send_request(resume_thread);
            if (previous_count)
                *previous_count = shared->control.count;

            return result;
        }

        bool GetThreadContext(std::uintptr_t thread, CONTEXT& context, std::uint32_t mode = 1) {
            if (!shared || !thread)
                return false;

            shared->control.thread = reinterpret_cast<void*>(thread);
            shared->control.mode = mode;

            if (!send_request(get_thread_context))
                return false;

            CopyMemory(&context, shared->payload, sizeof(CONTEXT));
            return true;
        }

        bool SetThreadContext(std::uintptr_t thread, const CONTEXT& context) {
            if (!shared || !thread)
                return false;

            CopyMemory(shared->payload, &context, sizeof(CONTEXT));
            shared->control.thread = reinterpret_cast<void*>(thread);

            return send_request(set_thread_context);
        }

        bool HideProcessPages(std::uintptr_t address, std::size_t size) {
            if (!shared)
                return false;

            if (!process)
                GetProcess();

            shared->control.process = reinterpret_cast<void*>(process);
            shared->control.address = address;
            shared->control.size = size;

            return send_request(hide_process_pages);
        }

        std::uintptr_t MapProcessPage(std::uintptr_t physical_address) {
            if (!shared)
                return 0;

            shared->control.address = physical_address;
            if (!send_request(map_process_page))
                return 0;

            return reinterpret_cast<std::uintptr_t>(shared->control.address2);
        }

        std::uintptr_t HyperspaceAllocateVirtual(std::size_t size) {
            if (!shared)
                return 0;

            if (!process)
                GetProcess();

            shared->control.process = reinterpret_cast<void*>(process);
            shared->control.size = size;

            if (!send_request(hyperspace_allocate_virtual))
                return 0;

            return shared->control.address;
        }

        bool HyperspaceFreeVirtual(std::uintptr_t address) {
            if (!shared)
                return false;

            if (!process)
                GetProcess();

            shared->control.process = reinterpret_cast<void*>(process);
            shared->control.address = address;

            return send_request(hyperspace_free_virtual);
        }

        template<typename Ret = void, typename... Args>
        Ret Call(std::uintptr_t address, Args... args) {
            using FuncType = Ret(__fastcall*)(Args...);
            static FuncType fn = reinterpret_cast<FuncType>(address);
            if constexpr (std::is_void_v<Ret>) {
                fn(args...);
            }
            else {
                return fn(args...);
            }
        }
    };

    inline Driver* Object = nullptr;
}
