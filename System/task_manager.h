/***********************************************************************************/
/* Copyright (c) 2013, Wictor Lund. All rights reserved.			   */
/* Copyright (c) 2013, Åbo Akademi University. All rights reserved.		   */
/* 										   */
/* Redistribution and use in source and binary forms, with or without		   */
/* modification, are permitted provided that the following conditions are met:	   */
/*      * Redistributions of source code must retain the above copyright	   */
/*        notice, this list of conditions and the following disclaimer.		   */
/*      * Redistributions in binary form must reproduce the above copyright	   */
/*        notice, this list of conditions and the following disclaimer in the	   */
/*        documentation and/or other materials provided with the distribution.	   */
/*      * Neither the name of the Åbo Akademi University nor the		   */
/*        names of its contributors may be used to endorse or promote products	   */
/*        derived from this software without specific prior written permission.	   */
/* 										   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND */
/* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED   */
/* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE	   */
/* DISCLAIMED. IN NO EVENT SHALL ÅBO AKADEMI UNIVERSITY BE LIABLE FOR ANY	   */
/* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES	   */
/* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;	   */
/* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND	   */
/* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT	   */
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS   */
/* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 		   */
/***********************************************************************************/

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <task.h>

#include <System/queue.h>
#include <System/tree.h>
#include <System/elf.h>
#include <System/types.h>

/*
 * Checkpointing defines.
 */

typedef enum {
  cp_req_rtu,
  cp_req_tm
} cp_req_t;

typedef void (*request_hook_fn_t)(cp_req_t req_type);

/*
 * Migrator lock type define.
 */

typedef enum {
	task_control,
	migrator_control
} migrator_lock;

/*
 * Defines for task_section structure. Implemented as a double-linked
 * list.
 */

typedef struct task_section_cons_t {
	const char      *name;
	Elf32_Half       section_index;
	void		*amem;
	LIST_ENTRY(task_section_cons_t) sections;
} task_section_cons;

typedef LIST_HEAD(task_section_list_t,
		  task_section_cons_t)
     task_section_list;

/*
 * Defines for task_dynmemsect structure. Implemented as a splay tree
 * for smaller memory foot print than rb trees.
 */

typedef struct task_dynmemsect_cons_t {
	size_t	 size;
	void	*ptr;
	SPLAY_ENTRY(task_dynmemsect_cons_t) dynmemsects;
} task_dynmemsect_cons;

typedef SPLAY_HEAD(task_dynmemsect_tree_t,
		   task_dynmemsect_cons_t)
     task_dynmemsect_tree;

/*
 * Defines for task_register structure. Implemented as a rb tree for
 * fast search when doing malloc()/free().
 */

typedef struct task_register_cons_t {
	const char		*name;
	Elf32_Ehdr		*elfh;
	xTaskHandle		 task_handle;
	request_hook_fn_t	 request_hook;
	void                    *cont_mem;
	size_t			 cont_mem_size;
	task_section_list	 sections;
	task_dynmemsect_tree	 dynmemsects;
	volatile migrator_lock	 migrator_lock;
	RB_ENTRY(task_register_cons_t) tasks;
} task_register_cons;

typedef RB_HEAD(task_register_tree_t,
		task_register_cons_t)
     task_register_tree;

/*
 * Prototypes and compare function for task_dynmemsect structure.
 */

static __inline__ int task_dynmemsect_cons_cmp
(task_dynmemsect_cons *op1, task_dynmemsect_cons *op2)
{
	u_int32_t op1i = (u_int32_t)op1->ptr;
	u_int32_t op2i = (u_int32_t)op2->ptr;
	return (int64_t)op1i - (int64_t)op2i;
}

SPLAY_PROTOTYPE(task_dynmemsect_tree_t, task_dynmemsect_cons_t, dynmemsects, task_dynmemsect_cons_cmp)


/*
 * Prototypes and compare function for task_register structure.
 */

static __inline__ int task_register_cons_cmp
(task_register_cons *op1, task_register_cons *op2)
{
	u_int32_t op1i = (u_int32_t)op1->task_handle;
	u_int32_t op2i = (u_int32_t)op2->task_handle;
	if (op1->task_handle == 0 || op2->task_handle == 0)
		return 1;
	else
		return (int64_t)op1i - (int64_t)op2i;
}

RB_PROTOTYPE(task_register_tree_t, task_register_cons_t, tasks, task_register_cons_cmp)

/*
 * Macros
 */

#define TASK_ACQUIRE_TR_LOCK() do { } while(0)
#define TASK_RELEASE_TR_LOCK() do { } while(0)

#define APPTASK_MALLOC_CALL(s) pvPortMalloc(s)
#define APPTASK_FREE_CALL(p) vPortFree(p)

/*
 * Prototypes.
 */

task_register_tree	*task_get_trc_root();
task_register_cons	*task_find(const char *name);
void			*task_get_section_address(task_register_cons *, Elf32_Half);
request_hook_fn_t	 task_find_request_hook(task_register_cons *trc);
int			 task_link(task_register_cons *trc);
int			 task_alloc(task_register_cons *trc);
int			 task_free(task_register_cons *trc);
int			 task_start(task_register_cons *trc);
task_register_cons	*task_register(const char *name, Elf32_Ehdr *elfh);
task_dynmemsect_cons    *task_find_dynmemsect(task_register_cons *trc, void *p);
int			 task_detach(task_register_cons *trc);
int			 task_attach(task_register_cons *trc);
int			 task_call_crh(task_register_cons *trc, cp_req_t req_type);
task_register_cons	*task_get_current_trc();
void			*task_get_symbol_address(task_register_cons *trc, char *name);
int			 task_wait_for_checkpoint(task_register_cons *trc, cp_req_t req_type);

#ifdef IN_APPTASK
void			*apptask_malloc(size_t size);
void			 apptask_free(void *ptr);
#else /* !IN_APPTASK */
void			*task_apptask_malloc(size_t size, task_register_cons *trc);
void			 task_apptask_free(void *ptr, task_register_cons *trc);
#endif /* IN_APPTASK */

#endif /* TASK_MANAGER_H */
