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
#include <stdint.h>
#include "main_functions.h"

// This is the default main used on systems that have the standard C entry
// point. Other devices (for example FreeRTOS or ESP32) that have different
// requirements for entry code (like an app_main function) should specialize
// this main.cc file in a target-specific subfolder.
extern "C" {
  int person_detection_main(int argc, char* argv[]);
};

#include <stdio.h>
int person_detection_main(int argc, char* argv[]) {
  static int inited = false;
  if (inited == false) {
    person_detection_setup();
    inited = true;
  }
  int counter = 10;
  while (counter-->0) {
    person_detection_loop();
    printf("test inference count donw: %d\n", counter);
  }
  return 0;
}