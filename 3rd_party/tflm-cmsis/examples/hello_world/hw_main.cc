/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "main_functions.h"
#include <stdio.h>

// This is the default main used on systems that have the standard C entry
// point. Other devices (for example FreeRTOS or ESP32) that have different
// requirements for entry code (like an app_main function) should specialize
// this main.cc file in a target-specific subfolder.
extern "C" int hello_world_main(int argc, char* argv[]);
int hello_world_main(int argc, char* argv[]) {
  // in this demo, we linke everything together
  // so check if the hello_world_setup() has been called or not
  static int inited = false;
  if (inited == false) {
    hello_world_setup();
    inited = true;
  }
  int counter = 100;
  while (counter-->0) {
    printf("inference test count down: %d\n", counter);
    hello_world_loop();
  }
  return 0;
}
