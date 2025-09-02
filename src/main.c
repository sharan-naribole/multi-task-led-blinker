/**
* @file main.c
* @author sharan-naribole
* @brief Tiny RTOS‑style cooperative scheduler on Cortex‑M4 using SysTick
* (time base) and PendSV (context switch). Blinks four LEDs at
* different rates by scheduling 4 tasks + an idle task.
*
* Key ideas:
* - Each task has a private stack and a small TCB with state and PSP value.
* - Tasks transition READY ⇄ BLOCKED via task_delay() and SysTick unblocking.
* - SysTick sets PendSV pending after housekeeping; task_delay() also pends
* PendSV for immediate yield.
* - PendSV saves R4..R11 to the current task stack, switches PSP, and restores
* the next task.
*/

#include "main.h"
#include "led.h"

#include <stdint.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
// Task Control Block (TCB)
/* This is a task control block carries private information of each task */
typedef struct
{
	uint32_t psp_value; // Process stack pointer snapshot
	uint32_t block_count; // Wakeup tick
	uint8_t current_state; // READY or BLOCKED
	void (*task_handler)(void); // Entry function
} TCB_t;

/* Each task has its own TCB */
TCB_t user_tasks[MAX_TASKS];

uint32_t PSP_INIT_ADDRS[MAX_TASKS] = {
		IDLE_STACK_START,
		T1_STACK_START,
		T2_STACK_START,
		T3_STACK_START,
		T4_STACK_START
};

// Forward declarations ---------------------------------------------------------
void task1_handler(void);
void task2_handler(void);
void task3_handler(void);
void task4_handler(void);
void idle_handler(void);

void init_systick_timer(uint32_t tick_hz);
__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack);
void init_tasks_stack(void);
void enable_processor_faults(void);
__attribute__((naked)) void switch_sp_to_psp(void);
uint32_t get_psp_value(void);
void save_psp_value(uint32_t current_psp);
void update_current_task(void);

// Task blocking state machine
/* This variable gets updated from SysTick handler for every SysTick interrupt */
uint32_t g_tick_count = 0;
void task_delay(uint32_t tick_count);
void update_global_tick_count(void);
void unblock_tasks(void);
void schedule(void);

// Current running task index: start with Task1 (user task)
uint8_t current_task = 1;


int main(void)
{
	enable_processor_faults();
	init_scheduler_stack(SCHED_STACK_START);
	init_tasks_stack();
	led_init_all();
	init_systick_timer(TICK_HZ);
	switch_sp_to_psp();

	// Start the first task explicitly (returns via context switches afterward)
	task1_handler();

	/* Loop forever */
	for(;;);
}

void enable_processor_faults(void)
{
	uint32_t volatile* pSHCSR = (uint32_t*)SHCRS_REG;

	// Enable the configurable faults
	*pSHCSR |= (1 << MEM_MANAGE_EN_BIT);
	*pSHCSR |= (1 << BUS_FAULT_EN_BIT);
	*pSHCSR |= (1 << USAGE_FAULT_EN_BIT);
}

void init_systick_timer(uint32_t tick_hz)
{
	uint32_t count_val = (SYSTICK_TIM_CLK/tick_hz) - 1;

	// Load count value in SYST_RVR register
	uint32_t volatile* pSysRvr = (uint32_t*)SYST_RVR_ADDR;
	*pSysRvr &= ~(0x00FFFFFF); // Clear bits
	*pSysRvr |= (count_val << 0);

	// Enable SysTick countdown
	uint32_t volatile* pSysCsr = (uint32_t*)SYST_CSR_ADDR;
	// Set TICKINT
	*pSysCsr |= (0x1 << 1);
	// Set CLKSOURCE
	*pSysCsr |= (0x1 << 2);
	// Set ENABLE
	*pSysCsr |= (0x1 << 0);
}

__attribute__((naked)) void init_scheduler_stack(uint32_t top_sched_stack)
{
	__asm volatile("MSR MSP,%0": : "r"(top_sched_stack)); // Assign MSP address
	__asm volatile("BX LR"); // Return from Function call
}

void init_tasks_stack(void)
{
	uint32_t* pPSP;

	user_tasks[0].task_handler = idle_handler;
	user_tasks[1].task_handler = task1_handler;
	user_tasks[2].task_handler = task2_handler;
	user_tasks[3].task_handler = task3_handler;
	user_tasks[4].task_handler = task4_handler;

	for(int i =0; i < MAX_TASKS; i++)
	{
		user_tasks[i].current_state = TASK_READY_STATE;
		user_tasks[i].psp_value = PSP_INIT_ADDRS[i];
		pPSP = (uint32_t*) user_tasks[i].psp_value;

		pPSP--; // PSR
		*pPSP = DUMMY_XPSR;//0x01000000

		pPSP--; // PC
		*pPSP = (uint32_t) user_tasks[i].task_handler;

		pPSP--; // LR
		/*
		 * Table 2-17 Exception return behavior
		 * Return to Thread mode, exception return uses
		 * non-floating-point state from
		 * the PSP and execution uses PSP after return.
		 */
		*pPSP = 0xFFFFFFFD;

		// R12, R3, R2, R1, R0 (auto‑popped on exception return)
		for (int j = 0; j < 5; ++j) { *--pPSP = 0; }
		// R4‑R11 (manually pushed/popped by PendSV)
		for (int j = 0; j < 8; ++j) { *--pPSP = 0; }

		user_tasks[i].psp_value = (uint32_t)pPSP;
	}
}

