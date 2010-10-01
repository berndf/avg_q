# Copyright (C) 2009,2010 Bernd Feige
# 
# This file is part of avg_q.
# 
# avg_q is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# avg_q is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with avg_q.  If not, see <http://www.gnu.org/licenses/>.
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
