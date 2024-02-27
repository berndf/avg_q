import numpy
import mne

def to_Evokeds(epochs, montage='standard_1020'):
 '''epochs is a list of epochs as avg_q.numpy_Script epochs
 '''
 evokeds=[]
 for epoch in epochs:
  info=mne.create_info(ch_names=epoch.channelnames,sfreq=epoch.sfreq,ch_types='eeg')
  # cf https://mne.tools/dev/generated/mne.EvokedArray.html#mne.EvokedArray
  ev=mne.EvokedArray(
   epoch.data.transpose(),
   info,
   tmin= -epoch.beforetrig/epoch.sfreq,
   comment=epoch.comment,
   nave=epoch.nrofaverages,
   kind='average')
  ev.set_montage(montage)
  evokeds.append(ev)
 return evokeds

def mne_Epochsource(mne_epochs):
 import avg_q.numpy_Script
 epochs=[]
 for thisepoch in mne_epochs:
  epoch=avg_q.numpy_Script.numpy_epoch(thisepoch.data.transpose())
  epoch.sfreq=thisepoch.info['sfreq']
  epoch.beforetrig= -round(thisepoch.tmin*thisepoch.info['sfreq'])
  epoch.nrofaverages=thisepoch.nave
  epoch.comment=' '.join([thisepoch.comment,thisepoch.get_channel_types()[0]])
  epoch.channelnames=thisepoch.ch_names
  epoch.channelpos=[x[:3] for x in mne.channels.find_layout(thisepoch.info).pos]
  epochs.append(epoch)
 epochsource=avg_q.numpy_Script.numpy_Epochsource(epochs)
 return epochsource

def SphereSourceSpace_coords(circle_divisions=25, percentage=85):
 # Iter to yield theta and phi coordinates regularly distributed on a sphere
 # (open at the bottom towards theta=90° by 100-percentage)
 # This is separated out to reconstruct those coordinates later if needed
 dphi=numpy.pi/circle_divisions
 for theta in numpy.arange(dphi,numpy.pi*percentage/100,dphi):
  rdphi=dphi/numpy.sin(theta)
  # Correct this angle to avoid irregular (too close) distances between first and last point
  rdphi=2*numpy.pi/numpy.round(2*numpy.pi/rdphi)
  for phi in numpy.arange(0,2*numpy.pi,rdphi):
   yield (theta,phi)

def make_SphereSourceSpace(sphereHM, radius=80, circle_divisions=25, percentage=85):
 '''sphereHM is a fitted spherical head model (mne.make_sphere_model)
    circle_divisions defines the number of sources on a full circle
    percentage is the coverage of the full sphere, leaving out part of its bottom
 '''
 # Try to approximate sources on a sphere surface by using "exclude"
 # Actually mne.setup_source_space() is for distributing sources on a surface
 # but requires FreeSurfer files
 # Source in /usr/lib/python3.11/site-packages/mne/source_space/_source_space.py
 #src1 = mne.setup_volume_source_space(sphere=sphere, pos=10.0, exclude=76) # 10mm grid
 # src is a 1-element list of dict, 'rr' contains the coordinates of *all* originally created sources
 # 'inuse' defines which are actually used

 r=radius/1000
 rr=numpy.array([
  (r*numpy.sin(theta)*numpy.sin(phi),r*numpy.sin(theta)*numpy.cos(phi),r*numpy.cos(theta))
  for theta,phi in SphereSourceSpace_coords(circle_divisions, percentage)
 ])
 np=rr.shape[0]

 ssdict={
  'type': 'discrete',
  'coord_frame': mne._fiff.constants.FIFF.FIFFV_COORD_MRI,
  'id': mne._fiff.constants.FIFF.FIFFV_MNE_SURF_UNKNOWN,
  'np': np,
  'nuse': np,
  'rr': rr+sphereHM['r0'],
  'inuse': numpy.full((np,),True),
  'nn': numpy.array([[0,0,1]]).repeat(np,axis=0),
  'vertno': numpy.arange(np),
  'neighbor_vert': numpy.array([[-1]*26]).repeat(np,axis=0),
  'ntri': 0,
  'use_tris': None,
  'nearest': None,
  'dist': None,
  #'src_mri_t': mne.source_space._source_space._make_voxel_ras_trans(move=numpy.array([0,0,0]),voxel_size=numpy.ones(3),ras=numpy.eye(3)),
 }
 import os
 return mne.SourceSpaces([ssdict], dict(working_dir=os.getcwd(), command_line="None"))

def VolSourceToEvoked(stc, src, r0, ev):
 info=mne.create_info(stc.shape[0], stc.sfreq, ch_types='eeg')
 source_ev=mne.EvokedArray(
  stc.data,
  info,
  tmin=stc.tmin,
  comment=ev.comment,
  nave=ev.nave,
  kind='average')
 montage=ev.get_montage()
 ch_pos={ch_name: src[0]['rr'][int(ch_name)]-r0 for ch_name in source_ev.ch_names}
 dig=mne.channels.make_dig_montage(ch_pos=ch_pos, nasion=montage.dig[1]['r'], lpa=montage.dig[0]['r'], rpa=montage.dig[2]['r'], hsp=None, hpi=None, coord_frame='head')
 source_ev.set_montage(dig)
 return source_ev
