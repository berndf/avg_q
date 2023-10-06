# Copyright (C) 2008-2011,2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).

# Entry is [schemas,booknos,channelnames] where each is a list of strings
setups=[
 # 7 Channels= `DAISY'
 [['DAISY'], [],
  ["vEOG", "hEOGl", "hEOGr", "C3", "C4", "EMG", "EKG"]],
 # 8 Channels= `RK-GOOFY'
 [['RK-GOOFY', 'RKGOOFY'], [],
  ["vEOG", "hEOG", "C3", "C4", "EMG", "EKG", "BEMGl", "BEMGr"]],
 # 10 Channels
 [['DAGOBERT'], [],
  ["vEOG", "hEOGl", "hEOGr", "C3", "C4", "Fz", "Cz", "Pz", "EMG", "EKG"]],
 [['BALU'], ['n3434'], # n3434=DAISY
  ["vEOG", "hEOGl", "hEOGr", "C3", "C4", "EMG", "EKG",
  # Nasal airflow, thoracal and abdominal strain gauges:
   "Airflow", "ATMt", "ATMa"]],
 [['PLDAISY'], [],
  ["vEOG", "hEOGl", "hEOGr", "UNK1", "UNK2", "UNK3", "C3", "C4", "EMG", "EKG"]],
 [['GOOFY'], [],
  ["vEOG", "hEOGl", "hEOGr", "C3", "C4", "EMG", "EKG", "BEMGl", "BEMGr", "ATM"]],
 # 11 Channels= `DAGOBERT+ALLES' or RKDagobert?
 [['DAGOBERT','DAG.+ALLES'], [],
  ["vEOG", "hEOG", "C3", "C4", "EMG", "EKG", "BEMGl", "BEMGr",
  # Nasal airflow, thoracal and abdominal strain gauges:
   "Airflow", "ATMt", "ATMa"]],
 # 12 Channels
 [['DAGOBERT','DAG.+ALLES'], [],
  ["vEOG", "hEOGl", "hEOGr", "C3", "C4", "EMG", "EKG", "POLY1", "POLY2", "K25",
   "K26", "ATM"]],
 # 12 Channels= `DAG.+ATM+B'
 [[], ['n3039'],
  ["vEOG", "hEOG", "C3", "C4", "EMG", "EKG", "BEMGl", "BEMGr",
  # Nasal airflow, thoracal and abdominal strain gauges:
   "Airflow", "ATMt", "ATMa", "ATM"]],
 # 14 Channels= `ALADIN'
 [[], [],
  ["vEOG", "hEOGl", "hEOGr", "C3", "C4", "Fz", "Cz", "Pz",
   "POLY1", "POLY2", "K25", "K26", "EMG", "EKG"]],
 # 15 Channels= `DAG.+ATM+B'
 [[], [],
  ["vEOG", "hEOGl", "hEOGr", "C3", "C4", "Fz", "Cz", "Pz", "EMG", "EKG",
   "POLY1", "POLY2", "K25", "K26", "ATM"]],
 # 17 Channnels= `DAGOBERT+DAISY'
 [[], [],
  ["vEOG", "hEOGl", "hEOGr", "C3", "C4", "Fz", "Cz", "Pz", "EMG", "EKG",
   "vEOG", "hEOGl", "hEOGr", "C3", "C4", "EMG", "EKG"]],
]

somno=None
debug=False

import os
def sleep_channels(infile,nr_of_channels):
 from . import bookno
 global somno
 global connection
 if somno is None:
  import klinik
  import sqlalchemy as sa
  klinikdb = sa.create_engine(klinik.klinikurl)
  klinikdb_metadata=sa.MetaData()
  connection=klinikdb.connect()
  somno=sa.Table('somno',klinikdb_metadata,autoload_with=connection)

 infile,ext=os.path.splitext(infile)
 booknumber=os.path.basename(infile).lower()
 eegnr=bookno.database_bookno(booknumber.upper())
 s=connection.execute(somno.select().where(somno.c.bn==eegnr)).fetchall()
 schema=None
 if len(s)==0:
  print("Oops: Can't find %s in database!" % eegnr)
 else:
  schema=s[0].Schema
  if debug:
   print("Schema: " + schema)
 # First look for explicit file associations
 for schemas,booknumbers,channelnames in setups:
  if booknumber in booknumbers:
   if debug:
    print('Found bookno %s, schema %s overridden by %s' % (booknumber, schema, " ".join(schemas)))
   return channelnames
 # Then use the schema
 if schema:
  for schemas,booknumbers,channelnames in setups:
   if schema in schemas and len(channelnames)==nr_of_channels:
    if debug:
     print('Found %d channel schema %s' % (nr_of_channels, schema))
    return channelnames
 # Last resort: Use nr_of_channels only
 for schemas,booknumbers,channelnames in setups:
  if len(channelnames)==nr_of_channels:
   if debug:
    print('Using %d-channel setup for %s, schema=%s' % (nr_of_channels,booknumber,schema))
   return channelnames
 if debug:
  print('Oops, no schema for %d channels, returning enumerated channels for %s' % (nr_of_channels,booknumber))
 return [str(x) for x in range(1,nr_of_channels+1)]
