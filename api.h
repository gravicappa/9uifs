struct input_event {
  enum {
    IN_PTR_MOVE,
    IN_PTR_UP,
    IN_PTR_DOWN,
    IN_KEY_UP,
    IN_KEY_DOWN
  } type;
  unsigned int id;
  unsigned int ms;
  unsigned int x;
  unsigned int y;
  int dx;
  int dy;
  unsigned int state;
  unsigned int mod;
  unsigned int key;
  unsigned long rune;
  char *str;
};

/*
  srvfd — is fd of a server socket
  evfd — is fd of a event pipe to interrupt select on any event (e.g. input).
  frame_ms — minimum time per frame to maintain desired fps
*/
int uifs_process_io(int srvfd, int evfd, unsigned int frame_ms);
int uifs_update(int force);
int uifs_redraw(int force);
int uifs_input_event(struct input_event *ev);
