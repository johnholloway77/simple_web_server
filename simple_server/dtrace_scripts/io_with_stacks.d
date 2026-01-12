 #!/usr/sbin/dtrace -s

  #pragma D option quiet
  #pragma D option switchrate=10hz

  syscall::read:entry,
  syscall::write:entry,
  syscall::pread:entry,
  syscall::fstat:entry,
  syscall::fstatat:entry,
  syscall::lseek:entry,
  syscall::open:entry,
  syscall::openat:entry
  /execname == "simple_server"/
  {
      self->ts = timestamp;
      self->syscall = probefunc;
  }

  syscall::read:return,
  syscall::write:return,
  syscall::pread:return,
  syscall::fstat:return,
  syscall::fstatat:return,
  syscall::lseek:return,
  syscall::open:return,
  syscall::openat:return
  /execname == "simple_server" && self->ts/
  {
      @time[self->syscall, ustack()] = sum(timestamp - self->ts);
      @count[self->syscall, ustack()] = count();
      self->ts = 0;
      self->syscall = 0;
  }

  END {
      printf("\n=== I/O Time by Call Stack (nanoseconds) ===\n");
      printa("%-10s %@d ns (count: %@d)\n%k\n\n", @time, @count);
  }