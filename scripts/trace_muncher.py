import os
import sys
import gzip
import fbt_format_pb2
import cairo
import math

from optparse import OptionParser

def parse_args():
    parser = OptionParser()
    parser.add_option("-i", "--input-file", dest="input_file", default="trace.fbt", help="Input FBT file.")
    parser.add_option("-o", "--output-file", dest="output_file", default="trace.pdf",
                      help="Output file.")
    parser.add_option("-g", "--gl-times", action="store_true", dest="use_gl_times", help="Use GL events instead of CPU events")
    parser.add_option("-f", "--in-point", dest="in_point", default=0, help="In-point into frames.")
    parser.add_option("-n", "--frames-count", dest="number_of_frames", default=0, help="How many frames to visualize.")
    parser.add_option("-t", "--title", dest="title", help="Overrides the title of the diagram.")
    parser.add_option("-w", "--width", dest="width", help="Forces width to normalize to.")
    return parser.parse_args()


def open_session(filepath):
    # Open the input file
    if not os.path.exists(filepath):
        raise RuntimeError("Input file " + filepath + " does not exist.")

    trace_session = fbt_format_pb2.TraceSession()

    input_file = gzip.GzipFile(opts.input_file, 'rb')
    trace_session.ParseFromString(input_file.read())
    input_file.close()

    return trace_session

def inch_to_mm(inch):
    return inch*25.4

def mm_to_inch(mm):
    return mm * (1/25.4)

def ns_to_ms(ns):
    return ns * 1.0e-6

def ns_to_mus(ns):
    return ns * 1.0e-3

def color_8bit_normalize(r, g, b):
    return r/255.0, g/255.0, b/255.0

def draw_rounded_rect(ctx, x,y,w,h, r):
    if h < r * 2:
        r = h/2
    if w < r * 2:
        r = w/2

    ctx.move_to(x, y+r)
    ctx.arc(x+r,y+r, r, math.pi, math.pi * 1.5)
    ctx.line_to(x+w-r, y)
    ctx.arc(x+w-r,y+r, r, math.pi * -0.5, math.pi * 0.0)
    ctx.line_to(x+w, y+h-r)
    ctx.arc(x+w-r, y+h-r, r, math.pi * 0.0, math.pi * 0.5)
    ctx.line_to(x+r, y+h)
    ctx.arc(x+r, y+h-r, r, math.pi * 0.5, math.pi * 1.0)
    #ctx.line_to(x, y+r)
    ctx.close_path()
    ctx.fill()

def select_font_for_stage_medians(ctx):
    ctx.select_font_face ("Helvetica",
                          cairo.FONT_SLANT_NORMAL,
                          cairo.FONT_WEIGHT_BOLD)
    ctx.set_font_size(14)

def select_font_for_axis_labels(ctx):
    ctx.select_font_face ("Helvetica",
                          cairo.FONT_SLANT_NORMAL,
                          cairo.FONT_WEIGHT_BOLD)
    ctx.set_font_size(18)

def select_font_for_title(ctx):

    ctx.select_font_face ("Helvetica",
                          cairo.FONT_SLANT_NORMAL,
                          cairo.FONT_WEIGHT_BOLD)
    ctx.set_font_size(24)

def select_font_for_hud(ctx):

    ctx.select_font_face ("Helvetica",
                          cairo.FONT_SLANT_NORMAL,
                          cairo.FONT_WEIGHT_BOLD)
    ctx.set_font_size(16)

def calculate_max_pipeline_axis_width(ctx, stage_traces):
    max_text_width = 0
    select_font_for_axis_labels(ctx)
    for stage_trace in stage_traces:
        _, _, width, height, _, _= ctx.text_extents(stage_trace.name)
        if height > POINTS_HEIGHT_PER_STAGE:
            print "Warning: Pipeline stage labeling is larger than pipeline height in points."
        max_text_width = max(max_text_width, width)
    return max_text_width

