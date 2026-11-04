#!/usr/bin/env python3
"""Build the verified WMV2-only LGPL movie libraries for Linux packaging."""
import argparse
import platform
import linux_ffmpeg
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--jobs',type=int,default=2);a=p.parse_args()
    if platform.system()!='Linux' or not 1<=a.jobs<=8:p.error('Requires Linux and 1–8 jobs')
    base,receipt=linux_ffmpeg.build(jobs=a.jobs);print('Verified Linux media:',base)
