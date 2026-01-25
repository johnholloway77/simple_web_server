#!/usr/sbin/dtrace -s
#pragma D option quiet
#pragma D option switchrate=5hz
#pragma D option stackframes=20

BEGIN
{
    printf("%-10s %-10s %-8s %-12s %s\n",
           "TIME", "SYSCALL", "PID", "DUR(us)", "KERNEL STACK");
}

syscall::read:entry,
syscall::write:entry,
syscall::fstat:entry,
syscall::fstatat:entry
/execname == "simple_server"/
{
    self->start = timestamp;
    self->sc = probefunc;
}

syscall::read:return,
syscall::write:return,
syscall::fstat:return,
syscall::fstatat:return
/execname == "simple_server" && self->start/
{
    this->dur = (timestamp - self->start) / 1000;

    if (this->dur > 1000) {
        printf("%-10d %-10s %-8d %-12d\n",
               timestamp / 1000000000,
               self->sc,
               pid,
               this->dur);

        /* FreeBSD: stack() gives kernel stack here */
        print(stack());
    }

    self->start = 0;
    self->sc = 0;
}
