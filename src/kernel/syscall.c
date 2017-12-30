#include "kernel/task.h"
#include "kernel/syscall.h"
#include "kernel/lib/memory/map.h"
#include "kernel/lib/memory/mmu.h"
#include "kernel/lib/memory/layout.h"

#include "stdlib/assert.h"
#include "stdlib/string.h"
#include "stdlib/syscall.h"

#include "kernel/lib/console/terminal.h"
#define E_INVAL	1	// Invalid parameter
#define E_NO_MEM 2  // no memory

// LAB5 Instruction:
// - find page, virtual address `va' belongs to
// - insert it into `dest->pml4' and `src->pml4' if needed
__attribute__((unused))
static int task_share_page(struct task *dest, struct task *src, void *va, unsigned perm)
{
    //Задание No12
	// Теперь переходим к функции task_share_page. Удалите аттрибут unused.
	// Комментарии к функции должны помочь вам понять, что надо сделать. Надо прове­рить
	// разрешение на запись или что страница должна быть скопирована при попыткезаписи
	// в нее. Если обе проверки не завершились успешно, то достаточно только вста­вить страницу в dest.

	// Ahora ve a la función task_share_page. Eliminar el atributo no utilizado.
	// Los comentarios sobre la función deberían ayudarlo a comprender lo que se debe hacer.
	// Debe verificar el permiso de escritura o que la página debe copiarse cuando intenta escribir en ella.
	// Si ambos controles no se completaron correctamente, simplemente inserte la página en dest.
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.
	//  	 https://github.com/guanqun/mit-jos/blob/master/kern/syscall.c



	uintptr_t va_addr = (uintptr_t)va;
	struct page *p;

	//struct page *p = NULL;

	// page_lookups in map.c looks for page and return a pointer
	p = page_lookup(src->pml4, va_addr, NULL);
	assert(p != NULL); // test if returns NULL (panic)

	// ask about WRITE permisions
	if ((perm & PTE_W) != 0 || (perm & PTE_COW) != 0)
		{
			perm = (perm | PTE_COW) & ~PTE_W;
			if (page_insert(src->pml4, p, va_addr, perm) != 0)
				return -E_INVAL;
			if (page_insert(dest->pml4, p, va_addr, perm) != 0)
				return -E_INVAL;
		}
	else
		{
		if (page_insert(dest->pml4, p, va_addr, perm) != 0)
			return -E_NO_MEM; // no memory
		}


	//(void)src;
	//(void)dest;
	//(void)perm;
	//(void)va_addr;

	terminal_printf("allocated page %p (va: %p): refs: %d\n", p, va, p->ref);

	return 0;
}

// LAB5 Instruction:
// - create new task, copy context, setup return value
//
// - share pages:
// - check all entries inside pml4 before `USER_TOP'
// - check all entrins inside page directory pointer
// - check all entries inside page directory
// - check all entries inside page table and share if present
//
// - mark new task as `ready'
// - return new task id
__attribute__((unused))
static int sys_fork(struct task *task)
{
	// Una de las características del sistema operativo desarrollado es la capacidad de clonar procesos de manera
	// efectiva utilizando el mecanismo de copia sobre escritura. Los procesos de clonación se realizan
	// utilizando la llamada al sistema FORK. Esta llamada al sistema crea un nuevo proceso, lo asigna PML4 y
	// muestra todas las páginas del proceso principal, y para las páginas disponibles para escritura, el bit "W"
	// se restablece y el bit "COW" (11) se establece, lo que significa que la página debe copiarse cuando intento
	// de escribirle

	struct task *child = task_new("child");


	if (child == NULL)
		return -1;
	// copy same context to child
	child->context = task->context;
	child->context.gprs.rax = 0; // return value


	for (uint16_t i = 0; i <= PML4_IDX(USER_TOP); i++) {
		uintptr_t pdpe_pa = PML4E_ADDR(task->pml4[i]);
		if ((task->pml4[i] & PML4E_P) == 0)
			continue;


		pdpe_t *pdpe = VADDR(pdpe_pa);
		for (uint16_t j = 0; j < NPDP_ENTRIES; j++) {
			uintptr_t pde_pa = PDPE_ADDR(pdpe[j]);

			if ((pdpe[j] & PDPE_P) == 0)
				continue;

			pde_t *pde = VADDR(pde_pa);
			for (uint16_t k = 0; k < NPD_ENTRIES; k++) {
				uintptr_t pte_pa = PTE_ADDR(pde[k]);

				if ((pde[k] & PDE_P) == 0)
					continue;

				pte_t *pte = VADDR(pte_pa);
				for (uint16_t l = 0; l < NPT_ENTRIES; l++) {
					if ((pte[l] & PTE_P) == 0)
						continue;

					unsigned perm = pte[l] & PTE_FLAGS_MASK;
					if (task_share_page(child, task, PAGE_ADDR(i, j, k, l, 0), perm) != 0) {
						task_destroy(child);
						return -1;
					}
				}
			}
		}
	}
	child->state = TASK_STATE_READY;
	return child->id;
}

// LAB5 Instruction:
// - implement `puts', `exit', `fork' and `yield' syscalls
// - you can get syscall number from `rax'
// - return value also should be passed via `rax'
void syscall(struct task *task)
{
	//https://github.com/guanqun/mit-jos/blob/master/kern/syscall.c
	enum syscall syscall = task->context.gprs.rax;
	int64_t ret = 0;


	switch (syscall) {
	case SYSCALL_PUTS:
		terminal_printf("task [%d]: %s", task->id, (char *)task->context.gprs.rbx);
		break;
	case SYSCALL_EXIT:
		terminal_printf("task [%d] exited with value `%d'\n", task->id, task->context.gprs.rbx);
		task_destroy(task);
		return schedule();
	case SYSCALL_FORK:
		ret = sys_fork(task);
		break;
	case SYSCALL_YIELD:
		return schedule();
	default:
		panic("unknown syscall `%u'\n", syscall);
	}

	task->context.gprs.rax = ret; // ZERO or child ID
	task_run(task);
}
