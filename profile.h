enum {
  PROF_LOOP = 0,
  PROF_EVENTS,
  PROF_IO,
  PROF_DRAW,
  PROF_DRAW_BLIT,
  PROF_DRAW_VIEW,
  PROF_UPDATE_VIEW,
  PROF_END
};

void profile_start(int track);
void profile_end(int track);
void profile_show(void);
