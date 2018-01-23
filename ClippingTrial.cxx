#include <cstdlib>

#include <vtkm/filter/ClipWithField.h>
#include <vtkm/io/reader/VTKDataSetReader.h>

// Required for compile, from the example.
#include <vtkm/rendering/internal/OpenGLHeaders.h>

#include <GL/glut.h>

#include <vtkm/cont/DataSetFieldAdd.h>
#include <vtkm/rendering/CanvasGL.h>
#include <vtkm/rendering/ColorTable.h>
#include <vtkm/rendering/MapperGL.h>
#include <vtkm/rendering/View3D.h>


vtkm::rendering::View3D *view = nullptr;

const vtkm::Int32 W = 512, H = 512;
int buttonStates[3] = {GLUT_UP, GLUT_UP, GLUT_UP};
bool shiftKey = false;
int lastx = -1, lasty = -1;

void reshape(int, int) {
  // Don't allow resizing window.
  glutReshapeWindow(W, H);
}

// Render the output using simple OpenGL
void displayCall() {
  view->Paint();
  glutSwapBuffers();
}

// Allow rotations of the camera
void mouseMove(int x, int y) {
  const vtkm::Id width = view->GetCanvas().GetWidth();
  const vtkm::Id height = view->GetCanvas().GetHeight();

  // Map to XY
  y = static_cast<int>(height - y);

  if (lastx != -1 && lasty != -1) {
    vtkm::Float32 x1 = vtkm::Float32(lastx * 2) / vtkm::Float32(width) - 1.0f;
    vtkm::Float32 y1 = vtkm::Float32(lasty * 2) / vtkm::Float32(height) - 1.0f;
    vtkm::Float32 x2 = vtkm::Float32(x * 2) / vtkm::Float32(width) - 1.0f;
    vtkm::Float32 y2 = vtkm::Float32(y * 2) / vtkm::Float32(height) - 1.0f;

    if (buttonStates[0] == GLUT_DOWN) {
      if (shiftKey)
        view->GetCamera().Pan(x2 - x1, y2 - y1);
      else
        view->GetCamera().TrackballRotate(x1, y1, x2, y2);
    } else if (buttonStates[1] == GLUT_DOWN)
      view->GetCamera().Zoom(y2 - y1);
  }

  lastx = x;
  lasty = y;
  glutPostRedisplay();
}

// Respond to mouse button
void mouseCall(int button, int state, int vtkmNotUsed(x), int vtkmNotUsed(y)) {
  int modifiers = glutGetModifiers();
  shiftKey = modifiers & GLUT_ACTIVE_SHIFT;
  buttonStates[button] = state;

  // mouse down, reset.
  if (buttonStates[button] == GLUT_DOWN) {
    lastx = -1;
    lasty = -1;
  }
}

// Compute and render the pseudocolor plot for the dataset
int renderDataSet(vtkm::cont::DataSet &dataset) {
  lastx = lasty = -1;
  glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowSize(W, H);
  glutCreateWindow("Clip and Iso-surfacing");
  glutDisplayFunc(displayCall);
  glutMotionFunc(mouseMove);
  glutMouseFunc(mouseCall);
  glutReshapeFunc(reshape);

  vtkm::rendering::Color bg(0.2f, 0.2f, 0.2f, 1.0f);
  vtkm::rendering::CanvasGL canvas;
  vtkm::rendering::MapperGL mapper;

  vtkm::rendering::Scene scene;
  scene.AddActor(vtkm::rendering::Actor(
      dataset.GetCellSet(), dataset.GetCoordinateSystem(), dataset.GetField(0),
      vtkm::rendering::ColorTable("rainbow")));

  // Create vtkm rendering stuff.
  view = new vtkm::rendering::View3D(scene, mapper, canvas, bg);
  view->Initialize();
  glutMainLoop();

  return 0;
}

int performTrivialIsoVolume(vtkm::cont::DataSet& input,
                            char *variable,
                            vtkm::filter::Result& result,
                            vtkm::Float32 isoValMin)
{
  vtkm::filter::ClipWithField clip;
  clip.SetClipValue(isoValMin);
  result = clip.Execute(input, std::string(variable));
  clip.MapFieldOntoOutput(result, input.GetPointField(variable));
  return 0;
}