def draw_text_centered(ctx, text, location_x, location_y):

    x_bearing, y_bearing, width, height, x_advance, y_advance = ctx.text_extents(text)
    x = location_x-(width/2 + x_bearing)
    y = location_y-(height/2 + y_bearing)

    ctx.move_to(x, y)
    ctx.show_text(text)

def draw_text_right_aligned_vert_centered(ctx, text, location_x, location_y):

    x_bearing, y_bearing, width, height, x_advance, y_advance = ctx.text_extents(text)

    x = location_x-(width + x_bearing)
    y = location_y-(height/2 + y_bearing)

    ctx.move_to(x, y)
    ctx.show_text(text)

def draw_text_left_aligned_vert_centered(ctx, text, location_x, location_y):

    x_bearing, y_bearing, width, height, x_advance, y_advance = ctx.text_extents(text)

    x = location_x
    y = location_y-(height/2 + y_bearing)

    ctx.move_to(x, y)
    ctx.show_text(text)

def calculate_pipeline_canvas_extent(stage_traces):

    global POINTS_WIDTH_PER_MILLI_SECOND

    first_stage_trace = stage_traces[0]
    last_stage_trace = stage_traces[-1]

    begin_trace = [x for x in first_stage_trace.event_traces if (x.type == BEGIN_EVENT)][0]
    if begin_trace is None:
        raise RuntimeError("Could not get begin trace.")

    begin_time_ns = begin_trace.trace_times_ns[CONFIG_FRAME_IN_POINT]
    begin_time_ms = ns_to_ms(begin_time_ns )

    end_trace = [x for x in last_stage_trace.event_traces if (x.type == END_EVENT)][0]
    if end_trace is None:
        raise RuntimeError("Could not match end trace to begin trace.")

    end_time_ns = end_trace.trace_times_ns[CONFIG_FRAME_IN_POINT + CONFIG_NUMBER_OF_FRAMES_TO_VISUALIZE-1]
    end_time_ms = ns_to_ms(end_time_ns)

    time_span_ms = end_time_ms - begin_time_ms

    begin_draw_frame_num = CONFIG_FRAME_IN_POINT
    while begin_draw_frame_num > 0 and end_trace.trace_times_ns[begin_draw_frame_num] > begin_time_ns:
        begin_draw_frame_num -= 1

    end_draw_frame_num = CONFIG_FRAME_IN_POINT + CONFIG_NUMBER_OF_FRAMES_TO_VISUALIZE-1
    while end_draw_frame_num < len(begin_trace.trace_times_ns) and begin_trace.trace_times_ns[end_draw_frame_num] < end_time_ns:
        end_draw_frame_num += 1


    if opts.width is not None:
        TARGET_WIDTH = int(opts.width)
        POINTS_WIDTH_PER_MILLI_SECOND = TARGET_WIDTH / time_span_ms
    else:
        POINTS_WIDTH_PER_MILLI_SECOND = 40

    width_pt = time_span_ms * POINTS_WIDTH_PER_MILLI_SECOND + POINTS_TIME_CANVAS_PADDING_RIGHT
    height_pt = len(stage_traces) * POINTS_COMPLETE_HEIGHT_STAGE

    return width_pt, height_pt, begin_time_ms, end_time_ms, begin_draw_frame_num, end_draw_frame_num

def calculate_title_bar_height(ctx, session):
    select_font_for_title(ctx)
    _, _, _, height, _, _= ctx.text_extents(session.name)
    height = height + 2*POINTS_VERT_PADDING_TITLE_LABEL
    return height

def calculate_time_axis_label_height(ctx):
    select_font_for_axis_labels(ctx)
    _, _, _, height, _, _= ctx.text_extents(PIPELINE_TIME_AXIS_LABEL)
    height = height + 2*POINTS_VERT_PADDING_TIME_AXIS_LABELS
    return height

