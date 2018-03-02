#include <cstdlib>
#include <iostream>
#include <vector>
#include <cmath>

#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/DataSetBuilderRectilinear.h>
#include <vtkm/cont/DataSetFieldAdd.h>
#include <vtkm/io/writer/VTKDataSetWriter.h>

#define PARAMS 10

int getIndexFromLogical(int x, int y, int z,
                        vtkm::Vec<vtkm::Id, 3>& dims)
{
  // z*slice + y*width + x;
  int sliceSize = dims[0]*dims[1];
  int width = dims[0];
  return z*sliceSize + y*width + x;
}

int populateFieldData(std::vector<vtkm::Float32>& fieldDataVector,
                      vtkm::Vec<vtkm::Id, 3> &dims)
{
  for(int x = 0; x < dims[0]; x++)
    for(int y = x; y < dims[1] - x; y++)
      for(int z = x; z < dims[2] - x; z++)
      {
        int index = getIndexFromLogical(x,y,z,dims);
        fieldDataVector[index] = 1;
      }
}

int main(int argc, char **argv) {
  if (argc < PARAMS) {
    std::cout << "Invalid number of parameters" << std::endl;
    exit(1);
  }
  // Where we write the dataset;
  std::string filename(argv[1]);
  std::string variable("myscalar");

  int numParams = argc - 2;
  float *params = new float[numParams];
  for (int index = 0; index < numParams; index++)
    params[index] = atof(argv[index + 2]);

  vtkm::Vec<vtkm::Float32, 3> origin(params[0], params[1], params[2]);

  vtkm::Vec<vtkm::Float32, 3> extremes(params[3], params[4], params[5]);

  vtkm::Vec<vtkm::Id, 3> dims(params[6], params[7], params[8]);

  vtkm::Vec<vtkm::Float32, 3> spacing(
      (extremes[0] - origin[0]) / (float)(dims[0] - 1),
      (extremes[1] - origin[1]) / (float)(dims[1] - 1),
      (extremes[2] - origin[2]) / (float)(dims[2] - 1));

  std::vector<vtkm::Float32> xCords, yCords, zCords;
  for (int index = 0; index < dims[0]; index++)
    xCords.push_back(origin[0] + index * spacing[0]);
  for (int index = 0; index < dims[1]; index++)
    yCords.push_back(origin[1] + index * spacing[1]);
  for (int index = 0; index < dims[2]; index++)
    zCords.push_back(origin[2] + index * spacing[2]);

  vtkm::cont::DataSetBuilderRectilinear dataSetBuilder;
  vtkm::cont::DataSet dataset = dataSetBuilder.Create(xCords, yCords, zCords);

  vtkm::Id numFields = dims[0] * dims[1] * dims[2];
  std::vector<vtkm::Float32> fieldDataVector(numFields, 0.0f);

  populateFieldData(fieldDataVector, dims);

  vtkm::cont::ArrayHandle<vtkm::Float32> fieldDataHandle =
      vtkm::cont::make_ArrayHandle(fieldDataVector);

  // Add derived field to dataset.
  vtkm::cont::DataSetFieldAdd datasetFieldAdder;
  datasetFieldAdder.AddPointField(dataset, variable, fieldDataHandle);

  std::cout << "Number of Fields " << dataset.GetNumberOfFields() << std::endl;

  std::cout << "writing the dataset into " << filename << std::endl;
  vtkm::io::writer::VTKDataSetWriter writer(filename);
  writer.WriteDataSet(dataset, static_cast<vtkm::Id>(0));

  return 0;
}
