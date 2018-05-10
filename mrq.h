
enum cmds {
  PUSH = 1,
  PULL,
  CMDS_END
};

struct settings {
  int port; 
  int max_memory;
};

extern struct settings settings;


