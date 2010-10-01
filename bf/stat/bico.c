#include <math.h>
#include "stat.h"

float bico(int n, int k) {
 return floor(0.5+exp(factln(n)-factln(k)-factln(n-k)));
}
