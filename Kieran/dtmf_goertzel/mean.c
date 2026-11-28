/*
 * File: mean.c
 *
 * MATLAB Coder version            : 26.2
 * C/C++ source code generated on  : 27-Nov-2026 20:57:30
 */

/* Include Files */
#include "mean.h"

/* Function Definitions */
/*
 * Arguments    : const float x[205]
 * Return Type  : float
 */
float mean(const float x[205])
{
  float y;
  int k;
  y = x[0];
  for (k = 0; k < 204; k++) {
    y += x[k + 1];
  }
  y /= 205.0F;
  return y;
}

/*
 * File trailer for mean.c
 *
 * [EOF]
 */
