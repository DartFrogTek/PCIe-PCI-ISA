/* Just a simple thingy to generate list of GF1 recording frequencies */

#include <stdio.h>

int main (void)
{
/* 617400 = 7^3 * 5^2 * 3^2 * 2^3 */
int i7, i5, i3, i2;
int f7, f5, f3, f2;

for (i2=0, f2=1; i2<=3; i2++, f2*=2)
 for (i3=0, f3=1; i3<=2; i3++, f3*=3)
  for (i5=0, f5=1; i5<=2; i5++, f5*=5)
   for (i7=0, f7=1; i7<=3; i7++, f7*=7)
	{
	int freq;
	int reg;

	freq = f2 * f3 * f5 * f7;
	reg = 617400 / freq - 2;
	if (reg >= 0 && reg <= 255)
		{
		printf ("%6d,\n", freq);
		}
	}

return 0;
}
