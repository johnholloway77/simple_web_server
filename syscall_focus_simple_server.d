#!/usr/sbin/dtrace -s
#pragma D option quiet
#pragma D option bufsize=16m
#pragma D option aggsize=64m

inline string TARGET = "simple_server";

syscall:::entry
/execname == TARGET/
{
    self->ts = timestamp;
}

syscall:::return
/execname == TARGET && self->ts/
{
    this->delta = timestamp - self->ts;   /* ns */
    @h[probefunc] = quantize(this->delta);
    @sum[probefunc] = sum(this->delta);
    @cnt[probefunc] = count();
    self->ts = 0;
}

END
{
    printf("\n--- Syscall latency histogram (ns) for %s ---\n", TARGET);
    printa(@h);

    printf("\n--- Syscalls by total time (ns) ---\n");
    printa(@sum);

    printf("\n--- Syscall counts ---\n");
    printa(@cnt);
}
