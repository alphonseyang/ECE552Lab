#include<stdlib.h>

int main () {
   int i;

   char* mb = (char*)malloc(sizeof(char) * 1000000);


   // we skip every 128 here, so that some of the lines that cache prefetched are not used, increase the miss rate
   for (i = 0; i < 1000000; i=i+128) {
       mb[i] = 'a';
   }

   return 0;
}
