/*******************************************************************************
*
* Copyright (c) {2003-2018} Murex S.A.S. and its affiliates.
* All rights reserved. This program and the accompanying materials
* are made available under the terms of the Eclipse Public License v1.0
* which accompanies this distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*
*******************************************************************************/

#include <stdlib.h>

#include "pmx.h"
#include "structFOO.h"
#include "types_FOO.h"

extern "C" {

TypePrinterEntry type_printer_extension[] = {
      {"FOO", print_struct_FOO, "customized FOO struct", 1, 1},
      // always include a NULL entry at the end of the array
      {NULL, NULL, NULL, 1, 0}
};


int checkConsistency(mxProc * p, int force)
{
   printf("customized function: %s\n", __FUNCTION__);
   return 0;
}


}
