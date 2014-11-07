from pyreplib import replay
import sys

fname = sys.argv[1]

try:
    replay.Replay(fname)
except:
    print fname, ": bad replay"
