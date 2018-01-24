import sys

numArgs = len(sys.argv)
if(numArgs < 3) :
  print "Invalid number of arguments"
  sys.exit(0)

filename = sys.argv[1]
varaible = sys.argv[2]

option = 0
isoValMin = sys.float_info.min
isoValMax = sys.float_info.max
if(numArgs >= 5) :
  if not sys.argv[4] is "_" :
    isoValMin = float(sys.argv[4])
if(numArgs >= 6) :
  isoValMax = float(sys.argv[5])

OpenDatabase("localhost:/home/abhishek/big/noise.vtk", 0)
AddPlot("Mesh", "Mesh", 1, 1)
AddOperator("Isovolume", 1)
SetActivePlots(0)

IsovolumeAtts = IsovolumeAttributes()
IsovolumeAtts.lbound = isoValMin
IsovolumeAtts.ubound = isoValMax
IsovolumeAtts.variable = "hardyglobal"

SetOperatorOptions(IsovolumeAtts, 1)
DrawPlots()
ExportDBAtts = ExportDBAttributes()
ExportDBAtts.allTimes = 0
ExportDBAtts.filename = "worked_db"
ExportDBAtts.timeStateFormat = "_%04d"
ExportDBAtts.db_type = "VTK"
ExportDBAtts.db_type_fullname = "VTK_1.0"
ExportDBAtts.variables = ("default")
ExportDBAtts.writeUsingGroups = 0
ExportDBAtts.groupSize = 48
ExportDBAtts.opts.types = (5)
ExportDBAtts.opts.help = ""
ExportDatabase(ExportDBAtts)

print "finished exporting VTK dataset"
