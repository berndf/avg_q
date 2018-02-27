REM Associate show_any with (most of) the known file types.

REM ftype avg_q.File=C:\Python36\python.exe Q:\PPT-A-EKP-Labor\avg_q32\python\show_any %1 %*
ftype avg_q.File=C:\Python36\pythonw.exe C:\avg_q64\python\show_any %1 %*
assoc .avg=avg_q.File
assoc .eeg=avg_q.File
assoc .cnt=avg_q.File
assoc .vhdr=avg_q.File
assoc .asc=avg_q.File
assoc .hdf=avg_q.File
assoc .edf=avg_q.File
assoc .rec=avg_q.File
assoc .bdf=avg_q.File
assoc .emg=avg_q.File
assoc .vpd=avg_q.File
assoc .ebm=avg_q.File
assoc .cfs=avg_q.File
