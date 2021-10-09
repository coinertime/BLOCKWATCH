//#include <BlockWatchy.h> //include the Watchy library
#include "BlockWatchy.h"

Watchy w; //instantiate your watchface

void setup() {
  Serial.begin(9600);
  Serial.println("Init");
    
  w.init(); //call init in setup
}
      
void loop() {
  // this should never run, Watchy deep sleeps after init();
}
