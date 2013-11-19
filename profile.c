#include <stdio.h>
#include <time.h>

#define PROFILE_C
#include "profile.h"

static unsigned int n[PROF_END] = {0};
static unsigned int times[PROF_END] = {0};
static unsigned int start[PROF_END] = {0};

void
profile_start(int track)
{
  if (track >= 0 && track < PROF_END && !start[track])
    start[track] = clock();
}

void
profile_end2(int track)
{
  if (track >= 0 && track < PROF_END && start[track]) {
    times[track] += clock() - start[track];
    start[track] = 0;
  }
}

void
profile_end(int track)
{
  if (track >= 0 && track < PROF_END && start[track]) {
    ++n[track];
    times[track] += clock() - start[track];
    start[track] = 0;
  }
}

void
profile_show(void)
{
  int i;
  double t;

  printf("%2s\t%10s\t%5s\t%8s\t%8s\n", "Id", "Tag", "N", "T/N(s)", "T(s)");
  for (i = 0; i < PROF_END; ++i) {
    t = (double)times[i] / CLOCKS_PER_SEC;
    printf("%2d\t%10s\t%5d\t%8f\t%8f\n", i, profile_tags[i], n[i],
           (n[i]) ? t / n[i] : -1, t);
  }
}
