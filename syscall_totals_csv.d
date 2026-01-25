#!/usr/sbin/dtrace -s
#pragma D option quiet
#pragma D option aggsize=256m
#pragma D option bufsize=64m
#pragma D option dynvarsize=64m

inline string TARGET = "simple_server";

syscall:::entry
/execname == TARGET/
{
    self->ts = timestamp;
}

syscall:::return
/execname == TARGET && self->ts/
{
    this->d = timestamp - self->ts;
    @sum[probefunc] = sum(this->d);
    @cnt[probefunc] = count();
    self->ts = 0;
}

END
{
    printf("syscall,sum_ns,count\n");
    printa("%s,%@d,%@d\n", @sum, @cnt);
}

