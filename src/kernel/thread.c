#include "stdlib/assert.h"
#include "stdlib/string.h"

#include "kernel/asm.h"
#include "kernel/thread.h"

#include "kernel/misc/gdt.h"
#include "kernel/misc/util.h"

#include "kernel/lib/memory/map.h"
#include "kernel/lib/memory/layout.h"
#include "kernel/lib/console/terminal.h"

#if LAB >= 8
// arguments are passed via `rdi', `rsi', `rdx' (see IA-32 calling conventions)
static void thread_foo(struct task *thread, thread_func_t foo, void *arg)
{
	assert(thread != NULL && foo != NULL);

	foo(arg);

	task_destroy(thread);

	// call schedule
	asm volatile ("int3");
}
#endif

/*
 * LAB8 Instruction:
 * 1. create new task
 * 2. allocate and map stack (hint: you can use `USER_STACK_TOP')
 * 3. pass function arguments via `rdi, rsi, rdx' (store `data' on new stack)
 * 4. setup segment registers
 * 5. setup instruction pointer and stack pointer
 */
// Don't override stack (don't use large `data')
struct task *thread_create(const char *name, thread_func_t foo, const uint8_t *data, size_t size)
{
	struct page *stack;
	struct task *task;

	//(2)
	if ((task = task_new(name)) == NULL)
		goto cleanup;
	//(3)
	if ((stack = page_alloc()) == NULL) {
		terminal_printf("Can't create thread `%s': no memory for stack\n", name);
		goto cleanup;
	}
	// I use `USER_*' constants here just because I don't want to create
	// separete ones for threads.

	//(4)
	if (page_insert(task->pml4, stack, USER_STACK_TOP-PAGE_SIZE, PTE_U | PTE_W) != 0) {
		terminal_printf("Can't create thread `%s': page_insert(stack) failed\n", name);
		goto cleanup;
	}

	// prepare stack and arguments
	uint8_t *stack_top = (uint8_t *)USER_STACK_TOP;
	{
		//(5)
		uintptr_t cr3 = rcr3();  //(A)
		lcr3(PADDR(task->pml4));

		//(6) es null
		if (data != NULL) {
			// pointers must be ptr aligned

			//(6) alineado
			void *data_ptr = (void *)ROUND_DOWN((uintptr_t)(stack_top-size), sizeof(void *));
			//(7)
			memcpy(data_ptr, data, size);
			data = stack_top = data_ptr;
		}

		// return address
		//(8)
		stack_top -= sizeof(uintptr_t);
		*(uintptr_t *)stack_top = (uintptr_t)0;

		//(9)
		task->context.gprs.rdi = (uintptr_t)task;
		task->context.gprs.rsi = (uintptr_t)foo;
		task->context.gprs.rdx = (uintptr_t)data;

		// 1st and 2nd args are from kernel space (not from stack),
		// so we can save direct pointers
		// se devuelve el valor copiado anteriormente (A)

		lcr3(cr3);
	}
	//(10)
	task->context.cs = GD_KT;
	task->context.ds = GD_KD;
	task->context.es = GD_KD;
	task->context.ss = GD_KD;

	task->context.rip = (uintptr_t)thread_foo;
	task->context.rsp = (uintptr_t)stack_top;

	return task;

	//(1)
cleanup:
	if (task != NULL)
		task_destroy(task);
	return NULL;
}

// LAB8 Instruction: just change `state', so scheduler can run this thread

// TASK 21 /- Agregue la función thread_run. Es suficiente cambiar el estado de la transmisión a.
// TASK_STATE_READY

void thread_run(struct task *thread)
{	// This is a macro that implements a runtime assertion, which can be used
	// to verify assumptions made by the program and print a diagnostic message if this assumption is false.
	assert(thread->state == TASK_STATE_DONT_RUN);
	thread->state = TASK_STATE_READY;

}
