#include <bitset>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <set>
#include <vector>

#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/DataSetFieldAdd.h>
#include <vtkm/cont/Timer.h>
#include <vtkm/filter/ClipWithField.h>
#include <vtkm/io/reader/VTKDataSetReader.h>
#include <vtkm/worklet/DispatcherMapField.h>
#include <vtkm/worklet/DispatcherMapTopology.h>
#include <vtkm/worklet/WorkletMapField.h>
#include <vtkm/worklet/WorkletMapTopology.h>

bool performTrivialIsoVolume(vtkm::cont::DataSet &input,
                             const std::string variable,
                             const vtkm::Float32 isoVal,
                             vtkm::cont::DataSet &output) {
  using DeviceAdapterTag = VTKM_DEFAULT_DEVICE_ADAPTER_TAG;
  using DeviceAlgorithm =
      typename vtkm::cont::DeviceAdapterAlgorithm<DeviceAdapterTag>;
  vtkm::filter::Result result;
  vtkm::filter::ClipWithField filter;
  // Apply clip isoVal.
  filter.SetClipValue(isoVal);
  result = filter.Execute(input, variable);
  filter.MapFieldOntoOutput(result, input.GetPointField(variable.c_str()));
  // Output of clip.
  output = result.GetDataSet();
  return true;
}

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cout << "Invalid number of arguments" << std::endl;
    exit(1);
  }
  using DeviceAdapterTag = VTKM_DEFAULT_DEVICE_ADAPTER_TAG;
  using DeviceAlgorithm =
      typename vtkm::cont::DeviceAdapterAlgorithm<DeviceAdapterTag>;

  const std::string filename(argv[1]);
  const std::string variable(argv[2]);
  float isoValue = atof(argv[3]);
  std::cout << "Analyzing cases for " << filename << " on variable " << variable
            << " for isovalue " << isoValue << std::endl;

  vtkm::io::reader::VTKDataSetReader reader(filename.c_str());
  vtkm::cont::DataSet dataset = reader.ReadDataSet();

  int numOfCells = dataset.GetCellSet(0).GetNumberOfCells();
  std::cout << "Number of CellSets : " << dataset.GetNumberOfCellSets()
            << std::endl;
  std::cout << "Number of cells " << dataset.GetCellSet(0).GetNumberOfCells()
            << std::endl;
  std::cout << "Number of fields " << dataset.GetNumberOfFields() << std::endl;

  //begin timing
  vtkm::cont::Timer<VTKM_DEFAULT_DEVICE_ADAPTER_TAG> timer;

  vtkm::cont::DataSet output;
  std::cout << "Input Cells : " << dataset.GetCellSet(0).GetNumberOfCells()
            << std::endl;
  performTrivialIsoVolume(dataset, variable, isoValue, output);
  std::cout << "Output Cells : " << output.GetCellSet(0).GetNumberOfCells()
            << std::endl;
  std::cout << "Time taken : " << timer.GetElapsedTime() << std::endl;
}
