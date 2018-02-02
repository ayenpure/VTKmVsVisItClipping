#define VTKM_DEVICE_ADAPTER VTKM_DEVICE_ADAPTER_TBB

#define __BUILDING_TBB_VERSION__

#include "ClippingTrialOffScreen.cxx"
#include <tbb/task_scheduler_init.h>
