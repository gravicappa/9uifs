struct screen {
  char *pixels;
  SDL_Surface *front;
  SDL_Surface *back;
  Imlib_Image  *imlib;
};

extern struct screen screen;

int init_screen(int w, int h);
void release_screen();
void refresh_screen();
