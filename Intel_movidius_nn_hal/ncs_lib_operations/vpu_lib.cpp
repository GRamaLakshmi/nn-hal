// Copyright 2018 Intel Corporation.
// The source code, information and material ("Material") contained herein is
// owned by Intel Corporation or its suppliers or licensors, and title to such
// Material remains with Intel Corporation or its suppliers or licensors.
// The Material contains proprietary information of Intel or its suppliers and
// licensors. The Material is protected by worldwide copyright laws and treaty
// provisions.
// No part of the Material may be used, copied, reproduced, modified, published,
// uploaded, posted, transmitted, distributed or disclosed in any way without
// Intel's prior express written permission. No license under any patent,
// copyright or other intellectual property rights in the Material is granted to
// or conferred upon you, either expressly, by implication, inducement, estoppel
// or otherwise.
// Any license under such intellectual property rights must be express and
// approved by Intel in writing.


#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <mvnc.h>
#include <log/log.h>
#include "fp.h"
#include "vpu_lib.h"

// Global Variables
#define NCS_NUM 1
#define NAME_SIZE 100


//TODO Check what is needed

mvncStatus retCode;
void *deviceHandle;
bool device_online = false;
char devName[100];
unsigned int graphFileLen;


void* resultData16;
void* userParam;
unsigned int lenResultData;

unsigned int lenip1_fp16, lenip2_fp16;

// 16 bits.  will use this to store half precision floats since C++ has no
// built in support for it.
typedef unsigned short half;
mvncStatus status;
half *ip1_fp16;

//mvncStatus init(int ncs_num);

mvncStatus ncs_init(int ncs_num){

  if(!device_online){
    retCode = mvncGetDeviceName(ncs_num-1, devName, NAME_SIZE);

    if (retCode != MVNC_OK)
    {   // failed to get device name, maybe none plugged in.
        ALOGE("Error- No NCS Device found ErrorCode: %d",retCode); //printf("Error - No NCS devices found.\n");
        device_online = false;
        return retCode;
    }else{
      ALOGE("NCS Device found with ErrorCode: %d",retCode);
      device_online = true;
    }

    retCode = mvncOpenDevice(devName, &deviceHandle);

    if (retCode != MVNC_OK)
    {   // failed to open the device.
        ALOGE("Error - Could not open NCS device ErrorCode: %d",retCode); //printf("Error - Could not open NCS device.\n");
        device_online = false;
        return retCode;
    }
    device_online = true;
  }
  return retCode;
}

//mvncStatus deinit();

mvncStatus ncs_deinit(){
  retCode = mvncCloseDevice(deviceHandle);
  deviceHandle = NULL;
  if (retCode != MVNC_OK)
  {
      ALOGE("Error - Could not close NCS device ErrorCode: %d",retCode);
  }
  return retCode;
}//end of deinit


//graph file loading
void *LoadgraphFile(const char *path, unsigned int *length){

  FILE *fp;
  char *buf;
  fp = fopen(path, "rb");
  if(fp == NULL)
  {
    ALOGE("unable to open graph file");
  }
  fseek(fp, 0, SEEK_END);
  *length = ftell(fp);
  rewind(fp);
  if(!(buf = (char*) malloc(*length)))
  {
    ALOGE("unable to create graph buffer");
  }
  if(fread(buf, 1, *length, fp) != *length)
  {
    fclose(fp);
    free(buf);
  }
  fclose(fp);
  return buf;
}

mvncStatus ncs_rungraph(float *input_data, uint32_t input_num_of_elements,
                    float *output_data, uint32_t output_num_of_elements){

                      char *path;
                      path="/data/ncs_graph";
                      void* graphHandle;
                      void* graphFileBuf = LoadgraphFile(path, &graphFileLen);

                      // allocate the graph
                      retCode = mvncAllocateGraph(deviceHandle, &graphHandle, graphFileBuf, graphFileLen);
                      if (retCode != MVNC_OK){
                        ALOGE("Could not allocate graph for file: %d",retCode);
                        return retCode;
                      }
                      ALOGD("Graph Allocated successfully!");
                      //convert inputs from fp32 to fp16



                      //allocate fp16 input1 with inpu1 shape
                      ip1_fp16 = (half*) malloc(sizeof(*ip1_fp16) * input_num_of_elements);
                      floattofp16((unsigned char *)ip1_fp16, input_data, input_num_of_elements);

                      lenip1_fp16 = input_num_of_elements * sizeof(*ip1_fp16);

                      // start the inference with mvncLoadTensor()
                      retCode = mvncLoadTensor(graphHandle, ip1_fp16, lenip1_fp16, NULL);
                      if (retCode != MVNC_OK){
                        ALOGE("Could not LoadTensor into NCS: %d",retCode);
                        return retCode;
                      }
                      ALOGD("Input Tensor Loaded successfully!");
                      retCode = mvncGetResult(graphHandle, &resultData16, &lenResultData, &userParam);
                      if (retCode != MVNC_OK){
                        ALOGE("NCS could not return result %d",retCode);
                        return retCode;
                      }

                      retCode = mvncDeallocateGraph(graphHandle);
                      if (retCode != MVNC_OK){
                        ALOGE("NCS could not Deallocate Graph %d",retCode);
                        return retCode;
                      }
                      ALOGD("Graph Deallocated successfully!");
                      graphHandle = NULL;
                      int numResults = lenResultData / sizeof(half);

                      fp16tofloat(output_data, (unsigned char*)resultData16, numResults);

                      free(graphFileBuf);
                      free(ip1_fp16);
                      ALOGD("Error code end of the rungraph is : %d",retCode);

                      return retCode;
                    }

int ncs_execute(float *input_data, uint32_t input_num_of_elements,float *output_data, uint32_t output_num_of_elements){

  if (device_online != true) {
    if (ncs_init(NCS_NUM) != MVNC_OK){
      ALOGE("Device Unavilable");
      exit(-1);
    }
  }
  ALOGD("Device avilable");
  status = ncs_rungraph((float*) input_data, input_num_of_elements, output_data, output_num_of_elements);
  /*
  if (ncs_deinit() != MVNC_OK){
    ALOGE("Device not Closed properly");
    exit(-1);
  }

  ALOGD("Device Closed properly");
  */
  return 0;
}
