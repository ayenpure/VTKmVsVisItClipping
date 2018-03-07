#include <bitset>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <set>
#include <vector>

#include <vtkm/cont/CellSetPermutation.h>
#include <vtkm/cont/DataSet.h>
#include <vtkm/cont/DataSetFieldAdd.h>
#include <vtkm/cont/TryExecute.h>
#include <vtkm/filter/ClipWithField.h>
#include <vtkm/filter/Threshold.h>
#include <vtkm/io/reader/VTKDataSetReader.h>
#include <vtkm/worklet/CellDeepCopy.h>
#include <vtkm/worklet/DispatcherMapField.h>
#include <vtkm/worklet/DispatcherMapTopology.h>
#include <vtkm/worklet/WorkletMapField.h>
#include <vtkm/worklet/WorkletMapTopology.h>

#ifndef VTKM_DEVICE_ADAPTER
#define VTKM_DEVICE_ADAPTER VTKM_DEVICE_ADAPTER_SERIAL
#endif

using clipping_futures = std::vector<std::future<bool>>;

using ExplicitType =
    vtkm::cont::CellSetPermutation<vtkm::cont::CellSetExplicit<>>;
using ExplicitSingleType =
    vtkm::cont::CellSetPermutation<vtkm::cont::CellSetSingleType<>>;
using Structured2d =
    vtkm::cont::CellSetPermutation<vtkm::cont::CellSetStructured<2>>;
using Structured3d =
    vtkm::cont::CellSetPermutation<vtkm::cont::CellSetStructured<3>>;

template <typename CellSetType> struct DeepCopy {
  const CellSetType &m_input;
  vtkm::cont::CellSetExplicit<> &m_output;

  DeepCopy(CellSetType &input, vtkm::cont::CellSetExplicit<> &output)
      : m_input(input), m_output(output) {}

  template <typename Device> bool operator()(Device device) {
    m_output = vtkm::worklet::CellDeepCopy::Run(m_input, Device());
    return true;
  }
};

class GetCases : public vtkm::worklet::WorkletMapPointToCell {
public:
  VTKM_CONT
  GetCases(vtkm::Float32 isoValue) : Value(isoValue) {}

  typedef void ControlSignature(CellSetIn, FieldInPoint<ScalarAll>, FieldOut<>);
  typedef void ExecutionSignature(CellShape, PointCount, _2, _3);

  template <typename CellShapeTag, typename PointCountType,
            typename FieldVecType, typename CaseIdType>
  VTKM_EXEC void operator()(CellShapeTag shape, PointCountType pointCount,
                            FieldVecType &fieldData, CaseIdType &caseId) const {
    (void)shape; // C4100 false positive workaround
    const vtkm::Id mask[] = {1, 2, 4, 8, 16, 32, 64, 128};
    caseId = 0;
    for (int i = 0; i < pointCount; ++i) {
      caseId |= (static_cast<vtkm::Float32>(fieldData[i]) > this->Value)
                    ? mask[i]
                    : 0;
    }
  }

private:
  vtkm::Float32 Value;
};

int CalculateAffectedEdges(std::vector<vtkm::Id> &caseToEdge) {
  const int edges[12][2] = {{0, 1}, {1, 3}, {2, 3}, {0, 2}, {4, 5}, {5, 7},
                            {6, 7}, {4, 6}, {0, 4}, {1, 5}, {3, 7}, {2, 6}};

  for (int i = 0; i < 255; i++) {
    std::bitset<8> casebits(i);
    std::set<int> affectededges;
    for (int edgeind = 0; edgeind < 12; edgeind++) {
      int end1 = edges[edgeind][0];
      int end2 = edges[edgeind][1];
      if ((casebits[end1] == 1 && casebits[end2] == 0) ||
          (casebits[end2] == 1 && casebits[end1] == 0))
        affectededges.insert(edgeind);
    }
    caseToEdge.push_back(affectededges.size());
  }
  caseToEdge.push_back(-1);
}

