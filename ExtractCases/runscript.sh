#!/bin/bash
mkdir -p runs
if [ $1 -eq 0 ]
then 
  echo "Running for unoptimized program"
  ./vanillaCU datasets/noise.vtk hardyglobal 3.2 &> runs/vn
  ./vanillaCU datasets/fishtank256.vtk grad_magnitude 42 &> runs/vf256
  ./vanillaCU datasets/fishtank348.vtk grad_magnitude 42 &> runs/vf348
  ./vanillaCU datasets/fishtank512.vtk grad_magnitude 42 &> runs/vf512
  echo "Running for optimized program"
  ./caseextractorCU datasets/noise.vtk hardyglobal 3.2 &> runs/cn
  ./caseextractorCU datasets/fishtank256.vtk grad_magnitude 42 &> runs/cf256
  ./caseextractorCU datasets/fishtank348.vtk grad_magnitude 42 &> runs/cf348
  ./caseextractorCU datasets/fishtank512.vtk grad_magnitude 42 &> runs/cf512
else
  echo "Running for unoptimized program"
  nvprof -m warp_execution_efficiency,achieved_occupancy ./vanillaCU datasets/noise.vtk hardyglobal 3.2 &> runs/vnp
  nvprof -m warp_execution_efficiency,achieved_occupancy ./vanillaCU datasets/fishtank256.vtk grad_magnitude 42 &> runs/vf256p
  nvprof -m warp_execution_efficiency,achieved_occupancy ./vanillaCU datasets/fishtank348.vtk grad_magnitude 42 &> runs/vf348p
  nvprof -m warp_execution_efficiency,achieved_occupancy ./vanillaCU datasets/fishtank512.vtk grad_magnitude 42 &> runs/vf512p
  echo "Running for optimized program"
  nvprof -m warp_execution_efficiency,achieved_occupancy ./caseextractorCU datasets/noise.vtk hardyglobal 3.2 &> runs/cn
  nvprof -m warp_execution_efficiency,achieved_occupancy ./caseextractorCU datasets/fishtank256.vtk grad_magnitude 42 &> runs/cf256p
  nvprof -m warp_execution_efficiency,achieved_occupancy ./caseextractorCU datasets/fishtank348.vtk grad_magnitude 42 &> runs/cf348p
  nvprof -m warp_execution_efficiency,achieved_occupancy ./caseextractorCU datasets/fishtank512.vtk grad_magnitude 42 &> runs/cf512p
fi


