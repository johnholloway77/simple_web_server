#!/usr/sbin/dtrace -s
#pragma D option quiet
#pragma D option switchrate=5hz

BEGIN
{
    printf("%-10s %-15s %-8s %-15s %s\n",
           "TIME", "SYSCALL", "PID", "DURATION(us)", "FUNCTION");
}

syscall::read:entry,
syscall::write:entry,
syscall::fstat:entry,
syscall::fstatat:entry
/execname == "simple_server"/
{
    self->start = timestamp;
    self->syscall = probefunc;
}

syscall::read:return,
syscall::write:return,
syscall::fstat:return,
syscall::fstatat:return
/execname == "simple_server" && self->start/
{
    this->duration = (timestamp - self->start) / 1000;  /* microseconds */

    /* Only show slow operations (>1ms) */
    if (this->duration > 1000) {
        printf("%-10d %-15s %-8d %-15d ",
               timestamp / 1000000000,   /* seconds since boot */
               self->syscall,
               pid,
               this->duration);

        /* Print one user stack frame (top frame) */
        printf("%k\n", ustack(1));
        /* Alternative: just print the syscall name without stack:
         * printf("%s\n", self->syscall);
         */
    }

    self->start = 0;
    self->syscall = 0;
}
