/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <copyinout.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char **args, unsigned long argc)
{
	DEBUG(DB_SYSCALL, "runprogram() called.\n");
	DEBUG(DB_SYSCALL, "*************************************************\n");

	if (progname == NULL){
		return EFAULT;
	}

	int result, offset;
	unsigned long total_args;
	const int VADDR = sizeof(vaddr_t);
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	KASSERT(curproc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

    /* Assignment 2b changes below */
    total_args = 1 + argc; // +1 for NULL
	vaddr_t argsPtr_array[total_args];

    // 8-byte aligned
    offset = stackptr % 8;
	stackptr = stackptr - offset;
    
	unsigned int i = 0;
	while(i <= total_args - 2){
		offset = 1 + strlen(args[i]);
		stackptr = stackptr - offset;
		char* src = args[i];
		userptr_t dest = (userptr_t)stackptr;
		result = copyoutstr(src, dest, offset, NULL);
        if (result){
        	return result;
        }
		argsPtr_array[i] = stackptr;
		i = i+1;
	}
    argsPtr_array[argc] = 0; // Set last ptr value to be 0
    
    DEBUG(DB_SYSCALL, "runprogram(): finished copyoutstr to userspace.\n");
    
    // string stack 4-byte aligned
    offset = stackptr % 4;
    stackptr = stackptr - offset;

    i = 0;
    while(i < total_args){
        offset = ROUNDUP(VADDR,4);
        stackptr = stackptr - offset;
		userptr_t dest = (userptr_t)stackptr;
        result = copyout(&argsPtr_array[i],dest,VADDR);
        if (result){
        	return result;
        }
    	i= i+1;
    }

    DEBUG(DB_SYSCALL, "runprogram(): about to enter_new_process.\n");
    
	/* Warp to user mode. */
	userptr_t argv = (userptr_t)stackptr;
	enter_new_process(argc /*argc*/, 
					  argv /*userspace addr of argv*/,
			          stackptr, 
			          entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

