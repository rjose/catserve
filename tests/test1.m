#include <err.h>
#include <stdlib.h>
#include <string.h>

#import "Testing.h"


int main()
{
        START_SET("Placeholder");

        pass(1 == 1, "Placeholder");

        END_SET("Placeholder");

        return 0;
}
