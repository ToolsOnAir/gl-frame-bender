import os
import sys
import gzip
import fbt_format_pb2
import math
import shutil

from optparse import OptionParser

def fixup_mebi_to_mega(folder):

    trace_file = os.path.join(folder, "trace.fbt")

    if os.path.exists(trace_file):

        trace_session = fbt_format_pb2.TraceSession()

        input_file = gzip.GzipFile(trace_file, 'rb')
        trace_session.ParseFromString(input_file.read())
        input_file.close()

        if trace_session.HasField('session_statistic'):
            mebibytes_per_second = trace_session.session_statistic.avg_throughput_mb_per_sec
            megabytes_per_second = (mebibytes_per_second * (1024.0*1024.0)) / 1000000.0

            print "Fixing '" + trace_file + "': Mebibytes/sec " + `mebibytes_per_second` + " -> Megabytes/sec " + `megabytes_per_second`

            trace_session.session_statistic.avg_throughput_mb_per_sec = megabytes_per_second

        f = gzip.GzipFile(trace_file, 'wb')
        f.write(trace_session.SerializeToString())
        f.close()

    for file_folder in os.listdir(folder):
        path = os.path.join(folder, file_folder)
        if os.path.isdir(path):
            fixup_mebi_to_mega(path)

def parse_args():
    parser = OptionParser()
    parser.add_option("-i", "--input-folder", dest="input_folder", default=".", help="Input folder where we look for trace.fbt files recursively.")
    return parser.parse_args()

(opts, args) = parse_args()

if not os.path.exists(opts.input_folder):
    raise RuntimeError("Input path " + opts.input_folder + " does not exist.")

fixup_mebi_to_mega(opts.input_folder)