class NegateFieldValues : public vtkm::worklet::WorkletMapField
{
public :
  typedef void ControlSignature(FieldInOut<> val);
  typedef void ExecutionSignature(_1);

  template<typename T>
  VTKM_EXEC void operator()(T& val) const
  {
    val = -val;
  }
};

int performMinMaxIsoVolume(vtkm::cont::DataSet& input,
                           char *variable,
                           vtkm::filter::Result& result,
                           vtkm::Float32 isoValMin,
                           vtkm::Float32 isoValMax)
{
  using DeviceAdapterTag = vtkm::cont::DeviceAdapterTagSerial;
  using DeviceAlgorithm = typename vtkm::cont::DeviceAdapterAlgorithm<DeviceAdapterTag>;

  vtkm::filter::Result firstResult, secondResult;
  vtkm::filter::ClipWithField firstClip, secondClip;

  //Apply clip with Min.
  firstClip.SetClipValue(isoValMin);
  firstResult = firstClip.Execute(input, std::string(variable));
  firstClip.MapFieldOntoOutput(firstResult, input.GetPointField(variable));

  //Output of first clip.
  vtkm::cont::DataSet& firstClipped = firstResult.GetDataSet();

  //Negate field to apply clip once again.
  vtkm::cont::Field field = firstClipped.GetPointField(variable);
  vtkm::cont::DynamicArrayHandle fieldData = field.GetData();
  vtkm::cont::ArrayHandle<vtkm::Float32> newFieldData;
  newFieldData.Allocate(fieldData.GetNumberOfValues());
  fieldData.CopyTo(newFieldData);
  vtkm::worklet::DispatcherMapField<NegateFieldValues, DeviceAdapterTag>().Invoke(newFieldData);

  //Add derived field to dataset.
  std::string newVariable("newVar");
  vtkm::cont::DataSetFieldAdd datasetFieldAdder;
  datasetFieldAdder.AddPointField(firstClipped, newVariable, newFieldData);

  //Apply clip with Max.
  secondClip.SetClipValue(-isoValMax);
  secondResult = secondClip.Execute(firstClipped, newVariable);
  secondClip.MapFieldOntoOutput(secondResult, firstClipped.GetPointField(newVariable));

  //Result of the Min-Max IsoVolume operation.
  result = secondResult;
  return 0;
}

int parseParameters(int argc, char **argv, char **filename, char **variable,
                    vtkm::Id &option, vtkm::Float32 &isoValMin,
                    vtkm::Float32 &isoValMax) {
  if (argc < 3)
    std::cerr << "Invalid number of arguments" << std::endl;

  /*Default Value for IsoVolume*/
  isoValMin = 3.0f;

  *filename = argv[1];
  *variable = argv[2];

  if(argc >= 4)
    option = atoi(argv[3]);
  if(argc >= 5)
    isoValMin = atof(argv[4]);
  if(argc == 6)
    isoValMax = atof(argv[5]);
}

int main(int argc, char **argv) {

  if (argc < 3)
    std::cerr << "Invalid num of arguments " << std::endl;

  char *filename, *variable;
  vtkm::Id option = 0;
  vtkm::Float32 isoValMin = 0.0f, isoValMax = 0.0f;
  parseParameters(argc, argv, &filename, &variable, option, isoValMin, isoValMax);

  glutInit(&argc, argv);

  // Read dataset
  vtkm::io::reader::VTKDataSetReader reader(filename);
  vtkm::cont::DataSet input = reader.ReadDataSet();

  // Query original dataset
  std::cout << "Original number of Cells : "
            << input.GetCellSet(0).GetNumberOfCells() << std::endl;

  // Apply filter
  vtkm::filter::Result result;

  switch(option)
  {
    case 5:
      std::cout << "Executing Min-Max IsoVolume." << std::endl;
      performMinMaxIsoVolume(input, variable, result, isoValMin, isoValMax);
      break;
    default :
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

  renderDataSet(clipped);

  return 0;
}
