plot.fftfilter<-function(sfreq,boundaries) {
 script<-paste("
 null_source", sfreq, "1 1 1s 1s
 fftfilter -V", boundaries,"
 null_sink
 ")

 library(bfown)

 a <- open.avgq("avg_q_vogl")
 cat(script,"\n-\n!echo -F stdout End of script\\n\nnull_sink\n-\n",
     file=a$fifo_in,sep = "")
 fftsize<-NULL
 freq.resolution<-NULL
 factors<-NULL
 while (TRUE) {
  line <- readLines(a$fifo_out, n = 1, ok = FALSE)
  if (line == "End of script") break
  #print(line)
  r<-sub('^fftfilter: FFT size ([0-9]+) points, Frequency resolution ([.0-9]+)Hz$','\\1 \\2',line)
  if (r!=line) {
   #print(r)
   r<-strsplit(r," ",fixed=T)[[1]]
   fftsize<-as.integer(r[1])
   freq.resolution<-as.double(r[2])
   next
  }
  r<-sub('^fftfilter: Factors\t','',line)
  if (r!=line) {
   s<-strsplit(strsplit(r,"\t",fixed=T)[[1]],":",fixed=T)
   points<-as.vector(lapply(s,function(x) x[1]),mode="double")
   factors<-as.vector(lapply(s,function(x) x[2]),mode="double")
   #print(factors)
   next
  }
 }
 close.avgq(a)

 allpoints<-seq(0,fftsize/2)
 allfactors<-rep(1.0,length(allpoints))
 allfactors[points+1]<-factors
 x<-allpoints*freq.resolution

 plot(x,allfactors,type="l",xlab="Freq[Hz]",ylab="Factor",main=list(paste("fftfilter",boundaries),cex=1))
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
plot.fftfilter(sfreq=200, boundaries="0 0 1Hz 2Hz  48Hz 49Hz 51Hz 52Hz  70Hz 75Hz 1 1")
dev.off()
pdf("fftfilter2.pdf",width=7,height=4)
plot.fftfilter(sfreq=200, boundaries="-0.5 10Hz 15Hz 20Hz 22Hz  48Hz 49Hz 51Hz 52Hz  -2 70Hz 75Hz 1 1")
dev.off()
