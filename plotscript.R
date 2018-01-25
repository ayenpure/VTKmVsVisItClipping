library("ggplot2")
visitdata <- read.csv(file="/home/abhishek/repositories/VTKmVsVisItClipping/visitfile.csv", head=TRUE, sep=",")
vtkmdata <- read.csv(file="/home/abhishek/repositories/VTKmVsVisItClipping/vtkmfile.csv", head=TRUE, sep=",")

visitrows <- nrow(visitdata)
vtkmrows <- nrow(vtkmdata)

expected <- visitrows + vtkmrows
combine <- rbind(visitdata, vtkmdata)

ggplot(combine, aes(x=occur, fill=app)) + geom_histogram(binwidth=.5, position="dodge")
#ggplot(combine, aes(x=occur, fill=app)) + geom_density(alpha=.3)