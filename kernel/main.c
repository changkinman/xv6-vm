#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#define ENABLE_COLOR 1

#if ENABLE_COLOR
#define ANSI_RESET  "\033[0m"
#define ANSI_CYAN   "\033[1;36m"
#define ANSI_GREEN  "\033[1;32m"
#define ANSI_YELLOW "\033[1;33m"
#else
#define ANSI_RESET  ""
#define ANSI_CYAN   ""
#define ANSI_GREEN  ""
#define ANSI_YELLOW ""
#endif

// QEMU virt exposes the RISC-V time counter at 10 MHz.
#define TIME_TICKS_PER_MSEC 10000ULL

extern char end[];
extern uint64 boot_start_time;

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    uint memory_mb = (uint)((PHYSTOP - KERNBASE) / (1024 * 1024));
    uint kernel_kb = (uint)(((uint64)end - KERNBASE) / 1024);
    consoleinit();
    printfinit();
    printf(
      ANSI_CYAN
      "\n"
      "========================================\n"
      "          XV6 System Information        \n"
      "========================================\n"
      ANSI_RESET
    );

    printf(ANSI_GREEN " Physical memory : %d MB\n" ANSI_RESET,
           memory_mb);
    printf(ANSI_GREEN " Kernel image    : %d KB\n" ANSI_RESET,
           kernel_kb);
    printf(ANSI_YELLOW " Max CPU support : %d harts\n" ANSI_RESET,
           NCPU);
    printf(ANSI_YELLOW " Current hart    : %d\n" ANSI_RESET,
           cpuid());

    printf(
      ANSI_CYAN
      " QEMU CPU config : 3 harts (default)\n"
      "========================================\n"
      ANSI_RESET
      "\n"
    );
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    uint64 boot_end_time = r_time();
    uint boot_elapsed_ms = (uint)((boot_end_time - boot_start_time) /
                                  TIME_TICKS_PER_MSEC);
    printf(ANSI_GREEN " Boot time       : %d ms\n" ANSI_RESET,
           boot_elapsed_ms);
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
