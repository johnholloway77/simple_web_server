#!/usr/sbin/dtrace -s
#pragma D option quiet
#pragma D option aggsize=128m
#pragma D option bufsize=16m

inline string TARGET = "simple_server";

BEGIN
{
  printf("Tracing %s I/O attribution (magic.mgc vs rest)... Ctrl-C to end.\n", TARGET);
}

/* open(const char *path, int flags, ...) */
syscall::open:entry
/execname == TARGET/
{
  self->open_path = copyinstr(arg0);
  self->have_open_path = 1;
}

syscall::open:return
/execname == TARGET && self->have_open_path && arg0 >= 0/
{
  self->fdpath[arg0] = self->open_path;
  self->open_path = "";
  self->have_open_path = 0;
}

/* openat(int fd, const char *path, int flags, ...) */
syscall::openat:entry
/execname == TARGET/
{
  self->openat_path = copyinstr(arg1);
  self->have_openat_path = 1;
}

syscall::openat:return
/execname == TARGET && self->have_openat_path && arg0 >= 0/
{
  self->fdpath[arg0] = self->openat_path;
  self->openat_path = "";
  self->have_openat_path = 0;
}

/* clear mapping on close(fd) */
syscall::close:entry
/execname == TARGET/
{
  self->fdpath[arg0] = 0;
}

/* time read/pread, attribute by fd->path */
syscall::read:entry,
syscall::pread:entry
/execname == TARGET/
{
  self->io_start = timestamp;
  self->io_fd = arg0;
}

syscall::read:return,
syscall::pread:return
/execname == TARGET && self->io_start/
{
  this->dur = timestamp - self->io_start;
  this->bytes = (arg0 > 0) ? arg0 : 0;
  this->p = self->fdpath[self->io_fd];

  this->is_magic = (this->p != 0) && (strstr(this->p, "magic.mgc") != NULL);

  @io_time[this->is_magic ? "magic.mgc" : "other"] = sum(this->dur);
  @io_bytes[this->is_magic ? "magic.mgc" : "other"] = sum(this->bytes);
  @io_calls[this->is_magic ? "magic.mgc" : "other"] = count();

  self->io_start = 0;
  self->io_fd = -1;
}

END
{
  printf("\n=== I/O time (ns) ===\n");   printa("%-10s %@d\n", @io_time);
  printf("\n=== I/O bytes ===\n");      printa("%-10s %@d\n", @io_bytes);
  printf("\n=== I/O calls ===\n");      printa("%-10s %@d\n", @io_calls);
}
