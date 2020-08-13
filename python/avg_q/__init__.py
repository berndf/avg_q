# Copyright (C) 2008-2011,2013 Bernd Feige
# This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
"""
avg_q module root directory.
"""

__all__=[
 'avg_q',
 'Script',
 'Epochsource',
 'avg_q_file',
 'escape_filename',
 'escape_channelname',
 'unescape_channelname',
 'channel_list2arg',
 'channel_arg2list',
]
# Allow the avg_q object to be addressed by avg_q.avg_q() instead of
# by avg_q.avg_q.avg_q() ;-)
from .avg_q import avg_q,Script,Epochsource,escape_filename,escape_channelname,unescape_channelname,channel_list2arg,channel_arg2list
from .avg_q_file import avg_q_file
