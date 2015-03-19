import os
import shutil
import subprocess
import re
import tempfile
import time
import csv
from optparse import OptionParser

def parse_args():
    parser = OptionParser()
    parser.add_option("-o", "--output-directory", dest="out_dir", default= ".", help="Defines the output directory.")
    parser.add_option("-t", "--trace", action="store_true", dest="do_trace", help="If set, new traces will be created, otherwise only trace muncher runs.")
    parser.add_option("-f", "--force", action="store_true", dest="force_overwrite", help="If set, existing directories and possibly old traces will be overwritten without asking.")
    parser.add_option("-a", "--append", action="store_true", dest="append", help="If set, existing directories won't be deleted, and only runs will run, whose directory doesn't exist yet.")
    parser.add_option("-e", "--executable", dest="executable", default= "", help="Sets the framebender executable to use")
    parser.add_option("-r", "--resourcedir", dest="resourcedir", default= "", help="Sets the resource-dir to use")
    parser.add_option("-s", "--sequencedir", dest="sequencedir", default= "", help="Sets the sequence dir to use")
    return parser.parse_args()

folder_id = 0
num_runs = 0
num_failed_runs = 0
folder_id_to_run_map = []

def run_frame_bender(name_prefix, list_of_parameters):
    global num_runs
    global num_failed_runs
    global folder_id
    global folder_id_to_run_map
    global opts

    folder_id += 1

    folder_name_of_run = name_prefix
    human_read_name_of_run = name_prefix
    command_line_options = []
    enable_upload_download_gl_traces = True

    command_line_options.append("--program.glsl_sources_location=%s/glsl" % opts.resourcedir)
    command_line_options.append("--program.sequences_location=%s" % opts.sequencedir)
    command_line_options.append("--program.textures_location=%s/textures" % opts.resourcedir)

    for option_name, option_parameters in list_of_parameters:
        human_read_name_of_run += " :: "
        folder_name_of_run += "_"
        folder_name_of_run += "".join([x if x.isalnum() else "_" for x in option_name])
        human_read_name_of_run += option_name
        for option_line in option_parameters:
            command_line_options.append(option_line)

    folder_id_to_run_map.append([folder_id, human_read_name_of_run])

    output_directory = os.path.join(opts.out_dir, `folder_id`)
    if not os.path.exists(output_directory):
        os.makedirs(output_directory)
    elif opts.append:
        print "Run " + `folder_id` + " already exists, skipping."
        return

    print "Running Debug run of '" + human_read_name_of_run + "'"

    trace_output_file = os.path.join(output_directory, "trace.fbt")

    frame_bender_dbg_run_args = [
        framebender_executable,
        "-c",
        master_config_file,
        "--logging.output_file="+os.path.abspath(os.path.join(output_directory, "debug_run.log")),
        "--program.config_output_file="+os.path.abspath(os.path.join(output_directory, "debug_run.cfg")),
        "--logging.min_severity=DEBUG",
        "--opengl.context.debug=yes",
        "--profiling.trace_name=" + human_read_name_of_run
    ]

    for opt in command_line_options:
        frame_bender_dbg_run_args.append(opt)

    try:
        subprocess.check_call(frame_bender_dbg_run_args)
    except subprocess.CalledProcessError:
        print "Execution of debug run '" + human_read_name_of_run + "' did not succeed."
        num_failed_runs += 1
        return

    print "Running Trace run of '" + human_read_name_of_run + "'"

    frame_bender_trace_run_args = [
        framebender_executable,
        "-c",
        master_config_file,
        "--logging.output_file="+os.path.abspath(os.path.join(output_directory, "trace_run.log")),
        "--program.config_output_file="+os.path.abspath(os.path.join(output_directory, "trace_run.cfg")),
        "--logging.min_severity=INFO",
        "--opengl.context.debug=no",
        "--profiling.trace_output_file="+os.path.abspath(trace_output_file),
        "--profiling.trace_name=" + human_read_name_of_run
    ]

    for opt in command_line_options:
        frame_bender_trace_run_args.append(opt)

    try:
        subprocess.check_call(frame_bender_trace_run_args)
    except subprocess.CalledProcessError:
        print "Execution of trace run '" + human_read_name_of_run + "' did not succeed."
        num_failed_runs += 1
        return

    num_runs += 1


