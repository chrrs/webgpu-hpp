import yaml
import argparse
import urllib.request
import zipfile
import os

import gen

wgpu_native_version = 'v24.0.0.2'
# specification: https://github.com/webgpu-native/webgpu-headers/tree/bac520839ff5ed2e2b648ed540bd9ec45edbccbc
# comparison to main: https://github.com/webgpu-native/webgpu-headers/compare/bac520839ff5ed2e2b648ed540bd9ec45edbccbc...main

parser = argparse.ArgumentParser('fetch.py')
parser.add_argument('--target', help='target specifier to download / compile', required=True)
parser.add_argument('--bin-dir', help='output directory to put the downloaded target in', required=True)
args = parser.parse_args()

target_dir = f"{args.bin_dir}/{args.target}"
zip_path = target_dir + ".zip"
url = f"https://github.com/gfx-rs/wgpu-native/releases/download/{wgpu_native_version}/{args.target}.zip"

if os.path.exists(zip_path):
    os.remove(zip_path)

# Download the release from GitHub
if not os.path.exists(target_dir):
    print('downloading from', url)
    os.makedirs(args.bin_dir)
    urllib.request.urlretrieve(url, zip_path)

    print('unzipping into', target_dir)
    with zipfile.ZipFile(zip_path, 'r') as z:
        os.makedirs(target_dir)
        z.extractall(target_dir)

    os.remove(zip_path)

spec = None
with open(f"{target_dir}/wgpu-native-meta/webgpu.yml", 'r') as f:
    spec = yaml.safe_load(f)

print('generating webgpu.hpp')
cpp_src = gen.generate_webgpu_hpp(spec)
with open(f"{target_dir}/include/webgpu/webgpu.hpp", 'w') as f:
    f.write(cpp_src)