def draw_stage_phases(ctx, stage_traces, pts_origin_x, pts_origin_y):

    # fill background
    # ctx.rectangle( pts_origin_x, pts_origin_y, POINTS_PIPELINE_CANVAS_EXTENTS_WIDTH, POINTS_PIPELINE_CANVAS_EXTENTS_HEIGHT)
    # ctx.set_source_rgb(0.9, 0.9, 0.9)
    # ctx.fill()

    ctx.save()

    ctx.rectangle(pts_origin_x, pts_origin_y, POINTS_PIPELINE_CANVAS_EXTENTS_WIDTH, POINTS_PIPELINE_CANVAS_EXTENTS_HEIGHT)
    ctx.clip()

    stage_pos_y = POINTS_COMPLETE_HEIGHT_STAGE * len(stage_traces) - POINTS_COMPLETE_HEIGHT_STAGE

    ctx.set_source_rgb(1.0, 0.0, 0.0)

    for stage_trace in stage_traces:

        begin_trace = [x for x in stage_trace.event_traces if (x.type == BEGIN_EVENT)][0]
        if begin_trace is None:
            raise RuntimeError("Could not get begin trace.")

        end_trace = [x for x in stage_trace.event_traces if (x.type == END_EVENT)][0]
        if end_trace is None:
            raise RuntimeError("Could not get begin trace.")

        for nr in range(BEGIN_DRAW_FRAME_NUM, END_DRAW_FRAME_NUM):

            origin_y = stage_pos_y + POINTS_STAGE_VERT_PADDING

            alpha = 1.0
            if nr in range(CONFIG_FRAME_IN_POINT, CONFIG_FRAME_IN_POINT + CONFIG_NUMBER_OF_FRAMES_TO_VISUALIZE):
                color = STAGE_COLORS[nr % len(STAGE_COLORS)]
            else:
                color = color_8bit_normalize(220, 220, 220)
                alpha = 0.5

            rel_begin_time_ms = ns_to_ms(begin_trace.trace_times_ns[nr]) - TIME_ORIGIN_MS
            rel_end_time_ms = ns_to_ms(end_trace.trace_times_ns[nr]) - TIME_ORIGIN_MS

            # Hacky padding for minimum time
            MINIMUM_WIDTH = 2

            phase_pt_width = (rel_end_time_ms - rel_begin_time_ms) * POINTS_WIDTH_PER_MILLI_SECOND

            if phase_pt_width < MINIMUM_WIDTH:
                # How much is missing?
                padding_time = (MINIMUM_WIDTH - phase_pt_width)/POINTS_WIDTH_PER_MILLI_SECOND
                rel_begin_time_ms -= padding_time/2
                rel_end_time_ms += padding_time/2

            ctx.set_source_rgba(color[0], color[1], color[2], alpha)

            # TODO: make nice functions here
            draw_rounded_rect( ctx,
                               pts_origin_x + rel_begin_time_ms * POINTS_WIDTH_PER_MILLI_SECOND,
                               pts_origin_y + origin_y,
                               (rel_end_time_ms - rel_begin_time_ms) * POINTS_WIDTH_PER_MILLI_SECOND,
                               POINTS_HEIGHT_PER_STAGE,
                               3)
            ctx.fill()

        stage_pos_y -= POINTS_COMPLETE_HEIGHT_STAGE

    ctx.restore()

