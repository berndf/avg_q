# Copyright (C) 2008,2009 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
avg_q module root directory.
"""

# Allow the avg_q object to be addressed by avg_q.avg_q() instead of
# by avg_q.avg_q.avg_q() ;-)
from .avg_q import avg_q
# Dito for sleep_eeg...
from .sleep_eeg import sleep_eeg
from .avg_q_file import avg_q_file
