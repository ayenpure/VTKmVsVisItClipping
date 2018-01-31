#include <cstdlib>
#include <fstream>

#include <vtkm/cont/DataSetFieldAdd.h>
#include <vtkm/filter/ClipWithField.h>
#include <vtkm/io/reader/VTKDataSetReader.h>

// For offscreen rendering
#include <vtkm/rendering/Actor.h>
#include <vtkm/rendering/CanvasRayTracer.h>
#include <vtkm/rendering/MapperRayTracer.h>
#include <vtkm/rendering/Scene.h>
#include <vtkm/rendering/View3D.h>

#ifndef VTKM_DEVICE_ADAPTER
#define VTKM_DEVICE_ADAPTER VTKM_DEVICE_ADAPTER_SERIAL
#endif

// Compute and render the pseudocolor plot for the dataset
int renderDataSet(vtkm::cont::DataSet &dataset) {

  vtkm::rendering::CanvasRayTracer canvas;
  vtkm::rendering::MapperRayTracer mapper;

  vtkm::rendering::Actor actor(dataset.GetCellSet(),
                               dataset.GetCoordinateSystem(),
                               dataset.GetField(0),
                               vtkm::rendering::ColorTable("temperature"));
  vtkm::rendering::Scene scene;
  scene.AddActor(actor);

  // save image.
  vtkm::rendering::View3D view(scene, mapper, canvas);
  view.Initialize();
  view.Paint();
  view.SaveAs("clipped.ppm");

  return 0;
}

class PopulateIndices : public vtkm::worklet::WorkletMapField {
public:
  typedef void ControlSignature(FieldIn<> input, FieldOut<> output);
  typedef void ExecutionSignature(_1, _2);

  template <typename T>
  VTKM_EXEC void operator()(const T &input, T &output) const {
    output = input;
  }
};

int performTrivialIsoVolume(vtkm::cont::DataSet &input, char *variable,
                            vtkm::filter::Result &result,
                            vtkm::Float32 isoValMin) {
  using DeviceAdapterTag = VTKM_DEFAULT_DEVICE_ADAPTER_TAG;
  using DeviceAlgorithm =
      typename vtkm::cont::DeviceAdapterAlgorithm<DeviceAdapterTag>;

  // Add CellIds as cell centerd field.
  vtkm::Id numCells = input.GetCellSet(0).GetNumberOfCells();

  std::cout << "Number of Cells : " << numCells << std::endl;

  vtkm::cont::ArrayHandle<vtkm::Id> cellIds;
  cellIds.Allocate(numCells);
  cellIds.PrepareForInPlace(DeviceAdapterTag());
  vtkm::cont::ArrayHandleIndex indicesImplicitType(numCells);
  vtkm::worklet::DispatcherMapField<PopulateIndices, DeviceAdapterTag>().Invoke(
      indicesImplicitType, cellIds);
  indicesImplicitType.ReleaseResources();

  // Add derived field to dataset.
  std::string cellIdsVar("cellIds");
  vtkm::cont::DataSetFieldAdd datasetFieldAdder;
  datasetFieldAdder.AddCellField(input, cellIdsVar, cellIds);

  vtkm::filter::ClipWithField clip;
  clip.SetClipValue(isoValMin);
  result = clip.Execute(input, std::string(variable));
  clip.MapFieldOntoOutput(result, input.GetPointField(variable));
  clip.MapFieldOntoOutput(result, input.GetCellField(cellIdsVar));
  return 0;
}

class NegateFieldValues : public vtkm::worklet::WorkletMapField {
public:
  typedef void ControlSignature(FieldInOut<> val);
  typedef void ExecutionSignature(_1);

  template <typename T> VTKM_EXEC void operator()(T &val) const { val = -val; }
};

int performMinMaxIsoVolume(vtkm::cont::DataSet &input, char *variable,
                           vtkm::filter::Result &result,
                           vtkm::Float32 isoValMin, vtkm::Float32 isoValMax) {
  using DeviceAdapterTag = VTKM_DEFAULT_DEVICE_ADAPTER_TAG;
  using DeviceAlgorithm =
      typename vtkm::cont::DeviceAdapterAlgorithm<DeviceAdapterTag>;
  vtkm::cont::DataSetFieldAdd datasetFieldAdder;

  // Add CellIds as cell centerd field.
  vtkm::Id numCells = input.GetCellSet(0).GetNumberOfCells();

  std::cout << "Number of Cells : " << numCells << std::endl;

  vtkm::cont::ArrayHandle<vtkm::Id> cellIds;
  cellIds.Allocate(numCells);
  cellIds.PrepareForInPlace(DeviceAdapterTag());
  vtkm::cont::ArrayHandleIndex indicesImplicitType(numCells);
  vtkm::worklet::DispatcherMapField<PopulateIndices, DeviceAdapterTag>().Invoke(
      indicesImplicitType, cellIds);
  indicesImplicitType.ReleaseResources();

  // Add derived field to dataset.
  std::string cellIdsVar("cellIds");
  datasetFieldAdder.AddCellField(input, cellIdsVar, cellIds);

  vtkm::filter::Result firstResult, secondResult;
  vtkm::filter::ClipWithField firstClip, secondClip;

  // Apply clip with Min.
  firstClip.SetClipValue(isoValMin);
  firstResult = firstClip.Execute(input, std::string(variable));
  firstClip.MapFieldOntoOutput(firstResult, input.GetPointField(variable));
  firstClip.MapFieldOntoOutput(firstResult, input.GetCellField(cellIdsVar));

  // Output of first clip.
  vtkm::cont::DataSet &firstClipped = firstResult.GetDataSet();

  // Negate field to apply clip once again.
  vtkm::cont::Field field = firstClipped.GetPointField(variable);
  vtkm::cont::DynamicArrayHandle fieldData = field.GetData();
  vtkm::cont::ArrayHandle<vtkm::Float32> newFieldData;
  newFieldData.Allocate(fieldData.GetNumberOfValues());
  fieldData.CopyTo(newFieldData);
  vtkm::worklet::DispatcherMapField<NegateFieldValues, DeviceAdapterTag>()
      .Invoke(newFieldData);

  // Add derived field to dataset.
  std::string newVariable("newVar");
  datasetFieldAdder.AddPointField(firstClipped, newVariable, newFieldData);

  // Apply clip with Max.
  secondClip.SetClipValue(-isoValMax);
  secondResult = secondClip.Execute(firstClipped, newVariable);
  secondClip.MapFieldOntoOutput(secondResult,
                                firstClipped.GetPointField(newVariable));
  secondClip.MapFieldOntoOutput(secondResult,
                                firstClipped.GetCellField(cellIdsVar));

  // Result of the Min-Max IsoVolume operation.
  result = secondResult;
  return 0;
}