def draw_axes(ctx):

    ARROW_LENGTH = 8

    ctx.set_source_rgb(0.1, 0.1, 0.1)
    ctx.set_line_width(2.0)

    ctx.set_line_cap(cairo.LINE_CAP_ROUND)
    ctx.set_line_join(cairo.LINE_JOIN_ROUND)

    ctx.move_to(POINTS_STAGE_DIAGRAM_ORIGIN_X, POINTS_STAGE_DIAGRAM_ORIGIN_Y + POINTS_PIPELINE_CANVAS_EXTENTS_HEIGHT)
    ctx.rel_line_to(POINTS_PIPELINE_CANVAS_EXTENTS_WIDTH, 0)
    ctx.rel_line_to(-ARROW_LENGTH, ARROW_LENGTH)
    ctx.move_to(POINTS_STAGE_DIAGRAM_ORIGIN_X + POINTS_PIPELINE_CANVAS_EXTENTS_WIDTH, POINTS_STAGE_DIAGRAM_ORIGIN_Y + POINTS_PIPELINE_CANVAS_EXTENTS_HEIGHT)
    ctx.rel_line_to(-ARROW_LENGTH, -ARROW_LENGTH)
    ctx.stroke()

    ctx.move_to(POINTS_STAGE_DIAGRAM_ORIGIN_X, POINTS_STAGE_DIAGRAM_ORIGIN_Y + POINTS_PIPELINE_CANVAS_EXTENTS_HEIGHT)
    ctx.rel_line_to(0, -POINTS_PIPELINE_CANVAS_EXTENTS_HEIGHT)
    # ctx.rel_line_to(-ARROW_LENGTH, ARROW_LENGTH)
    # ctx.move_to(POINTS_STAGE_DIAGRAM_ORIGIN_X, POINTS_STAGE_DIAGRAM_ORIGIN_Y)
    # ctx.rel_line_to(ARROW_LENGTH, ARROW_LENGTH)
    ctx.stroke()

def draw_labels(ctx, stage_traces, origin_x, origin_y):

    stage_pos_y = POINTS_COMPLETE_HEIGHT_STAGE * len(stage_traces) - POINTS_COMPLETE_HEIGHT_STAGE

    for stage_trace in stage_traces:

        statistic = [x for x in stage_trace.delta_statistics if (x.begin_event == BEGIN_EVENT and x.end_event == END_EVENT)][0]

        text_coord_x = origin_x + POINTS_PIPELINE_LABEL_COLUMN_WIDTH - POINTS_HORIZ_PADDING_PIPELINE_AXIS_LABELS
        text_coord_y = origin_y + stage_pos_y + POINTS_COMPLETE_HEIGHT_STAGE/2

        ctx.set_source_rgb(0.6, 0.6, 0.6)
        select_font_for_stage_medians(ctx)

        draw_text_right_aligned_vert_centered(ctx, (("%.0f") % ns_to_mus(statistic.median_ns)) + u" \u03BC" + "s", text_coord_x+3, text_coord_y)

        select_font_for_axis_labels(ctx)
        ctx.set_source_rgb(0.2, 0.2, 0.2)

        draw_text_right_aligned_vert_centered(ctx,
                                              stage_trace.name,
                                              text_coord_x - 50,
                                              origin_y + stage_pos_y + POINTS_COMPLETE_HEIGHT_STAGE/2)
        stage_pos_y -= POINTS_COMPLETE_HEIGHT_STAGE

    draw_text_centered(ctx, PIPELINE_TIME_AXIS_LABEL, origin_x + POINTS_PIPELINE_LABEL_COLUMN_WIDTH + POINTS_PIPELINE_CANVAS_EXTENTS_WIDTH/2, origin_y + POINTS_PIPELINE_CANVAS_EXTENTS_HEIGHT + POINTS_TIME_AXIS_LABEL_HEIGHT/2)

def draw_grid(ctx, origin_x, origin_y, start_time_ms, end_time_ms):

    time_ms = start_time_ms
    ctx.set_source_rgb(0.85,0.85,0.85)
    ctx.set_line_width(1.0)
    ctx.set_line_cap(cairo.LINE_CAP_SQUARE)
    ctx.set_line_join(cairo.LINE_JOIN_MITER)

    while (time_ms < end_time_ms):

        ctx.move_to(origin_x + (time_ms - TIME_ORIGIN_MS) * POINTS_WIDTH_PER_MILLI_SECOND, origin_y)
        ctx.rel_line_to(0, POINTS_PIPELINE_CANVAS_EXTENTS_HEIGHT)
        ctx.stroke()
        time_ms += 1.0

