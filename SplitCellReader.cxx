#include <cstdlib>
#include <vector>
#include <vtkm/cont/DataSet.h>
#include <vtkm/io/reader/VTKDataSetReader.h>

int processForSplitCells(vtkm::cont::DataSet &dataSet) {
  using DeviceAdapterTag = vtkm::cont::DeviceAdapterTagSerial;
  using DeviceAlgorithm =
      typename vtkm::cont::DeviceAdapterAlgorithm<DeviceAdapterTag>;

  std::cout << "Number of Fields : " << dataSet.GetNumberOfFields() << std::endl;

  std::string cellIdsVar("cellIds");
  // Get the array handle for the cellIds variable
  vtkm::cont::DynamicArrayHandle fieldData =
      dataSet.GetCellField(cellIdsVar).GetData();
  vtkm::Id numCellIds = fieldData.GetNumberOfValues();
  vtkm::cont::ArrayHandle<vtkm::Id> fieldDataHandle;
  fieldDataHandle.Allocate(numCellIds);
  fieldData.CopyTo(fieldDataHandle);
  vtkm::cont::ArrayHandleConstant<vtkm::Id> toReduce(1, numCellIds);
  // Sort
  DeviceAlgorithm::Sort(fieldDataHandle);
  // Extract unique, these were the cells that appear after the
  // IsoVolume operation.
  vtkm::cont::ArrayHandle<vtkm::Id> uniqueCellIds;
  vtkm::cont::ArrayHandle<vtkm::Id> countCellIds;
  // Reduce by Key
  DeviceAlgorithm::ReduceByKey(fieldDataHandle, toReduce, uniqueCellIds,
                               countCellIds, vtkm::Add());
  std::cout << "Number of unique Cell IDs : "
            << uniqueCellIds.GetNumberOfValues() << std::endl;
  auto keyPortal = uniqueCellIds.GetPortalConstControl();
  auto countPortal = countCellIds.GetPortalConstControl();
  for (int i = 0; i < 10; i++)
    std::cout << keyPortal.Get(i) << " : " << countPortal.Get(i) << std::endl;
  return 0;
}

int parseFileForVisIt(char* filename)
{
  //Get the File
  //look for avtOriginalCellNumbers
  //Parse ignoring 0s and processing all other numbers
  //Put the Data in a new vector and make an ArrayHandle out of it.
  std::vector<vtkm::Id> fieldData;


  vtkm::cont::ArrayHandle<vtkm::Id> fieldDataHandle(fieldData);
  // Array with al 1s to get count when reduced by key.
  vtkm::cont::ArrayHandleConstant<vtkm::Id> toReduce(1, numCellIds);

  // Sort
  DeviceAlgorithm::Sort(fieldDataHandle);

  // Extract unique, these were the cells that appear after the
  // IsoVolume operation.
  vtkm::cont::ArrayHandle<vtkm::Id> uniqueCellIds;
  vtkm::cont::ArrayHandle<vtkm::Id> countCellIds;

  // Reduce by Key
  DeviceAlgorithm::ReduceByKey(fieldDataHandle, toReduce, uniqueCellIds,
                               countCellIds, vtkm::Add());
  std::cout << "Number of unique Cell IDs : "
            << uniqueCellIds.GetNumberOfValues() << std::endl;
  auto keyPortal = uniqueCellIds.GetPortalConstControl();
  auto countPortal = countCellIds.GetPortalConstControl();
  for (int i = 0; i < 10; i++)
    std::cout << keyPortal.Get(i) << " : " << countPortal.Get(i) << std::endl;
  return 0;
}


int main(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "Invalid number of arguments" << std::endl;
    exit(0);
  }
  char* filename = argv[1];
  std::cout << "Calculating the number of Cell Splits" << std::endl;
  vtkm::io::reader::VTKDataSetReader reader(filename);
  vtkm::cont::DataSet input = reader.ReadDataSet();
  processForSplitCells(input);
}