int parseParameters(int argc, char **argv, char **filename, char **variable,
                    vtkm::Id &option, vtkm::Float32 &isoValMin,
                    vtkm::Float32 &isoValMax) {
  if (argc < 3)
    std::cerr << "Invalid number of arguments" << std::endl;

  /* Default Value for IsoVolume */
  isoValMin = 3.0f;

  *filename = argv[1];
  *variable = argv[2];

  if (argc >= 4)
    option = atoi(argv[3]);
  if (argc >= 5)
    isoValMin = atof(argv[4]);
  if (argc == 6)
    isoValMax = atof(argv[5]);
}

int processForSplitCells(vtkm::cont::DataSet &dataSet) {
  using DeviceAdapterTag = VTKM_DEFAULT_DEVICE_ADAPTER_TAG;
  using DeviceAlgorithm =
      typename vtkm::cont::DeviceAdapterAlgorithm<DeviceAdapterTag>;

  std::string cellIdsVar("cellIds");
  // Get the array handle for the cellIds variable
  vtkm::cont::DynamicArrayHandle fieldData =
      dataSet.GetCellField(cellIdsVar).GetData();
  vtkm::Id numCellIds = fieldData.GetNumberOfValues();
  vtkm::cont::ArrayHandle<vtkm::Id> fieldDataHandle;
  fieldDataHandle.Allocate(numCellIds);
  fieldData.CopyTo(fieldDataHandle);
  vtkm::cont::ArrayHandleConstant<vtkm :: Id> toReduce(1, numCellIds);
  // Sort
  DeviceAlgorithm::Sort(fieldDataHandle);
  // Extract unique, these were the cells that appear after the
  // IsoVolume operation.
  vtkm::cont::ArrayHandle<vtkm::Id> uniqueCellIds;
  vtkm::cont::ArrayHandle<vtkm::Id> countCellIds;
  // Reduce by Key
  DeviceAlgorithm::ReduceByKey(fieldDataHandle, toReduce, uniqueCellIds,
                               countCellIds, vtkm::Add());
  vtkm::Id uniqueKeys = uniqueCellIds.GetNumberOfValues();
  std::cout << "Number of unique Cell IDs : " << uniqueKeys << std::endl;
  auto keyPortal = uniqueCellIds.GetPortalConstControl();
  auto countPortal = countCellIds.GetPortalConstControl();
  std::ofstream vtkmfile;
  vtkmfile.open("vtkmfile.csv");
  vtkmfile << "cellid, occur, app" << std::endl;
  for(int i = 0; i < uniqueKeys; i++)
    vtkmfile << keyPortal.Get(i) << ", " << countPortal.Get(i) << ", VTK-m" << std::endl;
  vtkmfile.close();

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
  vtkmfile.open("vtkmbinningfile.csv");
  for(int i = 0; i < uniqueKeys; i++)
    vtkmfile << splitCountPortal.Get(i) << ", " << likeCountPortal.Get(i) << std::endl;
  vtkmfile.close();

}

int main(int argc, char **argv) {

  if (argc < 3)
  {
    std::cerr << "Invalid num of arguments " << std::endl;
    exit(0);
  }
  char *filename, *variable;
  vtkm::Id option = 0;
  vtkm::Float32 isoValMin = 0.0f, isoValMax = 0.0f;
  parseParameters(argc, argv, &filename, &variable, option, isoValMin,
                  isoValMax);
  // Read dataset
  vtkm::io::reader::VTKDataSetReader reader(filename);
  vtkm::cont::DataSet input = reader.ReadDataSet();

  // Query original dataset
  std::cout << "Original number of Cells : "
            << input.GetCellSet(0).GetNumberOfCells() << std::endl;

  // Apply filter
  vtkm::filter::Result result;

  switch (option) {
  case 5:
    std::cout << "Executing Min-Max IsoVolume." << std::endl;
    performMinMaxIsoVolume(input, variable, result, isoValMin, isoValMax);
    break;
  default:
    std::cout << "Executing trivial IsoVolume." << std::endl;
    performTrivialIsoVolume(input, variable, result, isoValMin);
    break;
  }

  // Retrieve resultant dataset
  vtkm::cont::DataSet clipped = result.GetDataSet();

  // Query resultant dataset
  std::cout << "Filtered number of Cells : "
            << clipped.GetCellSet(0).GetNumberOfCells() << std::endl;
  std::cout << "Filtered number of Fields : " << clipped.GetNumberOfFields()
            << std::endl;

  processForSplitCells(clipped);
  // Render for verification if the dataset looks like VisIt.
  renderDataSet(clipped);

  return 0;
}
