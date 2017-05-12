#!/usr/bin/env Rscript

source("plot_fftfilter.R")

plot.fftfilter.boundaries<-function(sfreq,boundaries) {
 script<-paste("
 null_source", sfreq, "1 1 1s 1s
 fftfilter -V", boundaries,"
 null_sink
 ")

 library(bfown)

 a <- open.avgq("avg_q_vogl")
 cat(script,"\n-\n!echo -F stdout End of script\\n\nnull_sink\n-\n",
     file=a$fifo_in,sep = "")
 plot.fftfilter(a$fifo_out,main=list(paste("fftfilter",boundaries),cex=1))
 close.avgq(a)

 b<-strsplit(boundaries," +")[[1]]
 b<-b[grep("-",b,fixed=T,invert=T)]
 b<-sapply(b,function(x) {r<-sub("Hz","",x); return(if (r!=x) as.double(r) else as.double(x)*sfreq/2)})
 for (i in seq(from=0,to=length(b)-4,by=4)) {
  abline(v=b[i+1],col="blue")
  abline(v=b[i+2],col="blue")
  abline(v=b[i+3],col="red")
  abline(v=b[i+4],col="red")
 }
}

pdf("fftfilter1.pdf",width=7,height=4)
plot.fftfilter.boundaries(sfreq=200, boundaries="0 0 1Hz 2Hz  48Hz 49Hz 51Hz 52Hz  70Hz 75Hz 1 1")
dev.off()
pdf("fftfilter2.pdf",width=7,height=4)
plot.fftfilter.boundaries(sfreq=200, boundaries="-0.5 10Hz 15Hz 20Hz 22Hz  48Hz 49Hz 51Hz 52Hz  -2 70Hz 75Hz 1 1")
dev.off()
