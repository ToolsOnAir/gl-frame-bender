import os
import sys
import gzip
import fbt_format_pb2
import math
import shutil

from optparse import OptionParser

def ns_to_ms(ns):
    return ns * 1.0e-6

def add_ms_per_frame(folder):

    trace_file = os.path.join(folder, "trace.fbt")

    if os.path.exists(trace_file):

        trace_session = fbt_format_pb2.TraceSession()

        input_file = gzip.GzipFile(trace_file, 'rb')
        trace_session.ParseFromString(input_file.read())
        input_file.close()

        if trace_session.HasField('session_statistic'):

            BEGIN_EVENT = 4
            END_EVENT = 5

            stats = trace_session.session_statistic

            first_stage_trace = trace_session.stage_traces[0]
            last_stage_trace = trace_session.stage_traces[-1]

            begin_event_trace = [x for x in first_stage_trace.event_traces if (x.type == BEGIN_EVENT)][0]
            if begin_event_trace is None:
                raise RuntimeError("Could not get begin trace.")

            # need to calculate -1, because begin_stage is called once more, before exiting during benchmarking
            in_point = len(begin_event_trace.trace_times_ns) - stats.number_of_frames_processed -1

            print ("In point: " + `int(in_point)`)

            start_ms = ns_to_ms(begin_event_trace.trace_times_ns[in_point])

            begin_event_trace = [x for x in last_stage_trace.event_traces if (x.type == BEGIN_EVENT)][0]
            if begin_event_trace is None:
                raise RuntimeError("Could not get begin trace.")

            end_ms = ns_to_ms(begin_event_trace.trace_times_ns[-1])

            time_span_ms = end_ms - start_ms

            millisecs_per_frame = time_span_ms / stats.number_of_frames_processed

            epsilon = 0.00001
            if stats.HasField('avg_millisecs_per_frame'):
                if not (((millisecs_per_frame - epsilon) < stats.avg_millisecs_per_frame) and (millisecs_per_frame + epsilon) > stats.avg_millisecs_per_frame):
                    raise RuntimeError("Sanity check failed, calculate '" + `millisecs_per_frame` + "' but found '" + `stats.avg_millisecs_per_frame` + "'.")
                else:
                    print ("Already exists, sanity check OK, not overwriting.")

                return

            print "Adding '" + `millisecs_per_frame` + " as msec_per_frame, writing " + trace_file

            trace_session.session_statistic.avg_millisecs_per_frame = millisecs_per_frame

            f = gzip.GzipFile(trace_file, 'wb')
            f.write(trace_session.SerializeToString())
            f.close()

    for file_folder in os.listdir(folder):
        path = os.path.join(folder, file_folder)
        if os.path.isdir(path):
            add_ms_per_frame(path)

def parse_args():
    parser = OptionParser()
    parser.add_option("-i", "--input-folder", dest="input_folder", default=".", help="Input folder where we look for trace.fbt files recursively.")
    return parser.parse_args()

(opts, args) = parse_args()

if not os.path.exists(opts.input_folder):
    raise RuntimeError("Input path " + opts.input_folder + " does not exist.")

add_ms_per_frame(opts.input_folder)