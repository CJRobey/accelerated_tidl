/******************************************************************************
 * Copyright (c) 2019-2020, Texas Instruments Incorporated - http://www.ti.com/
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *       * Neither the name of Texas Instruments Incorporated nor the
 *         names of its contributors may be used to endorse or promote products
 *         derived from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *   THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/
#include <iostream>
#include <string>
#include "reader.h"
// This will include necessary opencv and file stream headers
#include "../common/video_utils.h"
#include "../common/utils.h"

using namespace cv;
using namespace std;

int IMAGE_CLASSES_NUM = 0;
#define MAX_CLASSES 10
#define MAX_SELECTED_ITEMS 10
std::string labels_classes[MAX_CLASSES];
int selected_items_size = 0;
int selected_items[MAX_SELECTED_ITEMS];


static int get_classindex(std::string str2find)
{
  if(selected_items_size >= MAX_SELECTED_ITEMS)
  {
     std::cout << "Max number of selected classes is reached! (" << selected_items_size << ")!" << std::endl;
     return -1;
  }
  for (int i = 0; i < IMAGE_CLASSES_NUM; i ++)
  {
    if(labels_classes[i].compare(str2find) == 0)
    {
      selected_items[selected_items_size ++] = i;
      return i;
    }
  }
  std::cout << "Not found: " << str2find << std::endl << std::flush;
  return -1;
}


int populate_selected_items(const char *filename)
{
  ifstream file(filename);
  if(file.is_open())
  {
    string inputLine;

    while (getline(file, inputLine) )                 //while the end of file is NOT reached
    {
      int res = get_classindex(inputLine);
      std::cout << "Searching for " << inputLine  << std::endl;
      if(res >= 0) {
        std::cout << "Found: " << res << std::endl;
      } else {
        std::cout << "Not Found: " << res << std::endl;
      }
    }
    file.close();
  }
#if 0
  std::cout << "==Total of " << selected_items_size << " items!" << std::endl;
  for (int i = 0; i < selected_items_size; i ++)
    std::cout << i << ") " << selected_items[i] << std::endl;
#endif
  return selected_items_size;
}

void populate_labels(const char *filename)
{
  ifstream file(filename);
  if(file.is_open())
  {
    string inputLine;

    while (getline(file, inputLine) )                 //while the end of file is NOT reached
    {
      labels_classes[IMAGE_CLASSES_NUM ++] = string(inputLine);
    }
    file.close();
  }
#if 1
  std::cout << "==Total of " << IMAGE_CLASSES_NUM << " items!" << std::endl;
  for (int i = 0; i < IMAGE_CLASSES_NUM; i ++)
    std::cout << i << ") " << labels_classes[i] << std::endl;
#endif
}
