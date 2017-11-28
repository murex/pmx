#include "pmx.h"
#include "structFOO.h"
#include "types_FOO.h"

extern "C" {

void checkConsistency(mxProc * p, int force)
{
   printf("customized function: %s\n", __FUNCTION__);
}

}
