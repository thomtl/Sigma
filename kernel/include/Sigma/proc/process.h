#ifndef SIGMA_KERNEL_PROCESS
#define SIGMA_KERNEL_PROCESS

#include <Sigma/common.h>
#include <Sigma/arch/x86_64/drivers/apic.h>
#include <Sigma/arch/x86_64/idt.h>
#include <Sigma/acpi/madt.h>
#include <Sigma/smp/cpu.h>
#include <Sigma/types/vector.h>
#include <Sigma/types/pair.h>
#include <Sigma/proc/ipc.h>

namespace proc::process
{
    struct thread_context {
        thread_context(): rax(0), rbx(0), rcx(0), rdx(0), rsi(0), rdi(0), rbp(0), rsp(0), \
                          r8(0), r9(0), r10(0), r11(0), r12(0), r13(0), r14(0), r15(0), \
                          rip(0), cr3(0), rflags(0), \
                          ss(0), ds(0), cs(0) {}
        uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp; // Normal Registers
        uint64_t r8, r9, r10, r11, r12, r13, r14, r15; // General Purpose Registers
        uint64_t rip, cr3, rflags; // Control Registers
        uint16_t ss, ds, cs; // Segment Registers
    };

    struct thread_resources {
        thread_resources(): frames(types::vector<uint64_t>()) {}
        types::vector<uint64_t> frames;
    };

    struct thread_image {
        thread_image(): stack_top(0), stack_bottom(0), heap_bottom(0), heap_top(0) {}
        uint64_t stack_top;
        uint64_t stack_bottom;
        uint64_t heap_bottom;
        uint64_t heap_top;
    };

    enum class thread_state {DISABLED, IDLE, RUNNING, BLOCKED};
    enum class thread_privilege_level {KERNEL, DRIVER, APPLICATION};

    struct thread {
        thread(): context(proc::process::thread_context()), resources(proc::process::thread_resources()), \
                  image(proc::process::thread_image()), state(proc::process::thread_state::DISABLED), \
                  privilege(proc::process::thread_privilege_level::APPLICATION), tid(0){}
        proc::process::thread_context context;
        proc::process::thread_resources resources;
        proc::process::thread_image image;
        proc::process::thread_state state;
        proc::process::thread_privilege_level privilege;
        proc::ipc::thread_ipc_manager ipc_manager;

        tid_t tid;
    };
    

    struct managed_cpu {
        managed_cpu() {}
        managed_cpu(smp::cpu_entry cpu, bool enabled, proc::process::thread* current_thread): cpu(cpu), enabled(enabled), current_thread(current_thread) {}
        smp::cpu_entry cpu;
        bool enabled;
        proc::process::thread* current_thread;
    };

    constexpr uint64_t cpu_quantum = 100;
    constexpr uint16_t cpu_quantum_interrupt_vector = 64;

    void init_multitasking(acpi::madt& madt);
    void init_cpu();


    using kernel_thread_function = void (*)();
    proc::process::thread* create_kernel_thread(kernel_thread_function function);
    proc::process::thread* create_thread(void* rip, uint64_t stack, uint64_t cr3, proc::process::thread_privilege_level privilege);
    proc::process::thread* create_blocked_thread(void* rip, uint64_t stack, uint64_t cr3, proc::process::thread_privilege_level privilege);
    void set_thread_state(proc::process::thread* thread, proc::process::thread_state new_state);

    proc::process::thread* thread_for_tid(tid_t tid);
    proc::process::thread* get_current_thread();

    bool send_message(tid_t origin, size_t buffer_length, uint8_t* buffer);
    bool receive_message(tid_t& origin, size_t& size, types::vector<uint8_t>& data);
} // namespace proc::sched


#endif