#include "log.h"

int main ()
{
   /*
   FILE *fp;
   char *s="123124";
   fp = fopen("file.txt", "w+");
   fputs(s, fp);
   printf("%s\n",s);
   fclose(fp);
   */

   Log* log = Log::get_instance();
   log->init();
   LOG_INFO("123");
   LOG_INFO("123");
   return 0;
}