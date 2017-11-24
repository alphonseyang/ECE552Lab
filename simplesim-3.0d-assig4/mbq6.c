#include<stdlib.h>

int main () {

// Depend on the size of mb, the data we access might not fit into the data address miss queue.
//if the size of mb is 640, then all data we access will fit into the miss queue since 640/ Block size=10 < 16, the miss rate is close to 0
//if the size of mb is 64000,then all data we access will not fit into the miss queue since 64000/ Block size=1000 > 16, the miss rate is close to 100%
   int i;
   int j;
   char* mb = (char*)malloc(sizeof(char) * 640);

   int stride_1 = 128;
   int stride_2 = 128;

   // to have a outer loop so that our prefetcher can use the data address miss queue
    for (j = 0; j < 100000; j ++) {
   for (i = 0; i < 640; i += stride_1) {
		mb[i] = 'a';

	    // this will alternate the stride
       	if (stride_1 == stride_2) {
			stride_1 = stride_2/ 2;
       	}

       	else {
           stride_1 = stride_2;
       }
   }
}
   return 0;
}
