library("ggplot2")
visitdata <- read.csv(file="~/visitdata.csv", head=TRUE, sep=",")
vtkmdata <- read.csv(file="~/vtkmdata.csv", head=TRUE, sep=",")

visitrows <- nrow(visitdata)
vtkmrows <- nrow(vtkmdata)

expected <- visitrows + vtkmrows
combine <- rbind(visitrows, vtkmrows)

if (expected == nrow(combine))
{
  ggplot(dat, aes(x=occur, fill=app)) +
    geom_histogram(binwidth=.5, alpha=.5, position="identity")
}
