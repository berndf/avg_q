# Copyright (C) 2011,2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
'''
bookno: Sleep 'book' number conventions
These two functions return a book number in the form without leading
zeros, as used in file names, or with 4-digit number as used in the database.
'''

import re
L_number_regex=re.compile('^L(\d+)$',flags=re.IGNORECASE)

def file_bookno(bookno):
 if isinstance(bookno,str):
  if bookno.strip()=='':
   bookno=None
  else:
   bookno=bookno.upper()
   m=L_number_regex.match(bookno)
   if m:
    bookno="L%d" % int(m.group(1))
 return bookno
def database_bookno(bookno):
 if isinstance(bookno,str):
  if bookno.strip()=='':
   bookno=None
  else:
   bookno=bookno.upper()
   m=L_number_regex.match(bookno)
   if m:
    bookno="L%04d" % int(m.group(1))
 return bookno
