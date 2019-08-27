
# FreeBSD Port

I initially started to do things entirely within a FreeBSD kernel module (KLD)
but that was primarily for investigatory reasons. After understanding, a tiny
bit more, the Linux perf-fuzz code, I realized the desire to have some fairly
static things in the kernel be modified for this implementation. For this 
reason, I am currently storing patches to FreeBSD 12.0's /usr/src/sys path.
I will pull in a full tree at some point in the future, I think, but I do
not want the repo to be so heavy just yet.

When applying these, each is a diff against 12.0-RELEASE, so you only want
one patch.


## diffs


- **fpf-001.diff**: Adding the kernel compilation option bits for snapshot().
I add a dummy holder and a real implementation, mostly because I just wanted
to be sure things were defined (likely overkill of me forgetting how to do
things properly). Effectively, the implementation so far has snapshotting of
open file descriptors and reset of fd state back to those logged open at 
snapshot time. This does not attempt to reverse anything, just a hammer
closing fds open that were not open at snapshot time. The implementation 
differs from the Linux one, ignoring bits not implemented yet, in that the
snapshot code does keep some information about processes that are using it.
These are structures that are also pointed to by some values within 
`struct proc`. Currently, there is no clean-up for these snapshot using 
processes... If that proc goes away, the snapshot proc data is still there
pointing at who knows what; so that's an issue. All in all, this is a first
step.


- **fpf-002.diff**: Add mechanisms for listing existing snapshots and cleaning
up those existing. The diff is a superset of the prior diff. I will make things
patch sets or make a git soon enough.