####
# Man script execution
####


(opts, args) = parse_args()

if opts.do_trace:

    if opts.force_overwrite and opts.append:
        print "Can't have both options: force_overwrite and append!"
        exit()

    if not os.path.exists(opts.out_dir):
        os.makedirs(opts.out_dir)
    else:
        if opts.force_overwrite:
            print "Overwriting old directory."
            shutil.rmtree(opts.out_dir)
        elif opts.append:
            print "Output directory already exists, appending to previous runs."
        else:
            print "Output directory already exists, either delete manually or add '-f' as program option."
            exit()

    # For each quality_* config file

    script_dir = os.path.dirname(os.path.realpath(__file__))
    resources_dir = opts.resourcedir
    framebender_executable = opts.executable

    if not os.path.exists(resources_dir):
        raise RuntimeError("Path " + resources_dir + " does not exist.")

    master_config_file = os.path.join(resources_dir, "resources/benchmark_master.cfg")
    if not os.path.exists(master_config_file):
        raise RuntimeError("Master benchmark config file '" + master_config_file + "' does not exist.")

    input_sequences = [
        ("HD 1080p", ["--input.sequence.width=1920",
                      "--input.sequence.height=1080",
                      "--input.sequence.id=horse_v210_1920_1080p",
                      "--input.sequence.loop_count=10"]),

        ("UHD-1", ["--input.sequence.width=3840",
                   "--input.sequence.height=2160",
                   "--input.sequence.id=rain_fruits_v210_3820_2160p",
                   "--input.sequence.loop_count=40"])]

    optimization_flags = [
        ("single GL context & single threaded", []),

        ("single GL context & async host copies", ["--pipeline.optimization_flags=ASYNC_INPUT",
                                                   "--pipeline.optimization_flags=ASYNC_OUTPUT"]),

        ("multiple GL contexts & async host copies", ["--pipeline.optimization_flags=ASYNC_INPUT",
                                                     "--pipeline.optimization_flags=ASYNC_OUTPUT",
                                                     "--pipeline.optimization_flags=MULTIPLE_GL_CONTEXTS"]),
    ]

    optimization_flags.append(("single GL context & ARB persistent memory & async host copies", ["--pipeline.optimization_flags=ASYNC_INPUT",
                                                                         "--pipeline.optimization_flags=ASYNC_OUTPUT",
                                                                         "--pipeline.optimization_flags=ARB_PERSISTENT_MAPPING"]))

    optimization_flags.append(("multiple GL contexts & ARB persistent memory & async host copies", ["--pipeline.optimization_flags=ASYNC_INPUT",
                                                                         "--pipeline.optimization_flags=ASYNC_OUTPUT",
                                                                         "--pipeline.optimization_flags=MULTIPLE_GL_CONTEXTS",
                                                                         "--pipeline.optimization_flags=ARB_PERSISTENT_MAPPING"]))


    optimization_flags_multi_gl_and_both_async_idx = 1

    optimization_flags_throughput_runs = [

        ("async host copies", ["--pipeline.optimization_flags=ASYNC_INPUT",
                               "--pipeline.optimization_flags=ASYNC_OUTPUT"]),

        ("multiple GL contexts & async host copies", ["--pipeline.optimization_flags=ASYNC_INPUT",
                                                         "--pipeline.optimization_flags=ASYNC_OUTPUT",
                                                         "--pipeline.optimization_flags=MULTIPLE_GL_CONTEXTS"]),

    ]

    optimization_flags_best_key, optimization_flags_best_value = optimization_flags_throughput_runs[optimization_flags_multi_gl_and_both_async_idx]

    optimization_flags_throughput_runs.append(("single GL Context & ARB persistent memory & async host copies", ["--pipeline.optimization_flags=ASYNC_INPUT",
                                                                         "--pipeline.optimization_flags=ASYNC_OUTPUT",
                                                                         "--pipeline.optimization_flags=ARB_PERSISTENT_MAPPING"]))

    optimization_flags_throughput_runs.append(("multiple GL Contexts & ARB persistent memory & async host copies", ["--pipeline.optimization_flags=ASYNC_INPUT",
                                                                         "--pipeline.optimization_flags=ASYNC_OUTPUT",
                                                                         "--pipeline.optimization_flags=MULTIPLE_GL_CONTEXTS",
                                                                         "--pipeline.optimization_flags=ARB_PERSISTENT_MAPPING"]))


    debug_flags_throughput_runs = [

        ("GPU upload-only" , ["--debug.output_is_enabled=false",
                             "--debug.input_is_enabled=true",
                             "--debug.rendering_is_enabled=false",
                             "--debug.host_input_copy_is_enabled=false",
                             "--debug.host_output_copy_is_enabled=false"]),

        ("GPU download-only", ["--debug.output_is_enabled=true",
                              "--debug.input_is_enabled=false",
                              "--debug.rendering_is_enabled=false",
                              "--debug.host_input_copy_is_enabled=false",
                              "--debug.host_output_copy_is_enabled=false"]),

        ("GPU upload & download", ["--debug.output_is_enabled=true",
                                  "--debug.input_is_enabled=true",
                                  "--debug.rendering_is_enabled=false",
                                  "--debug.host_input_copy_is_enabled=false",
                                  "--debug.host_output_copy_is_enabled=false"]),

        ("GPU upload & download & CPU host copying", ["--debug.output_is_enabled=true",
                                                     "--debug.input_is_enabled=true",
                                                     "--debug.rendering_is_enabled=false",
                                                     "--debug.host_input_copy_is_enabled=true",
                                                     "--debug.host_output_copy_is_enabled=true"]),

        ("GPU upload & download & render ", ["--debug.output_is_enabled=true",
                                            "--debug.input_is_enabled=true",
                                            "--debug.rendering_is_enabled=true",
                                            "--debug.host_input_copy_is_enabled=false",
                                            "--debug.host_output_copy_is_enabled=false"]),

        ("GPU upload & download & render & CPU host copying ", ["--debug.output_is_enabled=true",
                                                                "--debug.input_is_enabled=true",
                                                                "--debug.rendering_is_enabled=true",
                                                                "--debug.host_input_copy_is_enabled=true",
                                                                "--debug.host_output_copy_is_enabled=true"]),

    ]

    pipeline_size_configuration_high_throughput_idx = 1

    pipeline_size_configurations = [
        ("low latency pipeline" ,
            ["--pipeline.upload.gl_texture_count=1",
             "--pipeline.upload.gl_pbo_count=3",
             "--pipeline.upload.copy_to_unmap_queue_token_count=2",
             "--pipeline.upload.unmap_to_unpack_queue_token_count=1",
             "--pipeline.download.gl_texture_count=1",
             "--pipeline.download.gl_pbo_count=3",
             "--pipeline.download.pack_to_map_queue_token_count=1",
             "--pipeline.download.map_to_copy_queue_token_count=2",
             "--pipeline.input.token_count=3",
             "--pipeline.output.token_count=3"]),
        ("high throughput pipeline" ,
            ["--pipeline.upload.gl_texture_count=3",
             "--pipeline.upload.gl_pbo_count=5",
             "--pipeline.upload.copy_to_unmap_queue_token_count=2",
             "--pipeline.upload.unmap_to_unpack_queue_token_count=3",
             "--pipeline.download.gl_texture_count=3",
             "--pipeline.download.gl_pbo_count=4",
             "--pipeline.download.pack_to_map_queue_token_count=2",
             "--pipeline.download.map_to_copy_queue_token_count=2",
             "--pipeline.input.token_count=8",
             "--pipeline.output.token_count=8"])
    ]

    input_load_constraint_non_interleaved_idx = 0
    input_load_constraint_all_interleaved_idx = 3

    input_load_constraints = [

        ("non-interleaved", ["--pipeline.download.pack_to_map_load_constraint_count=0",
                                                    "--pipeline.download.format_converter_to_pack_load_constraint_count=0",
                                                    "--pipeline.upload.unmap_to_unpack_load_constraint_count=0",
                                                    "--pipeline.upload.unpack_to_format_converter_load_constraint_count=0"]),

        ("(un)map <-> (un)pack interleaved", ["--pipeline.download.pack_to_map_load_constraint_count=2",
                                             "--pipeline.download.format_converter_to_pack_load_constraint_count=0",
                                             "--pipeline.upload.unmap_to_unpack_load_constraint_count=2",
                                             "--pipeline.upload.unpack_to_format_converter_load_constraint_count=0"]),

        ("transfer <-> render interleaved", ["--pipeline.download.pack_to_map_load_constraint_count=0",
                                            "--pipeline.download.format_converter_to_pack_load_constraint_count=2",
                                            "--pipeline.upload.unmap_to_unpack_load_constraint_count=0",
                                            "--pipeline.upload.unpack_to_format_converter_load_constraint_count=2"]),

        ("(un)map <-> (un)pack & transfer <-> render interleaved", ["--pipeline.download.pack_to_map_load_constraint_count=2",
                                                                     "--pipeline.download.format_converter_to_pack_load_constraint_count=2",
                                                                     "--pipeline.upload.unmap_to_unpack_load_constraint_count=2",
                                                                     "--pipeline.upload.unpack_to_format_converter_load_constraint_count=2"])
    ]

    pipeline_size_configuration_high_throughput_key, pipeline_size_configuration_high_throughput_value = pipeline_size_configurations[pipeline_size_configuration_high_throughput_idx]

    input_load_constraint_non_interleaved_key,input_load_constraint_non_interleaved_value = input_load_constraints[input_load_constraint_non_interleaved_idx]
    input_load_constraint_all_interleaved_key,input_load_constraint_all_interleaved_value = input_load_constraints[input_load_constraint_all_interleaved_idx]

    render_pixel_format_render_runs = [
        ("RGBA 8-bit Integer" , [ "--render.pixel_format=RGBA_8BIT" ]),
        ("RGBA 16-bit Float" , [ "--render.pixel_format=RGBA_FLOAT_16BIT" ]),
        ("RGBA 32-bit Float" , [ "--render.pixel_format=RGBA_FLOAT_32BIT" ])
    ]

    chroma_filter_type_render_runs = [

        ("drop/replicate" , ["--render.format_conversion.v210.decode.chroma_filter_type=none",
                            "--render.format_conversion.v210.encode.chroma_filter_type=none"]),

        ("basic", ["--render.format_conversion.v210.decode.chroma_filter_type=basic",
                  "--render.format_conversion.v210.encode.chroma_filter_type=basic"]),

        ("high", ["--render.format_conversion.v210.decode.chroma_filter_type=high",
                 "--render.format_conversion.v210.encode.chroma_filter_type=high"])

    ]

    glsl_mode_render_runs = [

        ("GLSL 3.3 (FS per fragment)" , ["--render.format_conversion_mode=glsl_330"]),
        ("GLSL 4.2 (imageStore via FS per V210 group)" , ["--render.format_conversion_mode=glsl_420"]),
        ("GLSL 4.2 & ARB_framebuffer_no_attachments (imageStore via FS per V210 group)" , ["--render.format_conversion_mode=glsl_420_no_buffer_attachment_ext"]),
        ("GLSL 4.3 (imageStore via CS per V210 group caching reads in shared memory)" , ["--render.format_conversion_mode=glsl_430_compute"]),
        ("GLSL 4.3 (imageStore via CS per V210 group no caching via shared memory)" , ["--render.format_conversion_mode=glsl_430_compute_no_shared"]),
    ]

    glsl_mode_best_key, glsl_mode_best_value = glsl_mode_render_runs[2]

    glsl_mode_render_runs_compute_only = [

        ("GLSL 4.3 (imageStore via CS per V210 group caching reads in shared memory)" , ["--render.format_conversion_mode=glsl_430_compute"]),
        ("GLSL 4.3 (imageStore via CS per V210 group no caching via shared memory)" , ["--render.format_conversion_mode=glsl_430_compute_no_shared"]),
    ]

    v210_unpack_mode_render_runs = [
        ("RGB10_A2UI internal tex format" , ["--render.format_conversion.v210.bitextraction_in_shader_is_enabled=false"]),
        ("GL_R32UI internal tex format + shader bit ops" , ["--render.format_conversion.v210.bitextraction_in_shader_is_enabled=true"])
    ]

    compute_shader_working_group_sizes_render_runs = [

        ("workgroup size 16" , ["--render.format_conversion.v210.decode.glsl_430_work_group_size=16",
                               "--render.format_conversion.v210.encode.glsl_430_work_group_size=16"]),

        ("workgroup size 32" , ["--render.format_conversion.v210.decode.glsl_430_work_group_size=32",
                               "--render.format_conversion.v210.encode.glsl_430_work_group_size=32"]),

        ("workgroup size 64" , ["--render.format_conversion.v210.decode.glsl_430_work_group_size=64",
                               "--render.format_conversion.v210.encode.glsl_430_work_group_size=64"]),

        ("workgroup size 96" , ["--render.format_conversion.v210.decode.glsl_430_work_group_size=96",
                               "--render.format_conversion.v210.encode.glsl_430_work_group_size=96"]),

        ("workgroup size 128" , ["--render.format_conversion.v210.decode.glsl_430_work_group_size=128",
                               "--render.format_conversion.v210.encode.glsl_430_work_group_size=128"]),
    ]

    start_time = time.time()

    ####
    ## Show
    ####

    # Iterating over resolutions and optimizations flags, wide pipeline mode is fixed

    for input_sequence_key, input_sequence_value in input_sequences:
        for optimization_flag_key, optimization_flag_value in optimization_flags:

            batched_options = [(input_sequence_key, input_sequence_value),
                               (optimization_flag_key, optimization_flag_value),
                               (glsl_mode_best_key, glsl_mode_best_value)]
            run_frame_bender("Threading optimizations", batched_options)

    # Iterating over pipeline size configs, and input load constraints.

    for input_sequence_key, input_sequence_value in input_sequences:
        # Now try different input_load_constraints, and see how that changes the image
        for pipeline_size_configuration_key, pipeline_size_configuration_value in pipeline_size_configurations:

            for input_load_constraint_key, input_load_constraint_value in input_load_constraints:

                # Only run through different interleaved modes, if we are in high-throughput mode
                if pipeline_size_configuration_key != pipeline_size_configuration_high_throughput_key and input_load_constraint_key != input_load_constraint_non_interleaved_key:
                    continue

                batched_options = [
                    (input_sequence_key, input_sequence_value),
                    (pipeline_size_configuration_key, pipeline_size_configuration_value),
                    (input_load_constraint_key, input_load_constraint_value),
                    (optimization_flags_best_key, optimization_flags_best_value),
                    (glsl_mode_best_key, glsl_mode_best_value)
                ]

                run_frame_bender("Pipeline configuration", batched_options)

    # Iterating over switching on/off different stages of the pipeline for througput bottleneck determination (debug)
    # Always using througput-optimized pipeline

    for input_sequence_key, input_sequence_value in input_sequences:
        for optimization_flag_key, optimization_flag_value in optimization_flags_throughput_runs:
            for debug_flags_key, debug_flags_value in debug_flags_throughput_runs:

                batched_options = [ (input_sequence_key, input_sequence_value),
                                    (debug_flags_key, debug_flags_value),
                                    (optimization_flag_key, optimization_flag_value),
                                    (glsl_mode_best_key, glsl_mode_best_value)]

                run_frame_bender("Isolated stages", batched_options)

    for input_sequence_key, input_sequence_value in input_sequences:
        for render_pixel_format_key, render_pixel_format_value in render_pixel_format_render_runs:

                batched_options = [
                    (input_sequence_key, input_sequence_value),
                    (render_pixel_format_key, render_pixel_format_value),
                    (optimization_flags_best_key, optimization_flags_best_value),
                    (glsl_mode_best_key, glsl_mode_best_value)
                ]

                run_frame_bender("Render formats", batched_options)

    for input_sequence_key, input_sequence_value in input_sequences:
        for glsl_mode_render_key, glsl_mode_render_value in glsl_mode_render_runs:
            for chroma_filter_type_key, chroma_filter_type_value in chroma_filter_type_render_runs:

                batched_options = [
                    (input_sequence_key, input_sequence_value),
                    (glsl_mode_render_key, glsl_mode_render_value),
                    (chroma_filter_type_key, chroma_filter_type_value),
                    (optimization_flags_best_key, optimization_flags_best_value)
                ]

                run_frame_bender("GLSL mode & chroma filters", batched_options)

    for input_sequence_key, input_sequence_value in input_sequences:
        for v210_unpack_mode_key, v210_unpack_mode_value in v210_unpack_mode_render_runs:

            batched_options = [
                (input_sequence_key, input_sequence_value),
                (v210_unpack_mode_key, v210_unpack_mode_value),
                (optimization_flags_best_key, optimization_flags_best_value),
                (glsl_mode_best_key, glsl_mode_best_value)
            ]

            run_frame_bender("GLSL V210 10-bit handling", batched_options)

    for input_sequence_key, input_sequence_value in input_sequences:
        for glsl_mode_render_key, glsl_mode_render_value in glsl_mode_render_runs_compute_only:
            for compute_shader_working_group_sizes_key, compute_shader_working_group_sizes_value in compute_shader_working_group_sizes_render_runs:

                batched_options = [
                    (input_sequence_key, input_sequence_value),
                    (compute_shader_working_group_sizes_key, compute_shader_working_group_sizes_value),
                    (glsl_mode_render_key, glsl_mode_render_value),
                    (optimization_flags_best_key, optimization_flags_best_value)
                ]

                run_frame_bender("GLSL compute workgroup sizes", batched_options)

    end_time = time.time()

    csv_file = open(os.path.join(opts.out_dir, '_run_table.csv'), 'wb')

    csv_writer = csv.writer(csv_file)

    for row in folder_id_to_run_map:
        csv_writer.writerow(row)

    csv_file.close()

    print "Ran " + `num_runs` + " number of executions in " + `end_time - start_time` + " seconds."

    if num_failed_runs != 0:
        raise RuntimeError("One or more runs did not run succesfully, please check your logs (" + `num_failed_runs` + " runs failed.)")
# Now render the traces

if not os.path.exists(opts.out_dir):
    print "Output folder '" + opts.out_dir + "' doesn't exist, can't render traces."
    exit()

print "Rendering traces."

for file_folder in os.listdir(opts.out_dir):

    folder_output = os.path.join(opts.out_dir, file_folder)

    if not os.path.isdir(folder_output):
        continue

    trace_file_path = os.path.join(folder_output, "trace.fbt")

    if not os.path.exists(trace_file_path):
        continue

    # Visualize the host trace
    subprocess.check_call([
        "/usr/bin/python2",
        "trace_muncher.py",
        "--input-file",
        trace_file_path,
        "--output-file",
        os.path.join(folder_output, "host_trace.pdf"),
        "--in-point",
        "1000",
        "--frames-count",
        "12",
        "--width",
        "1000"
    ])

    # Visualize the GPU trace
    subprocess.check_call([
        "/usr/bin/python2",
        "trace_muncher.py",
        "--input-file",
        trace_file_path,
        "--output-file",
        os.path.join(folder_output, "gpu_trace.pdf"),
        "--gl-times",
        "--in-point",
        "1000",
        "--frames-count",
        "10",
        "--width",
        "1000"
    ])
