import os
import shutil
import subprocess
import re
import tempfile
from optparse import OptionParser

def parse_args():
    parser = OptionParser()
    parser.add_option("-o", "--output-directory", dest="out_dir", default=".", help="Defines the output directory.")
    return parser.parse_args()

(opts, args) = parse_args()

if not os.path.exists(opts.out_dir):
    os.makedirs(opts.out_dir)

# For each quality_* config file

script_dir = os.path.dirname(os.path.realpath(__file__))
resources_dir = script_dir + "/../resources"

framebender_executable = script_dir + "/../ide/vs2012/Release_x64/FrameBender.exe"

if not os.path.exists(resources_dir):
    raise RuntimeError("Path " + resources_dir + " does not exist.")

quality_config_pattern = re.compile("fbquality_(?P<name>.+).cfg")

configurations = [f for f in os.listdir(resources_dir) if os.path.isfile(os.path.join(resources_dir,f))]

tmp_folder = tempfile.mkdtemp("framebender")

ffmpeg_binary = script_dir + "/../external/ffmpeg/ffmpeg.exe"

for c in configurations:

    pattern_match = quality_config_pattern.match(os.path.basename(c))
    if pattern_match is None:
        continue
    run_name = pattern_match.groupdict()['name']

    print(run_name)

    subprocess.check_call([
        framebender_executable,
        "-c",
        os.path.join(resources_dir,c),
        "--write_output_folder="+tmp_folder
    ])

    # Now use ffmpeg (a bit of a hack, actually) to convert the raw RGBA format
    # into a standard PNG format (not losing any information here, actually)

    subprocess.check_call([
        ffmpeg_binary,
        "-video_size",
        "1920x1080",
        "-vcodec", "rawvideo",
        "-pix_fmt", "rgba64be",
        "-f", "image2",
        "-i", os.path.join(tmp_folder,"00000000.raw"),
        "-vf", "format=rgba64be",
        "-f", "image2",
        os.path.join(opts.out_dir, run_name + ".png")
    ])

shutil.rmtree(tmp_folder)