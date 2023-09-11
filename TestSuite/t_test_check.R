# Copyright (C) 2009,2010 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

library(bfown)

# Test one-sample t test
r<-matrix(runif(100,min=-0.5,max=0.5)+0.1)
print(t.test(r))
write.avgq(r,"write_generic a.tmp float32
null_sink
-
read_generic -c a.tmp 0 1 float32
average -t
Post:
write_generic stderr string
-
",xdata=F)

# Test two-sample t test
r2<-matrix(runif(20,min=-0.5,max=0.5)+0.2)
print(t.test(r2,r,var.equal=T))
write.avgq(r2,"write_generic a2.tmp float32
null_sink
-
read_generic -c a.tmp 0 1 float32
average -t -u
Post:
writeasc -b a_avg_tu.asc
-
read_generic -c a2.tmp 0 1 float32
average -t -u
Post:
subtract -t a_avg_tu.asc
write_generic stderr string
-
",xdata=F)
