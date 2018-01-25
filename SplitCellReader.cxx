#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
#include <vtkm/cont/DataSet.h>
#include <vtkm/io/reader/VTKDataSetReader.h>

int parseFileForVisIt(char *filename) {
  using DeviceAdapterTag = vtkm::cont::DeviceAdapterTagSerial;
  using DeviceAlgorithm =
      typename vtkm::cont::DeviceAdapterAlgorithm<DeviceAdapterTag>;

  // Get the File
  // look for avtOriginalCellNumbers
  // Parse ignoring 0s and processing all other numbers
  // Put the Data in a new vector and make an ArrayHandle out of it.
  std::vector<vtkm::Id> fieldData;
  std::ifstream toRead;
  toRead.open(filename);
  if (!toRead) {
    std::cout << "No file found to read" << std::endl;
    exit(0);
  }
  bool lineReached = false;
  std::string buffer;
  while(!toRead.eof() && !lineReached)
  {
    std::getline(toRead, buffer);
    if(buffer.find("avtOriginalCellNumbers") != std::string::npos)
      lineReached = true;
  }

  vtkm::Id zero, cellId;
  while(toRead >> zero >> cellId)
  {
    fieldData.push_back(cellId);
  }

  vtkm::cont::ArrayHandle<vtkm::Id> fieldDataHandle =
      vtkm::cont::make_ArrayHandle(fieldData);
  // Array with al 1s to get count when reduced by key.
  vtkm::Id numCellIds = fieldDataHandle.GetNumberOfValues();
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
  vtkm::Id uniqueKeys =  uniqueCellIds.GetNumberOfValues();
  std::cout << "Number of unique Cell IDs : "
            << uniqueKeys << std::endl;
  auto keyPortal = uniqueCellIds.GetPortalConstControl();
  auto countPortal = countCellIds.GetPortalConstControl();
  std::ofstream visitfile;
  visitfile.open("visitfile.csv");
  for(int i = 0; i < uniqueKeys; i++)
    visitfile << keyPortal.Get(i) << ", " << countPortal.Get(i) << std::endl;
  visitfile.close();

  //For binning
  DeviceAlgorithm::Sort(countCellIds);
  vtkm::cont::ArrayHandleConstant<vtkm::Id> toReduceCounts(1, uniqueKeys);

  vtkm::cont::ArrayHandle<vtkm::Id> uniqueCounts;
  vtkm::cont::ArrayHandle<vtkm::Id> likeCountCells;

  DeviceAlgorithm::ReduceByKey(countCellIds, toReduceCounts, uniqueCounts,
                               likeCountCells, vtkm::Add());

  auto splitCountPortal = uniqueCounts.GetPortalConstControl();
  auto likeCountPortal = likeCountCells.GetPortalConstControl();
  uniqueKeys = uniqueCounts.GetNumberOfValues();
  visitfile.open("visitbinningfile.csv");
  for(int i = 0; i < uniqueKeys; i++)
    visitfile << splitCountPortal.Get(i) << ", " << likeCountPortal.Get(i) << std::endl;
  visitfile.close();
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "Invalid number of arguments" << std::endl;
    exit(0);
  }
  char *filename = argv[1];
  std::cout << "Calculating the number of Cell Splits" << std::endl;
  vtkm::io::reader::VTKDataSetReader reader(filename);
  vtkm::cont::DataSet input = reader.ReadDataSet();
  //processForSplitCells(input);
  parseFileForVisIt(filename);
}
