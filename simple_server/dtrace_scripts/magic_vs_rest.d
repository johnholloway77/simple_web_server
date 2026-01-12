#!/usr/sbin/dtrace -s
#pragma D option quiet
#pragma D option aggsize=64m
#pragma D option dynvarsize=64m

inline string TARGET = "simple_server";

BEGIN
{
  printf("Tracing %s: magic.mgc I/O vs other + fstat timing... Ctrl-C to end.\n", TARGET);
}

/* ---- fd -> path mapping (stash at entry, use at return) ---- */

/* open(path, ...) */
syscall:freebsd:open:entry
/execname == TARGET/
{
  self->open_path = copyinstr(arg0);
  self->have_open_path = 1;
}

syscall:freebsd:open:return
/execname == TARGET && self->have_open_path/
{
  /* return value is arg0 for syscall:return probes on FreeBSD */
  if (arg0 >= 0) {
    self->fdpath[arg0] = self->open_path;
  }
  self->have_open_path = 0;
  self->open_path = 0;
}

/* openat(dirfd, path, ...) */
syscall:freebsd:openat:entry
/execname == TARGET/
{
  self->openat_path = copyinstr(arg1);
  self->have_openat_path = 1;
}

syscall:freebsd:openat:return
/execname == TARGET && self->have_openat_path/
{
  if (arg0 >= 0) {
    self->fdpath[arg0] = self->openat_path;
  }
  self->have_openat_path = 0;
  self->openat_path = 0;
}

/* close(fd) clears mapping */
syscall:freebsd:close:entry
/execname == TARGET/
{
  self->fdpath[arg0] = 0;
}

/* ---- attribute read/pread time to magic.mgc vs other ---- */

syscall:freebsd:read:entry,
syscall:freebsd:pread:entry
/execname == TARGET/
{
  self->io_start = timestamp;
  self->io_fd = arg0;
}

syscall:freebsd:read:return,
syscall:freebsd:pread:return
/execname == TARGET && self->io_start/
{
  this->dur = timestamp - self->io_start;
  this->bytes = (arg0 > 0) ? arg0 : 0;   /* bytes read returned in arg0 */
  this->p = self->fdpath[self->io_fd];

  this->is_magic = (this->p != 0) && (strstr(this->p, "magic.mgc") != NULL);

  @read_time[this->is_magic ? "magic.mgc" : "other"] = sum(this->dur);
  @read_bytes[this->is_magic ? "magic.mgc" : "other"] = sum(this->bytes);
  @read_calls[this->is_magic ? "magic.mgc" : "other"] = count();

  self->io_start = 0;
  self->io_fd = -1;
}

/* ---- separately measure fstat/fstatat time (often “server overhead”) ---- */

syscall:freebsd:fstat:entry,
syscall:freebsd:fstatat:entry
/execname == TARGET/
{
  self->st_start = timestamp;
  self->st_name = probefunc;
}

syscall:freebsd:fstat:return,
syscall:freebsd:fstatat:return
/execname == TARGET && self->st_start/
{
  this->dur = timestamp - self->st_start;
  @stat_time[self->st_name] = sum(this->dur);
  @stat_calls[self->st_name] = count();

  self->st_start = 0;
  self->st_name = 0;
}

END
{
  printf("\n=== read/pread time (ns) ===\n");  printa("%-10s %@d\n", @read_time);
  printf("\n=== read/pread bytes ===\n");     printa("%-10s %@d\n", @read_bytes);
  printf("\n=== read/pread calls ===\n");     printa("%-10s %@d\n", @read_calls);

  printf("\n=== fstat/fstatat time (ns) ===\n");  printa("%-10s %@d\n", @stat_time);
  printf("\n=== fstat/fstatat calls ===\n");      printa("%-10s %@d\n", @stat_calls);
}
