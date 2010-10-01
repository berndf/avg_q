# ica - Perform Independent Component Analysis, standalone-version
#
#   Run the ICA algorithm of Bell & Sejnowski (1996) or the extended-ICA 
#   of Lee, Girolami & Sejnowski (1998). Original Matlab code: Scott Makeig,
#   Tony Bell, et al.; C++ code: Sigurd Enghoff, CNL / Salk Institute 7/98
#
#   Usage:   % ica < my.sc
#
#   Leading # -> use default values
#   Edit a copy of this file to run an ica decomposition
#   Contacts: {enghoff,scott,terry,tony,tewon}@salk.edu

# Required variables:
 
    DataFile     floatdata # Input data to decompose (floats multiplexed
                           #   by channel (i.e., chan1, chan2, ...))
    chans        16        # Number of data channels (= data rows)   
    datalength   2000      # Number of data points   (= data columns) 

    WeightsOutFile data.wts # Output ICA weight matrix (floats)
    SphereFile   data.sph  # Output sphering matrix (floats)
 
# Processing options:
 
#   sphering     on        # Flag sphering of data (on/off)   {default: on}
#   bias         on        # Perform bias adjustment (on/off) {default: on}
#   extended     1         # Perform "extended-ICA" using tnah() with kurtosis
                           #  estimation every N training blocks. If N < 0,
                           #  fix number of sub-Gaussian components to -N 
                           #  {default|0: off}
#   pca          0         # Decompose a principal component subspace of
                           #  the data. Retain this many PCs. {default|0: all}
# Optional input variables:
 
#  WeightsInFile input.wts # Starting ICA weight matrix (nchans,ncomps)
                           #  {default: identity or sphering matrix}
#   lrate        1.0e-4    # Initial ICA learning rate (float << 1)
                           #  {default: heuristic ~5e-4}
#   blocksize    20        # ICA block size (integer << datalength) 
                           #  {default: heuristic fraction of log data length}
#   stop         1.0e-6    # Stop training when weight-change < this value
                           #  {default: heuristic ~0.000001}
#   maxsteps     128       # Max. number of ICA training steps {default: 128}
#   posact       on        # Make each component activation net-positive
                           # (on/off) {default: on}
#   annealstep   0.98      # Annealing factor (range (0,1]) - controls 
                           #  the speed of convergence.
#   annealdeg    60        # Angledelta threshold for annealing {default: 60}
#   momentum     0.0       # Momentum gain (range [0,1])      {default: 0}
#   verbose      on        # Give ascii messages (on/off) {default: on}
 
# Optional outputs:
 
#  ActivationsFile data.act # Activations of each component (ncomps,points)
#  BiasFile      data.bs   # Bias weights (ncomps,1)
#  SignFile      data.sgn  # Signs designating (-1) sub- and (1) super-Gaussian 
                           #  components (ncomps,1)

# This script, "ica.sc" is a sample ica script file. Copy and modify it as
# desired. Note that the input data file(s) must be native floats.