template <typename DeviceAdapterTag>
class GetAffectedEdgesCount : public vtkm::worklet::WorkletMapField {

private:
  using CaseToEdgePortal = typename vtkm::cont::ArrayHandle<
      vtkm::Id>::template ExecutionTypes<DeviceAdapterTag>::PortalConst;

public:
  typedef void ControlSignature(FieldIn<>, FieldOut<>);
  typedef void ExecutionSignature(_1, _2);
  CaseToEdgePortal caseToEdgePortal;

  VTKM_CONT
  GetAffectedEdgesCount(vtkm::cont::ArrayHandle<vtkm::Id> caseToEdge) {
    caseToEdgePortal = caseToEdge.PrepareForInput(DeviceAdapterTag());
  }

  VTKM_EXEC void operator()(vtkm::Id caseId, vtkm::Id &numOfEdges) const {
    numOfEdges = caseToEdgePortal.Get(caseId);
  }
};

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

void LaunchClippingThreads(std::vector<vtkm::cont::DataSet> &dataIn,
                           const std::string variable,
                           const vtkm::Float32 isoVal,
                           std::vector<vtkm::cont::DataSet> &dataOut,
                           int startPosition,
                           int chunkSize) {
  clipping_futures futures;
  // begin timing
  vtkm::cont::Timer<VTKM_DEFAULT_DEVICE_ADAPTER_TAG> timer;
  for (int i = startPosition; i < startPosition + chunkSize; i++) {
    //std::cout << "Launching thread : " << i << std::endl;
    futures.push_back(std::async(std::launch::async,
                                 performTrivialIsoVolume,
                                 std::ref(dataIn[i]),
                                 variable,
                                 isoVal,
                                 std::ref(dataOut[i])));
  }
  //Sync and end all threads in the current phase.
  for (int i = 0; i < chunkSize; i++) {
    //std::cout << "Getting future : " << i << std::endl;
    if(!futures[i].get())
    {
      std::cerr << "Error occured in syncing thread" << std::endl;
      exit(EXIT_FAILURE);
    }
  }
}

int CastCellSet(vtkm::cont::DataSet &input,
                std::vector<vtkm::cont::DataSet> &dataIn) {
  vtkm::cont::DataSet output;
  vtkm::cont::DynamicCellSet cellSet = input.GetCellSet();
  vtkm::cont::CellSetExplicit<> explicitCellSet;
  if (cellSet.IsSameType(ExplicitType())) {
    ExplicitType explicitType = cellSet.Cast<ExplicitType>();
    DeepCopy<ExplicitType> functor(explicitType, explicitCellSet);
    vtkm::cont::TryExecute(functor);
  } else if (cellSet.IsSameType(ExplicitSingleType())) {
    ExplicitSingleType explicitType = cellSet.Cast<ExplicitSingleType>();
    DeepCopy<ExplicitSingleType> functor(explicitType, explicitCellSet);
    vtkm::cont::TryExecute(functor);
  } else if (cellSet.IsSameType(Structured2d())) {
    Structured2d structuredType = cellSet.Cast<Structured2d>();
    DeepCopy<Structured2d> functor(structuredType, explicitCellSet);
    vtkm::cont::TryExecute(functor);
  } else if (cellSet.IsSameType(Structured3d())) {
    Structured3d structuredType = cellSet.Cast<Structured3d>();
    DeepCopy<Structured3d> functor(structuredType, explicitCellSet);
    vtkm::cont::TryExecute(functor);
  } else {
    output = input;
    return 0;
  }
  output.AddCellSet(explicitCellSet);
  vtkm::Id numCoordSys = input.GetNumberOfCoordinateSystems();
  for (vtkm::Id ind = 0; ind < numCoordSys; ind++)
    output.AddCoordinateSystem(input.GetCoordinateSystem(ind));
  vtkm::Id numFields = input.GetNumberOfFields();
  for (vtkm::Id ind = 0; ind < numFields; ind++)
    output.AddField(input.GetField(ind));
  dataIn.emplace_back(output);
  return 0;
}

