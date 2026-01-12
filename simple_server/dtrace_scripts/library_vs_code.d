  #!/usr/sbin/dtrace -s

  #pragma D option quiet

  pid$target::magic_*:entry
  {
      self->in_magic = 1;
      self->magic_start = timestamp;
  }

  pid$target::magic_*:return
  /self->in_magic/
  {
      @magic_time[probefunc] = sum(timestamp - self->magic_start);
      @magic_calls[probefunc] = count();
      self->in_magic = 0;
      self->magic_start = 0;
  }

  /* Track syscalls while in magic library */
  syscall::fstat:entry,
  syscall::fstatat:entry,
  syscall::read:entry,
  syscall::lseek:entry
  /execname == "simple_server" && self->in_magic/
  {
      self->magic_syscall_start = timestamp;
  }

  syscall::fstat:return,
  syscall::fstatat:return,
  syscall::read:return,
  syscall::lseek:return
  /execname == "simple_server" && self->in_magic && self->magic_syscall_start/
  {
      @magic_syscalls[probefunc] = sum(timestamp - self->magic_syscall_start);
      @magic_syscall_count[probefunc] = count();
      self->magic_syscall_start = 0;
  }

/* Track syscalls NOT in magic library */
syscall::read:entry,
syscall::write:entry,
syscall::fstat:entry
/execname == "simple_server" && !self->in_magic/
{
    self->user_syscall_start = timestamp;
}

syscall::read:return,
syscall::write:return,
syscall::fstat:return
/execname == "simple_server" && !self->in_magic && self->user_syscall_start/
{
    @user_syscalls[probefunc] = sum(timestamp - self->user_syscall_start);
    @user_syscall_count[probefunc] = count();
    self->user_syscall_start = 0;
}

  END {
      printf("\n=== Magic Library Function Times ===\n");
      printa("%-20s: %@d ns (%@d calls)\n", @magic_time, @magic_calls);

      printf("\n=== Syscalls INSIDE Magic Library ===\n");
      printa("%-15s: %@d ns (%@d calls)\n", @magic_syscalls, @magic_syscall_count);

      printf("\n=== Syscalls OUTSIDE Magic Library (Your Code) ===\n");
      printa("%-15s: %@d ns (%@d calls)\n", @user_syscalls, @user_syscall_count);
  }