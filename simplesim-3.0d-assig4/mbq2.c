#include<stdlib.h>

int main () {
   int i;
   
   char* mb = (char*)malloc(sizeof(char) * 1000000);

   // this should be lots of hit, because the stride pattern is easily to be detected
   for (i = 0; i < 1000000; i=i+64) {
       mb[i] = 'a';
   }

   // this is also easily detected, it's steady pattern
   for (i = 0; i < 1000000; i=i+128) {
       mb[i] = 'a';
   }

   int stride_1 = 128;
   int stride_2 = 128;

   // this should generate almost no prefetchs since the stride alternates
   for (i = 0; i < 1000000; i += stride_1) {
		mb[i] = 'a';

	    // this will alternate the stride
       	if (stride_1 == stride_2) {
			stride_1 = stride_2/ 2;
       	}

       	else {
           stride_1 = stride_2;
       }
   }
   return 0;
}
