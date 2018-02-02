#include <cfloat>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

#include <vtkm/cont/DataSetFieldAdd.h>
#include <vtkm/filter/ClipWithField.h>
#include <vtkm/filter/ClipWithImplicitFunction.h>
#include <vtkm/io/reader/VTKDataSetReader.h>
#include <vtkm/io/writer/VTKDataSetWriter.h>

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
int renderAndWriteDataSet(vtkm::cont::DataSet &dataset) {

  vtkm::rendering::CanvasRayTracer canvas;
  vtkm::rendering::MapperRayTracer mapper;

  vtkm::rendering::Actor actor(dataset.GetCellSet(),
                               dataset.GetCoordinateSystem(),
                               dataset.GetField(0),
                               vtkm::rendering::ColorTable("temperature"));
  vtkm::rendering::Scene scene;
  scene.AddActor(actor);

  // save images.
  vtkm::rendering::View3D view(scene, mapper, canvas);
  view.Initialize();
  view.SetBackgroundColor(vtkm::rendering::Color(1,1,1,1));
  view.SetForegroundColor(vtkm::rendering::Color(0,0,0,1));
  for(int i = 0; i < 16; i++)
  {
    std::ostringstream filename;
    filename << "clipped" << i << ".ppm";
    std::cout << "Writing " << filename.str() << std::endl;
    view.GetCamera().Azimuth(i*45.0);
    std::cout << "shifted " << std::endl;
    view.GetCamera().Elevation(i*45.0);
    std::cout << "lifted " << std::endl;
    view.Paint();
    std::cout << "painted" << std::endl;
    view.SaveAs(filename.str());
  }

  vtkm::io::writer::VTKDataSetWriter writer("workeddataset.vtk");
  writer.WriteDataSet(dataset, static_cast<vtkm::Id>(0));
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
  vtkm::cont::DataSet& firstClipped = firstResult.GetDataSet();

  vtkm::cont::Field field;
  vtkm::cont::DynamicArrayHandle fieldData;
  vtkm::cont::ArrayHandle<vtkm::Float32> newFieldData;
  std::string newVariable(variable);

  // Negate field to apply clip once again.
  field  = firstClipped.GetPointField(variable);
  fieldData = field.GetData();
  newFieldData.Allocate(fieldData.GetNumberOfValues());
  fieldData.CopyTo(newFieldData);
  vtkm::worklet::DispatcherMapField<NegateFieldValues, DeviceAdapterTag>()
      .Invoke(newFieldData);
  // Add derived field to dataset.
  datasetFieldAdder.AddPointField(firstClipped, newVariable, newFieldData);

  // Apply clip with Max.
  secondClip.SetClipValue(-isoValMax);
  secondResult = secondClip.Execute(firstClipped, newVariable);
  secondClip.MapFieldOntoOutput(secondResult,
                                firstClipped.GetPointField(newVariable));
  secondClip.MapFieldOntoOutput(secondResult,
                                firstClipped.GetCellField(cellIdsVar));

  newFieldData.ReleaseResources();

  vtkm::cont::DataSet& secondClipped = secondResult.GetDataSet();
  // Negate field to apply clip once again.
  field  = secondClipped.GetPointField(variable);
  fieldData = field.GetData();
  newFieldData.Allocate(fieldData.GetNumberOfValues());
  fieldData.CopyTo(newFieldData);
  vtkm::worklet::DispatcherMapField<NegateFieldValues, DeviceAdapterTag>()
      .Invoke(newFieldData);
  // Add derived field to dataset.
  datasetFieldAdder.AddPointField(secondClipped, newVariable, newFieldData);

  // Result of the Min-Max IsoVolume operation.
  result = secondResult;
  return 0;
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

int performTrivialClip(vtkm::cont::DataSet &input, char* variable,
                       vtkm::filter::Result &result,
                       vtkm::Vec<vtkm::Float32, 3> origin,
                       vtkm::Vec<vtkm::Float32, 3> normal)
{
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

  vtkm::filter::ClipWithImplicitFunction clip;
  clip.SetImplicitFunction(vtkm::cont::make_ImplicitFunctionHandle(vtkm::Plane(origin, normal)));
  result = clip.Execute(input);
  clip.MapFieldOntoOutput(result, input.GetPointField(variable));
  clip.MapFieldOntoOutput(result, input.GetCellField(cellIdsVar));
  return 0;
}

int parseParameters(int argc, char **argv,
                    char **filename, char **variable,
                    std::vector<float>& params)
{
  if (argc < 3)
    std::cerr << "Invalid number of arguments" << std::endl;
  *filename = argv[1];
  *variable = argv[2];
  int numParams = argc - 3;
  for(int i = 0; i < numParams; i++)
  {
    params.push_back(atof(argv[i+3]));
  }
}

int main(int argc, char **argv) {

  if (argc < 3)
  {
    std::cerr << "Invalid num of arguments " << std::endl;
    exit(0);
  }
  char *filename, *variable;
  std::vector<float> params;
  parseParameters(argc, argv, &filename, &variable, params);
  // Read dataset
  vtkm::io::reader::VTKDataSetReader reader(filename);
  vtkm::cont::DataSet input = reader.ReadDataSet();

  // Query original dataset
  std::cout << "Original number of Cells : "
            << input.GetCellSet(0).GetNumberOfCells() << std::endl;

  // Apply filter begins here.
  vtkm::filter::Result result;

  int option = params.size() == 0 ? 0: (int)params[0];
  float isoValMin = FLT_MIN, isoValMax = FLT_MAX;
  vtkm::Vec<vtkm::Float32, 3> origin;
  vtkm::Vec<vtkm::Float32, 3> normal;

  switch (option) {
  case 1 :
    // Case of Implicit Function.
    origin = vtkm::make_Vec(params[1], params[2], params[3]);
    normal = vtkm::make_Vec(params[4], params[5], params[6]);
    performTrivialClip(input, variable, result,origin, normal);
    break;
  case 2 :
    // Case for simple IsoVolume.
    isoValMax = (params.size() > 1) ? params[1] : 3.0f;
    std::cout << "Executing trivial IsoVolume." << std::endl;
    performTrivialIsoVolume(input, variable, result, isoValMax);
    break;
  case 5 :
    // Case of Min-Max IsoVolume.
    isoValMin = params[1];
    isoValMax = params[2];
    std::cout << "Executing Min-Max IsoVolume." << std::endl;
    performMinMaxIsoVolume(input, variable, result, isoValMin, isoValMax);
    break;
  default:
    std::cout << "Suitable option/params not provided" << std::endl;
    exit(1);
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
  renderAndWriteDataSet(clipped);

  return 0;
}
