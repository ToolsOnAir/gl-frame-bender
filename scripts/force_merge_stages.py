import os
import sys
import gzip
import fbt_format_pb2
import math
import shutil

from optparse import OptionParser

def merge_stage_traces(ctx, merge_name, traces):
    merged = ctx.stage_traces.add()
    merged.name = merge_name

    print("Merging into " + merge_name)
    for t in traces:
        print("    " + t.name)

    first_trace = traces[0]
    last_trace = traces[-1]

    begin_trace = [x for x in first_trace.event_traces if (x.type == 4)][0]
    end_trace = [x for x in last_trace.event_traces if (x.type == 5)][0]
    new_first_trace = merged.event_traces.add()
    new_first_trace.type = begin_trace.type
    new_first_trace.trace_times_ns.extend(begin_trace.trace_times_ns)
    new_last_trace = merged.event_traces.add()
    new_last_trace.type = end_trace.type
    new_last_trace.trace_times_ns.extend(end_trace.trace_times_ns)

    if [x for x in first_trace.event_traces if (x.type == 7)]:
        begin_trace = [x for x in first_trace.event_traces if (x.type == 7)][0]
        end_trace = [x for x in last_trace.event_traces if (x.type == 8)][0]
        new_first_trace = merged.event_traces.add()
        new_first_trace.type = begin_trace.type
        new_first_trace.trace_times_ns.extend(begin_trace.trace_times_ns)
        new_last_trace = merged.event_traces.add()
        new_last_trace.type = end_trace.type
        new_last_trace.trace_times_ns.extend(end_trace.trace_times_ns)


    # We just take over the delta statistics of the first frame, this is still quite a hack
    merged.delta_statistics.extend(traces[0].delta_statistics)

def parse_args():
    parser = OptionParser()
    parser.add_option("-i", "--input-file", dest="input_file", default=".", help="Input file to merge.")
    parser.add_option("-o", "--output-file", dest="output_file", default=".", help="Output file.")
    return parser.parse_args()

(opts, args) = parse_args()

if not os.path.exists(opts.input_file):
    raise RuntimeError("Input file" + opts.input_file + " does not exist.")

trace_file = opts.input_file

trace_session = fbt_format_pb2.TraceSession()

input_file = gzip.GzipFile(trace_file, 'rb')
trace_session.ParseFromString(input_file.read())
input_file.close()

stage_traces = trace_session.stage_traces[:]

del trace_session.stage_traces[:]

merge_stage_traces(trace_session, "Acquire", stage_traces[1:2])
merge_stage_traces(trace_session, "Upload", stage_traces[2:4])

merge_stage_traces(trace_session, "Render", stage_traces[4:7])

merge_stage_traces(trace_session, "Download", stage_traces[7:9])

merge_stage_traces(trace_session, "Deliver", stage_traces[9:11])

f = gzip.GzipFile(opts.output_file, 'wb')
f.write(trace_session.SerializeToString())
f.close()