def draw_title(ctx, session):
    select_font_for_title(ctx)

    text = ""
    if opts.title is not None:
        text = opts.title
    else:
        ctx.set_font_size(12)
        text = session.name

    draw_text_centered(ctx, text, POINTS_STAGE_DIAGRAM_ORIGIN_X + POINTS_PIPELINE_CANVAS_EXTENTS_WIDTH / 2, POINTS_VERT_PADDING_TITLE_LABEL + POINTS_TITLE_BAR_HEIGHT/2)

def draw_statistics_hud(ctx, stats):
    select_font_for_hud(ctx)

    loc_x = 2
    loc_y = 20

    # CONTINUE HERE AND CALCULATE AVG. PROCESSING TIME FROM FULL TRACE?

    draw_text_left_aligned_vert_centered(ctx, "Avg. frame time:  " + ("%.2f" % stat.avg_millisecs_per_frame) + " ms", loc_x, loc_y)
    loc_y += 18

    draw_text_left_aligned_vert_centered(ctx, "Avg. throughput:  " + ("%.2f" % stat.avg_throughput_mb_per_sec) + " MB/s", loc_x, loc_y)
    loc_y += 18

    draw_text_left_aligned_vert_centered(ctx, "Median latency:    " + (("%.2f" % ns_to_ms(stat.med_frame_processing_time_per_frame_ns))) + " ms", loc_x, loc_y)
    loc_y += 18
    draw_text_left_aligned_vert_centered(ctx, "Renderer:    " + session.opengl_info.renderer, loc_x, loc_y)

# PIPELINE_TIME_AXIS_LABEL += ",  + ", median of ms/frame = "+ ("%.2f" % ns_to_ms(stat.med_frame_processing_time_per_frame_ns))


####
## MAIN script execution
####

# Here are all the constants for drawing we can tweak
DPI = 72
POINTS_WIDTH_PER_MILLI_SECOND = 25
POINTS_HEIGHT_PER_STAGE = 28
POINTS_STAGE_VERT_PADDING = 4
POINTS_COMPLETE_HEIGHT_STAGE = POINTS_HEIGHT_PER_STAGE + 2*POINTS_STAGE_VERT_PADDING
POINTS_HORIZ_PADDING_PIPELINE_AXIS_LABELS = 10
POINTS_VERT_PADDING_TIME_AXIS_LABELS = 10
POINTS_VERT_PADDING_TITLE_LABEL = 10
POINTS_HORIZ_PADDING_COMPLETE_DIAGRAM = 4

POINTS_PIPELINE_LABEL_COLUMN_WIDTH = 204
POINTS_TIME_AXIS_LABEL_HEIGHT = 50
POINTS_TITLE_BAR_HEIGHT = 100
POINTS_TIME_CANVAS_PADDING_RIGHT = 10

(opts, args) = parse_args()

# Here are some constants which should come from user options
CONFIG_FRAME_IN_POINT = int(opts.in_point)
CONFIG_NUMBER_OF_FRAMES_TO_VISUALIZE = int(opts.number_of_frames)
SKIP_STAGES = ["FrameInput", "FrameOutput"]

show_gl_timer_query_traces = bool(opts.use_gl_times)

timer_name = ""
if show_gl_timer_query_traces:
    BEGIN_EVENT = 7
    END_EVENT = 8
    timer_name = "GPU time"
else:
    BEGIN_EVENT = 4
    END_EVENT = 5
    timer_name = "CPU time"

output_dir = os.path.dirname(opts.output_file)

if output_dir != "" and not os.path.exists(output_dir):
    os.makedirs(output_dir)

session = open_session(opts.input_file)

if not session.HasField("session_statistic"):
    raise RuntimeError("Expecting a session statistic to be present.")

stat = session.session_statistic

