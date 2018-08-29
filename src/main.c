#include "pgl_asset_collada.h"

void collada(parse)(const char *filename);

int main(void){
  collada(parse)("test.dae");
  return 0;
}