uint32_t get_psp_value(void)
{
	return user_tasks[current_task].psp_value;
}

__attribute__((naked)) void switch_sp_to_psp(void)
{
	// 1. Initialize PSP with Task 1 stack start address

	// Get PSP of current task
	// Branch with link
	// Returns to this function after called function executes
	// Return value is stored in R0 register
	__asm volatile("PUSH {LR}"); // Preserve LR which connects back to main()
	__asm volatile("BL get_psp_value"); // LR updated by BL command
	__asm volatile("MSR PSP,R0"); // initialize PSP
	__asm volatile("POP {LR}"); // pops back LR value

	// 2. Change SP to PSP using CONTROL register
	// Set SPSEL bit (2nd bit) as 1 for PSP
	__asm volatile("MOV R0,#0x02");
	__asm volatile("MSR CONTROL,R0");
	__asm volatile("BX LR");
}

void save_psp_value(uint32_t current_psp)
{
	user_tasks[current_task].psp_value = current_psp;
}

void update_current_task(void)
{
	int state = TASK_BLOCKED_STATE;

	for(int i= 0 ; i < (MAX_TASKS) ; i++)
	{
		current_task++;
		current_task %= MAX_TASKS;
		state = user_tasks[current_task].current_state;
		if( (state == TASK_READY_STATE) && (current_task != 0) )
			break;
	}

	if(state == TASK_BLOCKED_STATE)
		current_task = 0; // Only idle is runnable
}

void SysTick_Handler(void)
{
	update_global_tick_count();
	unblock_tasks();

	// Pend the PendSV Exception
	// Request a context switch after ISR completes
	uint32_t* pICSR = (uint32_t*)ICSR_ADDR;
	*pICSR |= (1 << PENDSVSET_BIT);
}

__attribute__((naked)) void PendSV_Handler(void)
{
	// Save content of current task

	// 1. Get current running task's PSP value
	__asm volatile("MRS R0,PSP");
	// 2. Using the PSP value, store SF2 (R4 to R11)
	// Push R4 through R11 onto the stack that grows down,
	// starting just below the current address in R0,
	// and update R0 to the new stack pointer afterward.
	__asm volatile("STMDB R0!,{R4-R11}");

	// Save LR
	__asm volatile("PUSH {LR}");

	// 3. Save the current value of PSP
	__asm volatile("BL save_psp_value");

	// Retrieve the context of next task
	// 1. Decide the next task to run
	__asm volatile("BL update_current_task");
	// 2. Get its past PSP value
	__asm volatile("BL get_psp_value");

	// 3. Using that PSP value retrieve SF2 (R4 to R11)
	// Load multiple registers, increment after
	__asm volatile("LDMIA R0!,{R4-R11}");

	// 4. Update PSP and exit
	__asm volatile("MSR PSP,R0");

	__asm volatile("POP {LR}");

	__asm volatile("BX LR"); // Exception return → next task
}

void unblock_tasks(void)
{
	for(int i=0; i< MAX_TASKS; i++)
	{
		if(user_tasks[i].current_state != TASK_READY_STATE)
		{
			if(user_tasks[i].block_count == g_tick_count)
			{
				user_tasks[i].current_state = TASK_READY_STATE;
			}
		}
	}
}

void update_global_tick_count(void)
{
	g_tick_count++;
}

void schedule(void)
{
	// Pend the PendSV Exception
	uint32_t* pICSR = (uint32_t*)ICSR_ADDR;
	*pICSR |= (1 << PENDSVSET_BIT);
}

void task_delay(uint32_t tick_count)
{
	INTERRUPT_DISABLE();

	user_tasks[current_task].block_count = g_tick_count + tick_count;
	user_tasks[current_task].current_state = TASK_BLOCKED_STATE;

	// Yield now (PendSV after this ISR boundary)
	schedule();

	INTERRUPT_ENABLE();
}

// -----------------------------------------------------------------------------
// Tasks
// -----------------------------------------------------------------------------

void task1_handler(void)
{
	while(1)
	{
		led_on(LED_GREEN);
		task_delay(LED_GREEN_FREQ);
		led_off(LED_GREEN);
		task_delay(LED_GREEN_FREQ);
	}

}

void task2_handler(void)
{
	while(1)
	{
		led_on(LED_ORANGE);
		task_delay(LED_ORANGE_FREQ);
		led_off(LED_ORANGE);
		task_delay(LED_ORANGE_FREQ);
	}

}


void task3_handler(void)
{
	while(1)
	{
		led_on(LED_BLUE);
		task_delay(LED_BLUE_FREQ);
		led_off(LED_BLUE);
		task_delay(LED_BLUE_FREQ);
	}

}

void task4_handler(void)
{
	while(1)
	{
		led_on(LED_RED);
		task_delay(LED_RED_FREQ);
		led_off(LED_RED);
		task_delay(LED_RED_FREQ);
	}
}

void idle_handler(void)
{
	while(1)
	{
		__asm volatile ("wfi");
	}
}

// -----------------------------------------------------------------------------
// Fault handlers (simple diagnostics)
// -----------------------------------------------------------------------------

void HardFault_Handler(void)
{
	printf("Exception : HardFault\n");
	while(1);
}

void MemManage_Handler(void)
{
	printf("Exception : MemManage\n");
	while(1);
}

void BusFault_Handler(void)
{
	printf("Exception : BusFault\n");
	while(1);
}

void UsageFault_Handler(void)
{
	printf("Exception : UsageFault\n");
	while(1);
}