# AUX constants
PIPELINE_TIME_AXIS_LABEL = "Slice #" + `CONFIG_FRAME_IN_POINT` + " - #" + `(CONFIG_FRAME_IN_POINT + CONFIG_NUMBER_OF_FRAMES_TO_VISUALIZE)` + " / " + `int(stat.number_of_frames_processed)` + " frames (" + timer_name + " in milliseconds)"


stage_traces = []

for stage in session.stage_traces:

    if stage.name in SKIP_STAGES:
        continue;

    begin_trace = [x for x in stage.event_traces if (x.type == BEGIN_EVENT)]
    if begin_trace is None or (len(begin_trace) == 0):
        continue

    stage_traces.append(stage)

if len(stage_traces) == 0:
    print ("Stage trace is empty, canceling drawing")
    exit(0)

POINTS_PIPELINE_CANVAS_EXTENTS_WIDTH, POINTS_PIPELINE_CANVAS_EXTENTS_HEIGHT, BEGIN_TIME_MS, END_TIME_MS, BEGIN_DRAW_FRAME_NUM, END_DRAW_FRAME_NUM = calculate_pipeline_canvas_extent(stage_traces)

#SHIFT the Time origin a bit
TIME_ORIGIN_MS = BEGIN_TIME_MS - 0.1

POINTS_COMPLETE_WIDTH = POINTS_PIPELINE_CANVAS_EXTENTS_WIDTH + POINTS_PIPELINE_LABEL_COLUMN_WIDTH + POINTS_HORIZ_PADDING_COMPLETE_DIAGRAM*2
POINTS_COMPLETE_HEIGHT = POINTS_PIPELINE_CANVAS_EXTENTS_HEIGHT + POINTS_TIME_AXIS_LABEL_HEIGHT + POINTS_TITLE_BAR_HEIGHT

print "Writing to '" + opts.output_file + "'"

surface = cairo.PDFSurface(opts.output_file, POINTS_COMPLETE_WIDTH, POINTS_COMPLETE_HEIGHT)
# surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, int(POINTS_COMPLETE_WIDTH)*4, int(POINTS_COMPLETE_HEIGHT)*4)

ctx = cairo.Context(surface)

# ctx.scale(4, 4)

font_options = ctx.get_font_options()

font_options.set_antialias(cairo.ANTIALIAS_SUBPIXEL)

ctx.set_font_options(font_options)

POINTS_STAGE_DIAGRAM_ORIGIN_X, POINTS_STAGE_DIAGRAM_ORIGIN_Y = POINTS_PIPELINE_LABEL_COLUMN_WIDTH + POINTS_HORIZ_PADDING_COMPLETE_DIAGRAM, POINTS_TITLE_BAR_HEIGHT

draw_grid(ctx, POINTS_STAGE_DIAGRAM_ORIGIN_X, POINTS_STAGE_DIAGRAM_ORIGIN_Y, TIME_ORIGIN_MS, TIME_ORIGIN_MS + (END_TIME_MS - BEGIN_TIME_MS))

STAGE_COLORS = []
STAGE_COLORS.append(color_8bit_normalize(15, 128, 140))
STAGE_COLORS.append(color_8bit_normalize(108, 140, 38))
STAGE_COLORS.append(color_8bit_normalize(242, 167, 27))
STAGE_COLORS.append(color_8bit_normalize(242, 106, 27))
STAGE_COLORS.append(color_8bit_normalize(217, 24, 24))

draw_stage_phases(ctx, stage_traces, POINTS_STAGE_DIAGRAM_ORIGIN_X, POINTS_STAGE_DIAGRAM_ORIGIN_Y)

draw_axes(ctx)

draw_labels(ctx, stage_traces, POINTS_HORIZ_PADDING_COMPLETE_DIAGRAM, POINTS_TITLE_BAR_HEIGHT)

draw_title(ctx, session)

draw_statistics_hud(ctx, stat)

# surface.write_to_png(file(opts.output_file, 'wb'))

surface.finish()
