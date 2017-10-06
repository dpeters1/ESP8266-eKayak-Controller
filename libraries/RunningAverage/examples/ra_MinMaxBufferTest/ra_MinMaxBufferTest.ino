//
//    FILE: ra_MinMaxBufferTest.ino
//  AUTHOR: Rob Tillaart
// VERSION: 0.1.00
//    DATE: 2015-SEP-04
//
// PUPROSE: demo
//

#include "RunningAverage.h"

RunningAverage myRA(10);
int samples = 0;

void setup(void)
{
  Serial.begin(115200);
  Serial.print("\nDemo ");
  Serial.println(__FILE__);
  Serial.print("Version: ");
  Serial.println(RUNNINGAVERAGE_LIB_VERSION);
  myRA.clear(); // explicitly start clean

  Serial.println("\nCNT\tMIN\tMINBUF\tMAX\tMAXBUF");
}

void loop(void)
{
  long rn = random(0, 1000);
  myRA.addValue(rn * 0.001);
  samples++;
  Serial.print(samples);
  Serial.print("\t");
  Serial.print(myRA.getMin(), 3);
  Serial.print("\t");
  Serial.print(myRA.getMinInBuffer(), 3);
  Serial.print("\t");
  Serial.print(myRA.getMaxInBuffer(), 3);
  Serial.print("\t");
  Serial.print(myRA.getMax(), 3);
  Serial.println();
  if (samples == 100)
  {
    samples = 0;
    myRA.clear();
    Serial.println("\nCNT\tMIN\tMINBUF\tMAX\tMAXBUF");
  }
  delay(10);
}