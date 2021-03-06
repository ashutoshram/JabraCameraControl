#include "MacFrameCapture.h"
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <sys/time.h>

double time_stamp ()
{
   struct timeval t;
   gettimeofday(&t, 0);
   return t.tv_sec + t.tv_usec*1e-6;
}

bool saveFrame(RawFrame * frame){
   const char * name = frame->format == PANACAST_FRAME_FORMAT_YUYV ?"shutup.yuyv" : "shutup.jpg";
   FILE * panafile = fopen(name, "wb");
   

   unsigned int frameSize = frame->format == PANACAST_FRAME_FORMAT_YUYV ? frame->width * frame->height * 2 : frame->size;  
   if (panafile != NULL){
      if ( fwrite(frame->buf, sizeof(char), frameSize, panafile) != frameSize ){
         printf("what the heck cannot write to file %s\n", name);
         return false;
      } else {
         fclose(panafile);
         printf("wrote to file %s\n", name);
         return true;
      }
   } else {
      printf("unable to write to file %s\n", name);
      return false;
   } 
}



int main(int argc, char * argv[])
{
   MacCameraCapture m;   
   if (!m.init(1280, 720, PANACAST_FRAME_FORMAT_YUYV, NULL)){
      printf("This code is retarded\n");
      return -1;
   }

   double startTime = time_stamp();
   double secondsToCount = 5;
   int counter = 0;
   unsigned int microseconds = 1e6/40;
   printf("sleep between frames = %u\n", microseconds);

   bool saveFrameFlag = false;
   while (true) {

      //printf("main: calling getNextFrame\n");
      RawFrame * frame = m.getNextFrame();

      if (frame != NULL) {
         if (saveFrameFlag) {
            saveFrame(frame);
            saveFrameFlag = false;
         }
         m.freeFrame(); // tell MacCameraCapture that we are done with this frame

         counter ++;
         double secondsPassed = (double)(time_stamp() - startTime);
         //printf("secondsPassed = %f\n", secondsPassed);

         if (secondsPassed >= secondsToCount) {

            double fps = counter / secondsPassed;
            std::cout << fps << " fps" << std::endl;
            startTime = time_stamp();
            counter = 0;

         }
      }
      
      //usleep(microseconds);
   }
   
}
