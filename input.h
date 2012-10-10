struct input_event {
  enum {
    IN_PTR_MOVE,
    IN_PTR_UP,
    IN_PTR_DOWN,
    IN_KEY_UP,
    IN_KEY_DOWN
  } type;
  unsigned int id;
  unsigned int x;
  unsigned int y;
  int dx;
  int dy;
  unsigned int state;
  unsigned long unicode;
  unsigned int key;
  unsigned int ms;
};
