#include "pgl_asset_collada.h"

void collada(parse)(const char *filename);

int main(int argc, const char** argv){
  collada(parse)(argv[argc-1]);
  return 0;
}
