  #!/usr/sbin/dtrace -s

  #pragma D option quiet

  syscall::open:entry,
  syscall::openat:entry
  /execname == "simple_server"/
  {
      self->filename = copyinstr(arg1);  /* filename being opened */
  }

  syscall::open:return,
  syscall::openat:return
  /execname == "simple_server" && self->filename != ""/
  {
      this->fd = arg1;  /* returned file descriptor */
      printf("OPEN: %s -> fd %d\n", self->filename, this->fd);

      /* Track this fd */
      fd_to_file[this->fd] = self->filename;
      self->filename = "";
  }

  syscall::read:entry
  /execname == "simple_server" && fd_to_file[arg0] != ""/
  {
      self->read_start = timestamp;
      self->read_fd = arg0;
      self->read_size = arg2;
  }

  syscall::read:return
  /execname == "simple_server" && self->read_start/
  {
      this->duration = timestamp - self->read_start;
      this->bytes = arg1;  /* bytes actually read */

      @file_read_time[fd_to_file[self->read_fd]] = sum(this->duration);
      @file_read_bytes[fd_to_file[self->read_fd]] = sum(this->bytes);
      @file_read_calls[fd_to_file[self->read_fd]] = count();

      printf("READ: %s (%d bytes, %d us)\n",
             fd_to_file[self->read_fd],
             this->bytes,
             this->duration / 1000);

      self->read_start = 0;
      self->read_fd = 0;
  }

  syscall::close:entry
  /execname == "simple_server" && fd_to_file[arg0] != ""/
  {
      printf("CLOSE: %s (fd %d)\n", fd_to_file[arg0], arg0);
      fd_to_file[arg0] = "";
  }

  END {
      printf("\n=== Per-File I/O Summary ===\n");
      printa("File: %s\n  Time: %@d ns\n  Bytes: %@d\n  Calls: %@d\n\n",
             @file_read_time, @file_read_bytes, @file_read_calls);
  }