template <typename FieldType>
int ApplyThresholdFilter(vtkm::cont::DataSet &dataset, FieldType lowerThreshold,
                         FieldType upperThreshold,
                         const std::string mapVariable,
                         const std::string thresholdVariable,
                         std::vector<vtkm::cont::DataSet> &dataIn) {
  vtkm::filter::Threshold thresholdFilter;
  vtkm::filter::Result result;
  thresholdFilter = vtkm::filter::Threshold();
  thresholdFilter.SetLowerThreshold(lowerThreshold);
  thresholdFilter.SetUpperThreshold(upperThreshold);
  result = thresholdFilter.Execute(dataset, thresholdVariable);
  thresholdFilter.MapFieldOntoOutput(result,
                                     dataset.GetPointField(mapVariable));
  CastCellSet(result.GetDataSet(), dataIn);
  return 0;
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
  int phases = 2;
  if(argc == 5)
    phases = atoi(argv[4]);
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

  vtkm::cont::DynamicArrayHandle fieldData =
      dataset.GetPointField(variable).GetData();

  vtkm::cont::ArrayHandle<vtkm::Id> caseArray;
  caseArray.Allocate(numOfCells);
  caseArray.PrepareForOutput(numOfCells, DeviceAdapterTag());
  GetCases extractCases(isoValue);
  vtkm::worklet::DispatcherMapTopology<GetCases, DeviceAdapterTag>
      extractCasesWorklet(extractCases);
  extractCasesWorklet.Invoke(dataset.GetCellSet(0), fieldData, caseArray);

  std::vector<vtkm::Id> caseToEdge;
  CalculateAffectedEdges(caseToEdge);
  vtkm::cont::ArrayHandle<vtkm::Id> caseToEdgeHandle =
      vtkm::cont::make_ArrayHandle(caseToEdge);
  GetAffectedEdgesCount<DeviceAdapterTag> getEdges(caseToEdgeHandle);
  vtkm::cont::ArrayHandle<vtkm::Id> numAffectedEdges;
  numAffectedEdges.Allocate(numOfCells);
  numAffectedEdges.PrepareForOutput(numOfCells, DeviceAdapterTag());
  vtkm::worklet::DispatcherMapField<GetAffectedEdgesCount<DeviceAdapterTag>,
                                    DeviceAdapterTag>
      getEdgeWorklet(getEdges);
  getEdgeWorklet.Invoke(caseArray, numAffectedEdges);

  const std::string countVar("afEdgeCount");
  vtkm::cont::DataSetFieldAdd datasetFieldAdder;
  datasetFieldAdder.AddCellField(dataset, countVar, numAffectedEdges);

  std::vector<vtkm::cont::DataSet> dataIn;

  ApplyThresholdFilter(dataset, 7, 12, variable, countVar, dataIn);
  ApplyThresholdFilter(dataset, 5, 6, variable, countVar, dataIn);
  ApplyThresholdFilter(dataset, 4, 4, variable, countVar, dataIn);
  ApplyThresholdFilter(dataset, 3, 3, variable, countVar, dataIn);
  //ApplyThresholdFilter(dataset, -1, -1, variable, countVar, dataIn);

  caseArray.ReleaseResources();
  numAffectedEdges.ReleaseResources();

  const size_t outSize = dataIn.size();
  std::vector<vtkm::cont::DataSet> dataOut(outSize);
  int pointer = 0;
  int phase = 0;
  //begin timing
  vtkm::cont::Timer<VTKM_DEFAULT_DEVICE_ADAPTER_TAG> timer;
  while(pointer < outSize)
  {
    if(pointer + phases > outSize)
      phases = outSize - pointer;
    LaunchClippingThreads(dataIn, variable, isoValue, dataOut, pointer, phases);
    pointer += phases;
  }
  std::cout << "Time taken : " << timer.GetElapsedTime() << std::endl;
}
