/*
 * File: dtmf_goertzel_2_final.c
 *
 * MATLAB Coder version            : 26.2
 * C/C++ source code generated on  : 27-Nov-2026 20:57:30
 */

/* Include Files */
#include "dtmf_goertzel_2_final.h"
#include "mean.h"

/* Function Definitions */
/*
 * Arguments    : const unsigned short x[205]
 *                float p[8]
 * Return Type  : void
 */
void dtmf_goertzel_2_final(const unsigned short x[205], float p[8])
{
  static const float fv[8] = {1.7032753F, 1.6358587F, 1.562297F,  1.4828668F,
                              1.163138F,  1.0088346F, 0.7900744F, 0.5594534F};
  float xs[205];
  float s1;
  int i;
  int n;
  for (i = 0; i < 205; i++) {
    xs[i] = x[i];
  }
  s1 = mean(xs);
  for (i = 0; i < 205; i++) {
    xs[i] -= s1;
  }
  for (i = 0; i < 8; i++) {
    float f;
    float s2;
    s1 = 0.0F;
    s2 = 0.0F;
    f = fv[i];
    for (n = 0; n < 205; n++) {
      float s0;
      s0 = (xs[n] + f * s1) - s2;
      s2 = s1;
      s1 = s0;
    }
    p[i] = (s1 * s1 + s2 * s2) - f * s1 * s2;
  }
}

/*
 * File trailer for dtmf_goertzel_2_final.c
 *
 * [EOF]
 */
