import sys, os, timeit

numArgs = len(sys.argv)
if(numArgs < 3) :
  print "Invalid number of arguments"
  sys.exit(0)

filename = sys.argv[1]
variable = sys.argv[2]

option = 0
isoValMin = sys.float_info.min
isoValMax = sys.float_info.max
if(numArgs >= 4) :
  if not sys.argv[3] is "_" :
    isoValMin = float(sys.argv[3])
if(numArgs >= 5) :
  isoValMax = float(sys.argv[4])

print "Min"
print isoValMin
print "Max"
print isoValMax

start = timeit.default_timer()

OpenDatabase(os.path.realpath(filename), 0)
AddPlot("Pseudocolor", variable, 1, 1)
AddOperator("Isovolume", 1)
SetActivePlots(0)

IsovolumeAtts = IsovolumeAttributes()
IsovolumeAtts.lbound = isoValMin
IsovolumeAtts.ubound = isoValMax
IsovolumeAtts.variable = variable

SetOperatorOptions(IsovolumeAtts, 1)
DrawPlots()

end = timeit.default_timer()
elapsed = end - start

print "Time taken : %f" %elapsed

exit()
