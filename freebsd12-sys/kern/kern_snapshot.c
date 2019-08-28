/*
 * Issues: locking on the _proc lists?
 *
 * Issues: how do you remove _proc's? like when do you kill it?
 *
 */
#include "opt_snapshot.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/mutex.h>
#include <sys/syscall.h>
#include <sys/kthread.h>
#include <sys/unistd.h>
#include <sys/time.h>
#include <sys/libkern.h>
#include <sys/filedesc.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/queue.h>

#ifdef	SNAPSHOT
#include <sys/snapshot.h>

#ifdef	SNAPSHOT_DEBUG
#define DPRINT(fmt, args...)						\
	do {								\
                printf("%s:%d: " fmt "\n", __func__, __LINE__, ##args);	\
        } while (0)
#else
#define	DPRINT(fmt, args...)
#endif

MALLOC_DEFINE(M_SNAPSHOT, "perf-fuzz snapshot", "Buffers to buffer with");
          
struct snapshot_ctx	snapshot_context;

static void
snapshot_init(void *arg __unused)
{
	memset(&snapshot_context, 0, sizeof(struct snapshot_ctx));
	sx_init(&snapshot_context.sc_lock, "snapshot_context lock");
	SNAP_CTX_XLOCK();
	SNAP_CTX_XUNLOCK();
}

static void
snapshot_fini(void *arg __unused)
{
	SNAP_CTX_XLOCK();
	SNAP_CTX_XUNLOCK();
	sx_destroy(&snapshot_context.sc_lock);
	memset(&snapshot_context, 0, sizeof(struct snapshot_ctx));
}
SYSINIT(snapshot_sysinit, SI_SUB_EXEC, SI_ORDER_ANY, snapshot_init, NULL);
SYSUNINIT(snapshot_sysfini, SI_SUB_EXEC, SI_ORDER_ANY, snapshot_fini, NULL);

/********************* **X** *********************/

static int
count_actual_open_files(struct fdescenttbl *ft)
{
	int k;
	int trueOpen = 0;

	for (k = 0; k < ft->fdt_nfiles; k++) {
		struct filedescent *ff = &ft->fdt_ofiles[k];	
		if (ff->fde_file != NULL) {
			++trueOpen;
		}
	}
	return trueOpen;
}

static void
do_files_snapshot(struct snapshot_proc *proc)
{
        struct filedescent *ff;
        struct fdescenttbl *ft;
        int k;
        int true_open, num_files;

	true_open = num_files = 0;

	sx_xlock(&proc->sp_proc->p_fd->fd_sx);

        ft = proc->sp_proc->p_fd->fd_files;
	true_open = count_actual_open_files(ft);

	proc->sp_fd.sf_inuse = (int *)malloc(true_open * sizeof(int),
	    M_SNAPSHOT, M_WAITOK|M_ZERO);

	proc->sp_fd.sf_num_entries = 0; // incr up to it and check against

        num_files = ft->fdt_nfiles;
	KASSERT(num_files >= 0, ("ft->fdt_nfiles is < 0"));

        for (k = 0; k < num_files; k++) {
		ff = &ft->fdt_ofiles[k];
		if (ff->fde_file != NULL) {
			DPRINT("* snapshot: insert %d at %d\n", k,
				proc->sp_fd.sf_num_entries);
			proc->sp_fd.sf_inuse[proc->sp_fd.sf_num_entries++] = k;
		}
	}

	for (k = 0; k < proc->sp_fd.sf_num_entries; k++) {
		DPRINT("* snapshot fd: %d at idx=%d\n",
		    proc->sp_fd.sf_inuse[k], k);
	}

	// This should be assert'd, I think, or panic()able.	
	if (true_open != proc->sp_fd.sf_num_entries) {
		DPRINT("do_files_snapshot: true_open != sf_num_entries\n");
	}
	sx_xunlock(&proc->sp_proc->p_fd->fd_sx);
	DPRINT("do_files_snapshot: number really open %d\n", true_open);
}



static void
clean_files_snapshot(struct snapshot_proc *proc)
{
        struct filedescent *ff;
        struct fdescenttbl *ft;
	int snap_nfiles;
	int k, x, nf;

	sx_xlock(&proc->sp_proc->p_fd->fd_sx);

	snap_nfiles = proc->sp_fd.sf_num_entries;
        ft = proc->sp_proc->p_fd->fd_files;
	nf = ft->fdt_nfiles;

	for (k = 0; k < nf; k++) {
		ff = &ft->fdt_ofiles[k];
		if (ft->fdt_ofiles[k].fde_file != NULL) {
			// XXX: this sucks, right here.
			for (x = 0; x < snap_nfiles; x++) {
				if (k == proc->sp_fd.sf_inuse[x]) {
					DPRINT("* %d found at idx=%d\n", k, x);
					x = -1;
					break;
				}
			}
			/* since it was found, skip closing it */
			if (x == -1) {
				continue;
			}
			DPRINT("* closing %d found at idx=%d\n", k, x);
                        fdclose(curthread, ff->fde_file, k);
                }
	}
	DPRINT("cleaning now have: %d\n", count_actual_open_files(ft));
	sx_xunlock(&proc->sp_proc->p_fd->fd_sx);
/*
                sx_slock(&td->td_proc->p_fd->fd_sx);
                nf = ft->fdt_nfiles;
                a = 0;
                for (k = 0; k < nf; k++) {
                        ff = &ft->fdt_ofiles[k];
                        if (ff->fde_file != NULL) {
 */
}

static void
make_snapshot(struct snapshot_proc *sproc)
{
//	reserve_context(arg);
//	reserve_brk();
//	do_memory_snapshot(arg); 

	do_files_snapshot(sproc);
}

static void
recover_snapshot(struct snapshot_proc *sproc)
{
	DPRINT("snapshot: recover_snapshot()\n");
	clean_files_snapshot(sproc);

/* XXX: This is a copy/paste from the GT work, so ... XXX
	if (have_snapshot(current->mm)) {
		clear_snapshot(sproc);
		recover_memory_snapshot(current->mm);
		recover_brk(current->mm);
		munmap_new_vmas(current->mm);
		clean_snapshot_vmas(current->mm);
		recover_files_snapshot();
		clean_files_snapshot(sproc);
		// clean_context(current->mm);
		// clear_snapshot(current->mm);
	}
 */
}

static struct snapshot_proc *
snapshot_lookup_pid(struct thread *td, pid_t pid, int make_new)
{
	struct snapshot_proc *p = NULL, *pIt;

	SNAP_CTX_XLOCK();
	p = snapshot_context.sc_head;	
	while (p && p->sp_pid != pid) {
		p = p->sp_next;
	}
	if (p != NULL) {
		SNAP_CTX_XUNLOCK();
		return p;
	}
	if (!make_new) {
		SNAP_CTX_XUNLOCK();
		return NULL;
	}

	
	p = (struct snapshot_proc *)malloc(sizeof(struct snapshot_proc),
	    M_SNAPSHOT, M_WAITOK|M_ZERO);
	p->sp_pid = pid;
	p->sp_proc = td->td_proc;
	p->sp_fd.sf_num_entries = 0;
	p->sp_fd.sf_inuse = NULL;
	p->sp_next = NULL;
	if (snapshot_context.sc_head == NULL) {
		snapshot_context.sc_head = p;
	} else {
		// Insert at last spot
		pIt = snapshot_context.sc_head;
		while (pIt->sp_next != NULL) {
			pIt = pIt->sp_next;
		}
		pIt->sp_next = p;		
	}
	SNAP_CTX_XUNLOCK();
	
	DPRINT("added new pid=%d\n", pid);
	return p;
}

/* proper thing would be to export this to userland, but whatever */
static int
snapshot_list_all()
{
        struct snapshot_proc *p;

	SNAP_CTX_SLOCK();
	if (snapshot_context.sc_head == NULL) {
		SNAP_CTX_SUNLOCK();
		return 0;
	}
        p = snapshot_context.sc_head;
	while (p) {
		printf("snapshot_list_all: pid=%d\n", p->sp_pid);
		p = p->sp_next;
        }
	SNAP_CTX_SUNLOCK();
	return 0;
}

static int
snapshot_clean_all()
{
	struct snapshot_proc *p, *next;

	SNAP_CTX_XLOCK();
	if (snapshot_context.sc_head == NULL) {
		SNAP_CTX_XUNLOCK();
		return 0;
	}
        p = snapshot_context.sc_head;
	snapshot_context.sc_head = NULL;
	while (p) {
		next = p->sp_next;
		if (p->sp_fd.sf_inuse) {
			free(p->sp_fd.sf_inuse, M_SNAPSHOT);
			p->sp_fd.sf_inuse = NULL;
		}
		memset(p, 0, sizeof(struct snapshot_proc));
		free(p, M_SNAPSHOT);
		p = next;
	}
	SNAP_CTX_XUNLOCK();
	return 0;
}

static int
snapshot_clean_pid(pid_t pid)
{
	struct snapshot_proc *p, *prior = NULL;

	SNAP_CTX_XLOCK();
	if (snapshot_context.sc_head == NULL) {
		SNAP_CTX_XUNLOCK();
		return ESRCH;
	}
	p = snapshot_context.sc_head;
	if (p->sp_pid == pid) {
		DPRINT("snapshot_clean_pid: found %d = %d\n", pid, p->sp_pid);
		snapshot_context.sc_head = p->sp_next;
		DPRINT("snapshot_clean_pid: head now %p\n",
		    snapshot_context.sc_head);
		if (p->sp_fd.sf_inuse) {
			free(p->sp_fd.sf_inuse, M_SNAPSHOT);
			p->sp_fd.sf_inuse = NULL;
		}
		memset(p, 0, sizeof(struct snapshot_proc));
		free(p, M_SNAPSHOT);
		p = NULL;
		SNAP_CTX_XUNLOCK();
		return 0;
	}
	while (p && p->sp_pid != pid) {
		prior = p;
		p = p->sp_next;
	}
	if (p == NULL) {
		printf("snapshot_clean_pid: unable to find pid=%d\n", pid);
		SNAP_CTX_XUNLOCK();
		return ESRCH;
	}
	printf("snapshot_clean_pid: found %d = %d\n", pid, p->sp_pid);
	prior->sp_next = p->sp_next;
	if (p->sp_fd.sf_inuse) {
		free(p->sp_fd.sf_inuse, M_SNAPSHOT);
		p->sp_fd.sf_inuse = NULL;
	}
	memset(p, 0, sizeof(struct snapshot_proc));
	free(p, M_SNAPSHOT);
	p = NULL;
	SNAP_CTX_XUNLOCK();
	return 0;
}

static void
clean_snapshot(struct snapshot_proc *sproc)
{
//	clean_memory_snapshot(current->mm);
//	clean_snapshot_vmas(current->mm);

	clean_files_snapshot(sproc);

//	clean_context(current->mm);
//	clear_snapshot(current->mm);
}

/*
 * primary entry point to the logic for snapshot and replay.
 */
int
kern_snapshot(struct thread *td, int cmd, pid_t pid)
{
	struct snapshot_proc *p = NULL;
	pid_t this_pid;

	if (pid == 0) {	
		this_pid = td->td_proc->p_pid;
	} else {
		this_pid = pid;
	}

	switch (cmd) {
	case SNAPSHOT_START:
		DPRINT("* snapshot_start\n");
		p = snapshot_lookup_pid(td, this_pid, 1);
		if (p == NULL) {
			DPRINT("snapshot_start: unable to find pid %d\n", this_pid);
			return ESRCH;
		}	
		make_snapshot(p);
		break;
	case SNAPSHOT_END:
		DPRINT("* snapshot_end\n");
		p = snapshot_lookup_pid(td, this_pid, 0);
		if (p == NULL) {
			DPRINT("snapshot_end: unable to find pid %d\n", this_pid);
			return ESRCH;
		}
		recover_snapshot(p);
		break;
	case SNAPSHOT_CLEAN_PID:
		DPRINT("* snapshot_clean_pid\n");
		return snapshot_clean_pid(this_pid);
	case SNAPSHOT_CLEAN_ALL:
		DPRINT("* snapshot_clean_all\n");
		return snapshot_clean_all();
	case SNAPSHOT_LIST_ALL:
		DPRINT("* snapshot_list_all\n");
		return snapshot_list_all();
	default:
		printf("kern_snapshot: invalid cmd=%d\n", cmd);
		break;
	}
	return 0;
}


int
sys_snapshot(struct thread *td, struct snapshot_args *uap)
{

	return (kern_snapshot(td, uap->cmd, uap->pid));
}
#else
int
sys_snapshot(struct thread *td, struct snapshot_args *uap)
{

	return ENOSYS;
}
#endif	// !SNAPSHOT
