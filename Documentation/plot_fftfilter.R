plot.fftfilter<-function(infile,add=F,...) {
 fftsize<-NULL
 freq.resolution<-NULL
 factors<-NULL
 while (TRUE) {
  line <- readLines(infile, n = 1, ok = TRUE)
  if (length(line)==0 || line == "End of script") break
  #print(line)
  r<-sub('^fftfilter: FFT size ([0-9]+) points, Frequency resolution ([.0-9]+)Hz$','\\1 \\2',line)
  if (r!=line) {
   #print(r)
   r<-strsplit(r," ",fixed=T)[[1]]
   fftsize<-as.integer(r[1])
   freq.resolution<-as.double(r[2])
   sfreq=fftsize*freq.resolution
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

 allpoints<-seq(0,fftsize/2)
 allfactors<-rep(1.0,length(allpoints))
 allfactors[points+1]<-factors
 x<-allpoints*freq.resolution

 if (add) {
  lines(x,allfactors,...)
 } else {
  plot(x,allfactors,type="l",xlab="Freq[Hz]",ylab="Coefficient",...)
 }
}
