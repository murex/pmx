#include <stdlib.h>

#include "pmx.h"
#include "structFOO.h"
#include "types_FOO.h"

extern "C" {

TypePrinterEntry type_printer_extension[] = {
      {"FOO", print_struct_FOO, "customized FOO struct", 1, 1},
      {NULL, NULL, NULL, 1, 0}
};


int checkConsistency(mxProc * p, int force)
{
   printf("customized function: %s\n", __FUNCTION__);
   return 0;
}


}
