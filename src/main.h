/**
* @file main.h
* @author sharan-naribole
* @brief Core configuration for the mini scheduler demo.
*/


#ifndef MAIN_H_
#define MAIN_H_


// -----------------------------------------------------------------------------
// Task / stack layout (Top of SRAM downward)
// -----------------------------------------------------------------------------
#define MAX_TASKS 5

// Some stack memory calculations
#define SIZE_TASK_STACK 1024U
#define SIZE_SCHED_STACK 1024U

#define SRAM_START 0x20000000u
#define SRAM_SIZE ((128) * (1024))
#define SRAM_END ((SRAM_START) + (SRAM_SIZE))

#define T1_STACK_START SRAM_END
#define T2_STACK_START ((SRAM_END) - (1 * (SIZE_TASK_STACK)))
#define T3_STACK_START ((SRAM_END) - (2 * (SIZE_TASK_STACK)))
#define T4_STACK_START ((SRAM_END) - (3 * (SIZE_TASK_STACK)))
#define IDLE_STACK_START ((SRAM_END) - (4 * (SIZE_TASK_STACK)))
#define SCHED_STACK_START ((SRAM_END) - (5 * (SIZE_TASK_STACK)))

#define TICK_HZ 1000U
#define HSI_CLOCK 16000000U
#define SYSTICK_TIM_CLK HSI_CLOCK
#define SYST_RVR_ADDR 0xE000E014
#define SYST_CSR_ADDR 0xE000E010
#define ICSR_ADDR 0xE000ED04
#define PENDSVSET_BIT 28
#define PENDSVCLR_BIT 27

#define DUMMY_XPSR 0x01000000 // T bit set

#define SHCRS_REG 0xE000ED24
#define MEM_MANAGE_EN_BIT 16
#define BUS_FAULT_EN_BIT 17
#define USAGE_FAULT_EN_BIT 18

#define TASK_READY_STATE  0x00
#define TASK_BLOCKED_STATE  0XFF

#define INTERRUPT_DISABLE()  do{__asm volatile ("MOV R0,#0x1"); asm volatile("MSR PRIMASK,R0"); } while(0)

#define INTERRUPT_ENABLE()  do{__asm volatile ("MOV R0,#0x0"); asm volatile("MSR PRIMASK,R0"); } while(0)


#endif /* MAIN_H_ */
