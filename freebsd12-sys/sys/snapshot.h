#ifndef	__SNAPSHOT_H
#define	__SNAPSHOT_H

struct snapshot_args;
int     sys_snapshot(struct thread *td, struct snapshot_args *uap);

#include "opt_snapshot.h"

#ifdef	SNAPSHOT
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/sx.h>

#define	SNAPSHOT_START	1
#define	SNAPSHOT_END	2
#define	SNAPSHOT_CLEAN_PID	3
#define	SNAPSHOT_CLEAN_ALL	4
#define	SNAPSHOT_LIST_ALL	5

struct snapshot_fd {
	int	sf_num_entries;
	int	*sf_inuse; // alloc sf_num_entries * sizeof(int)
};

struct snapshot_proc {
	pid_t	sp_pid;
	struct proc	*sp_proc;
	struct snapshot_fd	sp_fd;
	struct snapshot_proc	*sp_next;
};

struct snapshot_ctx {
	struct sx sc_lock;
	int	sc_nentries;
	int	sc_maxentries;

	struct snapshot_proc *sc_head;
};

extern struct snapshot_ctx	snapshot_context;


#define	SNAP_CTX_XLOCK()	sx_xlock(&snapshot_context.sc_lock)
#define	SNAP_CTX_XUNLOCK()	sx_xunlock(&snapshot_context.sc_lock)
#define	SNAP_CTX_SLOCK()	sx_slock(&snapshot_context.sc_lock)
#define	SNAP_CTX_SUNLOCK()	sx_sunlock(&snapshot_context.sc_lock)

#include <sys/malloc.h>

MALLOC_DECLARE(M_SNAPSHOT);

// A bit of a hack to add the pid_t argument like this.
int	kern_snapshot(struct thread *td, int command, pid_t pid);
#endif	// SNAPSHOT
#endif	// !__SNAPSHOT_H
