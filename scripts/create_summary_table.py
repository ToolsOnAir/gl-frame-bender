import os
import shutil
import subprocess
import re
import tempfile
import time
import csv
import gzip
import fbt_format_pb2

from optparse import OptionParser

def ns_to_ms(ns):
    return ns * 1.0e-6

def ns_to_mus(ns):
    return ns * 1.0e-3

def collect_traces(folder):

    global csv_file
    global did_write_header

    trace_file = os.path.join(folder, "trace.fbt")

    if os.path.exists(trace_file):

        trace_session = fbt_format_pb2.TraceSession()

        input_file = gzip.GzipFile(trace_file, 'rb')
        trace_session.ParseFromString(input_file.read())
        input_file.close()

        if not trace_session.HasField('session_statistic'):
            raise RuntimeError("File did not have a session_statistics.")

        megabytes_per_second = trace_session.session_statistic.avg_throughput_mb_per_sec
        frame_processing_time = trace_session.session_statistic.avg_millisecs_per_frame
        latency = ns_to_ms(trace_session.session_statistic.med_frame_processing_time_per_frame_ns)
        name = trace_session.name
        gl_renderer = trace_session.opengl_info.renderer

        BEGIN_EVENT = 7
        END_EVENT = 8


        decode_format_stage_median = 0
        if len(trace_session.stage_traces) > 4:
            decode_format_stage = trace_session.stage_traces[4]
            if decode_format_stage.name == "ConvertFormat":
                lookup = [x for x in decode_format_stage.delta_statistics if (x.begin_event == BEGIN_EVENT and x.end_event == END_EVENT)]
                if len(lookup) != 0:
                    decode_format_stage_stat = lookup[0]
                    decode_format_stage_median = ns_to_mus(decode_format_stage_stat.median_ns)

        render_stage_median = 0
        if len(trace_session.stage_traces) > 5:
            render_stage = trace_session.stage_traces[5]
            if render_stage.name == "Render":
                lookup = [x for x in render_stage.delta_statistics if (x.begin_event == BEGIN_EVENT and x.end_event == END_EVENT)]
                if len(lookup) != 0:
                    render_stage_stat = lookup[0]
                    render_stage_median = ns_to_mus(render_stage_stat.median_ns)


        encode_format_stage_median = 0
        if len(trace_session.stage_traces) > 6:
            encode_format_stage = trace_session.stage_traces[6]
            if encode_format_stage.name == "ConvertFormat":
                lookup = [x for x in encode_format_stage.delta_statistics if (x.begin_event == BEGIN_EVENT and x.end_event == END_EVENT)]
                if len(lookup) != 0:
                    encode_format_stage_stat = lookup[0]
                    encode_format_stage_median = ns_to_mus(encode_format_stage_stat.median_ns)

    
        device_name_path, run_number = os.path.split(folder)
        _, device_configuration_name = os.path.split(device_name_path)
        unique_key = device_configuration_name + "_" + (("%03d") % int(run_number))

        if not did_write_header:
            csv_writer.writerow(["Unique Key",
                                 "Configuration Name",
                                 "GL Renderer",
                                 "Avg. mb/sec",
                                 "Avg. msec/frame",
                                 "Median latency",
                                 "Formatconv-dec (med)",
                                 "Render (med)",
                                 "Formatconv-enc (med)"])
            did_write_header = True

        csv_writer.writerow([unique_key,
                             name,
                             gl_renderer,
                             megabytes_per_second,
                             frame_processing_time,
                             latency,
                             decode_format_stage_median,
                             render_stage_median,
                             encode_format_stage_median])
        print "Processed " + trace_file + " = " + unique_key

    for file_folder in os.listdir(folder):
        path = os.path.join(folder, file_folder)
        if os.path.isdir(path):
            collect_traces(path)


def parse_args():
    parser = OptionParser()
    parser.add_option("-i", "--input-folder", dest="input_folder", default=".", help="Input folder where we look for trace.fbt files recursively.")
    parser.add_option("-o", "--output-folder", dest="output_folder", default=".", help="Output folder.")
    return parser.parse_args()

(opts, args) = parse_args()

if not os.path.exists(opts.input_folder):
    raise RuntimeError("Input path " + opts.input_folder + " does not exist.")

did_write_header = False

csv_file = open(os.path.join(opts.output_folder, 'summary.csv'), 'wb')

csv_writer = csv.writer(csv_file, delimiter=';')

collect_traces(opts.input_folder)

print "Done, good-bye!"

csv_file.close()
