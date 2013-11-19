enum {
  PROF_LOOP = 0,
  PROF_EVENTS,
  PROF_IO,
  PROF_IO_READ,
  PROF_IO_UNPACK,
  PROF_IO_PACK,
  PROF_IO_PROCESS,
  PROF_IO_DEFERRED,
  PROF_IO_UPD_FDSET,
  PROF_IO_CLIENT,
  PROF_IO_MEMMOVE,
  PROF_IO_SEND,
  PROF_DRAW,
  PROF_DRAW_BLIT,
  PROF_UPDATE,
  PROF_UPDATE_POS,
  PROF_END
};

static const char *profile_tags[] = {
  [PROF_LOOP] = "loop",
  [PROF_EVENTS] = "events",
  [PROF_IO] = "io",
  [PROF_IO_READ] = "io/read",
  [PROF_IO_UNPACK] = "io/unpack",
  [PROF_IO_PACK] = "io/pack",
  [PROF_IO_PROCESS] = "io/process",
  [PROF_IO_DEFERRED] = "io/deferred",
  [PROF_IO_UPD_FDSET] = "io/upd_fdset",
  [PROF_IO_CLIENT] = "io/client",
  [PROF_IO_MEMMOVE] = "io/memmove",
  [PROF_IO_SEND] = "io/send",
  [PROF_DRAW] = "draw",
  [PROF_DRAW_BLIT] = "draw/blit",
  [PROF_UPDATE] = "update",
  [PROF_UPDATE_POS] = "update/pos",
};

void profile_start(int track);
void profile_end(int track);
void profile_show(void);
void profile_end2(int track);
