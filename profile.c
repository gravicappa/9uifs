#include <stdio.h>
#include <time.h>

#include "profile.h"

static unsigned int n[PROF_END] = {0};
static unsigned int times[PROF_END] = {0};
static unsigned int start[PROF_END] = {0};

void
profile_start(int track)
{
  if (track < 0 || track >= PROF_END || start[track])
    return;
  start[track] = clock();
}

void
profile_end(int track)
{
  if (track < 0 || track >= PROF_END)
    return;
  n[track]++;
  times[track] += clock() - start[track];
  start[track] = 0;
}

void
profile_show(void)
{
  int i;
  double t;

  for (i = 0; i < PROF_END; ++i) {
    t = (double)times[i] / CLOCKS_PER_SEC;
    printf("%2d\t%4d\t%8f\t%8f\n", i, n[i], (n[i]) ? t / n[i] : -1, t);
  }
}
